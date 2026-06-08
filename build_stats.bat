@echo off
echo Rebuilding recplay_core, recplay_stats, recplay_webserver ...
call "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat" -arch=x64 >nul 2>&1
cd /D "D:\yasukioo\RecPlay\out\build\x64-Debug"
ninja recplay_core recplay_stats recplay_webserver
if errorlevel 1 (
    echo BUILD FAILED
    pause
    exit /b 1
)
echo BUILD OK
pause
