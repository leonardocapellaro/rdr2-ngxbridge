# RDR2 Vulkan FG — build `0.82+hudfield-v4.7` (B390 + OptiScaler) — STABLE CLEANUP

> Look for the tag **`hudfield-v4.7`** on the first lines of `RDR2VulkanFG.log`.
> If it does not appear, an old `.asi` is still the one in use.

**What this build is:** the 0.82 base of the original project (public source,
MIT — the original author's license is intact in the repo's `LICENSE.txt`) plus
our field fixes, **stripped down to what actually works on the B390**: frame
generation 2x, the capture/activation stability fixes, input/menu fixes, and
always-on health logging. Everything that field testing could not make good
was removed — see below. No NVIDIA/Intel/Rockstar binaries here — only the
`.asi` we compile and the diagnostics collector.

**Proven on real hardware (B390, this exact feature set):** FG 2x under native
Vulkan with OptiScaler (~36 real fps → ~72 displayed at 3440x1440), stable
activation ~1 s after world rendering starts, no weapon-wheel flicker in mode
NONE (the only configuration ever observed flicker-free), menu and hotkeys
working.

## What changed in v4.7 — the cleanup (your call, backed by our own data)

v4.7 removes the two experimental programs that four B390 field sessions
(07–11 through 09–11) proved unfixable with the data available:

1. **UI modes 2–5 and the synthetic UI layer are gone.** The full chain of
   evidence: the tint test proved the SDK composites our reconstructed UI
   layer; the FG-off ablation proved the mode-3 flicker is the game's real UI
   (real frames) alternating with our estimated UI (generated frames); and the
   new alpha estimator (v4.6, offline-validated −66% coverage error against a
   synthetic ground truth) **did not reduce the blink in the field** — you
   confirmed "same thing". A reconstructed UI layer can approach the game's
   real UI but never match it, so composited modes always blink at the frame
   rate. UI handling is now fixed to mode NONE (whole-frame interpolation —
   exactly what mode 1 did), composition off. The INI keys `XeSSUiMode`,
   `XeSSUiComposition`, `XeSSSynthUI`, `XeSSSynthSmooth`, `XeSSSynthDebugTint`
   are recognized one last time only to log that they were removed; delete the
   lines when convenient.
2. **x3/x4 pinned to x2, software pacer deleted.** Our side of the multiplier
   was provably correct in every session since v4.4 (creation at the SDK max,
   live menu, `interp=` telemetry, successful SDK calls) — yet the SDK
   displayed 2 every single enabled second, and the final 09/11 forensics
   localized why: the software output pacer's identity witness reads
   DebugFill/code bytes (`0xCCCC…`, `0xE9`) at the pinned presenter offsets —
   the layout does not match the field Intel binary, so it can never bind
   without reverse-engineering a closed-source vendor DLL (unacceptable risk),
   and without it the SDK scheduler never presents more than one interpolated
   frame (the unbound-batch breadcrumb never fired across all sessions).
   `DLSSGFrames>1` now logs once and pins to x2; the menu shows only x2.
3. **What stays:** everything that fixed real problems — the HUD-less epoch
   race fix, render-pass marking consistency, menu double-toggle fix, the
   always-on FG health lines (`XeSS eligibility`, `HUD-less miss`,
   `displayed=`), hotkey proof lines, ShowFPS, and the v4.6
   two-sided alpha estimator's source lives on in git history in case a future
   SDK makes it salvageable.

## Installation (one file swap)

1. Game closed. Rename the current `.asi` (v4.6) to `RDR2VulkanFG.asi.v46.bak` — rollback.
2. Copy this `RDR2VulkanFG.asi` into the game folder. The optional collector goes
   to `RDR2VulkanFG\RDR2VulkanFGDiagnostics.dll` (subfolder, as before).
3. In `RDR2VulkanFG.ini`, the daily-driver config is just:
   ```ini
   FGProvider=XeSS
   DebugLogging=0
   ```
   Delete the old diagnostic lines (`DLSSGFrames` — now ignored, `XeSSUiMode`,
   `XeSSSynthUI`, `XeSSSynthDebugTint`, `XeSSUiComposition`); the build works
   without them and logs a one-time notice if they are present.

## Quick regression (optional, 2 minutes)

No test protocol — just play 2 minutes. The log should show:
- `hudfield-v4.7` at the top, `uiMode=NONE (fixed, no composition)` in the
  creation line, `createdInterp=1`;
- `XeSS eligibility: … eligible=1` within seconds of entering the world, and
  telemetry `interp=1 … displayed=2` while FG is on;
- no wheel flicker (there is no UI composition code left to flicker).
If anything is off, run `collect-once.bat` after the game closes and send the
zip — the health lines will localize it.

## Honest notes

- Base is **0.82**, not 1.0beta — the beta never reaches FG under OptiScaler
  (it fails its HUD-less gate before the first interpolated frame; closed
  source). The path we use exists identically in both.
- Real build: `zig c++ -target x86_64-windows-gnu` (zig **0.17.1**). Compiles
  clean; loads/unloads under Wine; **it does not run RDR2 here** — only the
  B390 proves it.
- **x2 only, by evidence, not by laziness**: every layer of our own multiplier
  stack was verified working; the wall is inside Intel's closed presentation
  scheduler when the pacing hook cannot bind. A future Intel driver/SDK update
  could change that — the `maxInterpolations=` field in the creation log
  reports the SDK's advertised capability every session, so a change is free
  to detect.
- The HUD-less capture matcher is the original author's design; v4.2 fixed a
  race and an inconsistency in it but did not redesign it. `HUD-less miss
  start ... lastReject=N` lines during normal play still localize stalls
  (5=loadOp appeared in the 09/11 session — known, by-design strict; the miss
  window self-heals). `lastReject=2` at the moment the wheel opens is expected
  extent-change behavior.
- Removed features are not "broken" as shipped code — they are gone. To fight
  for them again you need a different SDK, a game-provided UI texture, or
  vendor internals we cannot responsibly touch.

## Rollback

Game closed → delete this `.asi` → rename `RDR2VulkanFG.asi.v46.bak` back. The
`zz_ngxbridge` shim is untouched.
