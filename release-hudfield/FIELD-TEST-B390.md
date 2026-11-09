# B390 field test — package v4.5 (log tag: `hudfield-v4.5`)

Run ONE config per game session. After EVERY session: close the game, then
double-click `collect-once.bat` (it grabs ALL logs now — if you only run it at
the end, only the last session ever lands in a zip).

## Before anything (once, game closed)

1. On the B390 PC, download the zip from the release:
   https://github.com/leonardocapellaro/RDR2-Vulkan-FG/releases/latest
   Extract anywhere (e.g. `Downloads\hudfield`).
2. In the game folder: rename `RDR2VulkanFG.asi` → `RDR2VulkanFG.asi.v44.bak`.
3. Copy the package's `RDR2VulkanFG.asi` into the game folder; the
   `RDR2VulkanFGDiagnostics.dll` goes into the game folder's `RDR2VulkanFG\` subfolder.
4. Open `RDR2VulkanFG.ini` (next to the `.asi`) and make sure the TEST 1 block
   below is in place (it is basically the old baseline minus the v5 knobs).

Log path, always: `<game folder>\RDR2VulkanFG.log`. Every session starts with
`... RDR2 Vulkan FG 0.82+hudfield-v4.5 ...` — if that line is missing, the new
`.asi` is not the one running.

## How to run each test (repeat for every test)

- Game open, character in the world (not the main menu). 2–3 min of gameplay.
- Follow the test's ROUTE (below).
- Close the game. Double-click `collect-once.bat`.
- Rename the generated zip after the test: `coleta-BASE.zip`, `coleta-MODE4.zip`, …

Then send me the zips (or the paths).

---

## TEST 1 — DEFAULT BASELINE (mode 1, everything on defaults)

INI `[fgvk]`:
```ini
FGProvider=XeSS
DebugLogging=0
```
(remove or comment `XeSSUiMode`, `XeSSSynthUI`, `XeSSUiComposition`)

ROUTE: play 2–3 min world → open the weapon wheel ~10 s → close → keep playing.
Then answer:
- a) Did the wheel flicker? (mode 1 expectation: NO)
- b) Open the log, search `eligible=`. Count how many ON/OFF flips and the
  longest `eligible=1` stretch. Expected on v4.2: far fewer flips than the
  11/08 session, and `HUD-less miss start` lines should be rare — if present,
  note their `lastReject=` number.
- c) Stutter check: any stutter you feel should line up with an `eligible=0`
  window in the log times.

Send: `coleta-BASE.zip`

## TEST 2 — MENU + MULTIPLIER (v4.4: the self-close bug is fixed)

Keep the same INI as TEST 1.
ROUTE: in-world, press the MenuKey (in your INI: `MenuKey=0x78` = **F9**).
- a) Search the log for `menu: open`. On v4.2/v4.3 the menu opened and a second
  later the same physical press re-entered through the game's queue and closed
  it again (`menu: open` immediately followed by `menu: close`, ~9 ms apart).
  With v4.4 that must NOT happen — one press = one toggle, and every key line
  now carries `dedup=0/1`. Press once to open, again to close: you should see
  exactly one open and one close.
- b) With the overlay open (it should STAY open now): switch Multiplier to 4x,
  watch ShowFPS (`displayed` goes toward 4×, and the log prints
  `XeSS interpolation target set: x4 ...`). If it does not open: send the log
  anyway — the proof line tells us where the key died.
- c) FG toggle hotkey note: per INI, `KeyToggleFG=0x6A` is **numpad `***` (not
  F6), and `KeyCycleFrames=0x23` is **End** (not Tab) — the labels in some older
  notes were wrong.

Send: `coleta-MENU.zip`

## TEST 3 — MULTIPLIER VIA INI (v4.4: this is now REAL, was silently capped at x2)

Your 08/11 session proved the old build created the swapchain at 1 interpolated
frame regardless of INI/menu — `DLSSGFrames=2 (x3)` was read but `displayed` was
stuck at 2 forever. v4.4 creates the swapchain at the SDK max. Game closed, INI
`DLSSGFrames=2` → play 2 min → `collect-once.bat` → `coleta-3X.zip`. Verify in
the log, in order: `createdInterp=3` (creation line), `XeSS interpolation target
set: x3 ...`, then telemetry `displayed=3`. Expect ~3× displayed fps; FG on
~most of the time per TEST 1's method.

## TEST 4 — MODE 4 CONTROL (the flicker reference)

INI add: `XeSSUiMode=4`. ROUTE: weapon wheel 10 s.
Expected: flicker returns (known Intel extraction weakness) — this row calibrates
the comparison. Note the `uiMode=4` creation line.

Send: `coleta-MODE4.zip`

## TEST 5 — SYNTHETIC UI + MODE 3 (re-run for the record; v4.4 changes nothing here)

The 08/11 night re-run was VALID this time (FG on continuously ~105 s) and the
wheel still flickered — so re-running TEST 5 only matters if you want a second
sample. **Do TEST 5c first**: it decides WHICH component is flickering before
any more estimator work is worth it.

INI add:
```ini
XeSSSynthUI=1
XeSSUiMode=3
```
(`XeSSSynthSmooth=35` is the default; leave it alone unless doing TEST 5b.)

ROUTE: play 2–3 min → weapon wheel ~15 s → observe.
The log decides before your eyes do — check first:
- `XeSS synthetic UI armed` + `uiMode=3` in the creation lines,
- `Synthetic UI layer first frame: 3440x1440 ... smoothing=1` (pipeline ran,
  EMA on),
- **before opening the wheel**, search `eligible=1` — only judge the wheel while
  FG is on (long ON stretches; if you were mostly `eligible=0`, the verdict does
  not count — that is what burned TEST 5 on 08/11 night... and note FG then took
  ~85 s to first activate: the wheel test belongs to the `enabled=1` window).

Verdicts to report: wheel flicker (none/light-halo/same-as-mode-4), and any UI
ghosting or smear when the wheel appears/vanishes (EMA ramp is ~2–3 frames).

Send: `coleta-MODE3.zip`

## TEST 5b — OPTIONAL ABLATION (only if TEST 5 looked good)

Same as TEST 5 plus `XeSSSynthSmooth=0`. If THIS flickers and TEST 5 did not,
smoothing is proven as the working mechanism. Skip if TEST 5 was a clear win or
clear fail. Send: `coleta-MODE3NOSMOOTH.zip`.

## TEST 5c — ANSWERED 09/11 (12:10 session): everything went magenta

Our synthetic texture IS composited by the SDK in mode 3. The magenta covering
EVERYTHING (not just the UI) says our diff layer contains far more than UI —
the working hypothesis is the two snapshots (backbuffer vs HUD-less) being out
of step while the world moves. TEST 5c no longer needs running; 5d does.

## TEST 5d — MAGENTA: STILL VS PAN (decides misalignment vs bad estimate)

Same INI as TEST 5 (`XeSSSynthUI=1 XeSSUiMode=3`) PLUS `XeSSSynthDebugTint=1`.
ROUTE: FG on → weapon wheel open →
1. **hold the camera completely still 15 s** — describe: does the magenta
   recede to just the UI elements (wheel/HUD), or does the world stay magenta?
2. then **pan/rotate slowly 15 s** — does the world light up magenta again?

Verdict:
- Magenta UI-only when still, world magenta when moving → the two input
  snapshots are time-misaligned; the layer itself can be made clean (fix =
  alignment/pairing, not the alpha math).
- Magenta everywhere even when still → the estimate (or snapshot content) is
  fundamentally wrong in motion-less frames too; deeper rework.

Send: `coleta-TINTSTILL.zip`

## TEST 6 — x3 DIAGNOSTICS RUN (expect x2 displayed; the LINES are the point)

INI `DLSSGFrames=2` + normal gameplay 2–3 min. Search the log for, in order:
- `XeSS interpolation target set: x3` (ours — should be there) and telemetry
  `interp=2` (ours) vs `displayed=2` (the SDK's) — confirms the clamp is SDK-side;
- `XeSS output pacing: reject detail magicOk=... flagsIntel=? flagsHardware=? flagsSoftware=?` —
  the exact values the software pacer rejected on; this drives the next change;
- `XeSS output pacing: SDK batch observed unbound; outputs=?` — if this line
  appears at all, the stock SDK IS scheduling multi-output batches and the
  clamp is elsewhere. Whether it appears or not is the answer.

Send: `coleta-X3DIAG.zip`

---

## What each answer unlocks

- TEST 5d UI-only magenta when still → snapshot alignment fix next; mode 3
  becomes genuinely promising after it.
- TEST 5d magenta everywhere even still → the synthetic layer concept needs
  rework (or `XeSSSynthUI` stays off); mode 1 remains the recommendation.
- TEST 6 reject-detail `flagsSoftware=0` (or another mismatch) → we know the
  presenter mode the field SDK actually uses; the pacer gets a verified
  contract for it (or a documented "cannot bind on this SDK" statement).
- TEST 6 shows `outputs>=3` unbound → the SDK does schedule more frames;
  suspicion moves downstream (our present loop / pacing waits).
- TEST 2 menu still self-closes (`open`+`close` pair) despite the fix → send
  log; the `dedup=` lines localize it precisely.
- Frequent `HUD-less miss start ... lastReject=5/6` during normal play → matcher
  strictness is the remaining limiter (known; next patch target). `lastReject=2`
  only at the moment the wheel opens is the expected extent-change behavior.
