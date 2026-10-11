@echo off
rem Double-click to update this folder from GitHub and build the editor.
rem The full output is saved to build-log.txt next to this file.
setlocal
cd /d "%~dp0"

echo [1/3] Updating from GitHub...
git pull origin main

echo [2/3] Looking for Unreal Engine 5.8...
set "UE="
for /f "tokens=2,*" %%A in ('reg query "HKLM\SOFTWARE\EpicGames\Unreal Engine\5.8" /v InstalledDirectory 2^>nul ^| find "InstalledDirectory"') do set "UE=%%B"
if not defined UE set "UE=C:\Program Files\Epic Games\UE_5.8"
if not exist "%UE%\Engine\Build\BatchFiles\Build.bat" (
  echo.
  echo ERROR: Unreal Engine 5.8 was not found at "%UE%".
  echo Install UE 5.8 from the Epic Games Launcher, then run this again.
  echo ERROR: Unreal Engine 5.8 not found at "%UE%" > build-log.txt
  pause
  exit /b 1
)
echo Found: %UE%

echo [3/3] Building (this takes a few minutes)...
call "%UE%\Engine\Build\BatchFiles\Build.bat" HigataZukanEditor Win64 Development -Project="%~dp0HigataZukan.uproject" -WaitMutex > build-log.txt 2>&1
set "RESULT=%ERRORLEVEL%"
type build-log.txt

echo.
if "%RESULT%"=="0" (
  echo ==================================================
  echo  SUCCESS. Now double-click HigataZukan.uproject
  echo ==================================================
) else (
  echo ==================================================
  echo  FAILED. Send build-log.txt in this folder to Claude
  echo ==================================================
)
pause
