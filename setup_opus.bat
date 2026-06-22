@echo off
setlocal
REM Add opus dependency to M5AtomS3 Voice Stick firmware
call D:\esp-idf\export.bat > nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo EXPORT_FAILED
    exit /b 1
)
cd /d D:\WAN\yuyinzhushou\firmware
if %ERRORLEVEL% neq 0 (
    echo CD_FAILED
    exit /b 1
)
echo ADDING_OPUS_DEP...
idf.py add-dependency opus^1.4
if %ERRORLEVEL% neq 0 (
    echo ADD_DEPENDENCY_FAILED
    exit /b 1
)
echo DEPENDENCY_OK
