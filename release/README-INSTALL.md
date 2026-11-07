# zz_ngxbridge — instalação (RDR2 Vulkan + XeFG na Arc B390)

**O que é:** uma ponte (`.asi` único) que faz o OptiScaler 0.9.4 (que serve o
upscaling Vulkan) e o RDR2VulkanFG/Mor3a beta (XeFG 2x) se reconhecerem. A
causa raiz do FG nunca ligar (`input=0`, FG-109) está explicada em
`DIAGNOSIS.md`. O bridge **não substitui nem modifica** nenhum dos seus
componentes.

**Status honesto:** construído por análise de código + logs do seu setup, com
self-test automatizado verde (mocks fiéis). **Ainda não validado na B390 com
o RDR2 real** — os logs abaixo dizem exatamente até onde a cadeia chegou.

---

## Requisitos (o que você já tem)

- RDR2 Epic na pasta padrão com: `OptiScaler.asi` (0.9.4) + OptiPatcher,
  `RDR2VulkanFG.asi` (beta do Mor3a) ou equivalente + loader `dinput8.dll`,
  `fakenvapi.dll` (opcional);
- renderizador do jogo em **Vulkan**;
- Em `OptiScaler.ini`: `[Inputs] EnableDlssInputs=true` (sem isso a superfície
  NGX-Vulkan nem existe) e `[Upscalers] VulkanUpscaler=xess` (atenção: a chave
  é `[Upscalers]`, em `[General]` ela é ignorada).

## Instalação (2 passos)

1. Copie **esta pasta `release` inteira** para dentro da pasta do RDR2 (a do
   `RDR2.exe`).
2. Rode `install.bat` (duplo clique). Ele só cria/backupa arquivos
   `zz_ngxbridge.*` e avisa se OptiScaler ou o loader não aparecem na pasta.

Para remover: `uninstall.bat` (restaura backup anterior, se havia).

## Validação rápida ANTES de abrir o jogo

Rode `selftest-windows\run_selftest.bat` — usa só DLLs falsas da pasta do
próprio pacote, não toca o jogo. Deve imprimir `SELFTEST: PASS`. Se falhar já
no Windows, o problema é ambiente (antivírus bloqueando injeção? rode o jogo
também sem bloqueio).

## O que esperar no jogo

1. Jogo abre com Vulkan + upscale normalmente (o upscaling **não** muda);
2. Log da ponte em `RDR2VulkanFG\ngxbridge.log` deve ter, na ordem:
   - `spoofed provider path for capture module: ...\nvngx.dll` (1-2×);
   - `resolved 'NVSDK_NGX_VULKAN_...' from the OptiScaler handle by <módulo>` — o `<módulo>` é o primeiro componente não-sistema na pilha de quem pediu o símbolo (tipicamente `RDR2VulkanFG.asi`, pois o hook global dele encaminha a chamada; ver DIAGNOSIS §6.1);
   - `intercepted NVSDK_NGX_VULKAN_AllocateParameters ... by <exe do jogo>` — o jogo criou o objeto de parâmetros;
   - `mirror: ... parameter-factory export hooks created` (6/6 no OptiScaler real) e
     `mirror: vtable ... patched (16 typed slots)`;
3. Log do RDR2VulkanFG (o dele, na mesma pasta) deve passar a ter o que
   **nunca apareceu** nos seus logs atuais:
   - `NGX hooks: module=... commit=0 ...`
   - `FG input pipeline v2: accepted SR handle=... flags=...`
   - `NGX inputs first valid: depth ... mvec ... jitter ...`
4. Telemetria/watchdog: `evals>0`, `input=1`, `enabled=1`, FG-109 ausente,
   2× frames no OSD do XeFG.

## Sobre `FSR 2.2.1` vs XeSS-SR (não é bug do bridge)

O OSD `Vulkan | FSR 2.2.1 | Input: DLSS` que você via é o fallback silencioso
do OptiScaler: no caminho Vulkan ele só aceita XeSS-SR se existir um
`libxess.dll` com exportações `xessVK*` na pasta do jogo (DIAGNOSIS §4). O
XeFG **funciona com FSR 2.2.1 como SR** (modo aceito no briefing). Se quiser
XeSS-SR por cima, você mesmo coloca um `libxess.dll` seu na pasta (não
redistribuímos componentes Intel).

## Solução de problemas

| Sintoma | Causa provável / o que fazer |
|---|---|
| `RDR2VulkanFG\ngxbridge.log` ausente | ASI loader não carregou o bridge → confira `dinput8.dll` na pasta e o nome `zz_ngxbridge.asi` (o `zz` é para carregar por último) |
| Log existe, mas nunca `spoofed provider path` | o caminho do seu ASI do Mor3a não contém `rdr2vulkanfg` (renomeado?) → ajuste `CallerHints` no `zz_ngxbridge.ini` e reinicie; ou o jogo não está no Vulkan |
| `spoofed provider path` sim, mas nenhum `NGX hooks:`/`accepted SR handle` no log do Mor3a | mande os dois logs (o do bridge tem a linha `resolved ... by <modulo>` que fecha o diagnóstico — ver DIAGNOSIS §6.1) |
| FG liga mas trepida/piora em movimento | esperado: leitura de jitter falha pelo ABI reverso (DIAGNOSIS §6.2) — documentado, com follow-up pequeno |
| Antivírus/Defender gritando com `.asi` | injeção em processo de jogo é o que todo ASI loader faz; adicione a pasta à exceção (é o seu OptiScaler/loader que já faz isso) |

## Arquivos

| Arquivo | Papel |
|---|---|
| `zz_ngxbridge.asi` / `zz_ngxbridge.ini` | a ponte + config (chaves documentadas no próprio ini) |
| `install.bat` / `uninstall.bat` | instalação/rollback com backup |
| `selftest-windows/` | autoteste que não toca o jogo |
| `DIAGNOSIS.md` | causa raiz com evidência de código e logs |
| `CHANGES.md` | o que muda (nada existente) e mapa de posse de hooks |
| `TEST-RESULTS.md` | transcrições de teste |
| `BUILD.md` | como o binário foi compilado (MinGW, sem MSVC) |
| `THIRD-PARTY-NOTICES.txt` | licenças (só MinHook MIT; nada proprietário incluso) |
| `SHA-256SUMS.txt` | integridade dos arquivos |

## Se algo der errado

`uninstall.bat` remove tudo (restaura o estado anterior em segundos). Logs
úteis para reporte: `RDR2VulkanFG\ngxbridge.log`, o log do RDR2VulkanFG e um
print do OSD do OptiScaler.
