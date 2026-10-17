@echo off
setlocal
set "SCRIPT_DIR=%~dp0"
set "PROJECT_ROOT=%SCRIPT_DIR%..\.."
set "BLENDER_EXE=%~1"
if "%BLENDER_EXE%"=="" set "BLENDER_EXE=D:\HamunDev\HamunFinalTest\HamunEditor.exe"
if not exist "%BLENDER_EXE%" (
  echo [ERROR] HamunEditor executable not found: "%BLENDER_EXE%"
  echo Usage: run_hvs_smoke_test.bat "C:\path\to\HamunEditor.exe"
  exit /b 2
)
set "OUTPUT_DIR=%PROJECT_ROOT%\dist\hvs_smoke"
if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"
echo Running HVS smoke test...
"%BLENDER_EXE%" --background --factory-startup --python "%SCRIPT_DIR%smoke_test_hvs.py" -- "%OUTPUT_DIR%" > "%OUTPUT_DIR%\hvs_smoke.log" 2>&1
set "RET=%ERRORLEVEL%"
type "%OUTPUT_DIR%\hvs_smoke.log"
if not "%RET%"=="0" (
  echo [FAIL] Exit code: %RET%
  echo See: %OUTPUT_DIR%\hvs_smoke.log
  exit /b %RET%
)
if not exist "%OUTPUT_DIR%\HVS_Tree_0042.glb" (
  echo [FAIL] No GLB generated.
  exit /b 3
)
echo [PASS] First tree output: %OUTPUT_DIR%\HVS_Tree_0042.glb
echo [PASS] Editable scene: %OUTPUT_DIR%\HVS_Tree_0042.blend
endlocal
