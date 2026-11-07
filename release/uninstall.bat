@echo off
rem ============================================================
rem  zz_ngxbridge uninstall - remove apenas os arquivos desta ponte.
rem  Se havia versoes anteriores de zz_ngxbridge.* (backup feito pelo
rem  install.bat), a mais recente delas e restaurada.
rem  Uso: uninstall.bat   (na pasta do jogo)
rem       uninstall.bat "D:\caminho\para\pasta\do\RDR2"
rem ============================================================
setlocal
set "GAMEDIR=%~dp0"
if not "%~1"=="" set "GAMEDIR=%~1"
if "%GAMEDIR:~-1%"=="\" set "GAMEDIR=%GAMEDIR:~0,-1%"

if exist "%GAMEDIR%\zz_ngxbridge.asi" goto :remove
if exist "%GAMEDIR%\zz_ngxbridge.ini" goto :remove
echo Nada para remover em %GAMEDIR% (nenhum arquivo zz_ngxbridge encontrado).
exit /b 0

:remove
del /q "%GAMEDIR%\zz_ngxbridge.asi" 2>nul
del /q "%GAMEDIR%\zz_ngxbridge.ini"  2>nul
echo [OK] zz_ngxbridge.asi e zz_ngxbridge.ini removidos de %GAMEDIR%

if not exist "%GAMEDIR%\zz_ngxbridge_backup" goto :done
rem restaurar o conteudo do backup mais recente (nome mais recente = ultimo no listing)
for /f "delims=" %%D in ('dir /b /ad /o-n "%GAMEDIR%\zz_ngxbridge_backup"') do (
    for %%F in (zz_ngxbridge.asi zz_ngxbridge.ini) do (
        if exist "%GAMEDIR%\zz_ngxbridge_backup\%%D\%%F" (
            copy /y "%GAMEDIR%\zz_ngxbridge_backup\%%D\%%F" "%GAMEDIR%\%%F" >nul
            echo [RESTORE] %%F restaurado de %%D
        )
    )
    goto :done
)
:done
echo Nada mais foi alterado: OptiScaler, RDR2VulkanFG, OptiPatcher, loader
echo e RDR2.exe nunca sao tocados por estes scripts.
exit /b 0
