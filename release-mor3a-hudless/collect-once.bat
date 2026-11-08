@echo off
rem ============================================================
rem  coleta-uma-vez.bat - roda UMA vez depois de testar o FG
rem  (com o jogo fechado DEPOIS de ter jogado ~2 min com o teste)
rem  Gerado por zz_ngxbridge - nao toca em nada do jogo, so le.
rem ============================================================
setlocal EnableDelayedExpansion
set "GAME=C:\Program Files\Epic Games\RedDeadRedemption2"
if not exist "%GAME%\RDR2VulkanFG.ini" (
  set "GAME=%~dp0"
  if not exist "%GAME%\RDR2VulkanFG.ini" (
    echo NAO ENCONTREI RDR2VulkanFG.ini em "%GAME%".
    echo Arraste a pasta do jogo pro caminho acima ou rode este .bat
    echo de dentro da pasta do jogo.
    pause & exit /b 1
  )
)
set "STAMP=%DATE:~-4%%DATE:~3,2%%DATE:~0,2%-%TIME:~0,2%%TIME:~3,2%"
set "STAMP=%STAMP: =0%"
set "OUT=%USERPROFILE%\Desktop\coleta-%STAMP%"
mkdir "%OUT%" 2>nul

copy /y "%GAME%\RDR2VulkanFG.log" "%OUT%\" >nul 2>nul
copy /y "%GAME%\RDR2VulkanFG.ini" "%OUT%\" >nul 2>nul
if exist "%GAME%\RDR2VulkanFG\ngxbridge.log" copy /y "%GAME%\RDR2VulkanFG\ngxbridge.log" "%OUT%\" >nul 2>nul
dir /b "%GAME%\RDR2VulkanFG.asi" "%GAME%\RDR2VulkanFG" > "%OUT%\arquivos-da-pasta.txt" 2>nul
tasklist /m > "%OUT%\modulos-windows.txt" 2>nul
powershell -NoProfile -Command "Get-CimInstance Win32_OperatingSystem | Select-Object Caption,Version | Out-File -Encoding utf8 '%OUT%\sistema.txt'; Get-CimInstance Win32_VideoController | Select-Object Name,DriverVersion | Add-Content -Encoding utf8 '%OUT%\sistema.txt'"
echo fim da coleta > "%OUT%\_fim.txt"

powershell -NoProfile -Command "Compress-Archive -Path '%OUT%\*' -DestinationPath '%OUT%.zip' -Force"
echo.
echo ============================================================
echo  PRONTO. Manda pra mim o arquivo:
echo    %OUT%.zip
echo  (fica na sua Area de Trabalho)
echo ============================================================
pause
