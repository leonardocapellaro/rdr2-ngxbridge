# RDR2VulkanFG 0.82+hudfield-v2 — para B390 + OptiScaler

> Build `0.82+hudfield-v2` (procure esta tag na primeira linha do log). v2 adds
> pure-diagnostics (menu open/close, first clicks observed, focus-rejected hotkeys)
> and keeps the opt-in transfer capture; zero behavior change otherwise.

**Contexto honesto (pós-teste de campo):** o beta 1.0 prendia em
`HUD-less post-tone-map pass missing` com OptiScaler (nunca rodava FG). O 0.82
compilado deste fonte **funciona**: FG 2x validado na B390 (~36 reais → ~72
exibidos). A correção de transfer (`CapturePresentTransfers=1`) permanece
opt-in e inofensiva — em campo a assinatura normal já casou e ela não chegou a
ser usada (0 hits). Esta v2 adiciona só diagnóstico: `menu: open/close`,
primeiros cliques vistos na fila do jogo, hotkeys ignoradas por foco — pra
descobrir por que os cliques/teclas de multiplicador não registraram.

**O que é exatamente:** fonte público 0.82 do mod (GitHub, commit `bc853d0`) +
nosso patch (`patches/hud-capture-present-transfer.patch`, 175 linhas). Nada de NVIDIA/Intel/Rockstar; MIT.

**Build real:** `zig c++ -target x86_64-windows-gnu` (zig 0.17.1; o autor usa
0.16.0 — o 0.16 tem libc++ quebrado p/ windows-gnu em host Linux), deps nos
mesmos commits pinados do Build.ps1. Script reproduzível em
`case/ngxbridge/tools/build-mor3a-zig-linux.sh`. Verificações feitas aqui:
compila limpo; o `.asi` carrega e descarrega no Wine com a ini armada.
**Não rodou RDR2** — só a B390 prova.

## Instalação (substitui SÓ um arquivo)

1. Jogo fechado. Na pasta do jogo, renomeie seu `RDR2VulkanFG.asi` atual (o
   1.0beta) para `RDR2VulkanFG.asi.1.0beta.bak` — é o seu rollback.
2. Copie este `RDR2VulkanFG.asi` para a pasta do jogo.
3. No `RDR2VulkanFG.ini`, confira/adicione:
   ```ini
   FGProvider=XeSS
   CapturePresentTransfers=1
   DebugLogging=1
   ```
4. (uma vez só) confirme que o `RDR2VulkanFG\Streamline\` e
   `RDR2VulkanFG\XeSS\` **do seu pacote 1.0beta** continuam lá — o 0.82 usa os
   mesmos caminhos de runtime (se algo reclamar de versão de runtime, o rollback
   no passo 1 é imediato).

## O que deve aparecer no log (RDR2VulkanFG.log)

1. linha `config ...: ... CapturePresentTransfers=1` (prova de que a chave
   foi lida);
2. depois do `NGX inputs first valid`, a nossa linha nova
   `HUD capture first success via present transfer: frame=NNN dstImage=0x...`;
3. telemetria `XeSS telemetry: ... enabled=1` e frames gerados (FG-109/FG-201
   ausentes).

**Nunca** selecione DLSS-G / "Restart FG" no menu durante o teste: DLSS-G não
roda em Arc e trava a sessão com FG-201.

## Honestidades

- **Base 0.82, não 1.0beta:** você perde os recursos do beta (provider FSRFG
  incluso — inútil na B390 mesmo: o FG-3xx "Three independent pacing queues are
  unavailable" mostrou que o FidelityFX-FG Vulkan exige filas que o driver Intel
  não expõe). O caminho que usamos (NGX inputs + XeSS bridge) existe idêntico
  nos dois.
- **HUD pode entrar na snapshot:** o blit entrega o quadro *como apresentado*.
  Se o jogo já desenhou HUD, o FG interpola com HUD (risco de ghosting/halo no
  HUD). A alternativa render-pass continua disponível (chave desligada =
  comportamento original). Se ghosting aparecer, é isso — e é informação útil
  pro Mor3a (relatório pronto em `NEXUS-REPORT-draft.md`).
- Layout: tratamos o destino do blit como TRANSFER_DST (exigência da própria
  API durante o blit) e o capture restaura o layout depois — as barriers do
  jogo permanecem consistentes.
- Compilado com zig **0.17.1** (o autor usa 0.16): mesmas flags, mesmos deps;
  diferença de versão de toolchain declarada.

## Rollback

Feche o jogo → apague este `.asi` → renomeie `RDR2VulkanFG.asi.1.0beta.bak` de
volta para `RDR2VulkanFG.asi`. A ponte `zz_ngxbridge` não é tocada (continuei
com ela — é ela que faz os inputs chegarem).
