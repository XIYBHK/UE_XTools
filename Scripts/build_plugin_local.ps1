# ==============================================================================
#                 UE 插件本地打包验证脚本 (PowerShell) - V4.0 (最终修正版)
# ==============================================================================
#
# 版本 4.0 (最终修正版):
# - 彻底移除了所有对 BuildPlugin 命令无效的编译配置参数。
# - 此脚本将成功完成打包流程，不再报告 "Failed to find command" 错误。
# - 保留了 -NoHostProject 和 -Rocket 优化参数，以生成干净的最终产物。
#
# ==============================================================================

# --- 0. 环境设置 ---
$OutputEncoding = [System.Text.Encoding]::UTF8
[System.Console]::Title = "Unreal Engine 插件优化打包工具"

# --- 1. 用户配置区 (请根据你的实际情况修改这些路径) ---
$UeEditorExePath = "D:\Program Files\Epic Games\UE_5.3\Engine\Binaries\Win64\UnrealEditor.exe"
$PluginUpluginPath = "E:\GitHub\UE_XTools\XTools.uplugin"

# --- 2. 脚本自动处理区 ---
$UeRoot = (Get-Item $UeEditorExePath).Directory.Parent.Parent.Parent.FullName
$RunUATPath = Join-Path $UeRoot "Engine\Build\BatchFiles\RunUAT.bat"
$OutputDirectory = Join-Path (Split-Path $PluginUpluginPath -Parent) "..\Packaged\UE5.3"
$OutputDirectory = [System.IO.Path]::GetFullPath($OutputDirectory)

# --- 3. 执行打包流程 ---
try {
    Clear-Host
    Write-Host "===========================================================" -ForegroundColor Cyan
    Write-Host "         开始本地插件优化打包流程 (V4.0)"
    Write-Host "==========================================================="
    Write-Host "UE 安装根目录: $UeRoot"
    Write-Host "插件 .uplugin 路径: $PluginUpluginPath"
    Write-Host "打包输出目录: $OutputDirectory"
    Write-Host "优化参数: -NoHostProject -Rocket"
    Write-Host "-----------------------------------------------------------"

    if (-not (Test-Path $RunUATPath)) { throw "错误: 无法找到 RunUAT.bat" }
    if (-not (Test-Path $PluginUpluginPath)) { throw "错误: 无法找到 .uplugin 文件" }
    
    # 清理旧的输出目录，确保是全新打包
    if (Test-Path $OutputDirectory) {
        Write-Host "信息: 发现旧的输出目录，正在清理..." -ForegroundColor Yellow
        Remove-Item -Recurse -Force $OutputDirectory
    }
    New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null

    Write-Host "正在调用 RunUAT.bat 执行优化打包，请稍候..." -ForegroundColor Green
    Write-Host "注意: UE 5.3 将按默认流程编译多个版本，耗时会稍长。" -ForegroundColor Yellow
    
    # --- 调用最终正确的优化参数命令 ---
    & $RunUATPath BuildPlugin -Plugin="$PluginUpluginPath" -Package="$OutputDirectory" -CreateSubFolder -NoHostProject -Rocket

    # 检查上一个命令的执行结果
    if ($LASTEXITCODE -eq 0) {
        Write-Host ""
        Write-Host "===========================================================" -ForegroundColor Green
        Write-Host "      成功! 打包已完成，未发生任何错误。"
        Write-Host "===========================================================" -ForegroundColor Green
        Write-Host "打包好的插件位于: $OutputDirectory"
        
        # --- 4. 清理和压缩构建产物 ---
        Write-Host "-----------------------------------------------------------"
        Write-Host "开始执行构建产物清理和压缩..." -ForegroundColor Cyan

        # 自动获取插件名称
        $PluginName = (Get-Item $PluginUpluginPath).BaseName
        # -CreateSubFolder 会在输出目录下创建一个与插件同名的子目录
        # 修正: UE 5.3 的 BuildPlugin 命令在 -CreateSubFolder 模式下，会将插件内容直接放置在 Package 目录下，而不是创建子目录。
        $PackagedPluginPath = $OutputDirectory

        if (-not (Test-Path $PackagedPluginPath)) {
            throw "错误: 打包后的插件目录 '$PackagedPluginPath' 不存在。可能是 BuildPlugin 命令的行为已更改。"
        }
        
        Write-Host "清理目标: $PackagedPluginPath"

        # 4.1 删除 Intermediate 文件夹
        $IntermediatePath = Join-Path $PackagedPluginPath "Intermediate"
        if (Test-Path $IntermediatePath) {
            Write-Host "正在删除 Intermediate 文件夹..." -ForegroundColor Yellow
            Remove-Item -Recurse -Force $IntermediatePath
        } else {
            Write-Host "信息: 未找到 Intermediate 文件夹，无需清理。" -ForegroundColor Gray
        }

        # 4.2 删除 Binaries/Win64 中的 PDB 文件
        $PdbPath = Join-Path $PackagedPluginPath "Binaries\Win64"
        if (Test-Path $PdbPath) {
            $PdbFiles = Get-ChildItem -Path $PdbPath -Filter *.pdb -Recurse
            if ($PdbFiles) {
                Write-Host "正在删除 Binaries/Win64 中的 PDB 文件..." -ForegroundColor Yellow
                Remove-Item -Force $PdbFiles.FullName
            } else {
                Write-Host "信息: 未在 $PdbPath 中找到 PDB 文件，无需清理。" -ForegroundColor Gray
            }
        }

        # 4.3 将清理后的文件夹打包为 ZIP
        $ZipFileName = "$($PluginName).zip"
        $ZipFilePath = Join-Path (Split-Path $OutputDirectory -Parent) $ZipFileName
        
        if (Test-Path $ZipFilePath) {
            Write-Host "发现旧的ZIP包，正在删除..." -ForegroundColor Yellow
            Remove-Item -Force $ZipFilePath
        }
        
        Write-Host "正在将 '$PluginName' 打包为 ZIP 文件..." -ForegroundColor Green
        Compress-Archive -Path "$PackagedPluginPath\*" -DestinationPath $ZipFilePath
        
        Write-Host "ZIP 文件已创建: $ZipFilePath" -ForegroundColor Green
        Write-Host "-----------------------------------------------------------"

        # CI 环境下不执行打开目录操作
    } else {
        throw "插件打包过程中发生错误。请向上滚动查看详细的错误日志输出。"
    }
}
catch {
    Write-Host ""
    Write-Host "===========================================================" -ForegroundColor Red
    Write-Host "      脚本执行失败!" -ForegroundColor Red
    Write-Host "===========================================================" -ForegroundColor Red
    Write-Host "错误信息: $($_.Exception.Message)"
}
finally {
    Write-Host "-----------------------------------------------------------"
    Read-Host -Prompt "脚本执行完毕。请按 Enter 键退出..."
}