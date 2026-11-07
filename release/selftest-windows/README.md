# Self-test no Windows (antes de instalar no jogo)

Rodar isto **não toca o jogo nem nenhum dos seus MODs** — nada aqui é
copiado para o diretório do RDR2, nenhum arquivo deste pacote toca o jogo.
O `mock_game.exe` carrega apenas as três DLLs falsas desta pasta e exerce a
cadeia completa: spoof do provedor → gate do "Mor3a" aceita → espelho de
vtable corrige a leitura cruzada de tipos → observador registra as resoluções.

## Como rodar

1. Dê duas cliques em `run_selftest.bat` (ou rode no cmd desta pasta).
2. Espere a linha final: `SELFTEST: PASS` ou `SELFTEST: FAIL`.

É preciso ter o Visual C++ redistributable? **Não** — os binários são
estáticos. Windows 10/11 x64 basta.

## Arquivos

| Arquivo | Papel |
|---|---|
| `mock_game.exe` | simula o loader NGX do RDR2 (RAGE) |
| `mock_optiscaler.dll` | simula a superfície NGX do OptiScaler (inclusive o "falso carregar" do handle) |
| `mock_mor3a.dll` | simula o gate do RDR2VulkanFG (whitelist nvngx.dll) |
| `zz_ngxbridge.asi` | a ponte (o mesmo binário que vai para o jogo) |
| `zz_ngxbridge.ini` | configuração apontando para os mocks |
| `run_selftest.bat` | executa e verifica as 5 condições |

Se der `FAIL`, rode no cmd nesta pasta e mande o conteúdo de `stdout.log` e
de `RDR2VulkanFG\ngxbridge.log`.
