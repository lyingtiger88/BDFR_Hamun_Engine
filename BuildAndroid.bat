@echo off
setlocal

where py >nul 2>nul
if %errorlevel%==0 (
    py -3 "%~dp0Tools\Android\build_android.py" %*
    exit /b %errorlevel%
)

where python >nul 2>nul
if %errorlevel%==0 (
    python "%~dp0Tools\Android\build_android.py" %*
    exit /b %errorlevel%
)

echo [Hamun] Python 3 was not found.
echo Install Python 3 or enable the Python launcher, then run BuildAndroid.bat again.
exit /b 1
