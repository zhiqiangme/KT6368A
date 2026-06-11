import ble from '@ohos.bluetooth.ble';
import constant from '@ohos.bluetooth.constant';
import { BusinessError } from '@ohos.base';

export interface BleCallbacks {
  onStatus(message: string): void;
  onScanningChanged(isScanning: boolean): void;
  onDeviceFound(name: string | undefined, address: string | undefined): void;
  onConnectionChanged(connected: boolean): void;
  onTemperature(tempC: string | undefined, rawAscii: string): void;
  onDebug(message: string): void;
}

export class Kt6368aBle {
  private callbacks: BleCallbacks;
  private isScanning: boolean = false;
  private gattClient: ble.GattClientDevice | undefined;
  private deviceId: string | undefined;
  private scanTimerId: number | undefined;
  private bestScanResult: ble.ScanResult | undefined;
  private bestTargetResult: ble.ScanResult | undefined;
  private isScanListenerRegistered: boolean = false;

  private readonly deviceFoundHandler = (data: Array<ble.ScanResult>) => {
    if (data.length === 0) {
      return;
    }
    this.callbacks.onDebug(`Scan results: ${data.length}`);
    for (let i = 0; i < data.length; i++) {
      const device = data[i];
      const name = device.deviceName ?? '';
      this.callbacks.onDebug(`Found: ${name} (${device.deviceId}) RSSI=${device.rssi}`);
      if (!this.bestScanResult || device.rssi > this.bestScanResult.rssi) {
        this.bestScanResult = device;
        this.callbacks.onDeviceFound(name, device.deviceId);
      }
      if (this.isTargetName(name)) {
        if (!this.bestTargetResult || device.rssi > this.bestTargetResult.rssi) {
          this.bestTargetResult = device;
          this.callbacks.onDeviceFound(name, device.deviceId);
        }
      }
    }
  };

  private readonly connectStateHandler = (state: ble.BLEConnectionChangeState) => {
    const stateCode = state.state;
    const disconnectReason = state.reason;
    const connected = stateCode === constant.ProfileConnectionState.STATE_CONNECTED;
    const isConnecting = stateCode === constant.ProfileConnectionState.STATE_CONNECTING;
    const isDisconnecting = stateCode === constant.ProfileConnectionState.STATE_DISCONNECTING;
    const isDisconnected = stateCode === constant.ProfileConnectionState.STATE_DISCONNECTED;
    let label = 'unknown';
    if (connected) {
      label = 'connected';
    } else if (isConnecting) {
      label = 'connecting';
    } else if (isDisconnecting) {
      label = 'disconnecting';
    } else if (isDisconnected) {
      label = 'disconnected';
    }
    this.callbacks.onDebug(`Connection state: ${stateCode} (${label}), reason: ${disconnectReason ?? 'none'}`);
    if (connected) {
      this.callbacks.onConnectionChanged(true);
      this.callbacks.onStatus('Connected. Discovering services...');
      this.discoverServices().catch((err: BusinessError) => {
        this.callbacks.onStatus(`Discover services failed: ${err?.message ?? JSON.stringify(err)}`);
      });
    } else if (isConnecting) {
      this.callbacks.onStatus('Connecting...');
    } else if (isDisconnecting) {
      this.callbacks.onStatus('Disconnecting...');
    } else if (isDisconnected) {
      this.callbacks.onConnectionChanged(false);
      if (disconnectReason !== undefined) {
        this.callbacks.onStatus(`Disconnected with reason: ${disconnectReason}`);
        this.callbacks.onDebug(`Disconnected with reason: ${disconnectReason}`);
      } else {
        this.callbacks.onStatus('Disconnected.');
      }
      this.closeGatt();
    }
  };

  private readonly characteristicChangeHandler = (characteristic: ble.BLECharacteristic) => {
    const value = characteristic.characteristicValue;
    if (!value) {
      return;
    }
    const ascii = this.bytesToAscii(value);
    const temp = this.parseTemperatureFromAscii(ascii);
    this.callbacks.onTemperature(temp, ascii);
  };

  constructor(callbacks: BleCallbacks) {
    this.callbacks = callbacks;
  }

  startScan(): void {
    if (this.isScanning) {
      this.callbacks.onStatus('Scan already running.');
      this.callbacks.onDebug('Scan request ignored: already scanning.');
      return;
    }
    try {
      const filters: Array<ble.ScanFilter> = [{ serviceUuid: SERVICE_UUID }];
      const options: ble.ScanOptions = {
        interval: 0,
        dutyMode: ble.ScanDuty.SCAN_MODE_LOW_LATENCY
      };
      this.bestScanResult = undefined;
      this.bestTargetResult = undefined;
      this.callbacks.onDebug('Start BLE scan with FFF0 filter.');
      ble.on('BLEDeviceFind', this.deviceFoundHandler);
      this.isScanListenerRegistered = true;
      ble.startBLEScan(filters, options);
      this.isScanning = true;
      this.callbacks.onScanningChanged(true);
      this.callbacks.onStatus('Scanning for KT6368A service...');
      this.scanTimerId = setTimeout(() => {
        this.stopScan();
        // 名称可能为空或略有差异，优先连接命名匹配设备，否则回退到服务过滤命中的最强信号设备。
        const device = this.bestTargetResult ?? this.bestScanResult;
        if (device) {
          this.deviceId = device.deviceId;
          this.callbacks.onDeviceFound(device.deviceName, this.deviceId);
          this.callbacks.onDebug(`Auto-connect target: ${device.deviceName} (${device.deviceId})`);
          this.connect(device.deviceId);
          return;
        }
        this.callbacks.onStatus('No KT6368A BLE devices found.');
        this.callbacks.onDebug('Scan timeout: no target device.');
      }, 8000);
    } catch (err) {
      const error = err as BusinessError;
      this.callbacks.onStatus(`Scan failed: ${error?.message ?? JSON.stringify(err)}`);
      this.callbacks.onDebug(`Scan failed: ${error?.message ?? JSON.stringify(err)}`);
      this.stopScan();
      this.unregisterScanListener();
    }
  }

  stopScan(): void {
    if (!this.isScanning && !this.isScanListenerRegistered) {
      return;
    }
    this.callbacks.onDebug('Stop BLE scan.');
    if (this.isScanning) {
      try {
        ble.stopBLEScan();
      } catch (_) {
        // Ignore; scan might already be stopped.
      }
    }
    if (this.scanTimerId !== undefined) {
      clearTimeout(this.scanTimerId);
      this.scanTimerId = undefined;
    }
    this.unregisterScanListener();
    this.isScanning = false;
    this.callbacks.onScanningChanged(false);
  }

  // 独立清理扫描监听，确保 startBLEScan 失败时不会残留重复回调。
  private unregisterScanListener(): void {
    if (!this.isScanListenerRegistered) {
      return;
    }
    try {
      ble.off('BLEDeviceFind', this.deviceFoundHandler);
    } catch (_) {
      // Ignore; callback might already be removed.
    }
    this.isScanListenerRegistered = false;
  }

  disconnect(): void {
    if (!this.gattClient) {
      this.callbacks.onConnectionChanged(false);
      return;
    }
    this.callbacks.onStatus('Disconnecting...');
    this.callbacks.onDebug('Disconnect request.');
    try {
      this.gattClient.disconnect();
    } catch (err) {
      this.callbacks.onStatus(`Disconnect failed: ${JSON.stringify(err)}`);
    }
  }

  close(): void {
    this.stopScan();
    this.closeGatt();
  }

  private connect(deviceId: string): void {
    if (this.gattClient) {
      this.closeGatt();
    }
    this.callbacks.onStatus(`Connecting ${deviceId}...`);
    this.callbacks.onDebug(`Connecting to ${deviceId}`);
    try {
      this.gattClient = ble.createGattClientDevice(deviceId);
      this.gattClient.on('BLEConnectionStateChange', this.connectStateHandler);
      this.gattClient.on('BLECharacteristicChange', this.characteristicChangeHandler);
      this.gattClient.connect();
      this.callbacks.onDebug('Connect invoked.');
    } catch (err) {
      const error = err as BusinessError;
      this.callbacks.onStatus(`Connect failed: ${error?.message ?? JSON.stringify(err)}`);
      this.callbacks.onDebug(`Connect failed: ${error?.message ?? JSON.stringify(err)}`);
    }
  }

  private async discoverServices(): Promise<void> {
    if (!this.gattClient) {
      return;
    }
    try {
      const services = await this.gattClient.getServices();
      this.callbacks.onDebug(`Services discovered: ${services.length}`);
      const service = services.find((item) => this.isUuidMatch(item.serviceUuid, SERVICE_UUID));
      if (!service) {
        this.callbacks.onStatus('Service FFF0 not found.');
        this.callbacks.onDebug('Service FFF0 not found.');
        return;
      }
      await this.enableNotify(service, NOTIFY_UUID_1);
      await this.enableNotify(service, NOTIFY_UUID_2);
    } catch (err) {
      this.callbacks.onStatus(`Service discovery failed: ${JSON.stringify(err)}`);
      this.callbacks.onDebug(`Service discovery failed: ${JSON.stringify(err)}`);
    }
  }

  private async enableNotify(service: ble.GattService, uuid: string): Promise<void> {
    const characteristic = service.characteristics.find((item) =>
      this.isUuidMatch(item.characteristicUuid, uuid)
    );
    if (!characteristic) {
      this.callbacks.onStatus(`Notify characteristic missing: ${uuid}`);
      this.callbacks.onDebug(`Notify characteristic missing: ${uuid}`);
      return;
    }
    if (!this.gattClient) {
      return;
    }
    try {
      await this.gattClient.setCharacteristicChangeNotification(characteristic, true);
      this.callbacks.onStatus(`Notify enabled: ${uuid}`);
      this.callbacks.onDebug(`Notify enabled: ${uuid}`);
    } catch (err) {
      const error = err as BusinessError;
      this.callbacks.onStatus(`Enable notify failed: ${error?.message ?? JSON.stringify(err)}`);
      this.callbacks.onDebug(`Enable notify failed: ${error?.message ?? JSON.stringify(err)}`);
    }
  }

  private bytesToAscii(value: ArrayBuffer | Uint8Array): string {
    const bytes = value instanceof Uint8Array ? value : new Uint8Array(value);
    let text = '';
    for (let i = 0; i < bytes.length; i++) {
      text += String.fromCharCode(bytes[i]);
    }
    return text.replace(/\r/g, '').replace(/\n/g, '').trim();
  }

  private parseTemperatureFromAscii(ascii: string): string | undefined {
    const match = TEMP_REGEX.exec(ascii);
    if (!match || match.length < 2) {
      return undefined;
    }
    return match[1];
  }

  private closeGatt(): void {
    try {
      this.gattClient?.off('BLEConnectionStateChange', this.connectStateHandler);
    } catch (_) {
      // Ignore; listener might already be removed.
    }
    try {
      this.gattClient?.off('BLECharacteristicChange', this.characteristicChangeHandler);
    } catch (_) {
      // Ignore; listener might already be removed.
    }
    try {
      this.gattClient?.close();
    } catch (_) {
      // Ignore; device might already be closed.
    }
    this.gattClient = undefined;
  }

  private isUuidMatch(a: string, b: string): boolean {
    return a.toUpperCase() === b.toUpperCase();
  }

  // KT6368A 的广播名可能不带 -BLE 后缀，这里只判断核心型号。
  private isTargetName(name: string): boolean {
    return name.toUpperCase().includes(TARGET_NAME_TOKEN);
  }
}

export const SERVICE_UUID = '0000FFF0-0000-1000-8000-00805F9B34FB';
export const NOTIFY_UUID_1 = '0000FFF1-0000-1000-8000-00805F9B34FB';
export const NOTIFY_UUID_2 = '0000FFF2-0000-1000-8000-00805F9B34FB';
const TEMP_REGEX = /temp=([+-]?\d+(?:\.\d+)?)C/i;
const TARGET_NAME_TOKEN = 'KT6368A';
