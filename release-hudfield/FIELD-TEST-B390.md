# B390 field test — package v4.3 (log tag: `hudfield-v4.3`)

Run ONE config per game session. After EVERY session: close the game, then
double-click `collect-once.bat` (it grabs ALL logs now — if you only run it at
the end, only the last session ever lands in a zip).

## Before anything (once, game closed)

1. On the B390 PC, download the zip from the release:
   https://github.com/leonardocapellaro/RDR2-Vulkan-FG/releases/latest
   Extract anywhere (e.g. `Downloads\hudfield`).
2. In the game folder: rename `RDR2VulkanFG.asi` → `RDR2VulkanFG.asi.v42.bak`.
3. Copy the package's `RDR2VulkanFG.asi` into the game folder; the
   `RDR2VulkanFGDiagnostics.dll` goes into the game folder's `RDR2VulkanFG\` subfolder.
4. Open `RDR2VulkanFG.ini` (next to the `.asi`) and make sure the TEST 1 block
   below is in place (it is basically the old baseline minus the v5 knobs).

Log path, always: `<game folder>\RDR2VulkanFG.log`. Every session starts with
`... RDR2 Vulkan FG 0.82+hudfield-v4.3 ...` — if that line is missing, the new
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

## TEST 2 — MENU + MULTIPLIER (should finally work)

Keep the same INI as TEST 1.
ROUTE: in-world, press the MenuKey (in your INI: `MenuKey=0x78` = **F9**).
- a) Search the log for `menu: open` — plus one proof line: either
  `menu input: menu key F9 observed in game queue` or
  `hotkey: menu key F9 seen by present-loop poll`. With v4.2 a press CANNOT
  be silent anymore.
- b) If the overlay opens: switch Multiplier to 4x, watch ShowFPS
  (`displayed` goes toward 4×). If it does not open: send the log anyway — the
  proof line tells us where the key died.
- c) FG toggle hotkey note: per INI, `KeyToggleFG=0x6A` is **numpad `***` (not
  F6), and `KeyCycleFrames=0x23` is **End** (not Tab) — the labels in some older
  notes were wrong.

Send: `coleta-MENU.zip`

## TEST 3 — MULTIPLIER VIA INI (fallback if TEST 2 fails)

Game closed, INI `DLSSGFrames=2` → play 2 min → `collect-once.bat` →
`coleta-3X.zip`. Expect ~3× displayed fps; FG on ~most of the time per TEST 1's
method.

## TEST 4 — MODE 4 CONTROL (the flicker reference)

INI add: `XeSSUiMode=4`. ROUTE: weapon wheel 10 s.
Expected: flicker returns (known Intel extraction weakness) — this row calibrates
the comparison. Note the `uiMode=4` creation line.

Send: `coleta-MODE4.zip`

## TEST 5 — SYNTHETIC UI + MODE 3 (the v4.3 experiment)

v4.2 night proved mode 5 still flickered with FG on — mode 4/5 extract the UI
from the backbuffer, the known weak spot. **Mode 3 uses no extraction at all**:
the UI can only come from our synthetic layer, now temporally smoothed.

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

---

## What each answer unlocks

- TEST 1 good + TEST 5 good → mode 3 + smoothing becomes the recommendation;
  ship default.
- TEST 1 good + TEST 5 flickers with PROVEN FG uptime → the synthetic estimate
  itself is the limiter (next step: better alpha); stay on mode 1 default.
- TEST 2 proof line without `menu: open` → send log; the line localizes the fix.
- Frequent `HUD-less miss start ... lastReject=5/6` → matcher strictness is the
  remaining limiter (known; next patch target).
