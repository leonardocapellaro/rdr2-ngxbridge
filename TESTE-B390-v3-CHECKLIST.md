# B390 test — v3 package (log marker: `hudfield-v3`)

## Before anything else (only once, game closed)

1. On the B390 PC, download the release zip:
   https://github.com/leonardocapellaro/RDR2-Vulkan-FG/releases/tag/v0.82-hudfield-v3
   Extract it to any folder (e.g. `Downloads\hudfield-v3`).
2. In the game folder: rename `RDR2VulkanFG.asi` → `RDR2VulkanFG.asi.v2.bak`.
3. Copy the package's `RDR2VulkanFG.asi` into the game folder. The
   package's `RDR2VulkanFGDiagnostics.dll` goes in the game folder's `RDR2VulkanFG\` subfolder.
4. Open `RDR2VulkanFG.ini` (same folder as the `.asi`) and leave the BASELINE test block (below).

Log path, always: `<game folder>\RDR2VulkanFG.log`. Every session starts with the line
`... RDR2 Vulkan FG 0.82+hudfield-v3 ...` — if it doesn't appear, the new `.asi` is not in use.

## How to run each test (repeat the 4 steps)

- Game open, character in the world (not in the main menu). 2–3 min of gameplay.
- Do the ROUTE for the test (below each one).
- Close the game. In the extracted folder, double-click `collect-once.bat`.
- Rename the generated zip with the test label: `coleta-BASE.zip`, `coleta-MODE1.zip`, `coleta-COMP0.zip`.

Then send me the zips (or the path).

---

## TEST 1 — BASELINE (same setting as 2026-11-07, just to confirm the v3)

In the INI, section `[fgvk]`:
```ini
FGProvider=XeSS
DebugLogging=1
```
(delete/comment out `XeSSUiMode` and `XeSSUiComposition` if they exist; `CapturePresentTransfers` is not needed)

Route:
1. Walk around the world. It should give ~2x the "real" fps (as before: ~36→~72).
2. Open the MOD menu with the keypad `-` key. The log should show `menu: open`.
   - If it opened: test switching the Multiplier to 4x and watch the fps double again (~144).
   - If it did NOT open: reopen the game? NO — go to TEST 4 (input diagnostics).
3. Open the weapon wheel and the item wheel. They should FLICKER (that's the known problem — confirm and that's it).
4. Look in the log for the line: `virtual swapchain: ... uiMode=4 uiComposition=1`.

Expected result: works like the last session, the wheel flickers, and the log shows `uiMode=4 uiComposition=1`.

## TEST 2 — `XeSSUiMode=1` (the important test)

Same INI + one line:
```ini
XeSSUiMode=1
```

Route: same (the weapon/item wheel is the focus; pay attention mainly to the ITEM WHEEL).

What the results MEAN (tell me which one happened):
- **(a) The wheel stopped flickering, but blurs/melts when you rotate it or it animates**
  → the culprit is mode 4's "UI extraction by difference". Partial win: it opens the
    path of mode 5 (synthesizing the UI), which is the real fix. It's the most likely one.
- **(b) The wheel keeps flickering the same way**
  → the flicker doesn't come only from the extraction; I'll investigate the HUD path with OptiScaler.
- **(c) The world/objects started smearing or leaving trails (ghosting)** — the wheel may
  even stop, but it got worse
  → mode 1 sends the HUD through the interpolation AI, which is Intel's documented
    weakness. Not to be left on; it only measures the direction.
- Confirm `uiMode=1` in the log (if `uiMode=4` shows up, the INI wasn't read).

## TEST 3 — `XeSSUiComposition=0` (expected WORSE — just confirming the direction)

Same baseline INI, WITHOUT the `XeSSUiMode` line, with:
```ini
XeSSUiComposition=0
```

Route: 30–60 s of gameplay + wheel.

Expected result: WORSE than baseline (the HUD tends to disappear/thin out in the generated
frames, flickering badly). If it really gets worse: great, that confirms that composition being
on is what is holding the HUD up today. **Turn this key off and go back to baseline right away.**

## TEST 4 — MENU diagnostics (run together with TEST 1, if the menu doesn't open)

With the game in the foreground:
1. Press the keypad `-`. (That's the menu key, `MenuKey=0x2D`.)
2. Didn't open? Click once anywhere INSIDE the game window and press it again.
3. Didn't open? `Alt+Tab` out of the game and `Alt+Tab` back, press it again.
4. Didn't open? Press `End` and then the keypad `*` (these are hotkeys — the `*` toggles FG on/off;
   the fps should drop/double with it).

Then, in the log, tell me which of these lines exist:
- `menu: open` → the menu opened (use the menu to switch the multiplier)
- `menu input: left click observed` → clicks reach the game (the problem is only the menu)
- `hotkey: ... ignored — foreground` → the key arrived, the game was not focused
- none of these → the input doesn't even come close; this is where I'll fix it in the fork

## Without the menu (multiplier alternative, if needed)

With the game closed, in the INI: `DLSSGFrames=1` = x2, `2` = x3, `3` = x4. Restart the game on
every change and confirm in the fps and in the log's telemetry line.

## Rules

- One variable per session. Never switch the provider to DLSS-G during the session (it locks up).
- Don't delete the `.bak` files (beta and v2) — they are the rollback.
- Normal-use session after the tests: `DebugLogging=0` (saves a tiny bit of fps).
