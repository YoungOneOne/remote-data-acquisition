@echo off
setlocal
pushd "%~dp0.."
call "D:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 goto failed
if not exist tmp mkdir tmp
cl /nologo /W3 /D_CRT_SECURE_NO_WARNINGS /utf-8 /std:c11 /TC /I tools\new_display_host /I new_display\quiz\App /I new_display\quiz\Drv tools\test_new_display_protocol.c new_display\quiz\App\app_proto.c new_display\quiz\App\app_link.c /Fo:tmp\ /Fe:tmp\test_new_display_protocol.exe /link /INCREMENTAL:NO
if errorlevel 1 goto failed
tmp\test_new_display_protocol.exe %*
set "test_result=%ERRORLEVEL%"
popd
exit /b %test_result%
:failed
popd
exit /b 1
