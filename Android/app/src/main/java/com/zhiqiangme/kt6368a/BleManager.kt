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

/**
 * BLE 蓝牙管理器
 * 负责扫描、连接 KT6368A 蓝牙设备，并接收温度数据
 */
class BleManager(
    private val context: Context,
    private val callbacks: Callbacks,
) {
    /** 回调接口，用于向 UI 层传递蓝牙状态和数据 */
    interface Callbacks {
        fun onStatus(message: String)                // 状态消息
        fun onScanningChanged(isScanning: Boolean)   // 扫描状态变化
        fun onScanResult(name: String?, address: String, rssi: Int) // 扫描到设备
        fun onConnectionChanged(connected: Boolean)  // 连接状态变化
        fun onTemperature(tempC: String?, rawAscii: String)         // 收到温度数据
    }

    private val mainHandler = Handler(Looper.getMainLooper()) // 主线程 Handler，用于回调切线程
    private val bluetoothManager = context.getSystemService(BluetoothManager::class.java)
    private val adapter = bluetoothManager?.adapter             // 蓝牙适配器
    private var scanner: BluetoothLeScanner? = null            // BLE 扫描器
    private var scanCallback: ScanCallback? = null             // 当前扫描回调
    private val gattLock = Any()                               // GATT 操作锁
    @Volatile
    private var gatt: BluetoothGatt? = null                    // 当前 GATT 连接
    private val pendingDescriptors = ArrayDeque<BluetoothGattDescriptor>() // 待写入的描述符队列
    private val pendingValues = ArrayDeque<ByteArray?>()                   // 对应的描述符值队列
    @Volatile
    private var isWritingDescriptor = false                    // 是否正在写入描述符
    @Volatile
    private var isConnecting = false                           // 是否正在连接中
    private val scanTimeoutRunnable = Runnable { stopScan() }  // 扫描超时回调

    /** 检查蓝牙是否已开启 */
    fun isBluetoothEnabled(): Boolean {
        return try {
            adapter?.isEnabled == true
        } catch (e: SecurityException) {
            false
        }
    }

    /** 开始 BLE 扫描，发现设备后自动连接 */
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

            @SuppressLint("MissingPermission")
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

    /** 停止 BLE 扫描 */
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

    /** 断开当前 BLE 连接 */
    @SuppressLint("MissingPermission")
    fun disconnect() {
        val currentGatt = synchronized(gattLock) { gatt }
        if (currentGatt == null) {
            postConnectionChanged(false)
            return
        }
        postStatus("正在断开连接...")
        currentGatt.disconnect()
    }

    /** 释放资源，停止扫描并关闭 GATT 连接 */
    fun close() {
        stopScan()
        closeGatt()
    }

    /** 连接指定 BLE 设备 */
    @SuppressLint("MissingPermission")
    private fun connect(device: BluetoothDevice) {
        closeGatt()
        postStatus("正在连接 ${device.address}...")
        try {
            val newGatt = device.connectGatt(context, false, gattCallback, BluetoothDevice.TRANSPORT_LE)
            synchronized(gattLock) { gatt = newGatt }
        } catch (e: SecurityException) {
            isConnecting = false
            postStatus("缺少蓝牙连接权限")
        }
    }

    /** GATT 回调，处理连接状态变化和服务发现等事件 */
    private val gattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                if (status != BluetoothGatt.GATT_SUCCESS) {
                    isConnecting = false
                    postConnectionChanged(false)
                    postStatus("连接失败：$status")
                    closeGatt()
                    return
                }
                postConnectionChanged(true)
                postStatus("已连接，正在发现服务...")
                gatt.discoverServices()
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                isConnecting = false
                postConnectionChanged(false)
                if (status != BluetoothGatt.GATT_SUCCESS) {
                    postStatus("异常断开：$status")
                } else {
                    postStatus("已断开")
                }
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

    /** 为指定特征启用通知，并写入 CCCD 描述符 */
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
        enqueueDescriptorWrite(gatt, cccd, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE)
    }

    /** 处理收到的特征数据，解析温度值 */
    private fun handleCharacteristic(value: ByteArray) {
        val ascii = bytesToAscii(value)
        val temp = parseTemperatureFromAscii(ascii)
        postTemperature(temp, ascii)
    }

    /** 将描述符写入操作加入队列，串行执行 */
    private fun enqueueDescriptorWrite(
        gatt: BluetoothGatt,
        descriptor: BluetoothGattDescriptor,
        value: ByteArray? = null
    ) {
        synchronized(pendingDescriptors) {
            pendingDescriptors.addLast(descriptor)
            pendingValues.addLast(value)
            if (!isWritingDescriptor) {
                writeNextDescriptor(gatt)
            }
        }
    }

    /** 从队列取出下一个描述符并写入 */
    private fun writeNextDescriptor(gatt: BluetoothGatt) {
        val next: BluetoothGattDescriptor?
        val value: ByteArray?
        synchronized(pendingDescriptors) {
            next = pendingDescriptors.pollFirst()
            value = pendingValues.pollFirst()
            isWritingDescriptor = next != null
        }
        if (next == null) return
        // API 33+ 推荐使用 writeDescriptor(descriptor, value) 替代直接设置 descriptor.value
        val ok = if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.TIRAMISU && value != null) {
            gatt.writeDescriptor(next, value)
        } else {
            if (value != null) next.value = value
            gatt.writeDescriptor(next)
        }
        if (!ok) {
            postStatus("描述符写入被拒绝")
            synchronized(pendingDescriptors) { isWritingDescriptor = false }
        }
    }

    /** 关闭 GATT 连接并释放资源 */
    @SuppressLint("MissingPermission")
    private fun closeGatt() {
        synchronized(gattLock) {
            gatt?.disconnect()
            gatt?.close()
            gatt = null
        }
    }

    // 以下 post 方法将回调切换到主线程执行
    private fun postStatus(message: String) {
        mainHandler.post { callbacks.onStatus(message) }
    }

    private fun postScanningChanged(isScanning: Boolean) {
        mainHandler.post { callbacks.onScanningChanged(isScanning) }
    }

    @SuppressLint("MissingPermission")
    private fun postScanResult(device: BluetoothDevice, rssi: Int) {
        mainHandler.post { callbacks.onScanResult(device.name, device.address, rssi) }
    }

    private fun postConnectionChanged(connected: Boolean) {
        mainHandler.post { callbacks.onConnectionChanged(connected) }
    }

    private fun postTemperature(tempC: String?, rawAscii: String) {
        mainHandler.post { callbacks.onTemperature(tempC, rawAscii) }
    }

    /** 从 ASCII 字符串中解析温度值，格式如 "temp=36.5C" */
    private fun parseTemperatureFromAscii(ascii: String): String? {
        val match = TEMP_REGEX.find(ascii) ?: return null
        val value = match.groupValues.getOrNull(1) ?: return null
        return value
    }

    /** 将字节数组转换为 ASCII 字符串，去除换行和空格 */
    private fun bytesToAscii(bytes: ByteArray): String {
        val text = String(bytes, Charsets.US_ASCII)
        return text.replace("\r", "").replace("\n", "").trim()
    }

    companion object {
        val SERVICE_UUID: UUID = UUID.fromString("0000FFF0-0000-1000-8000-00805F9B34FB")   // KT6368A 主服务 UUID
        val NOTIFY_UUID_1: UUID = UUID.fromString("0000FFF1-0000-1000-8000-00805F9B34FB") // 通知特征 1
        val NOTIFY_UUID_2: UUID = UUID.fromString("0000FFF2-0000-1000-8000-00805F9B34FB") // 通知特征 2
        val CCCD_UUID: UUID = UUID.fromString("00002902-0000-1000-8000-00805F9B34FB")     // CCCD 描述符 UUID
        private val TEMP_REGEX = Regex("temp=([+-]?\\d+(?:\\.\\d+)?)C", RegexOption.IGNORE_CASE) // 温度值正则
        private const val SCAN_TIMEOUT_MS = 30_000L // 扫描超时时间（30秒）
    }
}

