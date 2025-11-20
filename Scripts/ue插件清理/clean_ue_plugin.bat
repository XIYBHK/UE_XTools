@echo off
chcp 65001 > nul
echo 正在启动UE插件清理工具...
python "%~dp0ue_plugin_cleaner.py" %*
pause
