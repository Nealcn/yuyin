@echo off
echo Extracting opus source...
tar -xzf D:\WAN\yuyinzhushou\opus.tar.gz -C D:\WAN\yuyinzhushou\firmware\components
if %ERRORLEVEL% neq 0 (
    echo FAILED - try: right click this file and run as administrator
    pause
    exit /b 1
)

rename D:\WAN\yuyinzhushou\firmware\components\opus-1.4 opus_src
if %ERRORLEVEL% neq 0 (
    echo rename failed
    pause
    exit /b 1
)

del D:\WAN\yuyinzhushou\opus.tar.gz
echo.
echo ===== OPUS EXTRACTED SUCCESSFULLY =====
echo.
echo Now run in Git Bash:
echo   python3 flash_fw.py
pause
