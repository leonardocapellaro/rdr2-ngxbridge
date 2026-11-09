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
- [x] 09/11 v4.4 session: menu applies live (`interpolation target set: x4`
      from an in-game switch) and `createdInterp=3` — our side is DONE.
- [x] CLOSED v4.7 (SDK-side wall, root-caused 09/11 13:55 session): the
      reject-detail line shows the pacer's witness reading DebugFill/code bytes
      (`nativeOk=0(cccccccc…)`, `flagsHardware=233=0xE9`) — the MFG-Unlocker
      pinned presenter layout does not match the field Intel binary, so the
      pacer can never bind without RE on a closed vendor DLL (rejected on
      safety/risk). With it unbound the SDK never batched >1 output (unbound
      breadcrumb fired ZERO times across all sessions) — displayed=2 is the
      SDK scheduler's own cap on this path.
- [x] Decision (user, 09/11: "mode 1 only, remove the rest"): v4.7 pins the
      multiplier to x2 (creation maxInterpolatedFrames=1, honest log + menu/
      cycle reflecting the pin) and DELETES xess_output_pacing. Revisit only
      if a future Intel driver/SDK update changes `maxInterpolations=`/
      `displayed=` behavior — the creation log watches it for free.

## P2 — Synthetic UI (XeSSSynthUI) validation — CLOSED v4.7 (removed; verdict below)
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
- [x] TESTE 5c ANSWERED 09/11 (v4.4 tint session, user-corrected): the UI
      elements (wheel/minimap) blink MAGENTA ↔ WHITE in mode 3. SDK composites
      our texture AND the layer is UI-localized. Flicker = real UI (white, on
      real frames) alternating with synthetic UI (magenta, on generated
      frames) — a coverage/edges mismatch problem, not noise, not alignment
      (that hypothesis died with the correction).
- [x] 5d ANSWERED 09/11 13:55 session (consolidated run): FG off via numpad
      `*` → blinking STOPS, pure white (field-confirmed; telemetry shows the
      enabled=0 stretch). Mechanism proven: mode-3 flicker = real UI on real
      frames vs our estimated UI on generated frames.
- [x] FINAL FIELD VERDICT on the v4.6 estimator: "same blink" (user), now on
      ALL UI incl. minimap = coverage expanded but the mismatch dominates —
      offline noise metrics cannot capture the real limiter (structural
      estimate-vs-real difference). Decision: synthetic UI + modes 2-5 + tint
      REMOVED in v4.7; mode NONE is the shipped and only UI handling.
- [x] SHIPPED EARLY in v4.6 (user directive: do not gate code on more field
      time; validate with local mocks instead): replaced `max channel delta ×2`
      with the two-sided per-pixel unmix of `B = a·U + (1−a)·C` —
      white model `(B−C)/(1−C)`, dark model `(C−B)/C`, per-pixel max, 0.012
      deadband, 0.15 floors. Offline-validated in `tools/sim-ui-estimator.py`
      vs synthetic ground truth: mae_ui 0.5569→0.1939, leak 0.0135→0.0096,
      EMA'd UI flicker 0.0133→0.0105, center bias halved. Field validation
      (consolidated session steps 2–4) still pending.
- (dropped v4.7: UI coverage/alpha readback — no UI layer to measure anymore)

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
