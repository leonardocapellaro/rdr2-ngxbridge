# FIELD-B390-4 — field session plan (one sitting)

> Superseded by FIELD-B390-5 (the 2026-11-08 session that actually ran this plan
> and cracked the FG-drop root cause). Kept as the record of what v4.1 asked for.

Build: `0.82+hudfield-v4.1` (asi `4c70dc45…`, zip `RDR2VulkanFG-hudfield-v4.1-B390.zip`). v3 became a subset of v4 (`hudfield-v4` = v3 + the opt-in synthetic UI layer); v4.1 only makes the synthesis cost nothing when disabled.
Package: `release-hudfield/` (also on the GitHub release: `RDR2-Vulkan-FG` tag `v0.82-hudfield-v4.1`).

## What the package brings (summary for the tester)

- Menu input diagnostics (v2 ones preserved): `menu: open/close`, `menu input: left click…`, `hotkey: … ignored — foreground…`.
- `XeSSUiMode` (1–5) and `XeSSUiComposition` (-1/0/1) in the INI; the creation log shows `uiMode=%u uiComposition=%d`; modes 2/3/5 clamp to 4 with a logged clamp.
- **v4:** `XeSSSynthUI=1` synthesizes the premultiplied UI (compute B−(1−a)C) and unlocks modes 3/5 — TESTE 5 in the package checklist.
- **v4.1:** with synthesis off (default) none of it is allocated (saves ~60 MB of VRAM); log strings fixed.
- Original author's Nexus update-check disabled (id=0).
- Visible branding: "RDR2 Vulkan FG".

## Session checklist (ordered; ~30–40 min)

1. Game closed: back up the v2 `.asi` → `.v2.bak`; copy the new `.asi`; baseline ini (read the zip's README).
2. Run 2–3 min in the open world. Run `collect-once.bat` BEFORE closing the game? No — run it right after closing (the script grabs the complete log). The first test is the baseline:
   - a) Does the menu open with the MenuKey? (log `menu: open`?)
   - b) If it opens: switch Multiplier to 4x → watch the `displayed=` telemetry change by ~2x.
   - c) If it does not open: clicks inside the window → look for `menu input: left click observed` in the log; test the End hotkey (cycle) with the game focused → `hotkey:` or `ignored — foreground`.
   - d) Item wheel: observe flicker (documented baseline).
3. `collect-once.bat` → zip 1 (baseline).
4. `XeSSUiMode=1` → repeat (a–d, focus on the wheel) → zip 2.
5. `XeSSUiComposition=0` (with mode 4) → just confirm it gets WORSE (HUD vanishes from generated frames) → zip 3 → **revert immediately to default**.
6. Confirm `DebugLogging=0` before the normal-use session.

## Decisions the data supports

- If `menu: open` appears and the multiplier changes → the combo works; the 2026-11-07 failure was focus/hook and is resolved by v2→v3 itself.
- If the log shows `menu input: left click observed` but the menu does not open → imgui is not receiving mouse-move/position from the hook → fix in the fork.
- If no input at all appears → the WH_GETMESSAGE hook is not installed on the OptiScaler path (menu.cpp:110-124) → fix in the fork.
- If mode 1 kills the wheel flicker and only smears on rotation → invest in synthesizing UI for mode 5 (Intel/OptiScaler HUDFix document the path).

## Do not

- Do not switch the provider to DLSS-G during the session (it locks up). Do not change two knobs in the same session. Do not delete the 1.0 beta backup.
