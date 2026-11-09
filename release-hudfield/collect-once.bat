@echo off
rem ============================================================
rem  collect-once.bat — run ONCE right after each FG test.
rem  (Game CLOSED, after ~2 min of gameplay with that config.)
rem  Read-only: copies logs out, never touches the game folder.
rem  Run it after EVERY game session — otherwise only the last
rem  session ever lands in a zip.
rem ============================================================
setlocal EnableDelayedExpansion
set "GAME=C:\Program Files\Epic Games\RedDeadRedemption2"
if not exist "%GAME%\RDR2VulkanFG.ini" (
  set "GAME=%~dp0"
  if not exist "%GAME%\RDR2VulkanFG.ini" (
    echo RDR2VulkanFG.ini NOT FOUND in "%GAME%".
    echo Put the game folder at that path, or run this .bat
    echo from inside the game folder.
    pause & exit /b 1
  )
)
set "STAMP=%DATE:~-4%%DATE:~3,2%%DATE:~0,2%-%TIME:~0,2%%TIME:~3,2%%TIME:~6,2%"
set "STAMP=%STAMP: =0%"
set "OUT=%USERPROFILE%\Desktop\coleta-%STAMP%"
mkdir "%OUT%" 2>nul

copy /y "%GAME%\RDR2VulkanFG*.log" "%OUT%\" >nul 2>nul
copy /y "%GAME%\RDR2VulkanFG.ini" "%OUT%\" >nul 2>nul
if exist "%GAME%\RDR2VulkanFG\ngxbridge.log" copy /y "%GAME%\RDR2VulkanFG\ngxbridge.log" "%OUT%\" >nul 2>nul
dir /b "%GAME%\RDR2VulkanFG.asi" "%GAME%\RDR2VulkanFG" > "%OUT%\arquivos-da-pasta.txt" 2>nul
tasklist /m > "%OUT%\modulos-windows.txt" 2>nul
powershell -NoProfile -Command "Get-CimInstance Win32_OperatingSystem | Select-Object Caption,Version | Out-File -Encoding utf8 '%OUT%\sistema.txt'; Get-CimInstance Win32_VideoController | Select-Object Name,DriverVersion | Add-Content -Encoding utf8 '%OUT%\sistema.txt'"
echo collection finished > "%OUT%\_fim.txt"

powershell -NoProfile -Command "Compress-Archive -Path '%OUT%\*' -DestinationPath '%OUT%.zip' -Force"
echo.
echo ============================================================
echo  DONE. Send me this file:
echo    %OUT%.zip
echo  (it is on your Desktop)
echo ============================================================
pause
