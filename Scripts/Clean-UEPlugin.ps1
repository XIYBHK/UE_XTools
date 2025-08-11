# ====================================================================================
# UE 插件智能批量清理工具
# 功能：自动查找并清理当前目录下所有子文件夹内、嵌套的UE插件临时文件。
# 作者：AI
# 版本：3.1 (最终调试版)
# ====================================================================================

# 设置错误处理方式为“继续”，这样即使某个文件夹处理失败，脚本也会继续处理下一个。
$ErrorActionPreference = "Continue"

# 清空控制台
Clear-Host

# 设置窗口标题
$Host.UI.RawUI.WindowTitle = "UE 插件智能批量清理工具"

# --- 辅助函数：格式化字节大小 ---
function Format-Bytes {
    param([long]$Bytes)
    if ($Bytes -eq 0) { return "0 B" }
    $units = "B", "KB", "MB", "GB", "TB", "PB"
    $power = [Math]::Floor([Math]::Log($Bytes, 1024))
    $result = $Bytes / [Math]::Pow(1024, $power)
    "{0:N2} {1}" -f $result, $units[$power]
}

# --- 脚本主逻辑 ---
$basePath = $PSScriptRoot
$versionFolders = Get-ChildItem -Path $basePath -Directory

if (-not $versionFolders) {
    Write-Host "在当前目录下未找到任何需要清理的版本文件夹。" -ForegroundColor Yellow
    Read-Host -Prompt "按 Enter 键退出"
    exit
}

Write-Host "=================================================================" -ForegroundColor Green
Write-Host "              UE 插件智能批量清理工具 (v3.1)" -ForegroundColor Green
Write-Host "=================================================================" -ForegroundColor Green
Write-Host
Write-Host "将在以下版本文件夹中查找插件并执行清理：" -ForegroundColor Cyan
$versionFolders.Name | ForEach-Object { Write-Host " - $_" }
Write-Host

# --- 警告与确认 ---
Write-Host "-----------------------------------------------------------------" -ForegroundColor Yellow
Write-Host "警告: 本脚本将查找并永久删除找到的所有插件内的 .PDB 文件和 Intermediate 文件夹。" -ForegroundColor Yellow
Write-Host "此操作无法撤销！" -ForegroundColor Red
Write-Host "-----------------------------------------------------------------" -ForegroundColor Yellow
Write-Host
Read-Host -Prompt "确认无误后，按 Enter 键开始批量清理，或按 Ctrl+C 组合键取消并退出"
Write-Host

# 初始化总计变量
$totalSizeBefore = 0
$totalSizeAfter = 0
$totalPluginsCleaned = 0

# --- 遍历每个版本文件夹 ---
foreach ($versionFolder in $versionFolders) {
    Write-Host "================================================================="
    Write-Host ">>> 正在扫描版本文件夹: $($versionFolder.Name)" -ForegroundColor Magenta
    
    # 智能查找此版本文件夹下的所有 .uplugin 文件
    $upluginFiles = Get-ChildItem -Path $versionFolder.FullName -Filter "*.uplugin" -Recurse -File -ErrorAction SilentlyContinue
    
    if (-not $upluginFiles) {
        Write-Host "  - 未在此版本文件夹中找到任何 .uplugin 文件，已跳过。" -ForegroundColor Yellow
        continue
    }

    # 可能一个版本文件夹里有多个插件，对每一个找到的插件进行处理
    foreach ($upluginFile in $upluginFiles) {
        $pluginRootPath = $upluginFile.Directory.FullName
        $pluginName = $upluginFile.Directory.Name
        $totalPluginsCleaned++

        Write-Host "  --- 正在清理插件: $pluginName ---" -ForegroundColor Cyan

        # 计算清理前的大小
        $sizeBefore = (Get-ChildItem -Path $pluginRootPath -Recurse -File -Force -ErrorAction SilentlyContinue | Measure-Object -Property Length -Sum).Sum
        $totalSizeBefore += $sizeBefore
        
        # 执行删除操作
        Get-ChildItem -Path $pluginRootPath -Filter "*.pdb" -Recurse -File -Force -ErrorAction SilentlyContinue | Remove-Item -Force
        
        $intermediatePath = Join-Path -Path $pluginRootPath -ChildPath "Intermediate"
        if (Test-Path -Path $intermediatePath -PathType Container) {
            Remove-Item -Path $intermediatePath -Recurse -Force
        }
        
        # 计算清理后的大小
        $sizeAfter = (Get-ChildItem -Path $pluginRootPath -Recurse -File -Force -ErrorAction SilentlyContinue | Measure-Object -Property Length -Sum).Sum
        $totalSizeAfter += $sizeAfter

        # 显示单个插件的结果
        $spaceSaved = $sizeBefore - $sizeAfter
        Write-Host "    - 清理前: $(Format-Bytes $sizeBefore)"
        Write-Host "    - 清理后: $(Format-Bytes $sizeAfter)"
        Write-Host "    - 节省空间: $(Format-Bytes $spaceSaved)" -ForegroundColor Green
    }
    Write-Host
}

# --- 显示最终的总报告 ---
$totalSpaceSaved = $totalSizeBefore - $totalSizeAfter

Write-Host "=================================================================" -ForegroundColor Green
Write-Host "                       所有任务完成！" -ForegroundColor Green
Write-Host "=================================================================" -ForegroundColor Green
Write-Host
Write-Host "总计报告:"
Write-Host "  - 共清理了 $totalPluginsCleaned 个插件。"
Write-Host "  - 清理前总大小: $(Format-Bytes $totalSizeBefore)"
Write-Host "  - 清理后总大小:  $(Format-Bytes $totalSizeAfter)"
Write-Host
Write-Host "  - 总共节省空间: $(Format-Bytes $totalSpaceSaved)" -ForegroundColor Green
Write-Host
Write-Host "=================================================================" -ForegroundColor Green
Write-Host

# --- 最终暂停 ---
Read-Host -Prompt "任务已结束。请检查以上输出。按 Enter 键退出"