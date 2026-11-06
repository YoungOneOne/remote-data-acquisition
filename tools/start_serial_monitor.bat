@echo off
chcp 65001 >nul
title QRSA 串口助手 (自动记录与AI联调服务)
echo 正在启动串口助手...
python "%~dp0serial_monitor.py" daemon --port COM5 --baud 115200
pause
