@echo off
setlocal
REM Build M5AtomS3 Voice Stick firmware
echo === ESP-IDF Export ===
call D:\esp-idf\export.bat > nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo ESP-IDF export failed
    pause
    exit /b 1
)
echo OK

cd /d D:\WAN\yuyinzhushou\firmware
if %ERRORLEVEL% neq 0 (
    echo Cannot find firmware directory
    pause
    exit /b 1
)

echo === Set target esp32s3 ===
call idf.py set-target esp32s3
if %ERRORLEVEL% neq 0 (
    echo set-target failed
    pause
    exit /b 1
)

echo === Build ===
call idf.py build
if %ERRORLEVEL% neq 0 (
    echo BUILD FAILED
    pause
    exit /b 1
)

echo === BUILD SUCCESS ===
pause
