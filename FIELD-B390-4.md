# FIELD-B390-4 — plano da sessão v3 (uma sentada)

Build: `0.82+hudfield-v3` (asi `206fd35f…`, zip `RDR2VulkanFG-hudfield-v3-B390.zip`, sha `914c4780…`).
Pacote: `release-hudfield-v3/` (vira `release/` na reorganização).

## O que a v3 traz (resumo p/ quem vai testar)

- Diagnostics de input do menu (v2 preservados): `menu: open/close`, `menu input: left click…`, `hotkey: … ignored — foreground…`.
- `XeSSUiMode` (1–5) e `XeSSUiComposition` (-1/0/1) no INI; log de criação mostra `uiMode=%u uiComposition=%d`; modos 2/3/5 com clamp logado p/ 4.
- Update-check Nexus do autor original desligado (id=0).
- Branding visível: "RDR2 Vulkan FG".

## Checklist da sessão (ordenado; ~30–40 min)

1. Jogo fechado: backup `.asi` v2 → `.v2.bak`; copiar v3; ini baseline (leia o README do zip).
2. Rodar 2–3 min em mundo aberto. Rode `collect-once.bat` ANTES de fechar o jogo? Não — rode logo após fechar (o script pega o log completo). Primeiro teste é o baseline:
   - a) menu abre com a tecla do MenuKey? (log `menu: open`?)
   - b) se abrir: trocar Multiplier p/ 4x → observar telemetria `displayed=` mudando em ~2x.
   - c) se não abrir: cliques dentro da janela → procurar `menu input: left click observed` no log; testar hotkey End (ciclo) com o jogo focado → `hotkey:` ou `ignored — foreground`.
   - d) roda de itens: observar flicker (baseline documentado).
3. `collect-once.bat` → zip 1 (baseline).
4. `XeSSUiMode=1` → repetir (a-d, foco na roda) → zip 2.
5. `XeSSUiComposition=0` (com modo 4) → só confirmar que PIORA (HUD some nos frames gerados) → zip 3 → **reverter imediatamente p/ padrão**.
6. Confirmar `DebugLogging=0` antes da sessão de uso normal.

## Decisões que os dados sustentam

- Se `menu: open` aparecer e o multiplier mudar → o combo funciona; a falha de 07/11 era foco/hook e fica resolvida pela própria v2→v3.
- Se o log mostrar `menu input: left click observed` mas o menu não abrir → imgui não está recebendo mouse-move/position do hook → correção no fork.
- Se nada de input aparecer → o WH_GETMESSAGE hook não está instalado no caminho OptiScaler (menu.cpp:110-124) → correção no fork.
- Se modo 1 mata o flicker da roda e borra só ao girar → investir em sintetizar UI p/ modo 5 (Intel/OptiScaler HUDFix documentam o caminho).

## Não fazer

- Não tocar provider p/ DLSS-G na sessão (trava). Não trocar dois knobs na mesma sessão. Não apagar o backup do beta 1.0.
