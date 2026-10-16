@echo off
set "ONESTEP_PS=pwsh.exe"
where pwsh.exe >nul 2>nul
if errorlevel 1 (
    set "ONESTEP_PS=%USERPROFILE%\.cache\codex-runtimes\codex-primary-runtime\dependencies\native\powershell\pwsh.exe"
    if not exist "%USERPROFILE%\.cache\codex-runtimes\codex-primary-runtime\dependencies\native\powershell\pwsh.exe" (
        echo PowerShell 7 bulunamadi. PowerShell 7 kurduktan sonra yeniden acin.
        pause
        exit /b 1
    )
)
"%ONESTEP_PS%" -NoProfile -STA -File "%~dp0scripts\Open-AuthAdmin.ps1"
if errorlevel 1 pause
