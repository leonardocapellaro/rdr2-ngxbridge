# Self-test on Windows (before installing in the game)

Running this **does not touch the game nor any of your MODs** — nothing here is
copied to the RDR2 directory, no file from this package touches the game.
`mock_game.exe` loads only the three fake DLLs from this folder and exercises the
full chain: provider spoof → the "Mor3a" gate accepts → the vtable mirror
fixes the cross-type read → the observer records the resolutions.

## How to run

1. Double-click `run_selftest.bat` (or run it in cmd from this folder).
2. Wait for the final line: `SELFTEST: PASS` or `SELFTEST: FAIL`.

Do you need the Visual C++ redistributable? **No** — the binaries are
static. Windows 10/11 x64 is enough.

## Files

| File | Role |
|---|---|
| `mock_game.exe` | simulates the RDR2 (RAGE) NGX loader |
| `mock_optiscaler.dll` | simulates OptiScaler's NGX surface (including the handle's "fake load") |
| `mock_mor3a.dll` | simulates the RDR2VulkanFG gate (nvngx.dll whitelist) |
| `zz_ngxbridge.asi` | the bridge (the same binary that goes into the game) |
| `zz_ngxbridge.ini` | configuration pointing at the mocks |
| `run_selftest.bat` | runs and checks the 5 conditions |

If you get `FAIL`, run in cmd from this folder and send the contents of `stdout.log` and
of `RDR2VulkanFG\ngxbridge.log`.
