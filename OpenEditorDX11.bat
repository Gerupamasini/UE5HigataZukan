@echo off
rem Open the editor with DirectX 11 instead of 12, to tell a DX12 / ray tracing / driver problem from anything else.
setlocal
cd /d "%~dp0"
set "UE="
for /f "tokens=2,*" %%A in ('reg query "HKLM\SOFTWARE\EpicGames\Unreal Engine\5.8" /v InstalledDirectory 2^>nul ^| find "InstalledDirectory"') do set "UE=%%B"
if not defined UE set "UE=C:\Program Files\Epic Games\UE_5.8"
if not exist "%UE%\Engine\Binaries\Win64\UnrealEditor.exe" (
  echo ERROR: Unreal Engine 5.8 was not found at "%UE%".
  pause
  exit /b 1
)
echo Opening the editor with DirectX 11...
start "" "%UE%\Engine\Binaries\Win64\UnrealEditor.exe" "%~dp0HigataZukan.uproject" -dx11
