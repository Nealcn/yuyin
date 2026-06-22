@echo off
setlocal
set LOGFILE=D:\WAN\yuyinzhushou\build_log.txt
echo Starting build at %DATE% %TIME% > %LOGFILE%

call D:\esp-idf\export.bat >> %LOGFILE% 2>&1
if %ERRORLEVEL% neq 0 (
    echo EXPORT_FAILED >> %LOGFILE%
    exit /b 1
)

cd /d D:\WAN\yuyinzhushou\firmware >> %LOGFILE% 2>&1

echo SET_TARGET >> %LOGFILE%
call idf.py set-target esp32s3 >> %LOGFILE% 2>&1
if %ERRORLEVEL% neq 0 (
    echo SET_TARGET_FAILED >> %LOGFILE%
    exit /b 1
)

echo BUILD_START >> %LOGFILE%
call idf.py build >> %LOGFILE% 2>&1
if %ERRORLEVEL% neq 0 (
    echo BUILD_FAILED >> %LOGFILE%
    exit /b 1
)

echo BUILD_SUCCESS >> %LOGFILE%
