@echo off
setlocal
cd /d "%~dp0"

echo [Hamun] Configuring Windows Test v0.1...
cmake -S . -B build -A x64 -DHAMUN_BUILD_SANDBOX=ON
if errorlevel 1 exit /b %errorlevel%

echo [Hamun] Building Release...
cmake --build build --config Release
if errorlevel 1 exit /b %errorlevel%

echo [Hamun] Packaging Test v0.1...
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Tools\Package\PackageWindows.ps1" -BuildDir build -Configuration Release -OutputDir dist/Hamun_Test_v0.1
if errorlevel 1 exit /b %errorlevel%

echo.
echo [Hamun] Done.
echo Package: %~dp0dist\Hamun_Test_v0.1
