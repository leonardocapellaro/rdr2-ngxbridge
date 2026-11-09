# FIELD-B390-2 — FG **worked** on the real machine (2026-11-07 23:42–23:45)

Build: `release-mor3a-hudless` (0.82+patch). Collection: `coleta-20261107-2346.zip`
(extracted into `field/coleta-2346/`). 3.5-minute session; clean session (exit 0).

## FG running — evidence
- `XeSS MFG experimental unlock: validated maximum=3 generated frames (4x)`
- `HUD capture first success: frame=61 renderPass=...` (7.2s)
- from frame ~5900 to the end: `enabled=1 displayed=2` continuous; `hud==epoch` (snapshot
  matches every frame); 3120 epochs in 87 s → **~35.8 real → ~71.6 displayed**.
- Only scare: FG-104 at 88.7s (`Bridge input size changed: 2293x960;
  resetting provider history`) — transient, "recovered" 25ms later.

## Honest surprise: my patch was NOT what unlocked it
`via present transfer`: **0 occurrences**. The capture matched via the NORMAL
render-pass signature, on the 0.82, with OptiScaler in place. The 1.0 beta failed
this signature under the SAME conditions (hud=0 forever). Therefore: the turning
point was **swapping the beta .asi → 0.82**; the beta's HUD-less gate regresses
under third-party SR Vulkan (exact reason unknown — closed source). The
`CapturePresentTransfers=1` key stayed armed with no need (harmless; path
never taken). Possible side effect: our CopyImage/BlitImage wrappers installed
in the dispatch — never observed changing anything.

## 3 remaining defects (user report)
1. **Switching 2x→3x/4x**: zero hotkey/menu events in the log. The 0.82 menu has
   NO multiplier selector for XeSS (only "Max multiplier: 4x" text,
   menu.cpp:475); what remains is the ini `DLSSGFrames` (game closed; clamp 32) and the End
   key (0x23) → `hotkey: generated frames -> ...` — none of these appear, so the key
   never arrived (the user probably looked in the menu). Ask how they tried.
2. **HUD/menu flickering**: a design cause — FG receives a `hudless` snapshot
   (xess_provider.cpp:706) and the SDK does not recompose the HUD; the recompositor exists
   (`HudMaskApply(real,hudless,generated)`, hud_mask.cpp:119-153, shaders and
   barriers ready) but **has no caller in public 0.82** (full grep;
   only hud_mask.h declares it). The beta carries the SAME strings — the beta probably
   has the wiring (unverifiable, closed). No ini key turns it on. An attempt
   at wiring = the next step, with risk (`generated` layouts at the handoff;
   an error → GPU hang in the user's rare session).
3. **Perf 47/45→31-35**: expected — FG at 3440x1440 nearly doubles the GPU
   workload (XeSS-FG inference + capture copies); base 47→~36 and displayed ~72.
   proxyPresent 10-16ms average on the present thread (bound by XeLLSleep, not CPU).

## Status by layer
bridge (zz_ngxbridge) ✔ · NGX inputs ✔ · HUD-less signature ✔ (only on the 0.82) ·
pacing/virtual swapchain ✔ · 2x generation+present ✔ · multiplier >2x: ini only
(improvable) · HUD on generated frames: wiring missing · FSRFG: dead (driver).
