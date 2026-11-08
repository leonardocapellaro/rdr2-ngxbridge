# FIELD-B390-3 — v2: investigação do multiplicador + NO-GO do HUD-mask (2026-11-08)

## 1. Roda de armas/item piscando → não dá pra "religar" o HudMask (NO-GO)
Mapeei o ciclo de vida do frame gerado (agente Explore + leitura própria):
- `CreatePresentation(..., XEFG_SWAPCHAIN_UI_MODE_BACKBUFFER_HUDLESS)`
  (xess_provider.cpp:534) — a **SDK** já compõe HUD dos frames reais nos
  interpolados via diff backbuffer−hudless (por isso HUD estático OK; roda
  translúcida animada pisca — limitação temporal da recomposição da SDK).
- O frame gerado **nunca existe como VkImage**: nasce dentro de
  `swapchain_->Present()` (xess_bridge_runtime.cpp:181-186), flip-model D3D12,
  sem NT handle, sem output-resource tag na API (xefg_swapchain.h:105-110).
- `HudMaskApply` (hud_mask.cpp:119) é órfão no 0.82 e religá-lo não tem onde:
  o único comando Vulkan controlável (`f.command`) roda ANTES do Present da SDK
  (:774). Alternativa do agente (mascarar a ENTRADA hudless antes do
  RecordColor) borra HUD no tempo — pioraria. Barriers EXTERNAL↔family nos
  SharedImages também conflitariam com as IGNORED do mask.
Ação: pergunta certa feita no NEXUS-REPORT-draft (modo UI da SDK + HudMask
como precursor abandonado). Nada implementado — certo não implementar.

## 2. Multiplicador 3x/4x: UI existe, evento nunca chegou
O combo "Multiplier" (menu.cpp:435, labels 2x/3x/4x Experimental) é top-level
do card e o runtime relê `Rt().frames` a cada present com clamp +
`SetInterpolations` (:677-681). Mas o log da sessão não tem UM
`menu:`/`hotkey:` → nem click nem End registraram. Cadeia: imgui só recebe
mensagens pelo hook WH_GETMESSAGE (`menu_input::Install`,
ProcessGameMessage:110-124; fallback polling só sem hook, e sem mouse-move).
v2 adiciona logs que seccionam a cadeia: `menu: open/close`,
`menu input: left click observed...`, `hotkey: ... ignored — foreground...`.

## 3. Perf: nada livre no código público; ganho real é config
CPU no present: proxyPresent 10-16ms médios (bound em XeLLSleep da SDK), não
otimável daqui. Custo real = inferência da SDK + cópias do bridge. **Livre:**
`DebugLogging=0` (remove ~uma linha/present no log; a escrita já é gatingada
em log.cpp:145, mas o parse das lines existe) — recomendar pro usuário.
`LogDebug` já é conditional; não mexer.

## Artefatos
- branch `hud-transfer-capture`: a1ce133 (transfer) + **41726f0** (v2 diag)
- patch exportado: 175 linhas; build `0.82+hudfield-v2`
  (sha f375dede→**3ea4cbe1…**), Diagnostics 6dc0e215→**e882fa19…**,
  zip **8dec95ef…**; strings verificadas; wine smoke OK
- zig agora em `tools/zig-x86_64-linux-0.17.1/` (permanente; /tmp some em
  reboot — shim Windows.h→windows.h refeito; documentar no script)
