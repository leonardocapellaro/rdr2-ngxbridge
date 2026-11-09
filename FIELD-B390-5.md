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
## Addendum 2 (2026-11-09) — three v4.3 sessions (23:43 mode 3, 23:48 mode 4, 23:51 mode 1)

User verdicts: activation is now fast (confirmed in logs: `NGX EvaluateFeature
hook live` +76.6 s → first `eligible=1` +77.6 s — 1.0 s, the v4.2 race fix
worked; the ~77 s before it is the game's own loading, not the bridge). Only
mode 1 was flicker-free; modes 3+EMA and 4 flickered. Multiplier dead in menu
and INI alike. Logs:

- **Mode 3 + EMA verdict is VALID** — `Synthetic UI layer first frame ...
  smoothing=1` at +70 s and the session ran with FG enabled for long
  continuous stretches (≈105 s window through the wheel test). So mode 3
  without extraction still flickers: either the composite ignores our texture
  or the alpha estimate is bad. Undecidable from this data — v4.4 ships
  `XeSSSynthDebugTint=1` (magenta layer) to decide in one session.
- **Menu F9 opened and self-closed** (this is why the multiplier "menu doesn't
  work"): `hotkey: ... poll` → `menu: open` (+72.627) → `menu input: menu key
  F9 observed in game queue` (+72.636, 9 ms later — the same physical press)
  → `menu: close`. The dedup only blocked the poll path after a queue toggle,
  not the queue path after a poll toggle. Fixed in v4.4 (both directions) with
  `dedup=` printed on every observed press.
- **Multiplier hard-ceiling proven**: 23:48 session ran with `DLSSGFrames=2
  (x3)` in the config line, creation line `maxInterpolations=3`, and the
  `displayed=` histogram over the whole session: `displayed=1`×37,
  `displayed=2`×11 — never 3. Swapchain was created with
  maxInterpolatedFrames=1 hardcoded; SetInterpolations cannot exceed the
  creation cap. v4.4 creates at `sdk.MaxInterpolations()` and logs
  `createdInterp=` + `XeSS interpolation target set:`.
- Residual matcher note: boot-transient `lastReject=5` at ~+0 s (78 ms,
  harmless) and one `lastReject=2` (extent) per wheel-open — RDR2's wheel
  variant changes render extent; refusing to stale-match is by-design. Noted
  for the matcher; not relaxed.
- Session inventory: 23:43 mode 3 (previous.log of coleta-235027), 23:48 mode 4
  (shared log between the two zips), 23:51 mode 1. All three zips uploaded.

## Addendum 3 (2026-11-09 noon) — v4.4 field session: tint ANSWERED, x3 clamp localized to SDK/pacing side

Sessions 12:02 (mode 1, x3 INI + menu to x4 live) and 12:10 (mode 3 + tint), both
v4.4 (`createdInterp=3` present):

- **Decisive tint test: the UI elements blink MAGENTA ↔ WHITE** (user report +
  `tint=1` in log). The SDK composites our synthetic UI texture in mode 3.
  *Correction the same evening:* the first reading said "magenta everywhere"
  — the user clarified it was ONLY the wheel/minimap blinking magenta↔white.
  That kills the whole-screen/snapshot-misalignment hypothesis and replaces it
  with a better story: real frames carry the game's actual UI (white),
  generated frames carry our composited synthetic UI (magenta); mode 3's
  flicker is the per-frame mismatch between the two, and our layer is
  UI-localized (the estimator finds real UI). TESTE 5d rewritten around it:
  FG-off ablation (numpad `*`) — blinking must stop → mismatch confirmed;
  then the estimator gets a proper per-pixel alpha unmix.
- **x3 status: our side is now provably correct** — `interpolation target set: x3`
  at attach and `x4` at 12:07:59 (the menu path WORKS now), `SetInterpolatedFrames`
  succeeds, yet `displayed=2` for the whole enabled stretch and the telemetry
  histogram shows never >2. (`requested=` was a boolean — misleading, fixed.)
- **New root-cause candidate found in old logs**: `XeSS output pacing: context
  ownership/mode rejected` appears in EVERY session since 11/07 (checked all five
  collected sessions) — the software pacer built for 2x/3x/4x on Intel cards has
  never bound. x2 needs no pacer; >x2 evidently does. v4.5 adds `reject detail`
  (witness stage + intel/hardware/software mode flags read from the presenter)
  and the unbound-batch breadcrumb (`outputs=N` if the stock SDK ever schedules
  multi-frame batches) to finish the localization in one more session.
- v4.5 also makes `SetInterpolations` edge-only (was every present — Intel perf
  note; also was resetting the cadence estimator each frame).

## Addendum 4 — v4.6: estimator rewritten offline (09/11 afternoon, no field time used)

User directive after the v4.5 checklist: *"já faz tudo junto, não tenho tempo
pra ficar pesquisando isso por fora.. ou voce mesmo faça testes com mocks aí
locais"* → stop gating code on field sessions; validate locally, ship, and
fold every open question into ONE field run.

**Mock testing done**: `tools/sim-ui-estimator.py` (pure Python, stdlib — no
numpy installed here) renders a synthetic scene with ground truth (weapon-wheel
ring with 1px AA, minimap, translucent panel, dark UI block), composites
`B = a0·U + (1−a0)·C` with per-frame independent σ=0.012 noise on BOTH captures
(two reads of the same world carry independent tone-map grain — that is the
adversary), scores estimator variants on exactly the field-visible quantities.
Final-scene numbers (60 frames):

| variant | mae_ui | edge_rmse | center_bias | leak_bg | ema_bg | ema_ui |
|---|---|---|---|---|---|---|
| V0 current (`max delta ×2`) | 0.5569 | 0.4307 | −0.2688 | 0.0135 | 0.0088 | 0.0133 |
| **V6 two-sided (shipped v4.6)** | **0.1939** | **0.1725** | **−0.1295** | **0.0096** | **0.0065** | **0.0105** |

(Also tried V5 = one-sided + deadband: leak/ema best of all — 0.0019–0.0057 —
but mae_ui stays ~0.50, i.e. it fixes noise while still missing all dark UI;
coverage is the bigger field-visible defect, so V6 wins overall.)

(V6: `dup=max(B−C)−0.012 over max(1−cmax,0.15)`, `ddn=max(C−B)−0.012 over
max(cmax,0.15)`, a=clamp(max(dup_a,ddn_a)) — white-UI and dark-UI unmix models
maxed per pixel; deadband ≈ 1σ of noise.) Why V0 loses: alpha≈0 on the dark UI
block (dark UI coverage simply missed) and ×2 amplifies grain into the
background. Where it matters most: opaque centers push to a=1, where the
generated frame reproduces B exactly and cannot flicker.

Honest limits: synthetic scene, CPU-simulated, not RDR2, not the real GPU
tonemap chain; it picked the estimator, the field still judges it.

**v4.6 = v4.5 + the new estimator** (`ui_diff.comp` regenerated into
`ui_diff_spv.h`, banner `hudfield-v4.6`). x3 untouched; the v4.5 diagnostics
lines fire in the same run. **FIELD-TEST-B390.md v4.6 replaces the whole test
grid with ONE ~5-minute session** (x3 INI + mode 3 + tint; play → wheel →
FG-off → wheel ×3 → collect-once). One zip answers x3's cause, the estimator's
look, and the 5d mechanism question together.

## Addendum 5 — v4.6 consolidated session (09/11 13:55, coleta-20261109-135553): both programs CLOSED, v4.7 cleanup

Field report (user): new estimator = "mesma coisa" — blinking everywhere UI
(now including minimap: coverage expanded as the sim predicted, mismatch still
dominates); FG off → pure white, blinking stops (mechanism confirmed); no
ghost seen under the blink storm.

**x3 — root cause found and it is a wall, not a bug** (first run where the
new forensics all fired):
- `reject detail magicOk=1 implOk=1 chainOk=1 presenterOk=1 nativeOk=0(cccccccccccccccc) deviceOk=0(cccccc001c49abe9) flagsIntel=0 flagsHardware=233 flagsSoftware=171`
  → the chain IS found, but the deeper witness fields read **MSVC DebugFill
  (0xCCCC…) and code bytes (0xE9/0xAB)** — the MFG-Unlocker-pinned presenter
  layout does not exist in the field Intel binary; the pacer can never bind
  without reverse-engineering a closed vendor DLL. Rejected on safety.
- `SDK batch observed unbound` breadcrumb: **zero hits in every session** →
  the SDK scheduler itself never batches >1 output unbound. `interp=2` ×26
  enabled samples, `displayed=2` 100% — clamp is inside Intel's path.
- Note: `DLSSGFrames` creation actually follows `shared_bridge_runtime.h`
  (wrapper passes MaxInterpolations()); provider's interpCap was cosmetic.
  Both pinned to 1 now.

**Decision (user's ultimatum: fix now or mode 1 only):** no responsible path
to x3 or to a match-perfect UI layer exists with our data → v4.7 removes
synthetic UI (ui_synth + shaders + all 5 knobs → mode NONE fixed), removes
xess_output_pacing, pins multiplier to x2 (honest log/menu/cycle). Kept all
v4.2 fixes + health lines. Fork 664ef7d; asi c1dcd522; dll 33871233; zip
e9f61be8; release v0.82-hudfield-v4.7 (latest), v4.6 superseded.
Also from this session: two `lastReject=5` (loadOp) misses in normal play —
known by-design strict, self-heals; extent matcher stays backlog.
Missing input: user mentioned a second zip ("extra") — never arrived on disk
(only 135553 exists, duplicate copies identical sha 610ccd2d).
