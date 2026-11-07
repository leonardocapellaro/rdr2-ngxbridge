@echo off
rem ============================================================
rem  zz_ngxbridge install - RDR2 Vulkan + XeFG compat bridge
rem  Install/rollback script. Only touches files that start with
rem  "zz_ngxbridge". Never modifies OptiScaler.asi, RDR2VulkanFG.asi,
rem  OptiPatcher, dinput8 loader, RDR2.exe or anything else.
rem
rem  Usage: copy this whole folder into the RDR2 game directory
rem  (the one containing RDR2.exe) and run install.bat from there.
rem  You may also run: install.bat "D:\path\to\RDR2"
rem ============================================================
setlocal enabledelayedexpansion

set "SRC=%~dp0"
set "GAMEDIR=%~dp0"
if not "%~1"=="" set "GAMEDIR=%~1"

rem --- verify target ---
if not exist "%GAMEDIR%\RDR2.exe" (
    echo [ERRO] RDR2.exe nao encontrado em: %GAMEDIR%
    echo Copie a pasta do pacote para o diretorio do jogo e rode de novo,
    echo ou rode: install.bat "C:\caminho\para\pasta\do\RDR2"
    exit /b 1
)
echo Instalando em: %GAMEDIR%

rem --- optional sanity note about prerequisites (never modified) ---
if not exist "%GAMEDIR%\OptiScaler.asi" echo [AVISO] OptiScaler.asi nao visto na pasta - o bridge nao tera o que fazer se ele faltar.
if not exist "%GAMEDIR%\dinput8.dll"     echo [AVISO] dinput8.dll (ASI loader) nao visto - sem ele nenhum .asi extra e carregado.

rem --- backup any previous zz_ngxbridge files ---
set "STAMP=%date:~-4%%date:~-7,2%%date:~-10,2%_%time:~0,2%%time:~3,2%"
set "STAMP=%STAMP: =0%"
set "BAK=%GAMEDIR%\zz_ngxbridge_backup\%STAMP%"
for %%F in (zz_ngxbridge.asi zz_ngxbridge.ini) do (
    if exist "%GAMEDIR%%%F" (
        if not exist "%BAK%" mkdir "%BAK%"
        move /y "%GAMEDIR%%%F" "%BAK%\%%F" >nul
        echo [BACKUP] %%F preservado em %BAK%
    )
)

rem --- install ---
copy /y "%SRC%zz_ngxbridge.asi" "%GAMEDIR%zz_ngxbridge.asi" >nul || goto :fail
copy /y "%SRC%zz_ngxbridge.ini"  "%GAMEDIR%zz_ngxbridge.ini"  >nul || goto :fail
echo [OK] zz_ngxbridge.asi e zz_ngxbridge.ini instalados.
echo.
echo Proximos passos:
echo   1) confira em OptiScaler.ini: [Inputs] EnableDlssInputs=true
echo   2) rode o jogo normalmente
echo   3) o log da ponte sai em: %GAMEDIR%\RDR2VulkanFG\ngxbridge.log
echo Detralhes e diagnostico: README-INSTALL.md e DIAGNOSIS.md
exit /b 0

:fail
echo [ERRO] falha ao copiar arquivos. Verifique permissoes da pasta.
exit /b 1
