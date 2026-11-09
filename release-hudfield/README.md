# RDR2 Vulkan FG — build `0.82+hudfield-v4.3` (B390 + OptiScaler)

> Look for the tag **`hudfield-v4.3`** on the first lines of `RDR2VulkanFG.log`.
> If it does not appear, the old `.asi` is still the one in use.

**What this build is:** the 0.82 base of the original project (public source,
MIT — the original author's license is intact in the repo's `LICENSE.txt`) plus
our changes: frame-generation stability fixes, input/menu fixes, XeSS-FG UI
knobs, and guards against configurations that would kill FG. No NVIDIA/Intel/
Rockstar binaries here — only the `.asi` we compile and the diagnostics collector.

**What is proven on real hardware (B390):** FG 2x works under native Vulkan with
OptiScaler (~36 real fps → ~72 displayed at 3440x1440). The 2026-11-08 session
also proved the fork's synthetic-UI pipeline runs end-to-end on the card
(`armed → first frame` in the log, zero errors).

## What changed in v4.3 (synthetic UI temporal smoothing + mode 3)

Your 2026-11-08 night report: **mode 5 (`XeSSSynthUI=1` + `XeSSUiMode=5`) still
flickered the weapon wheel — and this time with FG demonstrably on** (the log
shows FG enabled from +87 s to the end of the session, wheel opened inside that
window). That verdict is real, and it is informative: the synthetic layer
computes `B − (1−a)·C` from the exact same pair the SDK's mode-4 extraction uses
— so its alpha estimate carries the same noise Intel documents for translucent
UI, and mode 5 even re-applies the backbuffer extraction on top ("refine by
extracting"). Two changes:

1. **Temporal smoothing (EMA) of the synthetic layer — `XeSSSynthSmooth`**
   (0–100, default 35 = new-frame weight). A stationary UI (wheel, minimap)
   converges in ~2–3 frames while the per-frame alpha-estimation noise averages
   out; vanished UI fades to hard zero (no ghost tail). The accumulator restarts
   on provider history resets. `0` restores the raw v4.2 behaviour.
2. **Mode 3 is now the primary UI experiment** (`HUDLESS_UITEXTURE`: "interpolate
   HUD-less color, blend UI from UI texture"). Unlike 4/5 it does **no backbuffer
   extraction at all** — the wheel can only come from our (now smoothed) UI
   texture. If the flicker is extraction noise, mode 3 removes its source; if it
   survives mode 3, the synthetic estimate itself is the limiter and we know it
   from one session.

## What changed in v4.2 (the "why does FG keep dropping" fix — still in this build)

Analysis of the 2026-11-08 field logs (`FIELD-B390-5.md` in the project repo)
showed frame generation was actually **on only ~16% of the session** (and only
~45% of the 11/07 session) because the bridge disables FG whenever the HUD-less
snapshot is missing for >500 ms. Two real bugs in that path are fixed:

1. **HUD-less epoch race (root cause).** The capture stamp was inferred from the
   *current* frame counter at query time. RDR2 renders 2+ frames ahead of
   presenting; when the weapon wheel stalls the render thread, the present for
   frame N looked up frame N+1's counter, decided "capture missing" — while the
   capture had actually succeeded — and dropped FG for seconds. The capture is
   now stamped with its own acquire window at record time. This is strictly
   *safer* than before: it also closes a latent hole where an old capture could
   be accepted for a newer frame.
2. **Render-pass marking inconsistency.** The v1 render-pass hook marked color
   attachments only in subpass 0, while the RenderPass2 hook marked all subpasses;
   a final full-screen pass referenced only from subpass >0 (a known weapon-wheel
   / menu pattern) could be permanently invisible to the capture. Marking is now
   consistent (all other safety gates unchanged: exact extent, format, LOAD,
   real initial layout, swapchain image, single sample).

**Menu keys (multiplier) — fixed and self-proving.** Earlier logs showed mouse
messages reaching the game queue but *zero* keyboard lines — there was no
keyboard logging on that path at all, and the menu-key toggle was additionally
gated behind the overlay renderer being initialized. Now: the toggle no longer
waits for the renderer, an independent poll (GetAsyncKeyState, every present,
de-duplicated against the queue path) catches the key even if the game's
message queue never delivers keyboard messages, and every menu-key press prints
an always-on log line (`menu input: menu key F9 observed…` or
`hotkey: menu key … seen by present-loop poll`).

**Always-on FG health lines.** Even with `DebugLogging=0` you now get:
`XeSS eligibility: … eligible=1/0 …` on every FG on/off flip,
`HUD-less miss start/end` with a reject code explaining why the capture was
rejected, and the menu proof lines. A single 2-minute gameplay capture now
answers "was FG even on?" without guessing.

**Fork default changed: `XeSSUiMode=1`** (was 4). Mode 1 (NONE — the whole
backbuffer, HUD included, is interpolated) was the only configuration observed
flicker-free on the B390 weapon wheel; Intel documents mode 4's backbuffer
extraction as weak against semi-transparent UI. You can still pick 2–5 in the
INI.

## About the stutter you felt (mode 1)

Most likely it is exactly what the logs show: FG dropping to native 36 fps and
back ("stutter" is the 72→36→72 flapping), which has been happening in every
session — including the one that "worked". Also note: in mode 1 composition is
already off by design (Intel's auto rule); **do not** set
`XeSSUiComposition=1` with mode 1 — that asks the SDK for UI compositing work
mode 1 does not use. Leave the knob at `-1` (or remove the line). The v4.2
fixes attack the drops directly; the new log lines correlate stutters with FG
flips.

## Installation (one file swap)

1. Game closed. Rename the current `.asi` (v4.1/v4.2) to `RDR2VulkanFG.asi.v42.bak` — rollback.
2. Copy this `RDR2VulkanFG.asi` into the game folder. The optional collector goes
   to `RDR2VulkanFG\RDR2VulkanFGDiagnostics.dll` (subfolder, as before).
3. In `RDR2VulkanFG.ini`, keep:
   ```ini
   FGProvider=XeSS
   DebugLogging=0        ; 0 = less overhead; 1 = detailed logging (use 1 only while collecting)
   ```
   The new defaults are the recommended baseline — no new keys needed. To use
   more generated frames per real frame, set `DLSSGFrames=2` (3x) or
   `DLSSGFrames=3` (4x) — the menu Multiplier should work now (MenuKey per INI),
   but the INI path is the trusted one.

## Test grid v2 (each row = one INI + one game session)

Because previous results were confounded by the FG drops, re-establish the
ground truth. After **every** game session (game closed), double-click
`collect-once.bat` and rename the zip (`coleta-<name>.zip`) — the script now
grabs ALL rotated logs, and only the last session lands in a zip if you skip it.

| INI config | Expectation |
|---|---|
| *(default — nothing extra)* mode 1 | no wheel flicker (mode 1 baseline); FG flips should be far fewer than before — count `eligible=1` vs `eligible=0` windows in the log |
| `XeSSUiMode=4` | author default; wheel flicker expected (known Intel extraction weakness) — control row |
| `XeSSSynthUI=1` + `XeSSUiMode=3` | **the primary v4.3 experiment**: no backbuffer extraction at all; wheel comes only from the smoothed synthetic layer. Check `XeSS synthetic UI armed`, `uiMode=3`, `Synthetic UI layer first frame ... smoothing=1` lines — and judge only during long `eligible=1` stretches |
| `XeSSSynthUI=1` + `XeSSUiMode=3` + `XeSSSynthSmooth=0` | ablation row: same as above without temporal smoothing. If THIS flickers and the row above does not, smoothing is what fixes it — keep it |
| `XeSSSynthUI=1` + `XeSSUiMode=5` | kept for reference: on 08/11 night it flickered **with FG proven on** (extraction refinement on top); expect no better than mode 3 |
| `XeSSUiComposition=0` | only meaningful with modes 2–5 (mode 1 already has composition off); expected WORSE — HUD vanishes from generated frames; revert immediately |

Rules: one variable per session; verify the creation line
`... uiMode=%u uiComposition=%d` in the log matches what you set; the
`hudfield-v4.3` tag must appear.

## Honest notes

- Base is **0.82**, not 1.0beta — the beta never reaches FG under OptiScaler
  (it fails its HUD-less gate before the first interpolated frame; closed
  source). The path we use exists identically in both.
- Real build: `zig c++ -target x86_64-windows-gnu` (zig **0.17.1**; the author
  uses 0.16). Compiles clean; loads/unloads under Wine; **it does not run RDR2
  here** — only the B390 proves it.
- The HUD-less capture matcher is the original author's design; v4.2 fixes a
  race and an inconsistency in it but does not redesign it. If the new
  `HUD-less miss start ... lastReject=N` lines show `5` (loadOp) or `6`
  (UNDEFINED layout) during normal gameplay, the remaining stalls are matcher
  strictness by design — report the code, and the next step is known.
- Synthetic UI alpha is an estimate (`max channel delta ×2`, clamped) — on
  08/11 night mode 5 + raw layer flickered with FG on, so the estimate is real
  work in progress; v4.3's temporal EMA (`XeSSSynthSmooth=35` default) is the
  first countermeasure and mode 3 (no extraction) is its fair stage. Edge
  quality still needs eyes. With `XeSSSynthUI=0` (default) nothing of it is
  allocated.

## Rollback

Game closed → delete this `.asi` → rename `RDR2VulkanFG.asi.v42.bak` back. The
`zz_ngxbridge` shim is untouched.
