$vsDir = "D:\Program\VisualStudio"
$src = "D:\WAN\yuyinzhushou\desktop\windows"
$build = "$src\build-x64"
$cmake = "$vsDir\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$ninja = "$vsDir\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"

# Run everything in a single cmd.exe session that sources vcvars first
$cmd = @"
call "$vsDir\VC\Auxiliary\Build\vcvarsall.bat" x64
if errorlevel 1 exit /b 1
echo Configuring...
"$cmake" -S "$src" -B "$build" -G Ninja "-DCMAKE_MAKE_PROGRAM=$ninja" -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 exit /b 1
echo Building...
"$ninja" -C "$build"
if errorlevel 1 exit /b 1
echo OK
"@

$cmd | Out-File -FilePath "$env:TEMP\build_desktop_cmd.bat" -Encoding ASCII
cmd /c "$env:TEMP\build_desktop_cmd.bat"
