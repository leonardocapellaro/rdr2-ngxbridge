@echo off
rem ============================================================
rem  zz_ngxbridge uninstall - remove apenas os arquivos desta ponte.
rem  Se havia versoes anteriores de zz_ngxbridge.* (backup feito pelo
rem  install.bat), a mais recente delas e restaurada.
rem ============================================================
setlocal
set "GAMEDIR=%~dp0"
if not "%~1"=="" set "GAMEDIR=%~1"

del /q "%GAMEDIR%zz_ngxbridge.asi" 2>nul
del /q "%GAMEDIR%zz_ngxbridge.ini"  2>nul
echo [OK] zz_ngxbridge.asi e zz_ngxbridge.ini removidos de %GAMEDIR%

rem restaurar ultimo backup, se existir
if exist "%GAMEDIR%zz_ngxbridge_backup" (
    for /f "delims=" %%D in ('dir /b /ad /o-n "%GAMEDIR%zz_ngxbridge_backup"') do (
        for %%F in (zz_ngxbridge.asi zz_ngxbridge.ini) do (
            if exist "%GAMEDIR%zz_ngxbridge_backup\%%D\%%F" (
                copy /y "%GAMEDIR%zz_ngxbridge_backup\%%D\%%F" "%GAMEDIR%%%F" >nul
                echo [RESTORE] %%F restaurado de %%D
            )
        )
        goto :done
    )
)
:done
echo Nada mais foi alterado: OptiScaler, RDR2VulkanFG, OptiPatcher, loader
echo e RDR2.exe nunca sao tocados por estes scripts.
exit /b 0
