import { BleCallbacks, Kt6368aBle } from '../ble/Kt6368aBle';
import abilityAccessCtrl, { Context, PermissionRequestResult, Permissions } from '@ohos.abilityAccessCtrl';
import notificationManager from '@ohos.notificationManager';
import getContext from '@ohos.app.ability.common';

class BleCallbacksImpl implements BleCallbacks {
  private owner: Index;

  constructor(owner: Index) {
    this.owner = owner;
  }

  onStatus(message: string): void {
    this.owner.setStatusFromMessage(message);
  }

  onScanningChanged(isScanning: boolean): void {
    this.owner.isScanning = isScanning;
    if (isScanning) {
      this.owner.status = '扫描中';
      this.owner.statusDetail = '';
    }
  }

  onDeviceFound(name: string | undefined, address: string | undefined): void {
    const label = name ? `${name} (${address ?? 'unknown'})` : (address ?? 'unknown');
    this.owner.statusDetail = `已发现设备：${label}`;
  }

  onConnectionChanged(connected: boolean): void {
    this.owner.isConnected = connected;
    if (connected) {
      this.owner.status = '已连接';
      this.owner.statusDetail = '';
    } else {
      this.owner.status = '已断开';
      this.owner.statusDetail = '';
      this.owner.isReceiving = false;
      this.owner.clearTemperatureNotification();
    }
  }

  onTemperature(tempC: string | undefined, rawAscii: string): void {
    if (tempC) {
      this.owner.temperature = `${tempC}℃`;
      this.owner.updateTemperatureNotification(tempC);
    }
    this.owner.rawValue = rawAscii;
    this.owner.isReceiving = true;
    this.owner.status = '接收温度中';
    this.owner.statusDetail = '';
  }

  onDebug(message: string): void {
    // Debug hidden in release UI.
  }
}

@Entry
@Component
struct Index {
  @State status: string = '待连接';
  @State statusDetail: string = '';
  @State temperature: string = '--';
  @State rawValue: string = '';
  @State isScanning: boolean = false;
  @State isConnected: boolean = false;
  @State isReceiving: boolean = false;
  @State orbShift: boolean = false;
  @State orbShift2: boolean = false;

  private callbacks: BleCallbacks | undefined;
  private ble: Kt6368aBle | undefined;
  private animationTimerId: number | undefined;
  private notificationReady: boolean = false;
  private lastNotifiedTemp: string = '';
  private readonly notificationId: number = 1001;
  private hasNotified: boolean = false;

  async aboutToAppear() {
    if (!this.callbacks) {
      this.callbacks = new BleCallbacksImpl(this);
    }
    if (!this.ble && this.callbacks) {
      this.ble = new Kt6368aBle(this.callbacks);
    }
    await this.requestPermissions();
    this.startOrbAnimation();
  }

  aboutToDisappear() {
    this.ble?.close();
    this.stopOrbAnimation();
  }

  private async requestPermissions(): Promise<void> {
    try {
      const context: Context = getContext(this) as Context;
      const atManager = abilityAccessCtrl.createAtManager();
      const permissions: Array<Permissions> = [
        'ohos.permission.ACCESS_BLUETOOTH',
        'ohos.permission.USE_BLUETOOTH',
        'ohos.permission.DISCOVER_BLUETOOTH',
        'ohos.permission.LOCATION'
      ];
      const tokenId = context.applicationInfo?.accessTokenId;
      if (tokenId !== undefined) {
        let allGranted = true;
        for (let i = 0; i < permissions.length; i++) {
          const result = atManager.checkAccessTokenSync(tokenId, permissions[i]);
          if (result !== abilityAccessCtrl.GrantStatus.PERMISSION_GRANTED) {
            allGranted = false;
            break;
          }
        }
        if (allGranted) {
          this.status = '蓝牙未连接';
          this.statusDetail = '';
          return;
        }
      }
      const result: PermissionRequestResult =
        await atManager.requestPermissionsFromUser(context, permissions);
      let denied = result.authResults && result.authResults.some((item: number) => item !== 0);
      if (!denied && (!result.authResults || result.authResults.length === 0) && tokenId !== undefined) {
        let allGranted = true;
        for (let i = 0; i < permissions.length; i++) {
          const check = atManager.checkAccessTokenSync(tokenId, permissions[i]);
          if (check !== abilityAccessCtrl.GrantStatus.PERMISSION_GRANTED) {
            allGranted = false;
            break;
          }
        }
        denied = !allGranted;
      }
      if (denied) {
        this.status = '蓝牙权限被拒绝';
        this.statusDetail = '';
      } else {
        this.status = '蓝牙未连接';
        this.statusDetail = '';
      }
    } catch (err) {
      this.status = '权限申请失败';
      this.statusDetail = '';
    }
  }

  setStatusFromMessage(message: string): void {
    if (message.includes('Scanning')) {
      this.status = '扫描中';
      this.statusDetail = '';
      return;
    }
    if (message.includes('Connecting')) {
      this.status = '连接中';
      this.statusDetail = '';
      return;
    }
    if (message.includes('Connected')) {
      this.status = '已连接';
      this.statusDetail = '';
      return;
    }
    if (message.includes('Disconnected')) {
      this.status = '已断开';
      this.statusDetail = '';
      return;
    }
    if (message.includes('Notify enabled')) {
      this.status = '接收温度中';
      this.statusDetail = '';
      return;
    }
    this.statusDetail = message;
  }

  private statusPillColor(): number {
    if (this.isReceiving) {
      return 0xDFF4FF;
    }
    if (this.isScanning) {
      return 0xFFF3D6;
    }
    if (this.isConnected) {
      return 0xE7F8EA;
    }
    return 0xF1F3F6;
  }

  private statusTextColor(): number {
    if (this.isReceiving) {
      return 0x0B6AAE;
    }
    if (this.isScanning) {
      return 0xB66A00;
    }
    if (this.isConnected) {
      return 0x1A7F37;
    }
    return 0x5A6B7B;
  }

  private handlePrimaryAction(): void {
    if (this.isConnected) {
      this.ble?.disconnect();
      return;
    }
    if (this.isScanning) {
      this.ble?.stopScan();
      return;
    }
    this.ble?.startScan();
  }

  private async ensureNotificationSlot(): Promise<void> {
    if (this.notificationReady) {
      return;
    }
    try {
      await notificationManager.addSlot(notificationManager.SlotType.CONTENT_INFORMATION);
    } catch (_) {
      // Ignore; slot may already exist.
    }
    this.notificationReady = true;
  }

  updateTemperatureNotification(tempC: string): void {
    if (tempC === this.lastNotifiedTemp) {
      return;
    }
    this.lastNotifiedTemp = tempC;
    this.ensureNotificationSlot().then(() => {
      const content: notificationManager.NotificationContent = {
        notificationContentType: notificationManager.ContentType.NOTIFICATION_CONTENT_BASIC_TEXT,
        normal: {
          title: 'KT6368A 当前温度',
          text: `${tempC}℃`,
          additionalText: '实时体温数据'
        }
      };
      const request: notificationManager.NotificationRequest = {
        id: this.notificationId,
        notificationSlotType: notificationManager.SlotType.CONTENT_INFORMATION,
        isOngoing: true,
        updateOnly: this.hasNotified,
        content
      };
      notificationManager.publish(request).then(() => {
        this.hasNotified = true;
      }).catch(() => {
        // Ignore; notifications may be disabled by the user.
      });
    });
  }

  clearTemperatureNotification(): void {
    this.lastNotifiedTemp = '';
    this.hasNotified = false;
    notificationManager.cancel(this.notificationId).catch(() => {
      // Ignore; notification may not exist.
    });
  }

  private startOrbAnimation(): void {
    if (this.animationTimerId !== undefined) {
      return;
    }
    const loop = () => {
      animateTo({ duration: 5000, curve: Curve.EaseInOut }, () => {
        this.orbShift = !this.orbShift;
        this.orbShift2 = !this.orbShift2;
      });
      this.animationTimerId = setTimeout(loop, 5200);
    };
    loop();
  }

  private stopOrbAnimation(): void {
    if (this.animationTimerId === undefined) {
      return;
    }
    clearTimeout(this.animationTimerId);
    this.animationTimerId = undefined;
  }

  build() {
    Stack() {
      Text('')
        .width(260)
        .height(260)
        .backgroundColor(0xCFE2FF)
        .borderRadius(130)
        .opacity(0.55)
        .position({ x: this.orbShift ? -30 : -60, y: this.orbShift ? -40 : -80 })

      Text('')
        .width(220)
        .height(220)
        .backgroundColor(0xD8F6FF)
        .borderRadius(110)
        .opacity(0.45)
        .position({ x: this.orbShift2 ? 190 : 220, y: this.orbShift2 ? 160 : 120 })

      Column({ space: 18 }) {
        Text('KT6368A 电子温度计')
          .fontSize(24)
          .fontWeight(FontWeight.Bold)
          .fontColor(0x1C2A3A)

        Text(this.status)
          .fontSize(12)
          .fontColor(this.statusTextColor())
          .backgroundColor(this.statusPillColor())
          .padding({ left: 12, right: 12, top: 6, bottom: 6 })
          .borderRadius(14)

        Text(this.statusDetail || ' ')
          .fontSize(12)
          .fontColor(0x6A7685)
          .opacity(this.statusDetail ? 1 : 0)

        Column({ space: 8 }) {
          Text('当前温度')
            .fontSize(12)
            .fontColor(0x6A7685)

          Text(this.temperature)
            .fontSize(42)
            .fontWeight(FontWeight.Bold)
            .fontColor(0x15233A)
        }
        .padding(20)
        .width('88%')
        .backgroundColor(0xFFFFFF)
        .borderRadius(20)
        .shadow({ radius: 20, color: 0x1A2B5A33, offsetX: 0, offsetY: 8 })

        Text(`原始数据：${this.rawValue || ' '}`)
          .fontSize(12)
          .fontColor(0x6A7685)
          .opacity(this.rawValue ? 1 : 0)

        Button(this.isConnected ? '断开连接' : (this.isScanning ? '停止扫描' : '开始扫描'))
          .width('78%')
          .height(46)
          .fontSize(16)
          .fontColor(0xFFFFFF)
          .backgroundColor(this.isConnected ? 0xFF5B57 : 0x2F6BFF)
          .borderRadius(23)
          .shadow({ radius: 18, color: 0x2F6BFF33, offsetX: 0, offsetY: 8 })
          .onClick(() => this.handlePrimaryAction())
      }
      .width('100%')
      .height('100%')
      .justifyContent(FlexAlign.Center)
      .alignItems(HorizontalAlign.Center)
      .padding({ left: 24, right: 24, top: 20, bottom: 20 })
    }
    .width('100%')
    .height('100%')
    .expandSafeArea([SafeAreaType.SYSTEM, SafeAreaType.CUTOUT], [SafeAreaEdge.TOP, SafeAreaEdge.BOTTOM])
    .backgroundColor(0xF6F9FF)
  }
}
