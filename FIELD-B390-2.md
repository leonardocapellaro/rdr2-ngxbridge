# FIELD-B390-2 — FG **funcionou** na máquina real (2026-11-07 23:42–23:45)

Build: `release-mor3a-hudless` (0.82+patch). Coleta: `coleta-20261107-2346.zip`
(extraída em `field/coleta-2346/`). Sessão de 3,5 min; sessão limpa (exit 0).

## FG rodando — evidência
- `XeSS MFG experimental unlock: validated maximum=3 generated frames (4x)`
- `HUD capture first success: frame=61 renderPass=...` (7,2s)
- do frame ~5900 ao fim: `enabled=1 displayed=2` contínuo; `hud==epoch` (snapshot
  bate todo frame); 3120 epochs em 87 s → **~35,8 reais → ~71,6 exibidos**.
- Único susto: FG-104 aos 88,7s (`Bridge input size changed: 2293x960;
  resetting provider history`) — transitório, "recovered" 25ms depois.

## SURPRESA honesta: meu patch NÃO foi o que destravou
`via present transfer`: **0 ocorrências**. O capture casou pela assinatura de
render pass NORMAL, no 0.82, com o OptiScaler no lugar. O beta 1.0 falhava essa
assinatura sob as MESMAS condições (hud=0 para sempre). Logo: a virada foi
**trocar o .asi beta → 0.82**; a gate HUD-less do beta regride sob SR Vulkan de
terceiros (razão exata desconhecida — fonte fechada). Chave
`CapturePresentTransfers=1` ficou armada sem necessidade (inofensiva; caminho
nunca percorrido). Possível efeito colateral: nossos wrappers de
CopyImage/BlitImage instalados no dispatch — nunca observados alterando nada.

## 3 defeitos restantes (relatório usuário)
1. **Trocar 2x→3x/4x**: zero eventos de hotkey/menu no log. Menu 0.82 NÃO tem
   seletor de multiplicador p/ XeSS (só "Max multiplier: 4x" texto,
   menu.cpp:475); restam ini `DLSSGFrames` (jogo fechado; clamp 32) e tecla End
   (0x23) → `hotkey: generated frames -> ...` — nenhum aparece, então a tecla
   nunca chegou (usuário provavelmente procurou no menu). Perguntar como tentou.
2. **HUD/menu piscando**: causa de projeto — FG recebe snapshot `hudless`
   (xess_provider.cpp:706) e SDK não re-compõe HUD; o recompositor existe
   (`HudMaskApply(real,hudless,generated)`, hud_mask.cpp:119-153, shaders e
   barriers prontos) mas **não tem chamador no 0.82 público** (grep completo;
   só hud_mask.h declara). Beta carrega as MESMAS strings — provável que o beta
   tenha o wiring (inverificável, fechado). Sem chave de ini que ligue. Tentativa
   de wiring = próximo passo com risco (layouts de `generated` no handoff;
   erro → hang de GPU na sessão rara do usuário).
3. **Perf 47/45→31-35**: esperado — FG a 3440x1440 quase dobra o trabalho de
   GPU (inferência XeSS-FG + c6pia de capture); base 47→~36 e exibido ~72.
   proxyPresent 10-16ms médios no present thread (bound em XeLLSleep, não CPU).

## Status por camada
ponte (zz_ngxbridge) ✔ · inputs NGX ✔ · assinatura HUD-less ✔(só no 0.82) ·
pacing/swapchain virtual ✔ · geração+present 2x ✔ · multiplicador >2x: só ini
(melhorável) · HUD nos frames gerados: falta wiring · FSRFG: morto (driver).
