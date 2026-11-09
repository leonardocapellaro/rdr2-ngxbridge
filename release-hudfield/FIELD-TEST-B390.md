# B390 regression check — package v4.7 (log tag: `hudfield-v4.7`)

v4.7 is the stable cleanup: mode NONE only, x2 only, experiments removed.
**There is no test protocol.** Install, play, and optionally glance at the
log — that's all.

## Install (once, game closed)

1. Download the zip: https://github.com/leonardocapellaro/RDR2-Vulkan-FG/releases/latest
2. Rename `RDR2VulkanFG.asi` → `RDR2VulkanFG.asi.v46.bak`; copy the new
   `.asi` in; keep `RDR2VulkanFG\RDR2VulkanFGDiagnostics.dll` in the subfolder.
3. In `RDR2VulkanFG.ini`, keep `FGProvider=XeSS` and `DebugLogging=0`;
   delete the old experiment lines (`DLSSGFrames`, `XeSSUiMode`,
   `XeSSSynthUI`, `XeSSSynthDebugTint`, `XeSSUiComposition`). Leaving them
   does nothing except log a one-time removal notice.
4. Open the log after the first run: the first lines must say
   `... RDR2 Vulkan FG 0.82+hudfield-v4.7 ...`. No tag → old `.asi` in use.

## What "working" looks like

- Creation line: `createdInterp=1 ... uiMode=NONE (fixed, no composition)`.
- `XeSS eligibility: ... eligible=1` shortly after the world loads; telemetry
  (every ~120 frames) shows `interp=1 ... enabled=1 displayed=2`.
- Weapon wheel: nothing flickers — there is no UI-composition code path left.
- Optional: press the multiplier hotkey (End) — the log will honestly say the
  multiplier is pinned to x2 now.

## If something IS wrong

Close the game, double-click `collect-once.bat`, rename the zip
`coleta-V47.zip`, send it, and add one line about what you saw (field
observations decide; the log localizes). If all is well — no zip needed, just
play.
