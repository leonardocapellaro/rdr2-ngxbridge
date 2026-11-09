# FIELD-B390-1 — first validation on the real machine (2026-11-07)

Binaries: zz_ngxbridge 0.1.2 + RDR2VulkanFG 1.0beta (Nexus 10997, "With Amd
Support") + OptiScaler 0.9.4. Logs in `error_logs/` (bridge + RDR2VulkanFG).

## What the bridge proved on the real machine

DIAGNOSIS §7 chain fulfilled through step 4:

1. `spoofed provider path for capture module: ...\nvngx.dll` (W and A) ✔
2. the Mor3a gate accepted: `FG input pipeline v2: accepted SR handle=...
   flags=0x2b render=2293x960 output=3440x1440` — a line absent from the
   old logs (FG-109) ✔
3. `NGX inputs first valid: depth 2293x960 fmt=130, mvec ... fmt=83,
   jitter=(0.0000,-0.1667) mvScale=(1.0,1.0)` — depth/MV/jitter valid;
   **the jitter was read** (better than the limitation predicted in DIAGNOSIS §6.2) ✔
4. telemetry `input=` climbed past 725 — **FG-109 no longer exists** ✔

Observed anomalies (harmless):
- factory hooks: `6/6 created` but zero `intercepted` → the game creates the
  parameters object without going through the exported bodies we hooked
  (OptiScaler returns an internal impl via the GPA hook). The pipeline worked
  anyway; the mirror was not needed in practice (OptiScaler resolves
  the cross-type reads internally).
- the `LdrGetProcedureAddress` observer was armed but silent → the game's
  resolutions don't go through the ntdll resolver visible to the hook. The
  §6.1 question was left unanswered by the observer, but was in fact
  answered: the gate accepted, so the target-caller's resolution did happen.

## New obstacle (OUTSIDE the bridge — the Mor3a layer itself)

The XeSS bridge waits ~17 s for a "HUD-less post-tone-map" pass snapshot
(`haveHud`, xess_provider.cpp:676/683; error :691) and then turns FG off.
Required signature (hud_capture.cpp:130-148): a render pass whose color
attachment = the bridge's virtual swapchain image, full extent,
samples=1, **loadOp=LOAD**, layout≠UNDEFINED, 1 image in subpass-0, same
epoch. With OptiScaler, the game's final pass targets the OptiScaler offscreen
and the content reaches the images via copy/blit → the signature never matches.
**No ini key relaxes this.** `TagHUDLess/TagUI` are only valid for
native DLSS-G (Streamline slSetTagForFrame).

## FG-201 at 99s: self-inflicted by the menu, not XeSS

`Live DLSS-G setup failed` (xess_provider.cpp:493) is only reachable via an FG
**restart with the DLSS-G provider selected in the menu** (menu.cpp:416).
DLSS-G never runs on Arc: `LoadAmpereUnlock` only accepts RTX
families (ampere_boot.cpp:113) → sticky FG-201 lock until the game is restarted.
Rule: `FGProvider=XeSS` in the ini, game closed; **never** select DLSS-G
in the menu. `context ownership/mode rejected` (5s) is cosmetic (pacing
software of SDK 2.0.2.68). `libxell.dll`/`libxess_fg.dll` loaded OK.

## Source versioning

The GitHub repository = **public 0.82** (SOURCE-NOTES.txt; config.cpp does not
know FSRFG and migrates FSR3→DLSSG). The 1.0beta binary adds FSRFG
(FSR4/*dll in the package) on top; the XeSS path strings match the public
source (FG input pipeline v2, MFG unlock, virtual swapchain).

## Next steps — outcome (2026-11-07, end of day)

1. `FGProvider=FSRFG` on the 1.0beta: **FAILED on the B390.** Error in the
   game menu (user's photo): FG-3xx "FSR Vulkan features unavailable ...
   Three independent pacing queues are unavailable". FidelityFX-FG
   Vulkan requires 3 independent pacing queues that the Intel driver does
   not expose. Dead end until the driver changes; the XeSS+patch route is
   the only viable one today.
2. **DONE** — `hud_capture.cpp` patch on the public 0.82 source (opt-in key
   `CapturePresentTransfers=1`: a full-extent blit/copy into a virtual
   swapchain image counts as a HUD-less snapshot; the TRANSFER_DST layout
   + restoration keep the game's barriers
   consistent). Built on the DEV machine in Linux (no MSVC needed on the
   user's machine): `tools/build-mor3a-zig-linux.sh`, zig
   **0.17.1** (Build.ps1's 0.16.0 has broken libc++ windows-gnu
   on a Linux host; requires a `Windows.h`→windows.h shim in
   lib/libc/include/any-windows-any/). A clean baseline compiles
   (d2adebe8…) and only the patched binary contains the new strings
   (f375dede…); Wine LoadLibrary/FreeLibrary smoke OK. **Not validated
   in-game** — still needs a session on the B390. Deliverable:
   `release-mor3a-hudless/` (README pt-BR with install/rollback,
   patch, NEXUS-REPORT-draft.md, collect-once.bat single-collection script).
3. report to Mor3a: **draft ready** in
   `release-mor3a-hudless/NEXUS-REPORT-draft.md` (English, ready to paste;
   covers the HUD-less signature under third-party SR Vulkan, the FG-3xx
   pacing-queues issue and the DLSS-G menu trap on Arc). Sending is up to
   the user (the Nexus account is theirs).
