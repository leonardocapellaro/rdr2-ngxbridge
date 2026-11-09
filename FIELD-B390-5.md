# FIELD-B390-5 — analysis of the 2026-11-08 field session (v4.1)

Collection: `field/coleta-20261108-2107.zip` (extracted next to it). Single game
session of ~105 s captured (the earlier checklist tests ran in other game
sessions whose logs were not collected — only the LAST session, TESTE 5, is
evidenced). Banner: `RDR2 Vulkan FG 0.82+hudfield-v4.1` ✓. INI diff vs the
07/11 working baseline: only `XeSSUiMode=5` + `XeSSSynthUI=1` added.

## Decoded telemetry (from src emission sites)

`XeSS telemetry`: `requested=` our generate-eligibility decision,
`enabled=` SDK `actual.isFrameGenEnabled`, `displayed=` `actual.framesPresented`,
`input=` matched input order, `hud=` `HudCaptureFrameIndex()`. `XeSS
eligibility` lines fire only on flips.

## What the log proves

1. **Synthetic UI v4.1 pipeline works end-to-end on hardware**:
   `XeSS synthetic UI armed ... mode 5` → `uiMode=5 uiComposition=-1` →
   `Synthetic UI layer first frame: 3440x1440 format=44 (ring=3)`. No clamp, no
   record failure, no TagFrame/SDK errors. The implementation runs.
2. **But frame generation itself was mostly OFF**: telemetry samples with
   `enabled=1`: **7/44 ≈ 16 %** uptime. The old "FG works" session (07/11, ran
   the ORIGINAL author's .asi — no fork banner) shows **35/77 ≈ 45 %**.
3. Cause: `XeSS bridge: HUD-less post-tone-map pass missing for over 500 ms;
   waiting for a matching current-frame snapshot ... native=0` — the bridge
   disables FG while the NativeHUDProtection post-tone-map capture misses. In
   this session: continuous 70.7 → 84.8 s, one gap 102.9 → 103.1 s. Eligibility
   flips: 69.9 ON → 70.2 OFF → 85.0 ON → 103.0 OFF → 103.1 ON.
4. **The weapon-wheel observation in TESTE 5 therefore happened mostly with FG
   OFF** (native rendering, ~36 fps). "Mode 5 still flickers" is not a verdict
   on mode 5 — it is likely FG on/off flapping and/or native wheel behavior.

## Verdicts on the user-reported grid (with honesty)

| Test | User verdict | Log-based verdict |
|---|---|---|
| 1 baseline | wheel flickers | consistent with prior sessions (FG flapping at 45 % uptime) |
| 2 `XeSSUiMode=1` | flicker gone, frametime less smooth | plausible (composition off, whole-backbuffer interpolation) — confounded by uptime, but direction matches Intel's documented mode-4 weakness |
| 3 `XeSSUiMode=1`+`XeSSUiComposition=0` | "didn't worsen, maybe better" | **no-op**: mode NONE already forces composition disabled (`xess_bridge_runtime.cpp:97-100`) — identical config to test 2. The difference was placebo/uptime noise. Comp testing is only meaningful with modes 2–5. |
| 4 menu multiplier | still dead | **real bug, confirmed**: `mouse movement observed` + `left click observed in game queue` (queue hook works) but ZERO keyboard lines — not even a rejection. Key-detection path is dead. Workaround: INI `DLSSGFrames=2/3`. |
| 5 `XeSSSynthUI=1`+`XeSSUiMode=5` | still flickers, unsure if it worked | pipeline demonstrably ran, but FG was OFF ~84 % of the session — verdict void |

## Decisions (drive v4.2; see TODO.md)

- **P0**: fix/robustify the HUD-less pass matcher (author's path — the same
  stall existed on 07/11); add always-on INFO lines for FG enable/disable flips
  and hudless miss/recovery, so ANY future capture self-proves FG uptime with
  `DebugLogging=0`.
- **P1**: fix menu key path (mouse proven to arrive, keyboard never seen).
- **P2**: synthetic UI stays opt-in; add temporal smoothing candidate; re-test
  only in a session with proven high FG uptime.
- **Default change**: fork default `XeSSUiMode` 4 → 1 (only user-observed
  flicker-free configuration; documented in the release guide).
- **Everything English** from now on (docs, releases, commits).
- Collection protocol fix: `collect-once.bat` must grab ALL rotated logs
  (`RDR2VulkanFG*.log`), and the guide must insist: run it after EVERY game
  session (each config = its own zip), otherwise we only ever see the last run.

## Addendum (same day, after the user's follow-up) — TESTE 5 verdict corrected

The "verdict void" call was wrong and the user's correction is accepted: they
opened the weapon wheel right after FG first activated (~+87 s) — INSIDE the
enabled=1 window that lasts until the session end (7/44 telemetry samples
enabled=1, all at the tail; zero drops after activation). So:

- **Real verdict: `XeSSSynthUI=1` + `XeSSUiMode=5` flickers the wheel even with
  FG genuinely on.** The synthetic layer runs but is not sufficient.
- Structural explanation: our layer estimates alpha from the same (backbuffer,
  HUD-less) pair the SDK's mode-4/5 extraction uses, so it carries the same
  noise on translucent UI; mode 5 additionally "refines by extracting" from the
  backbuffer, layering the known weak spot on top.
- **Activation delay forensics** ("FG took forever to read inputs and turn on"):
  0→+68.8 s the game had not started SR evaluation at all (loading/menu —
  `NGX EvaluateFeature hook live` at +68.8 s) — not our latency;
  +68.8→+85 s the HUD-less matcher had not yet accepted any capture (the stall
  class fixed in v4.2); from +85 s FG ran without a single drop.
- Consequence: v4.3 adds EMA smoothing (`XeSSSynthSmooth`, default 35) and makes
  **mode 3** (no extraction at all) the primary experiment, with a smoothing-off
  ablation row. Mode 5 kept only as a reference row.
