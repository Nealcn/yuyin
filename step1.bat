@echo off
setlocal
set LOG=D:\WAN\yuyinzhushou\build_step.txt
echo Starting... > %LOG%
call D:\esp-idf\export.bat >> %LOG% 2>&1
echo EXPORT_DONE >> %LOG%
cd /d D:\WAN\yuyinzhushou\firmware >> %LOG% 2>&1
echo CD_DONE >> %LOG%
call idf.py set-target esp32s3 >> %LOG% 2>&1
echo SET_TARGET_EXIT=%ERRORLEVEL% >> %LOG%
