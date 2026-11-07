@echo off
rem ============================================================
rem  zz_ngxbridge install - RDR2 Vulkan + XeFG compat bridge
rem  Install/rollback script. Only touches files that start with
rem  "zz_ngxbridge". Never modifies OptiScaler.asi, RDR2VulkanFG.asi,
rem  OptiPatcher, dinput8 loader, RDR2.exe or anything else.
rem
rem  Modos:
rem    A) copiar a pasta do pacote para a pasta do jogo e rodar aqui
rem       (in-place: os arquivos ja estao no lugar; nada a copiar)
rem    B) rodar de qualquer lugar informando a pasta do jogo:
rem       install.bat "D:\SteamLibrary\...\RDR2"
rem       (esse modo faz backup de versoes anteriores antes de copiar)
rem ============================================================
setlocal enabledelayedexpansion

set "SRCDIR=%~dp0"
set "GAMEDIR=%~dp0"
if not "%~1"=="" set "GAMEDIR=%~1"
rem normalizar barra final para concatenacao uniforme
if "%GAMEDIR:~-1%"=="\" set "GAMEDIR=%GAMEDIR:~0,-1%"

if not exist "%GAMEDIR%\RDR2.exe" (
    echo [ERRO] RDR2.exe nao encontrado em: %GAMEDIR%
    echo Copie a pasta do pacote para o diretorio do jogo e rode de novo,
    echo ou rode: install.bat "C:\caminho\para\pasta\do\RDR2"
    exit /b 1
)
echo Instalando em: %GAMEDIR%

rem --- avisos de pre-requisito (nunca modificados por este script) ---
if not exist "%GAMEDIR%\OptiScaler.asi" echo [AVISO] OptiScaler.asi nao visto na pasta - o bridge nao tera o que fazer se ele faltar.
if not exist "%GAMEDIR%\dinput8.dll"     echo [AVISO] dinput8.dll (ASI loader) nao visto - sem ele nenhum .asi extra e carregado.

rem --- modo in-place: o proprio diretorio do jogo ja tem nossos arquivos ---
set "INPLACE=0"
if /i "%SRCDIR:~0,-1%"=="%GAMEDIR%" set "INPLACE=1"

if "%INPLACE%"=="1" (
    if not exist "%GAMEDIR%\zz_ngxbridge.asi" (
        echo [ERRO] rodando in-place mas zz_ngxbridge.asi nao esta aqui. Copie a pasta inteira para o jogo.
        exit /b 1
    )
    echo Instalacao in-place detectada: os arquivos ja estao na pasta do jogo.
    goto :finish
)

rem --- modo B: backup de versoes anteriores e copia ---
rem carimbo HHMMSS - %time% tem formato estavel entre locales (%date% nao)
set "STAMP=%time:~0,2%%time:~3,2%%time:~6,2%"
set "STAMP=%STAMP: =0%"
if not defined STAMP set "STAMP=v%RANDOM%"
set "BAK=%GAMEDIR%\zz_ngxbridge_backup\%STAMP%"
for %%F in (zz_ngxbridge.asi zz_ngxbridge.ini) do (
    if exist "%GAMEDIR%\%%F" (
        if not exist "%BAK%" mkdir "%BAK%"
        if exist "%BAK%" (
            move /y "%GAMEDIR%\%%F" "%BAK%\%%F" >nul && echo [BACKUP] %%F preservado em %BAK%
        ) else (
            echo [AVISO] nao consegui criar %BAK% - sobrescrevendo sem backup
        )
    )
)

copy /y "%SRCDIR%zz_ngxbridge.asi" "%GAMEDIR%\zz_ngxbridge.asi" >nul || goto :fail
copy /y "%SRCDIR%zz_ngxbridge.ini"  "%GAMEDIR%\zz_ngxbridge.ini"  >nul || goto :fail
echo [OK] zz_ngxbridge.asi e zz_ngxbridge.ini instalados.

:finish
echo.
echo Proximos passos:
echo   1) confira em OptiScaler.ini: [Inputs] EnableDlssInputs=true
echo   2) (opcional) faca o autoteste sem tocar no jogo:
echo      selftest-windows\run_selftest.bat   ^(precisa estar na pasta do pacote original^)
echo   3) rode o jogo normalmente
echo   4) o log da ponte sai em: %GAMEDIR%\RDR2VulkanFG\ngxbridge.log
echo Detalhes e diagnostico: README-INSTALL.md e DIAGNOSIS.md
exit /b 0

:fail
echo [ERRO] falha ao copiar arquivos. Verifique permissoes da pasta.
exit /b 1
