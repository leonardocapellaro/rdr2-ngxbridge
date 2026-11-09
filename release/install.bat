@echo off
rem ============================================================
rem  zz_ngxbridge install - RDR2 Vulkan + XeFG compat bridge
rem  Install/rollback script. Only touches files that start with
rem  "zz_ngxbridge". Never modifies OptiScaler.asi, RDR2VulkanFG.asi,
rem  OptiPatcher, dinput8 loader, RDR2.exe or anything else.
rem
rem  Modes:
rem    A) copy the package folder into the game folder and run here
rem       (in-place: the files are already in place; nothing to copy)
rem    B) run from anywhere, passing the game folder:
rem       install.bat "D:\SteamLibrary\...\RDR2"
rem       (this mode backs up previous versions before copying)
rem ============================================================
setlocal enabledelayedexpansion

set "SRCDIR=%~dp0"
set "GAMEDIR=%~dp0"
if not "%~1"=="" set "GAMEDIR=%~1"
rem normalize trailing slash for uniform concatenation
if "%GAMEDIR:~-1%"=="\" set "GAMEDIR=%GAMEDIR:~0,-1%"

if not exist "%GAMEDIR%\RDR2.exe" (
    echo [ERROR] RDR2.exe not found in: %GAMEDIR%
    echo Copy the package folder into the game directory and run again,
    echo or run: install.bat "C:\path\to\the\RDR2\folder"
    exit /b 1
)
echo Installing into: %GAMEDIR%

rem --- prerequisite warnings (never modified by this script) ---
if not exist "%GAMEDIR%\OptiScaler.asi" echo [WARNING] OptiScaler.asi not seen in the folder - the bridge has nothing to do if it is missing.
if not exist "%GAMEDIR%\dinput8.dll"     echo [WARNING] dinput8.dll (ASI loader) not seen - without it no extra .asi is loaded.

rem --- in-place mode: the game directory itself already has our files ---
set "INPLACE=0"
if /i "%SRCDIR:~0,-1%"=="%GAMEDIR%" set "INPLACE=1"

if "%INPLACE%"=="1" (
    if not exist "%GAMEDIR%\zz_ngxbridge.asi" (
        echo [ERROR] running in-place but zz_ngxbridge.asi is not here. Copy the whole folder into the game.
        exit /b 1
    )
    echo In-place install detected: the files are already in the game folder.
    goto :finish
)

rem --- mode B: back up previous versions and copy ---
rem stamp HHMMSS - %time% has a stable format across locales (%date% does not)
set "STAMP=%time:~0,2%%time:~3,2%%time:~6,2%"
set "STAMP=%STAMP: =0%"
if not defined STAMP set "STAMP=v%RANDOM%"
set "BAK=%GAMEDIR%\zz_ngxbridge_backup\%STAMP%"
for %%F in (zz_ngxbridge.asi zz_ngxbridge.ini) do (
    if exist "%GAMEDIR%\%%F" (
        if not exist "%BAK%" mkdir "%BAK%"
        if exist "%BAK%" (
            move /y "%GAMEDIR%\%%F" "%BAK%\%%F" >nul && echo [BACKUP] %%F preserved in %BAK%
        ) else (
            echo [WARNING] could not create %BAK% - overwriting without backup
        )
    )
)

copy /y "%SRCDIR%zz_ngxbridge.asi" "%GAMEDIR%\zz_ngxbridge.asi" >nul || goto :fail
copy /y "%SRCDIR%zz_ngxbridge.ini"  "%GAMEDIR%\zz_ngxbridge.ini"  >nul || goto :fail
echo [OK] zz_ngxbridge.asi and zz_ngxbridge.ini installed.

:finish
echo.
echo Next steps:
echo   1) check in OptiScaler.ini: [Inputs] EnableDlssInputs=true
echo   2) (optional) run the self-test without touching the game:
echo      selftest-windows\run_selftest.bat   ^(must be run from the original package folder^)
echo   3) run the game normally
echo   4) the bridge log is written to: %GAMEDIR%\RDR2VulkanFG\ngxbridge.log
echo Details and diagnostics: README-INSTALL.md and DIAGNOSIS.md
exit /b 0

:fail
echo [ERROR] failed to copy files. Check folder permissions.
exit /b 1
