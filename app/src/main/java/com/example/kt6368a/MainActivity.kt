package com.example.kt6368a

import android.app.Activity
import android.bluetooth.BluetoothAdapter
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.Button
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.core.content.ContextCompat
import com.example.kt6368a.ui.theme.KT6368ATheme
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsControllerCompat

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        WindowCompat.setDecorFitsSystemWindows(window, false)
        super.onCreate(savedInstanceState)
        window.statusBarColor = android.graphics.Color.TRANSPARENT
        WindowInsetsControllerCompat(window, window.decorView).isAppearanceLightStatusBars = true
        setContent {
            KT6368ATheme {
                BleScreen()
            }
        }
    }
}

private data class BleUiState(
    val status: String = "空闲",
    val deviceName: String? = null,
    val deviceAddress: String? = null,
    val temperatureC: String? = null,
    val rawHex: String? = null,
    val isScanning: Boolean = false,
    val isConnected: Boolean = false,
)

@Composable
private fun BleScreen() {
    val context = LocalContext.current
    val activity = context as Activity
    var uiState by remember { mutableStateOf(BleUiState()) }

    val requiredPermissions = remember { requiredPermissions() }
    val permissionsGranted = requiredPermissions.all { permission ->
        ContextCompat.checkSelfPermission(context, permission) == PackageManager.PERMISSION_GRANTED
    }

    val permissionLauncher = rememberLauncherForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { result ->
        val allGranted = requiredPermissions.all { result[it] == true }
        uiState = uiState.copy(
            status = if (allGranted) "权限已授予" else "缺少权限"
        )
    }

    val enableBluetoothLauncher = rememberLauncherForActivityResult(
        ActivityResultContracts.StartActivityForResult()
    ) {
        // Status is refreshed by the next recomposition.
        uiState = uiState.copy(status = "蓝牙启用请求已完成")
    }

    val bleManager = remember {
        BleManager(context.applicationContext, object : BleManager.Callbacks {
            override fun onStatus(message: String) {
                uiState = uiState.copy(status = message)
            }

            override fun onScanningChanged(isScanning: Boolean) {
                uiState = uiState.copy(isScanning = isScanning)
            }

            override fun onDeviceFound(name: String?, address: String) {
                uiState = uiState.copy(deviceName = name, deviceAddress = address)
            }

            override fun onConnectionChanged(connected: Boolean) {
                uiState = uiState.copy(isConnected = connected)
            }

            override fun onTemperature(tempC: String?, rawHex: String) {
                uiState = uiState.copy(temperatureC = tempC, rawHex = rawHex)
            }
        })
    }

    DisposableEffect(Unit) {
        onDispose { bleManager.close() }
    }

    val bluetoothEnabled = bleManager.isBluetoothEnabled()

    Column(
        modifier = Modifier.fillMaxSize()
    ) {
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .background(
                    Brush.verticalGradient(
                        colors = listOf(Color(0xFFf5efe6), Color(0xFFffffff))
                    )
                )
                .statusBarsPadding()
                .padding(horizontal = 20.dp, vertical = 24.dp)
        ) {
            Column(verticalArrangement = Arrangement.spacedBy(16.dp)) {
                Text(
                    text = "KT6368A 电子温度计",
                    style = MaterialTheme.typography.headlineMedium,
                    fontWeight = FontWeight.SemiBold
                )
                Text(
                    text = "状态：${uiState.status}",
                    style = MaterialTheme.typography.bodyLarge
                )
            }
        }

        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(PaddingValues(20.dp)),
            verticalArrangement = Arrangement.spacedBy(16.dp)
        ) {
            Card(
                modifier = Modifier.fillMaxWidth(),
                colors = CardDefaults.cardColors(containerColor = Color(0xFFF8F6F2)),
                elevation = CardDefaults.cardElevation(defaultElevation = 2.dp)
            ) {
                Column(
                    modifier = Modifier.padding(18.dp),
                    verticalArrangement = Arrangement.spacedBy(12.dp)
                ) {
                    Text(
                        text = "温度（℃）",
                        style = MaterialTheme.typography.titleMedium,
                        color = Color(0xFF6B4E2E)
                    )
                    Text(
                        text = uiState.temperatureC ?: "无",
                        style = MaterialTheme.typography.displaySmall,
                        fontWeight = FontWeight.Bold,
                        color = Color(0xFF8B5A2B)
                    )
                }
            }

            Card(
                modifier = Modifier.fillMaxWidth(),
                colors = CardDefaults.cardColors(containerColor = Color(0xFFFFFFFF)),
                elevation = CardDefaults.cardElevation(defaultElevation = 1.dp)
            ) {
                Column(
                    modifier = Modifier.padding(16.dp),
                    verticalArrangement = Arrangement.spacedBy(10.dp)
                ) {
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.SpaceBetween
                    ) {
                        Text(text = "正在扫描", style = MaterialTheme.typography.bodyMedium)
                        Text(text = if (uiState.isScanning) "是" else "否")
                    }
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.SpaceBetween
                    ) {
                        Text(text = "已连接", style = MaterialTheme.typography.bodyMedium)
                        Text(text = if (uiState.isConnected) "是" else "否")
                    }
                    Text(
                        text = "设备：${uiState.deviceName ?: "未知"}",
                        style = MaterialTheme.typography.bodyMedium
                    )
                    Text(
                        text = "地址：${uiState.deviceAddress ?: "无"}",
                        style = MaterialTheme.typography.bodyMedium
                    )
                    Text(
                        text = "接收数据：${uiState.rawHex ?: "无"}",
                        style = MaterialTheme.typography.bodyMedium
                    )
                }
            }

            if (!permissionsGranted || !bluetoothEnabled) {
                Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                    if (!permissionsGranted) {
                        FilledTonalButton(
                            onClick = { permissionLauncher.launch(requiredPermissions) },
                            modifier = Modifier.weight(1f)
                        ) {
                            Text("请求权限")
                        }
                    }
                    if (!bluetoothEnabled) {
                        FilledTonalButton(
                            onClick = {
                                val intent = Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE)
                                enableBluetoothLauncher.launch(intent)
                            },
                            modifier = Modifier.weight(1f)
                        ) {
                            Text("开启蓝牙")
                        }
                    }
                }
            }

            val primaryButtonLabel = when {
                uiState.isConnected -> "断开连接"
                uiState.isScanning -> "正在扫描..."
                else -> "扫描并连接"
            }
            val primaryButtonEnabled =
                permissionsGranted && bluetoothEnabled && (!uiState.isScanning || uiState.isConnected)

            Button(
                onClick = {
                    if (uiState.isConnected) {
                        bleManager.disconnect()
                    } else {
                        bleManager.startScan()
                    }
                },
                modifier = Modifier
                    .fillMaxWidth()
                    .clip(RoundedCornerShape(14.dp))
                    .widthIn(min = 180.dp),
                enabled = primaryButtonEnabled
            ) {
                Text(primaryButtonLabel)
            }
        }
    }
}

private fun requiredPermissions(): Array<String> {
    return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
        arrayOf(
            android.Manifest.permission.BLUETOOTH_SCAN,
            android.Manifest.permission.BLUETOOTH_CONNECT,
        )
    } else {
        arrayOf(android.Manifest.permission.ACCESS_FINE_LOCATION)
    }
}
