package com.zhiqiangme.kt6368a

import android.annotation.SuppressLint
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothGattService
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.le.BluetoothLeScanner
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.os.Handler
import android.os.Looper
import android.os.ParcelUuid
import java.util.ArrayDeque
import java.util.UUID

class BleManager(
    private val context: Context,
    private val callbacks: Callbacks,
) {
    interface Callbacks {
        fun onStatus(message: String)
        fun onScanningChanged(isScanning: Boolean)
        fun onScanResult(name: String?, address: String, rssi: Int)
        fun onConnectionChanged(connected: Boolean)
        fun onTemperature(tempC: String?, rawAscii: String)
    }

    private val mainHandler = Handler(Looper.getMainLooper())
    private val bluetoothManager = context.getSystemService(BluetoothManager::class.java)
    private val adapter = bluetoothManager.adapter
    private var scanner: BluetoothLeScanner? = null
    private var scanCallback: ScanCallback? = null
    @Volatile
    private var gatt: BluetoothGatt? = null
    private val pendingDescriptors = ArrayDeque<BluetoothGattDescriptor>()
    @Volatile
    private var isWritingDescriptor = false
    @Volatile
    private var isConnecting = false
    private val scanTimeoutRunnable = Runnable { stopScan() }

    fun isBluetoothEnabled(): Boolean {
        return try {
            adapter?.isEnabled == true
        } catch (e: SecurityException) {
            false
        }
    }

    @SuppressLint("MissingPermission")
    fun startScan() {
        if (!isBluetoothEnabled()) {
            postStatus("蓝牙未开启")
            return
        }
        if (scanCallback != null) {
            postStatus("扫描已在进行")
            return
        }
        isConnecting = false

        val filter = ScanFilter.Builder()
            .setServiceUuid(ParcelUuid(SERVICE_UUID))
            .build()
        val settings = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .build()

        scanner = adapter?.bluetoothLeScanner
        scanCallback = object : ScanCallback() {
            override fun onScanFailed(errorCode: Int) {
                postStatus("扫描失败：$errorCode")
                stopScan()
            }

            override fun onScanResult(callbackType: Int, result: ScanResult) {
                val device = result.device ?: return
                postScanResult(device, result.rssi)
                if (!isConnecting) {
                    isConnecting = true
                    stopScan()
                    connect(device)
                }
            }
        }

        try {
            val s = scanner
            if (s == null) {
                postStatus("蓝牙扫描器不可用")
                scanCallback = null
                return
            }
            s.startScan(listOf(filter), settings, scanCallback)
            postScanningChanged(true)
            postStatus("正在扫描 KT6368A...")
            mainHandler.postDelayed(scanTimeoutRunnable, SCAN_TIMEOUT_MS)
        } catch (e: SecurityException) {
            postStatus("缺少蓝牙扫描权限")
            stopScan()
        }
    }

    @SuppressLint("MissingPermission")
    fun stopScan() {
        mainHandler.removeCallbacks(scanTimeoutRunnable)
        val callback = scanCallback ?: return
        try {
            scanner?.stopScan(callback)
        } catch (_: SecurityException) {
            // Ignore; permissions may have been revoked.
        }
        scanCallback = null
        postScanningChanged(false)
    }

    @SuppressLint("MissingPermission")
    fun disconnect() {
        if (gatt == null) {
            postConnectionChanged(false)
            return
        }
        postStatus("正在断开连接...")
        gatt?.disconnect()
    }

    fun close() {
        stopScan()
        closeGatt()
    }

    @SuppressLint("MissingPermission")
    private fun connect(device: BluetoothDevice) {
        closeGatt()
        postStatus("正在连接 ${device.address}...")
        try {
            gatt = device.connectGatt(context, false, gattCallback, BluetoothDevice.TRANSPORT_LE)
        } catch (e: SecurityException) {
            postStatus("缺少蓝牙连接权限")
        }
    }

    private val gattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                postConnectionChanged(true)
                postStatus("已连接，正在发现服务...")
                gatt.discoverServices()
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                isConnecting = false
                postConnectionChanged(false)
                postStatus("已断开")
                closeGatt()
            }
        }

        override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
            if (status != BluetoothGatt.GATT_SUCCESS) {
                postStatus("服务发现失败：$status")
                return
            }

            val service = gatt.getService(SERVICE_UUID)
            if (service == null) {
                postStatus("未找到服务 FFF0")
                return
            }

            enableNotifyForCharacteristic(gatt, service, NOTIFY_UUID_1)
            enableNotifyForCharacteristic(gatt, service, NOTIFY_UUID_2)
        }

        override fun onDescriptorWrite(
            gatt: BluetoothGatt,
            descriptor: BluetoothGattDescriptor,
            status: Int
        ) {
            if (status != BluetoothGatt.GATT_SUCCESS) {
                postStatus("描述符写入失败：$status")
            }
            writeNextDescriptor(gatt)
        }

        @Deprecated("Deprecated in Java")
        override fun onCharacteristicChanged(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic
        ) {
            handleCharacteristic(characteristic.value)
        }

        override fun onCharacteristicChanged(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            value: ByteArray
        ) {
            handleCharacteristic(value)
        }
    }

    @SuppressLint("MissingPermission")
    private fun enableNotifyForCharacteristic(
        gatt: BluetoothGatt,
        service: BluetoothGattService,
        uuid: UUID
    ) {
        val characteristic = service.getCharacteristic(uuid)
        if (characteristic == null) {
            postStatus("未找到通知特征：$uuid")
            return
        }
        val hasNotify =
            characteristic.properties and BluetoothGattCharacteristic.PROPERTY_NOTIFY != 0
        if (!hasNotify) {
            postStatus("特征不支持通知：$uuid")
            return
        }

        val setOk = gatt.setCharacteristicNotification(characteristic, true)
        if (!setOk) {
            postStatus("启用通知失败：$uuid")
            return
        }

        val cccd = characteristic.getDescriptor(CCCD_UUID)
        if (cccd == null) {
            postStatus("未找到 CCCD：$uuid")
            return
        }
        cccd.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
        enqueueDescriptorWrite(gatt, cccd)
    }

    private fun handleCharacteristic(value: ByteArray) {
        val ascii = bytesToAscii(value)
        val temp = parseTemperatureFromAscii(ascii)
        postTemperature(temp, ascii)
    }

    private fun enqueueDescriptorWrite(gatt: BluetoothGatt, descriptor: BluetoothGattDescriptor) {
        synchronized(pendingDescriptors) {
            pendingDescriptors.add(descriptor)
            if (!isWritingDescriptor) {
                writeNextDescriptor(gatt)
            }
        }
    }

    private fun writeNextDescriptor(gatt: BluetoothGatt) {
        val next = synchronized(pendingDescriptors) {
            pendingDescriptors.pollFirst()
        }
        if (next == null) {
            isWritingDescriptor = false
            return
        }
        isWritingDescriptor = true
        val ok = gatt.writeDescriptor(next)
        if (!ok) {
            postStatus("描述符写入被拒绝")
            isWritingDescriptor = false
        }
    }

    @SuppressLint("MissingPermission")
    private fun closeGatt() {
        gatt?.close()
        gatt = null
    }

    private fun postStatus(message: String) {
        mainHandler.post { callbacks.onStatus(message) }
    }

    private fun postScanningChanged(isScanning: Boolean) {
        mainHandler.post { callbacks.onScanningChanged(isScanning) }
    }

    private fun postScanResult(device: BluetoothDevice, rssi: Int) {
        mainHandler.post { callbacks.onScanResult(device.name, device.address, rssi) }
    }

    private fun postConnectionChanged(connected: Boolean) {
        mainHandler.post { callbacks.onConnectionChanged(connected) }
    }

    private fun postTemperature(tempC: String?, rawAscii: String) {
        mainHandler.post { callbacks.onTemperature(tempC, rawAscii) }
    }

    private fun parseTemperatureFromAscii(ascii: String): String? {
        val match = TEMP_REGEX.find(ascii) ?: return null
        val value = match.groupValues.getOrNull(1) ?: return null
        return value
    }

    private fun bytesToAscii(bytes: ByteArray): String {
        val text = String(bytes, Charsets.US_ASCII)
        return text.replace("\r", "").replace("\n", "").trim()
    }

    companion object {
        val SERVICE_UUID: UUID = UUID.fromString("0000FFF0-0000-1000-8000-00805F9B34FB")
        val NOTIFY_UUID_1: UUID = UUID.fromString("0000FFF1-0000-1000-8000-00805F9B34FB")
        val NOTIFY_UUID_2: UUID = UUID.fromString("0000FFF2-0000-1000-8000-00805F9B34FB")
        val CCCD_UUID: UUID = UUID.fromString("00002902-0000-1000-8000-00805F9B34FB")
        private val TEMP_REGEX = Regex("temp=([+-]?\\d+(?:\\.\\d+)?)C", RegexOption.IGNORE_CASE)
        private const val SCAN_TIMEOUT_MS = 30_000L
    }
}

