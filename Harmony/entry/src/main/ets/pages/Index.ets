import { Kt6368aBle } from '../ble/Kt6368aBle';
import abilityAccessCtrl from '@ohos.abilityAccessCtrl';
import { getContext } from '@kit.ArkUI';

@Entry
@Component
struct Index {
  @State status: string = 'Ready.';
  @State temperature: string = '--';
  @State rawValue: string = '';
  @State isScanning: boolean = false;
  @State isConnected: boolean = false;

  private ble: Kt6368aBle = new Kt6368aBle({
    onStatus: (message: string) => {
      this.status = message;
    },
    onScanningChanged: (isScanning: boolean) => {
      this.isScanning = isScanning;
    },
    onDeviceFound: (name: string | undefined, address: string | undefined) => {
      const label = name ? `${name} (${address ?? 'unknown'})` : (address ?? 'unknown');
      this.status = `Found device: ${label}`;
    },
    onConnectionChanged: (connected: boolean) => {
      this.isConnected = connected;
    },
    onTemperature: (tempC: string | undefined, rawAscii: string) => {
      if (tempC) {
        this.temperature = `${tempC} C`;
      }
      this.rawValue = rawAscii;
    }
  });

  async aboutToAppear() {
    await this.requestPermissions();
  }

  aboutToDisappear() {
    this.ble.close();
  }

  private async requestPermissions(): Promise<void> {
    try {
      const context = getContext(this) as any;
      const atManager = abilityAccessCtrl.createAtManager();
      const permissions = [
        'ohos.permission.ACCESS_BLUETOOTH',
        'ohos.permission.USE_BLUETOOTH',
        'ohos.permission.DISCOVER_BLUETOOTH',
        'ohos.permission.LOCATION'
      ];
      const result = await atManager.requestPermissionsFromUser(context, permissions);
      const denied = result.authResults?.some((item: number) => item !== 0);
      if (denied) {
        this.status = 'Bluetooth permissions denied.';
      } else {
        this.status = 'Permissions granted. Ready to scan.';
      }
    } catch (err) {
      this.status = `Permission request failed: ${JSON.stringify(err)}`;
    }
  }

  private handlePrimaryAction(): void {
    if (this.isConnected) {
      this.ble.disconnect();
      return;
    }
    if (this.isScanning) {
      this.ble.stopScan();
      return;
    }
    this.ble.startScan();
  }

  build() {
    Column({ space: 16 }) {
      Text('KT6368A Thermometer')
        .fontSize(22)
        .fontWeight(FontWeight.Bold)

      Text(`Status: ${this.status}`)
        .fontSize(14)

      Text(`Temperature: ${this.temperature}`)
        .fontSize(28)
        .fontWeight(FontWeight.Medium)

      if (this.rawValue) {
        Text(`Raw: ${this.rawValue}`)
          .fontSize(12)
      }

      Button(this.isConnected ? 'Disconnect' : (this.isScanning ? 'Stop Scan' : 'Start Scan'))
        .width('80%')
        .onClick(() => this.handlePrimaryAction())
    }
    .width('100%')
    .height('100%')
    .justifyContent(FlexAlign.Center)
    .alignItems(HorizontalAlign.Center)
    .padding(20)
  }
}
