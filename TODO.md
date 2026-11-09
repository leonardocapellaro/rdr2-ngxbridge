# TODO — RDR2 Vulkan FG (maintained, English from 2026-11-08)

Priorities after the 2026-11-08 field session (see FIELD-B390-5.md). No more
"run-a-test-send-back" cycles: every change ships with log lines that make a
single 1-minute gameplay capture answer the question by itself.

## P0 — HUD-less snapshot stall (the real defect)
- [ ] The bridge drops FG whenever the post-tone-map HUD-less capture misses
      >500 ms. Measured FG uptime: ~45% (07/11, author's .asi) vs ~16%
      (08/11, v4.1). All earlier mode comparisons were confounded by this.
      Fix per investigation report; keep the never-stale-frame rule intact.
- [ ] Add always-on INFO lines: FG enabled/disabled state flips and
      hudless-miss/recovered (visible with DebugLogging=0).

## P1 — Menu multiplier dead on keys
- [ ] Field log proves mouse messages reach the game queue (observed lines) but
      keyboard produces nothing at all — not even a rejection line. Fix key
      path per investigation report; add an always-on "key seen" proof line.
- [ ] Interim user workaround (documented in release guide): set
      `DLSSGFrames=2` (3x) / `DLSSGFrames=3` (4x) in the INI instead of menu.

## P2 — Synthetic UI (XeSSSynthUI) validation
- [ ] v4.1 executed the pipeline end-to-end on hardware (armed + first-frame
      lines, no clamps, no errors) — implementation runs; VALUE unproven
      because FG was mostly off during observation.
- [ ] Consider temporal smoothing of the UI layer (EMA against the previous
      staged layer) so the tagged layer itself stops inheriting per-frame
      extraction noise from B−C.
- [ ] Optional: UI coverage/alpha stats via small readback so a future log
      proves the layer's content quality without eyes-on.
- [ ] Re-test mode 3/5 ONLY in a session where FG uptime is proven high by the
      new INFO lines.

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
