# zz_ngxbridge — installation (RDR2 Vulkan + XeFG on the Arc B390)

**What it is:** a bridge (a single `.asi`) that makes OptiScaler 0.9.4 (which serves
the Vulkan upscaling) and RDR2VulkanFG/Mor3a beta (XeFG 2x) recognize each other. The
root cause of FG never turning on (`input=0`, FG-109) is explained in
`DIAGNOSIS.md`. The bridge **does not replace or modify** any of your
components.

**Honest status:** built by code analysis + logs from your setup, with
a green automated self-test (faithful mocks). **Still not validated on the B390 with
real RDR2** — the logs below say exactly how far the chain got.

---

## Requirements (what you already have)

- RDR2 Epic in the default folder with: `OptiScaler.asi` (0.9.4) + OptiPatcher,
  `RDR2VulkanFG.asi` (Mor3a's beta) or equivalent + `dinput8.dll` loader,
  `fakenvapi.dll` (optional);
- game renderer set to **Vulkan**;
- In `OptiScaler.ini`: `[Inputs] EnableDlssInputs=true` (without it the NGX-Vulkan surface
  doesn't even exist) and `[Upscalers] VulkanUpscaler=xess` (note: the key is
  `[Upscalers]`; in `[General]` it is ignored).

## Installation (2 steps)

1. Copy **this entire `release` folder** into the RDR2 folder (the one with
   `RDR2.exe`).
2. Run `install.bat` (double-click). It only creates/backups
   `zz_ngxbridge.*` files and warns if OptiScaler or the loader don't appear in the folder.

To remove: `uninstall.bat` (restores the previous backup, if there was one).

## Quick validation BEFORE opening the game

Run `selftest-windows\run_selftest.bat` — it only uses the fake DLLs from the
package's own folder, it doesn't touch the game. It must print `SELFTEST: PASS`. If it fails already
in Windows, the problem is the environment (antivirus blocking injection? run the game
without blocking too).

## What to expect in the game

1. The game opens with Vulkan + upscaling as usual (the upscaling does **not** change);
2. The bridge log in `RDR2VulkanFG\ngxbridge.log` should contain, in order:
   - `spoofed provider path for capture module: ...\nvngx.dll` (1-2×);
   - `resolved 'NVSDK_NGX_VULKAN_...' from the OptiScaler handle by <module>` — `<module>` is the first non-system component on the stack of whoever requested the symbol (typically `RDR2VulkanFG.asi`, since its global hook forwards the call; see DIAGNOSIS §6.1);
   - `intercepted NVSDK_NGX_VULKAN_AllocateParameters ... by <game exe>` — the game created the parameters object;
   - `mirror: ... parameter-factory export hooks created` (6/6 on the real OptiScaler) and
     `mirror: vtable ... patched (16 typed slots)`;
3. The RDR2VulkanFG log (its own, in the same folder) should start showing what
   **never appeared** in your current logs:
   - `NGX hooks: module=... commit=0 ...`
   - `FG input pipeline v2: accepted SR handle=... flags=...`
   - `NGX inputs first valid: depth ... mvec ... jitter ...`
4. Telemetry/watchdog: `evals>0`, `input=1`, `enabled=1`, FG-109 absent,
   2× frames in the XeFG OSD.

## About `FSR 2.2.1` vs XeSS-SR (not a bridge bug)

The `Vulkan | FSR 2.2.1 | Input: DLSS` OSD you were seeing is OptiScaler's silent
fallback: on the Vulkan path it only accepts XeSS-SR if a
`libxess.dll` with `xessVK*` exports exists in the game folder (DIAGNOSIS §4). The
XeFG **works with FSR 2.2.1 as SR** (accepted mode in the briefing). If you want
XeSS-SR on top, you place your own `libxess.dll` in the folder yourself (we do not
redistribute Intel components).

## Troubleshooting

| Symptom | Probable cause / what to do |
|---|---|
| `RDR2VulkanFG\ngxbridge.log` missing | the ASI loader didn't load the bridge → check `dinput8.dll` in the folder and the name `zz_ngxbridge.asi` (the `zz` is to make it load last) |
| Log exists but never `spoofed provider path` | your Mor3a ASI path doesn't contain `rdr2vulkanfg` (renamed?) → adjust `CallerHints` in `zz_ngxbridge.ini` and restart; or the game isn't on Vulkan |
| `spoofed provider path` yes, but no `NGX hooks:`/`accepted SR handle` in the Mor3a log | send both logs (the bridge log has the `resolved ... by <module>` line that closes the diagnosis — see DIAGNOSIS §6.1) |
| FG turns on but stutters/worsens in motion | expected: jitter read fails via the reverse ABI (DIAGNOSIS §6.2) — documented, with a small follow-up |
| Antivirus/Defender complaining about `.asi` | injection into a game process is what every ASI loader does; add the folder to the exception (it's your OptiScaler/loader that already does this) |

## Files

| File | Role |
|---|---|
| `zz_ngxbridge.asi` / `zz_ngxbridge.ini` | the bridge + config (keys documented in the ini itself) |
| `install.bat` / `uninstall.bat` | installation/rollback with backup |
| `selftest-windows/` | self-test that doesn't touch the game |
| `DIAGNOSIS.md` | root cause with code and log evidence |
| `CHANGES.md` | what changes (nothing existing) and the hook ownership map |
| `TEST-RESULTS.md` | test transcriptions |
| `BUILD.md` | how the binary was compiled (MinGW, no MSVC) |
| `THIRD-PARTY-NOTICES.txt` | licenses (only MinHook MIT; no proprietary content included) |
| `SHA-256SUMS.txt` | file integrity |

## If something goes wrong

`uninstall.bat` removes everything (restores the previous state in seconds). Useful
logs for a report: `RDR2VulkanFG\ngxbridge.log`, the RDR2VulkanFG log and a
screenshot of the OptiScaler OSD.
