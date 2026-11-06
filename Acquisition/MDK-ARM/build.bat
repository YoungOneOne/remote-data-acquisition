@echo off
cd /d "%~dp0"
echo Starting Keil MDK build for Acquisition...
if exist "build.log" del /f /q "build.log"
"E:\KEIL5\UV4\UV4.exe" -b "Acquisition.uvprojx" -j0 -o "build.log"
echo Build finished. Output log:
echo =======================================================
if exist "build.log" (
    type "build.log"
) else (
    echo [ERROR] build.log was not generated.
)
echo =======================================================
