# TODO — RDR2 Vulkan FG (maintained, English from 2026-11-08)

Priorities after the 2026-11-08 field session (see FIELD-B390-5.md). No more
"run-a-test-send-back" cycles: every change ships with log lines that make a
single 1-minute gameplay capture answer the question by itself.

## P0 — HUD-less snapshot stall (the real defect) — FIXED IN v4.2, FIELD VALIDATION OPEN
- [x] Epoch race fixed (capture stamped at record time) + subpass-marking
      consistency in v4.2, with the never-stale-frame rule intact.
- [x] Always-on forensics shipped in v4.2: `XeSS eligibility:` flips,
      `HUD-less miss start/end` + `lastReject` code (visible with DebugLogging=0).
- [ ] Confirm in the next B390 session: FG uptime should approach 100% in-world;
      08/11 evidence says the stall was 15 s of never-matching capture after
      world start — watch `lastReject=` values if any miss lines remain
      (5=loadOp / 6=UNDEFINED layout would mean matcher strictness by design).

## P1 — Menu multiplier dead on keys — FIXED IN v4.2, FIELD VALIDATION OPEN
- [x] Toggle un-gated from renderer init, GetAsyncKeyState poll backstop with
      500 ms dedup, always-on proof line per press (queue or poll path).
- [ ] Next session: F9 must print either the queue line or the poll line; if it
      still does not open the overlay, the proof line localizes the failure.
- [ ] Workaround still documented: INI `DLSSGFrames=2` (3x) / `3` (4x).

## P2 — Synthetic UI (XeSSSynthUI) validation — ITERATION 2 IN v4.3
- [x] v4.1 executed the pipeline end-to-end on hardware (armed + first-frame,
      no errors) — implementation runs.
- [x] 08/11 follow-up: mode 5 + raw layer flickered the wheel WITH FG on
      (user-observed inside a log-verified enabled window). Verdict: raw layer
      insufficient — it re-derives the same extraction the SDK does.
- [x] EMA smoothing shipped in v4.3 (`XeSSSynthSmooth`, 0–100, default 35;
      accumulator resets with provider history; log shows `smoothing=1`).
- [ ] NEXT FIELD RUN: TESTE 5 = mode 3 (no extraction at all) + smoothing;
      TESTE 5b = ablation with smoothing off. Judge only inside long
      `eligible=1` stretches. Outcomes:
      mode 3 clean → new default candidate + ship guidance;
      mode 3 clean but 5b flickers → smoothing proven as the mechanism;
      mode 3 still flickers → the alpha estimate itself is the limiter →
      next step is a better estimator (multi-channel least squares, edge-aware).
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
