import bluetooth from '@ohos.bluetooth';
import { BusinessError } from '@ohos.base';

export interface BleCallbacks {
  onStatus(message: string): void;
  onScanningChanged(isScanning: boolean): void;
  onDeviceFound(name: string | undefined, address: string | undefined): void;
  onConnectionChanged(connected: boolean): void;
  onTemperature(tempC: string | undefined, rawAscii: string): void;
}

export class Kt6368aBle {
  private callbacks: BleCallbacks;
  private isScanning: boolean = false;
  private gattClient: any | undefined;
  private deviceId: string | undefined;
  private ble: any;

  private readonly deviceFoundHandler = (data: Array<any>) => {
    if (!Array.isArray(data) || data.length === 0) {
      return;
    }
    const device = data[0];
    this.deviceId = this.extractDeviceId(device);
    this.callbacks.onDeviceFound(device?.deviceName, this.deviceId);
    this.stopScan();
    if (this.deviceId) {
      this.connect(this.deviceId);
    } else {
      this.callbacks.onStatus('Device id not found in scan result.');
    }
  };

  private readonly connectStateHandler = (state: number) => {
    const connected = state === this.ble.BluetoothProfile?.STATE_CONNECTED || state === 2;
    this.callbacks.onConnectionChanged(connected);
    if (connected) {
      this.callbacks.onStatus('Connected. Discovering services...');
      try {
        this.gattClient?.getServices();
      } catch (err) {
        this.callbacks.onStatus(`Service discovery failed: ${JSON.stringify(err)}`);
      }
    } else {
      this.callbacks.onStatus('Disconnected.');
      this.closeGatt();
    }
  };

  private readonly servicesDiscoveredHandler = () => {
    const service = this.gattClient?.getService(SERVICE_UUID);
    if (!service) {
      this.callbacks.onStatus('Service FFF0 not found.');
      return;
    }
    this.enableNotify(service, NOTIFY_UUID_1);
    this.enableNotify(service, NOTIFY_UUID_2);
  };

  private readonly characteristicChangeHandler = (characteristic: any) => {
    const value = characteristic?.characteristicValue ?? characteristic?.value;
    if (!value) {
      return;
    }
    const ascii = this.bytesToAscii(value);
    const temp = this.parseTemperatureFromAscii(ascii);
    this.callbacks.onTemperature(temp, ascii);
  };

  constructor(callbacks: BleCallbacks) {
    this.callbacks = callbacks;
    this.ble = bluetooth as any;
  }

  startScan(): void {
    if (this.isScanning) {
      this.callbacks.onStatus('Scan already running.');
      return;
    }
    try {
      this.ble.on('BLEDeviceFind', this.deviceFoundHandler);
      this.ble.startBLEScan([{ serviceUuid: SERVICE_UUID }], {
        interval: 0,
        dutyMode: this.ble.ScanDuty?.SCAN_MODE_LOW_LATENCY ?? 0
      });
      this.isScanning = true;
      this.callbacks.onScanningChanged(true);
      this.callbacks.onStatus('Scanning for KT6368A service...');
    } catch (err) {
      const error = err as BusinessError;
      this.callbacks.onStatus(`Scan failed: ${error?.message ?? JSON.stringify(err)}`);
      this.stopScan();
    }
  }

  stopScan(): void {
    if (!this.isScanning) {
      return;
    }
    try {
      this.ble.stopBLEScan();
    } catch (_) {
      // Ignore; scan might already be stopped.
    }
    this.ble.off('BLEDeviceFind', this.deviceFoundHandler);
    this.isScanning = false;
    this.callbacks.onScanningChanged(false);
  }

  disconnect(): void {
    if (!this.gattClient) {
      this.callbacks.onConnectionChanged(false);
      return;
    }
    this.callbacks.onStatus('Disconnecting...');
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
    this.callbacks.onStatus(`Connecting ${deviceId}...`);
    try {
      this.gattClient = this.ble.createGattClientDevice(deviceId);
      this.gattClient.on('connectStateChange', this.connectStateHandler);
      this.gattClient.on('servicesDiscovered', this.servicesDiscoveredHandler);
      this.gattClient.on('characteristicChange', this.characteristicChangeHandler);
      this.gattClient.connect();
    } catch (err) {
      const error = err as BusinessError;
      this.callbacks.onStatus(`Connect failed: ${error?.message ?? JSON.stringify(err)}`);
    }
  }

  private enableNotify(service: any, uuid: string): void {
    const characteristic = service.getCharacteristic(uuid);
    if (!characteristic) {
      this.callbacks.onStatus(`Notify characteristic missing: ${uuid}`);
      return;
    }
    try {
      this.gattClient?.setCharacteristicChangeNotification(characteristic, true);
      this.callbacks.onStatus(`Notify enabled: ${uuid}`);
    } catch (err) {
      this.callbacks.onStatus(`Enable notify failed: ${JSON.stringify(err)}`);
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

  private extractDeviceId(device: any): string | undefined {
    return device?.deviceId ?? device?.deviceAddr ?? device?.address ?? device?.deviceAddress;
  }

  private closeGatt(): void {
    try {
      this.gattClient?.close();
    } catch (_) {
      // Ignore
    }
    this.gattClient = undefined;
  }
}

export const SERVICE_UUID = '0000FFF0-0000-1000-8000-00805F9B34FB';
export const NOTIFY_UUID_1 = '0000FFF1-0000-1000-8000-00805F9B34FB';
export const NOTIFY_UUID_2 = '0000FFF2-0000-1000-8000-00805F9B34FB';
const TEMP_REGEX = /temp=([+-]?\d+(?:\.\d+)?)C/i;
