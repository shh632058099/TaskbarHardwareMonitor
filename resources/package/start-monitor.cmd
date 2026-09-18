@echo off
setlocal
cd /d "%~dp0"

if not exist "TaskbarHardwareMonitor.exe" (
  echo [ERROR] TaskbarHardwareMonitor.exe not found.
  pause
  exit /b 1
)
if not exist "TaskbarBand.dll" (
  echo [ERROR] TaskbarBand.dll not found.
  pause
  exit /b 1
)
if not exist "TaskbarBandActivator.exe" (
  echo [ERROR] TaskbarBandActivator.exe not found.
  pause
  exit /b 1
)

echo Registering Taskbar Band from current folder...
"%SystemRoot%\System32\regsvr32.exe" /s "%CD%\TaskbarBand.dll"
if errorlevel 1 (
  echo [ERROR] Failed to register TaskbarBand.dll.
  pause
  exit /b 2
)

echo Activating Taskbar Band...
"%CD%\TaskbarBandActivator.exe"
if errorlevel 1 (
  echo [WARN] DeskBand activation did not complete. Explorer may need to be restarted.
)

echo Starting Hardware Monitor...
start "" /D "%CD%" "%CD%\TaskbarHardwareMonitor.exe"

echo Done. The monitor requests administrator permission for hardware access.
endlocal
