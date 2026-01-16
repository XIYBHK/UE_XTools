#!/usr/bin/env pwsh
# 生成 compile_commands.json 用于 clangd LSP 支持
# 使用方法: .\Scripts\GenerateClangDB.ps1 -EngineRoot "D:\UE_5.4"

param(
    [string]$EngineRoot = $env:UE_ROOT,
    [string]$ProjectPath = (Get-Location),
    [switch]$Force
)

$ErrorActionPreference = "Stop"

function Find-UEEngine {
    # 常见 UE 安装路径
    $paths = @(
        "D:\UE_5.7",
        "D:\UE_5.6",
        "D:\UE_5.5",
        "D:\UE_5.4",
        "D:\UE_5.3",
        "C:\Program Files\Epic Games\UE_5.7",
        "C:\Program Files\Epic Games\UE_5.6",
        "C:\Program Files\Epic Games\UE_5.5",
        "C:\Program Files\Epic Games\UE_5.4",
        "C:\Program Files\Epic Games\UE_5.3"
    )

    foreach ($path in $paths) {
        if (Test-Path $path) {
            $ubt = Join-Path $path "Engine\Build\BatchFiles\Build.bat"
            if (Test-Path $ubt) {
                return $path
            }
        }
    }
    return $null
}

# 查找引擎
if (-not $EngineRoot) {
    $EngineRoot = Find-UEEngine
    if (-not $EngineRoot) {
        Write-Error "未找到 Unreal Engine。请指定 -EngineRoot 参数或设置 UE_ROOT 环境变量"
        exit 1
    }
    Write-Host "自动检测到引擎: $EngineRoot" -ForegroundColor Green
}

# 标准化路径
$EngineRoot = (Resolve-Path $EngineRoot).Path
$ProjectPath = (Resolve-Path $ProjectPath).Path

# UE 头文件路径
$ueIncludePaths = @(
    "$EngineRoot\Engine\Source",
    "$EngineRoot\Engine\Source\Runtime",
    "$EngineRoot\Engine\Source\Runtime\Core",
    "$EngineRoot\Engine\Source\Runtime\Core\Public",
    "$EngineRoot\Engine\Source\Runtime\CoreUObject",
    "$EngineRoot\Engine\Source\Runtime\CoreUObject\Public",
    "$EngineRoot\Engine\Source\Runtime\Engine",
    "$EngineRoot\Engine\Source\Runtime\Engine\Public",
    "$EngineRoot\Engine\Source\Runtime\ApplicationCore",
    "$EngineRoot\Engine\Source\Runtime\ApplicationCore\Public",
    "$EngineRoot\Engine\Source\Developer",
    "$EngineRoot\Engine\Source\Editor"
)

# 构建 include 参数
$includeArgs = $ueIncludePaths | ForEach-Object { "-I`"$_`"" }

# 获取所有 .cpp 文件
$cppFiles = Get-ChildItem -Path "$ProjectPath\Source" -Filter "*.cpp" -Recurse -File

$compileCommands = @()

foreach ($file in $cppFiles) {
    $relativePath = $file.FullName.Substring($ProjectPath.Length + 1).Replace('\', '/')

    $command = @{
        directory = $ProjectPath.Replace('\', '/')
        file = $relativePath
        arguments = "clang++ -xc++ -std=c++20 -fms-compatibility-version=19.33 -D_MSC_VER=1933 -D_WIN32 -D_WIN64 -DUNREAL_BUILD -DWITH_ENGINE -DWITH_UNREAL_DEVELOPER_TOOLS $($includeArgs -join ' ') -c `"$relativePath`""
    }

    $compileCommands += $command
}

# 生成 compile_commands.json
$output = @{
    version = 2
    commands = $compileCommands
} | ConvertTo-Json -Depth 10

$outputFile = Join-Path $ProjectPath "compile_commands.json"
$output | Set-Content -Path $outputFile -Encoding UTF8

Write-Host "已生成: $outputFile" -ForegroundColor Green
Write-Host "包含 $($compileCommands.Count) 个编译条目" -ForegroundColor Cyan
