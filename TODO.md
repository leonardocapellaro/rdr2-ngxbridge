# TODO — RDR2 Vulkan FG (maintained, English from 2026-11-08)

Priorities after the 2026-11-08 field session (see FIELD-B390-5.md). No more
"run-a-test-send-back" cycles: every change ships with log lines that make a
single 1-minute gameplay capture answer the question by itself.

## P0 — HUD-less snapshot stall (the real defect) — FIXED IN v4.2, FIELD VALIDATED 08/11 NIGHT
- [x] Epoch race fixed (capture stamped at record time) + subpass-marking
      consistency in v4.2, with the never-stale-frame rule intact.
- [x] Always-on forensics shipped in v4.2: `XeSS eligibility:` flips,
      `HUD-less miss start/end` + `lastReject` code (visible with DebugLogging=0).
- [x] 08/11 night confirmed: `hook live` → first `eligible=1` in ~1.0 s
      (was ~15 s); user independently felt it ("bem mais rápido"). Remaining
      misses: transient `lastReject=5` at boot (78 ms) and `lastReject=2`
      (extent) exactly when the weapon wheel opens — by-design refusal.
- [ ] Open: if wheel-open misses bother gameplay further, investigate the
      extent-change variant matcher (do NOT relax the never-stale gate).

## P1 — Menu multiplier dead — TWO REAL BUGS FOUND 08/11 NIGHT, FIXED IN v4.4
- [x] v4.2 proof lines worked exactly as designed: the logs caught the failure
      itself — poll toggles open, the SAME press through the game queue 9 ms
      later toggles close (one-way dedup). Fixed v4.4: 500 ms guard in BOTH
      paths; every line prints `dedup=`.
- [x] Root cause of "multiplier does nothing at any value": swapchain was
      created with `maxInterpolatedFrames=1` hardcoded. INI parsing was fine
      all along (`DLSSGFrames=2 (x3)` in config line, `displayed` histogram
      maxed at 2 all session). Fixed v4.4: create at `sdk.MaxInterpolations()`
      (card reports 3 → x4 possible), log `createdInterp=` and
      `XeSS interpolation target set:`.
- [ ] Next session (v4.4): TESTE 2 menu stays open + switches live; TESTE 3
      INI x3 must show `createdInterp=3` + `displayed=3`.

## P2 — Synthetic UI (XeSSSynthUI) validation — ITERATION 2 IN v4.3
- [x] v4.1 executed the pipeline end-to-end on hardware (armed + first-frame,
      no errors) — implementation runs.
- [x] 08/11 follow-up: mode 5 + raw layer flickered the wheel WITH FG on
      (user-observed inside a log-verified enabled window). Verdict: raw layer
      insufficient — it re-derives the same extraction the SDK does.
- [x] EMA smoothing shipped in v4.3 (`XeSSSynthSmooth`, 0–100, default 35;
      accumulator resets with provider history; log shows `smoothing=1`).
- [x] 08/11 night: TESTE 5 re-run VALID (≈105 s continuous enabled window):
      mode 3 + EMA smoothing STILL FLICKERS the wheel. Fork default stays
      mode 1. Two irreducible possibilities remain — the composite may ignore
      our tagged texture entirely, or the alpha estimate is bad — and no
      further estimator work is worth it before knowing which.
- [x] v4.4 ships the decisive experiment: `XeSSSynthDebugTint=1` paints the
      layer premultiplied magenta. Mode 3 + tint=1, FG-on frames: magenta
      wheel → consumed (fix the estimator: multi-channel least squares /
      edge-aware); normal-color wheel → ignored upstream (fix the wiring).
- [ ] Next session: TESTE 5c (tint). Its answer picks the next code change.
- [ ] Optional: UI coverage/alpha stats via small readback so a future log
      proves the layer's content quality without eyes-on.

## P3 — Defaults and docs
- [ ] Fork default `XeSSUiMode` changed 4→1 (2026-11-08): mode 1 (NONE) is the
      only configuration the user observed flicker-free; composition is
      auto-disabled there anyway (xess_bridge_runtime.cpp).
- [ ] Everything English (docs, release notes, future commits). Historical PT
      field journals translated in place.

## Backlog (nice to have)
- [ ] Deterministic builds (pin PE TimeDateStamp) so rebuilds are byte-stable.
- [ ] `XeSSSynthAlphaScale` knob if the alpha heuristic needs field tuning.
- [ ] Investigate whether OptiScaler's copies (`CapturePresentTransfers=1`)
      could feed a second HUD-less candidate instead of only the tone-map pass.
- [ ] Nexus-style changelog page once menu/update path is settled (update check
      stays disabled: mod id 0).
