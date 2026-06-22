@echo off
setlocal
REM Clear MSYSTEM which confuses ESP-IDF on Windows
set MSYSTEM=
set LOG=D:\WAN\yuyinzhushou\build_log.txt
echo Starting > %LOG%
call D:\esp-idf\export.bat >> %LOG% 2>&1
if %ERRORLEVEL% neq 0 (
    echo EXPORT_FAILED >> %LOG%
    exit /b 1
)
echo EXPORT_OK >> %LOG%
cd /d D:\WAN\yuyinzhushou\firmware || (
    echo CD_FAILED >> %LOG%
    exit /b 1
)
echo CD_OK >> %LOG%
call idf.py set-target esp32s3 >> %LOG% 2>&1
echo SET_TARGET_EXIT=%ERRORLEVEL% >> %LOG%
