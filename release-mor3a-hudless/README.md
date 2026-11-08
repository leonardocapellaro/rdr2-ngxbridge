# RDR2VulkanFG (0.82 + patch HUD-less-transfer) — para B390 + OptiScaler

**Para que serve:** o seu 1.0beta prende em `HUD-less post-tone-map pass missing`
com OptiScaler no caminho Vulkan: o XeSS bridge só aceita snapshot de um render
pass que escreve **direto** nas imagens do swapchain virtual, mas o OptiScaler
entrega o quadro final por **blit** — a assinatura nunca casa e o FG nunca liga
(visto ao vivo nos seus logs: `input=725`, `enabled=0`). O patch adiciona a
chave opt-in `CapturePresentTransfers=1`, que deixa o **blit full-extent para a
imagem do swapchain virtual** contar como a snapshot do frame.

**O que é exatamente:** fonte público 0.82 do mod (GitHub, commit `bc853d0`) +
nosso patch (`patches/hud-capture-present-transfer.patch`, 129 linhas, 3
arquivos). Nada de NVIDIA/Intel/Rockstar; MIT.

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
