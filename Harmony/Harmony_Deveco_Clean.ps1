param(
    [string] $ProjectRoot
)

$ErrorActionPreference = 'Stop'
# 双击运行时，出错后保留窗口以便查看错误信息。
trap {
    Write-Host ''
    Write-Host "[ERROR] $($_.Exception.Message)" -ForegroundColor Red
    Write-Host 'Press Enter to exit...'
    Read-Host | Out-Null
    exit 1
}

# 目录自动识别：脚本所在目录名为 scripts 时，取上一级作为项目根目录；否则用脚本所在目录。
$scriptDir = $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $dirName = Split-Path -Path $scriptDir -Leaf
    if ($dirName -ieq 'scripts') {
        $ProjectRoot = Split-Path -Path $scriptDir -Parent
    }
    else {
        $ProjectRoot = $scriptDir
    }
}
$ProjectRoot = $ProjectRoot.Trim().Trim('"')
$script:Root = (Resolve-Path -LiteralPath $ProjectRoot).Path.TrimEnd('\')
Set-Location -LiteralPath $script:Root
Add-Type -AssemblyName Microsoft.VisualBasic

# 校验 DevEco Studio / HarmonyOS 项目特征：
#   1) 根目录存在 build-profile.json5、hvigorfile.ts、oh-package.json5、AppScope\app.json5
#   2) 递归存在 module.json5
function Test-HarmonyProject {
    foreach ($required in @('build-profile.json5', 'hvigorfile.ts', 'oh-package.json5', 'AppScope\app.json5')) {
        if (-not (Test-Path -LiteralPath (Join-Path $script:Root $required))) { return $false }
    }
    $moduleJson = Get-ChildItem -LiteralPath $script:Root -Recurse -Force -File -Filter 'module.json5' -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $moduleJson) { return $false }
    return $true
}
if (-not (Test-HarmonyProject)) {
    Write-Host '未检测到 DevEco Studio / HarmonyOS 项目特征（缺少 build-profile.json5/hvigorfile.ts/oh-package.json5/AppScope\app.json5 或 module.json5）…跳过清理' -ForegroundColor Yellow
    for ($i = 5; $i -gt 0; $i--) {
        Write-Host "`r$i 秒后自动退出…" -NoNewline -ForegroundColor Yellow
        Start-Sleep -Seconds 1
    }
    Write-Host ''
    exit 0
}

# 将 DevEco/HarmonyOS 生成物移入回收站，保留源码与配置。
function Move-ToRecycleBin {
    param([Parameter(Mandatory = $true)][string] $Path)
    if (-not (Test-Path -LiteralPath $Path)) { Write-Host "[SKIP] $Path"; return }
    $item = Get-Item -LiteralPath $Path -Force
    Write-Host "[RECYCLE] $($item.FullName)"
    if ($item.PSIsContainer) {
        [Microsoft.VisualBasic.FileIO.FileSystem]::DeleteDirectory($item.FullName, [Microsoft.VisualBasic.FileIO.UIOption]::OnlyErrorDialogs, [Microsoft.VisualBasic.FileIO.RecycleOption]::SendToRecycleBin)
        return
    }
    if (($item.Attributes -band [System.IO.FileAttributes]::ReadOnly) -ne 0) { $item.Attributes = $item.Attributes -band (-bnot [System.IO.FileAttributes]::ReadOnly) }
    [Microsoft.VisualBasic.FileIO.FileSystem]::DeleteFile($item.FullName, [Microsoft.VisualBasic.FileIO.UIOption]::OnlyErrorDialogs, [Microsoft.VisualBasic.FileIO.RecycleOption]::SendToRecycleBin)
}

Write-Host "[Clean] Project: $script:Root"
$cleaned = 0
foreach ($path in @('.hvigor', '.appanalyzer', '.cxx')) {
    $target = Join-Path $script:Root $path
    if (Test-Path -LiteralPath $target) {
        Move-ToRecycleBin -Path $target
        $cleaned++
    }
}
$names = @('build', '.test')
$dirs = Get-ChildItem -LiteralPath $script:Root -Directory -Recurse -Force -ErrorAction SilentlyContinue |
    Where-Object { $names -contains $_.Name } |
    Sort-Object { $_.FullName.Length } -Descending
foreach ($dir in $dirs) {
    Move-ToRecycleBin -Path $dir.FullName
    $cleaned++
}

if ($cleaned -eq 0) {
    Write-Host '[Clean] No DevEco Studio build artifacts found.'
}
else {
    Write-Host "[Clean] Done. ($cleaned item(s) recycled)"
}
