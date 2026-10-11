@echo off
rem Double-click to update this folder from GitHub and build the editor.
rem The full build output is saved to build-log.txt next to this file.
setlocal

rem Run from a copy in %TEMP%: git pull may replace this very file, and cmd reads a batch file while running it.
if /i not "%~1"=="--run" (
  copy /y "%~f0" "%TEMP%\HigataBuildEditor.bat" >nul
  "%TEMP%\HigataBuildEditor.bat" --run "%~dp0"
)
set "ROOT=%~2"
cd /d "%ROOT%"

tasklist /FI "IMAGENAME eq UnrealEditor.exe" 2>nul | find /I "UnrealEditor.exe" >nul
if not errorlevel 1 (
  echo.
  echo ==================================================
  echo  The Unreal Editor is still running.
  echo  Close it completely, then double-click BuildEditor.bat again.
  echo  If no editor window is open, end UnrealEditor.exe in Task Manager.
  echo ==================================================
  pause
  exit /b 1
)

echo [1/4] Updating from GitHub...
git diff --quiet
if errorlevel 1 (
  echo Local changes found, usually settings the editor wrote. Keeping them aside with git stash.
  git -c user.name=BuildEditor -c user.email=build@localhost stash push -m "BuildEditor: local changes before update"
)
git pull origin main
if errorlevel 1 (
  echo.
  echo ==================================================
  echo  UPDATE FAILED. Nothing was built.
  echo  Send a screenshot of this window to Claude.
  echo ==================================================
  pause
  exit /b 1
)

echo [2/4] Looking for Unreal Engine 5.8...
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

echo [3/4] Building (this takes a few minutes)...
call "%UE%\Engine\Build\BatchFiles\Build.bat" HigataZukanEditor Win64 Development -Project="%ROOT%HigataZukan.uproject" -WaitMutex > build-log.txt 2>&1
set "RESULT=%ERRORLEVEL%"
type build-log.txt

if not "%RESULT%"=="0" (
  echo.
  echo ==================================================
  echo  FAILED. Send a screenshot of this window to Claude
  echo ==================================================
  pause
  exit /b 1
)

echo [4/4] Making materials (the first time takes several minutes)...
"%UE%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%ROOT%HigataZukan.uproject" -run=pythonscript -script="%ROOT%tools\unreal\make_materials.py" -unattended -nop4 -nosplash -stdout -FullStdOutLogOutput > materials-log.txt 2>&1
find "HIGATA_MATERIALS_OK" materials-log.txt >nul
if errorlevel 1 (
  findstr /C:"[Higata materials]" /C:"HIGATA_MATERIALS" /C:"Error:" materials-log.txt
  echo.
  echo ==================================================
  echo  BUILD OK, but MATERIALS FAILED. The game still runs with plain colours.
  echo  Send a screenshot of this window to Claude.
  echo ==================================================
  pause
  exit /b 1
)

echo.
echo ==================================================
echo  SUCCESS. Now double-click HigataZukan.uproject
echo ==================================================
pause
