@echo off
rem ============================================================
rem  zz_ngxbridge uninstall - removes only this bridge's files.
rem  If previous zz_ngxbridge.* versions existed (backed up by
rem  install.bat), the most recent one is restored.
rem  Usage: uninstall.bat   (in the game folder)
rem         uninstall.bat "D:\path\to\the\RDR2\folder"
rem ============================================================
setlocal
set "GAMEDIR=%~dp0"
if not "%~1"=="" set "GAMEDIR=%~1"
if "%GAMEDIR:~-1%"=="\" set "GAMEDIR=%GAMEDIR:~0,-1%"

if exist "%GAMEDIR%\zz_ngxbridge.asi" goto :remove
if exist "%GAMEDIR%\zz_ngxbridge.ini" goto :remove
echo Nothing to remove in %GAMEDIR% (no zz_ngxbridge files found).
exit /b 0

:remove
del /q "%GAMEDIR%\zz_ngxbridge.asi" 2>nul
del /q "%GAMEDIR%\zz_ngxbridge.ini"  2>nul
echo [OK] zz_ngxbridge.asi and zz_ngxbridge.ini removed from %GAMEDIR%

if not exist "%GAMEDIR%\zz_ngxbridge_backup" goto :done
rem restore the newest backup's contents (newest name = first in the listing)
for /f "delims=" %%D in ('dir /b /ad /o-n "%GAMEDIR%\zz_ngxbridge_backup"') do (
    for %%F in (zz_ngxbridge.asi zz_ngxbridge.ini) do (
        if exist "%GAMEDIR%\zz_ngxbridge_backup\%%D\%%F" (
            copy /y "%GAMEDIR%\zz_ngxbridge_backup\%%D\%%F" "%GAMEDIR%\%%F" >nul
            echo [RESTORE] %%F restored from %%D
        )
    )
    goto :done
)
:done
echo Nothing else was touched: OptiScaler, RDR2VulkanFG, OptiPatcher, the
echo loader and RDR2.exe are never modified by these scripts.
exit /b 0
