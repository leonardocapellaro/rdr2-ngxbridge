# B390 field test — package v4.6 (log tag: `hudfield-v4.6`)

**v4.6 changed the plan: ONE session, about 5 minutes.** Earlier packages
needed a test per question; this single run answers all three open questions
together (why x3 clamps, how the new UI estimator looks, what the real-vs-
synthetic blink mechanism is). One INI, one route, one zip.

## Install (once, game closed)

1. Download the zip from the release:
   https://github.com/leonardocapellaro/RDR2-Vulkan-FG/releases/latest
   Extract anywhere (e.g. `Downloads\hudfield`).
2. In the game folder: rename `RDR2VulkanFG.asi` → `RDR2VulkanFG.asi.v45.bak`.
3. Copy this package's `RDR2VulkanFG.asi` into the game folder; the
   `RDR2VulkanFGDiagnostics.dll` goes into the game folder's `RDR2VulkanFG\` subfolder.
4. Replace the `[fgvk]` section of `RDR2VulkanFG.ini` with exactly this:

```ini
[fgvk]
FGProvider=XeSS
DLSSGFrames=2
XeSSSynthUI=1
XeSSUiMode=3
XeSSSynthDebugTint=1
DebugLogging=0
```

   (`DLSSGFrames=2` = x3 multiplier; `XeSSSynthDebugTint=1` paints our
   synthetic UI magenta — it is a diagnostic knob, delete it later.)

5. Game open, character in the world. The log
   (`<game folder>\RDR2VulkanFG.log`) must start with
   `... RDR2 Vulkan FG 0.82+hudfield-v4.6 ...` — if that tag is missing, the
   old `.asi` is still running; stop and redo step 2/3.

## ROUTE (~5 minutes, in order)

1. **Play 2–3 min normally.** The x3 diagnostics run silently in the
   background — no action needed. The log lines we read afterwards are listed
   below; there is nothing to judge with your eyes here.
2. **Weapon wheel open ~15 s, look at it.** This is the new estimator's debut
   (tint on = our layer is magenta). Report, in plain words, what you see:
   - Is the blink between magenta and white still there? Steadier or worse
     than last time? (New estimator: opaque UI centers should now be solid
     magenta most of the time — the flicker should shrink toward the ring
     edges; if it looks identical to before, say so — that is also an answer.)
   - Anything magenta that is NOT UI, or UI that never turns magenta?
3. **Still with the wheel open: press the FG hotkey (numpad `*`) to turn FG
   OFF, hold ~15 s.** Expected: blinking STOPS, UI goes pure white/real.
   If the UI keeps blinking with FG off, note it — that would mean something
   else composites too.
4. **FG back ON (numpad `*`), then open/close the wheel 3 times fast.**
   Does magenta appear with a delay or leave ghosts when the wheel vanishes?
   (The EMA smoothing ramps in ~2–3 frames; a visible smear/ghost is worth
   a word.)
5. Close the game. Double-click `collect-once.bat`. Rename the zip to
   `coleta-V46.zip` and send it (or tell me the path).

## What the log will tell me (no action needed — just don't skip the collect)

- `interp=2` vs `displayed=2` in telemetry — x3 requested (ours: `XeSS
  interpolation target set: x3` should be there), still x2 displayed:
  confirms again the clamp is SDK-side, not ours.
- `XeSS output pacing: reject detail ...` — the exact values the software
  pacer rejects on (which stage, and the presenter's mode flags). This line
  drives the next change for x3.
- `XeSS output pacing: SDK batch observed unbound; outputs=?` — if this line
  appears AT ALL, the stock SDK already schedules multi-output batches and
  the clamp is elsewhere. Its absence is equally an answer.
- The tint route (steps 2–4) characterizes the real-vs-synthetic mismatch the
  new estimator has to shrink: blinking stops with FG off = mechanism
  confirmed; the step-2 description tells us how much the new estimator
  already won.

## If something goes sideways

- No `hudfield-v4.6` tag → old `.asi` in use; redo the rename.
- FG never reaches `eligible=1`/`enabled=1` (telemetry line every ~2 s) →
  just collect and send; the log localizes it (this INI is a diagnostic
  config, not the daily driver — if you want to keep playing meanwhile, revert
  to mode 1 defaults after the run).
- Anything magenta outside the UI, white UI that never tints, blinking
  surviving FG off, or a crash mid-route → still run `collect-once.bat`,
  send the zip, and add one line about what you saw. Field observation wins;
  describe it your way, I'll match it to the log.
