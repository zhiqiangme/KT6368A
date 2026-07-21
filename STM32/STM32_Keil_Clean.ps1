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

# 校验 STM32 Keil MDK 项目特征：存在 MDK-ARM 子目录且其中包含 .uvprojx 工程文件。
$mdkDir = Join-Path $script:Root 'MDK-ARM'
$isMdkProject = $false
if (Test-Path -LiteralPath $mdkDir -PathType Container) {
    $uvprojx = Get-ChildItem -LiteralPath $mdkDir -Force -Filter '*.uvprojx' -File -ErrorAction SilentlyContinue
    if ($null -ne $uvprojx -and $uvprojx.Count -gt 0) {
        $isMdkProject = $true
    }
}
if (-not $isMdkProject) {
    Write-Host '未检测到 STM32 Keil MDK 项目特征（缺少 MDK-ARM 目录或 .uvprojx 工程文件）…跳过清理' -ForegroundColor Yellow
    for ($i = 5; $i -gt 0; $i--) {
        Write-Host "`r$i 秒后自动退出…" -NoNewline -ForegroundColor Yellow
        Start-Sleep -Seconds 1
    }
    Write-Host ''
    exit 0
}

# 将 Keil 裸机构建中间文件移入回收站，保留 opt/sct 等手动配置文件。
function Move-ToRecycleBin {
    param([Parameter(Mandatory = $true)][string] $Path)
    if (-not (Test-Path -LiteralPath $Path)) { return }
    $item = Get-Item -LiteralPath $Path -Force
    Write-Host "[RECYCLE] $($item.FullName)"
    if ($item.PSIsContainer) {
        [Microsoft.VisualBasic.FileIO.FileSystem]::DeleteDirectory($item.FullName, [Microsoft.VisualBasic.FileIO.UIOption]::OnlyErrorDialogs, [Microsoft.VisualBasic.FileIO.RecycleOption]::SendToRecycleBin)
        return
    }
    if (($item.Attributes -band [System.IO.FileAttributes]::ReadOnly) -ne 0) { $item.Attributes = $item.Attributes -band (-bnot [System.IO.FileAttributes]::ReadOnly) }
    [Microsoft.VisualBasic.FileIO.FileSystem]::DeleteFile($item.FullName, [Microsoft.VisualBasic.FileIO.UIOption]::OnlyErrorDialogs, [Microsoft.VisualBasic.FileIO.RecycleOption]::SendToRecycleBin)
}

Write-Host "Moving Keil generated files to recycle bin under: $script:Root"
$patterns = @('*.bak', '*.ddk', '*.edk', '*.lst', '*.lnp', '*.mpf', '*.mpj', '*.obj', '*.omf', '*.plg', '*.rpt', '*.tmp', '*.__i', '*.crf', '*.o', '*.d', '*.axf', '*.tra', '*.dep', 'JLinkLog.txt', '*.iex', '*.htm', '*.map', '*.json', '*.log', '*.dbgconf', '*.LINGZHUNING', '*.Administrator')
$seen = New-Object 'System.Collections.Generic.HashSet[string]' ([System.StringComparer]::OrdinalIgnoreCase)
$cleaned = 0
foreach ($pattern in $patterns) {
    Get-ChildItem -LiteralPath $script:Root -Recurse -Force -File -Filter $pattern -ErrorAction SilentlyContinue | ForEach-Object {
        if ($seen.Add($_.FullName)) {
            Move-ToRecycleBin -Path $_.FullName
            $cleaned++
        }
    }
}
if ($cleaned -eq 0) {
    Write-Host 'No Keil generated files found.'
}
else {
    Write-Host "Done. ($cleaned item(s) recycled)"
}
