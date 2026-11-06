@echo off
setlocal
pushd "%~dp0.."
call "D:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 goto failed
if not exist tmp\new-display-ui mkdir tmp\new-display-ui
echo. > tmp\new-display-ui\ltdc.h
rem The firmware uses 32-bit physical framebuffer addresses; ignore that host-only pointer warning.
cl /nologo /W3 /wd4312 /D_CRT_SECURE_NO_WARNINGS /utf-8 /std:c11 /TC /I tmp\new-display-ui /I tools\new_display_host /I new_display\quiz\App /I new_display\quiz\Drv tools\test_new_display_ui.c new_display\quiz\App\app_ui.c new_display\quiz\App\app_screen.c new_display\quiz\Drv\drv_font12.c new_display\quiz\Drv\drv_font16.c /Fo:tmp\new-display-ui\ /Fe:tmp\new-display-ui\test.exe /link /INCREMENTAL:NO
if errorlevel 1 goto failed
pushd tmp\new-display-ui
test.exe
if errorlevel 1 goto failed_inner
test.exe triple
if errorlevel 1 goto failed_inner
popd
popd
exit /b 0
:failed_inner
popd
:failed
popd
exit /b 1
