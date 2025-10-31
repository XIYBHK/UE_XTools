@echo off
chcp 65001 >nul
title Windows 软链接管理脚本

:: 获取脚本所在目录
cd /d "%~dp0"

:: 运行Python脚本
python symlink_manager.py

:: 如果Python脚本异常退出，显示错误信息
if errorlevel 1 (
    echo.
    echo 脚本执行出错，错误代码: %errorlevel%
    echo.
)

:: 保持窗口打开
pause

