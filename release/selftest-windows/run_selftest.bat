@echo off
rem ============================================================
rem  Self-test do zz_ngxbridge no Windows (NAO toca o jogo).
rem  Roda o mock_game.exe numa pasta isolada - usa apenas as DLLs
rem  fake deste pacote. Pode rodar com o jogo fechado ou aberto.
rem  Criterio: as 5 condicoes da cadeia precisam aparecer no log.
rem ============================================================
cd /d %~dp0
if exist RDR2VulkanFG\ngxbridge.log del /q RDR2VulkanFG\ngxbridge.log
echo Rodando mock_game.exe ...
mock_game.exe > stdout.log 2>&1

echo.
echo ---------------- stdout ----------------
type stdout.log
echo ---------------- bridge log ----------------
rem NAO usar if (...) else (...) aqui: cmd real reprova echo com parenteses
rem dentro de bloco (wine perdoa, o cmd.exe nao).
type RDR2VulkanFG\ngxbridge.log 2>nul || echo SEM LOG DA PONTE - o bridge nao carregou?
echo ---------------- veredito ----------------

findstr /c:"OBSERVER-OK" stdout.log >nul || goto :fail
findstr /c:"ACCEPT - installing" stdout.log >nul || goto :fail
findstr /c:"MIRROR-CROSS-TYPE-OK" stdout.log >nul || goto :fail
findstr /c:"STALE-GUARD-OK" stdout.log >nul || goto :fail
findstr /c:"spoofed provider path" RDR2VulkanFG\ngxbridge.log >nul || goto :fail
findstr /c:"patched" RDR2VulkanFG\ngxbridge.log >nul || goto :fail
echo SELFTEST: PASS
exit /b 0

:fail
echo SELFTEST: FAIL - compare com a secao "Saida esperada" de TEST-RESULTS.md
exit /b 1
