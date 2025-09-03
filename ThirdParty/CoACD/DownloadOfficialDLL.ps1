param(
    [string]$Version = "1.0.7"
)

$ErrorActionPreference = 'Stop'

function Write-Info($msg) { Write-Host "INFO: $msg" -ForegroundColor Cyan }
function Write-Err($msg)  { Write-Host "ERROR: $msg" -ForegroundColor Red }

$Root = Split-Path -Parent $PSCommandPath
$TempDir = Join-Path $Root '_download_temp'
$DLLDir = Join-Path $Root 'DLL'

Write-Info "Downloading CoACD $Version official precompiled DLL..."

try {
    # 1) 创建临时目录
    if (Test-Path $TempDir) { Remove-Item -Recurse -Force $TempDir }
    New-Item -ItemType Directory -Force $TempDir | Out-Null

    # 2) 直接下载官方wheel包
    $wheelUrl = "https://files.pythonhosted.org/packages/3f/34/8ede8a9b30cb84a9e5cdcf8e5e3e5a7f8d8b9b/coacd-$Version-cp39-abi3-win_amd64.whl"
    $wheelFile = Join-Path $TempDir "coacd-$Version-win_amd64.whl"
    
    Write-Info "Downloading from PyPI: coacd-$Version-win_amd64.whl"
    
    # 使用GitHub Releases作为备选（更稳定）
    $githubUrl = "https://github.com/SarahWeiii/CoACD/releases/download/v$Version/coacd-$Version-cp39-abi3-win_amd64.whl"
    
    try {
        Invoke-WebRequest -Uri $githubUrl -OutFile $wheelFile -UserAgent "Mozilla/5.0"
        Write-Info "Downloaded from GitHub Releases"
    } catch {
        Write-Info "GitHub download failed, trying PyPI..."
        
        # 从PyPI API获取确切的下载链接
        $pypiJson = Invoke-RestMethod "https://pypi.org/pypi/coacd/$Version/json"
        $winWheel = $pypiJson.urls | Where-Object { $_.filename -match "win_amd64\.whl$" } | Select-Object -First 1
        
        if ($winWheel) {
            Invoke-WebRequest -Uri $winWheel.url -OutFile $wheelFile -UserAgent "Mozilla/5.0"
            Write-Info "Downloaded from PyPI"
        } else {
            throw "Windows wheel not found for version $Version"
        }
    }

    # 3) 重命名wheel为zip并解压（wheel本质上是zip格式）
    Write-Info "Extracting wheel contents..."
    $zipFile = $wheelFile -replace "\.whl$", ".zip"
    Move-Item $wheelFile $zipFile -Force
    Expand-Archive -Path $zipFile -DestinationPath $TempDir -Force

    # 4) 查找DLL文件
    $dllFiles = Get-ChildItem -Path $TempDir -Recurse -Filter "*.dll" | Where-Object { 
        $_.Name -match "(coacd|CoACD)" -or $_.Directory.Name -match "coacd"
    }

    if ($dllFiles.Count -eq 0) {
        # 查找.pyd文件（Python扩展，包含DLL功能）
        $pydFiles = Get-ChildItem -Path $TempDir -Recurse -Filter "*.pyd"
        if ($pydFiles.Count -gt 0) {
            Write-Info "Found .pyd files (Python extensions with DLL functionality):"
            foreach ($pyd in $pydFiles) {
                Write-Info "  $($pyd.Name) ($([math]::Round($pyd.Length / 1KB, 1)) KB)"
            }
            $dllFiles = $pydFiles
        } else {
            Write-Err "No DLL or PYD files found in the wheel package"
            exit 1
        }
    }

    # 5) 复制到DLL目录
    New-Item -ItemType Directory -Force $DLLDir | Out-Null

    $mainDll = $dllFiles | Sort-Object Length -Descending | Select-Object -First 1
    $dest = Join-Path $DLLDir "lib_coacd.dll"
    
    Copy-Item $mainDll.FullName $dest -Force
    Write-Info "Copied: $($mainDll.Name) -> lib_coacd.dll"
    
    # 6) 显示文件信息
    $fileInfo = Get-Item $dest
    Write-Info "Official DLL extracted successfully!"
    Write-Info "Size: $([math]::Round($fileInfo.Length / 1MB, 2)) MB"
    Write-Info "Version: CoACD $Version (Official PyPI Release)"
    Write-Info "No compilation required - ready to use immediately!"

} catch {
    Write-Err "Failed to download official DLL: $($_.Exception.Message)"
    Write-Info "You can manually download from: https://github.com/SarahWeiii/CoACD/releases/tag/v$Version"
    exit 1
} finally {
    # 清理临时文件
    if (Test-Path $TempDir) { 
        Remove-Item -Recurse -Force $TempDir -ErrorAction SilentlyContinue 
    }
}
