# CHANGES — what this package adds (and what it doesn't touch)

## Nothing existing was modified

Zero bytes changed in: OptiScaler 0.9.4, RDR2VulkanFG (Mor3a 1.0 beta),
OptiPatcher, dinput8 ASI loader, fakenvapi, RDR2.exe, DRM, existing configs.
The `OptiScaler/` and `rdr2-vulkan-frame-generation/` repositories remain
intact (the paths cited in DIAGNOSIS.md are read-only).

## New files (everything)

| Path | What it is |
|---|---|
| `case/ngxbridge/src/ngxbridge.cpp` | the bridge's code (single ASI, ~600 lines C++17) |
| `case/ngxbridge/src/mirror_core.{h,cpp}` | versioned shared ABI: real slot map, defensive coercion table |
| `case/ngxbridge/tests/test_mirror.cpp` | native unit tests of the coercion |
| `case/ngxbridge/mock/` | faithful mocks for the self-test (game/OptiScaler/Mor3a) — never installed in the game |
| `case/ngxbridge/build.sh`, `selftest/run_wine.sh` | MinGW Windows-x64 build + automated self-test |
| `release/` | this package (binary, ini, scripts, docs, Windows self-test) |

## Design decisions (and why)

1. **A single ASI named `zz_ngxbridge.asi`** — the `zz` prefix ensures the
   ASI loader loads it last (alphabetical order), after OptiScaler
   and RDR2VulkanFG, so it observes and extends an already-assembled state. It
   neither replaces nor precedes anything.
2. **No `GetProcAddress` hook.** It already has TWO owners in the stack:
   OptiScaler (kernel32+kernelbase, always with fall-through) and Mor3a (MS
   Detours). A third inline engine at the same address — measured in practice
   — silences the previous layers. Everything in the bridge was repositioned to
   ownerless surfaces: `GetModuleFileNameW/A`, `ntdll!LdrGetProcedureAddress`
   and the bodies of the 6 parameter-factory exports.
3. **No `nvngx.dll` on disk.** The spoof is just the name answer to the
   right asker; loading a fake file would create a real module in
   front of OptiScaler (upscaling regression) — unacceptable (§6).
4. **Spoof with caller scoping** (OptiScaler handle + asker path
   ∈ `CallerHints`): no other module notices the bridge. It's the
   smallest possible surface to satisfy exactly the Mor3a gate.
5. **Mirror only on the 6 factory objects**: vtables are patched per
   new object (idempotent, map protected by mutex, never freed — the
   object lives as long as OptiScaler lives; no access to a Vulkan resource after
   release because the bridge doesn't touch Vulkan resources, only the typed
   values table of the parameters object).
6. **Defensive coercion with a closed list** (`MirrorCoerce` in mirror_core.h,
   documented line by line): `Ptr↔ULL` always; `I↔UI` with correct
   sign/extension; `ULL→I` with range check; `F/D→int` only if finite and integral;
   **never** `F→Ptr` nor any type→pointer coercion other than ULL/Ptr. We deliberately
   refused to answer jitter-float through the pointer slot — documented
   in DIAGNOSIS §6.2 as a limitation with a follow-up, instead of hiding the error
   with a bogus value.
7. **Instrumentation without spam**: `LOGT` macros with "first N + every 600";
   counters for GPFNs, spoofs, allocations and observed resolutions. Nothing per
   frame.

## Hook ownership map (final state of the stack)

| API | OptiScaler | Mor3a (RDR2VulkanFG) | zz_ngxbridge |
|---|---|---|---|
| `LdrLoadDll`/`NtLoadDll` | hook | — | — |
| `GetProcAddress` (k32/kb) | hook (fall-through) | Detours | — (doesn't touch) |
| `GetModuleFileNameW/A` | — | — | hook (caller scoping) |
| `LdrGetProcedureAddress` | — | — | hook (log-only observer) |
| `NVSDK_NGX_VULKAN_{Create,Create1,Evaluate}` exports | — | Detours (only after the gate accepts) | — |
| `{Allocate,Get}Parameters` exports × 3 APIs | — | — | hook (installs the mirror) |
| vtable of the parameter objects | — | read via reverse map | typed stubs per object |

## v0.1.1 — fixes from the pre-delivery review

An adversarial code review (code-review findings + `.bat` smoke test
under Wine) found and fixed 11 problems. The ones that matter for anyone
who will run this in the game:

| # | Problem | Consequence without the fix | Fix |
|---|---|---|---|
| R1 | Race in `MirrorEnsureObject` (check/patch/registration not atomic) | two threads creating the 1st object of each class could save stubs as "originals" → **self-recursion → stack overflow**, or silent provider drops | a single critical section covers check+patch+registration |
| R2 | `wcscpy` without bound in `g_fakePath[MAX_PATH]` | OptiScaler path ≥251 chars → **buffer overflow** in .bss | verified assembly; a too-long path disables only the spoof, logged |
| R3 | `fprintf`/`fflush` executing under the **loader lock** (GPFN/`LdrGPA` hooks) | classic loader-lock↔heap deadlock freezing the game's boot | static lock-free queue (no heap) drained on a normal thread |
| R4 | Mandatory observer (`!anyL` disarmed everything) | failing the **log** hook turned off the **fix** | only the spoof is load-bearing; observer/mirror degrade with a warning |
| R5 | `HookBoth` accepted kernelbase as sufficient | kernel32 not hooked → the forwarder disguises the caller → the spoof never fires **in silence** | kernel32 is mandatory; the failure is logged as CRITICAL and the rest arms for diagnostics |
| R6 | `Reset()` (slot 16) never mirrored | the post-Reset cross-type read would answer the **stale value** (possibly an already-freed resource) — violates briefing §6 | slot 16 patched; `Reset` clears the mirror's memory (test `STALE-GUARD-OK`) |
| R7 | Target searched for only 30 s then eternal give-up | slow OptiScaler (boot with many MODs) = bridge inert the whole session | polling while the process lives, warning after ~30 s |
| R8 | Observer always attributed "by kernel32.dll" | the §9 questions ("who resolved?") answered wrong | `RtlCaptureStackBackTrace` skipping system/trampolines; factories also attribute |
| R9 | Counter wrapping and skipping one `NoteSet` every 2^32 | 1 record lost per wrap (the comment said "harmless" — it wasn't) | the increment becomes statistics only; the store always happens |
| R10 | `install.bat` running in its own copied folder (README mode): moves to backup and copies from a source already moved | **installation failed in the documented main flow** | in-place mode detected; backup only in the with-argument mode |
| R11 | `%date%` for the backup timestamp + path without `\` in the argument + chained `if` that wine/cmd won't swallow | invalid timestamp, broken concatenation, uninstall without effect | `%time%` stamp, path normalization, `goto` instead of the double `if` |

The installer/uninstaller were tested under Wine across the 5 flows (clean
installation with argument, upgrade with backup+restore, uninstall with restore,
in-place mode, uninstall without backup). Self-test with the new `STALE-GUARD-OK`
condition and caller attribution: PASS.

## v0.1.2 — fix pointed out by the target machine's real cmd.exe

| # | Problem | Consequence | Fix |
|---|---|---|---|
| R12 | `run_selftest.bat`: `if exist … (type …) else (echo (SEM LOG…))` — the `)` of the echo closes the `else` block early; Wine forgives it, but the **real cmd.exe rejects it** (`) foi inesperado`) and the script dies before the verdict | the self-test on real Windows printed the stdout, but not the bridge log nor `SELFTEST: PASS`/`FAIL` | printing via `type … 2>nul \|\| echo …`, without parentheses; the only case of this pattern in the whole package (the other `(` in `echo` are outside blocks and are literals) |

No `.asi` changed between 0.1.1 and 0.1.2 — only the self-test `.bat` and the
declared version (package name/hash). The 4 stdout criteria already appeared
in the real cmd report; the 2 from the bridge log (spoof + vtable) were
verifiable via `findstr` in the same execution directory.

## Compliance with the briefing's constraints (§6)

- no recursive/duplicated hooks: a single MinHook, zero address overlap ✔
- no deadlock between mutex/present/Vulkan queues: the only mutex is the mirror's
  value map (never holding present locks; stubs don't
  call Vulkan) ✔
- no access to a Vulkan resource after release: the bridge doesn't touch resources ✔
- no previous frame as valid / no invented motion vectors: coercions only
  reformat the current frame's own value; no current value → the provider's original
  Fail ✔
- safety checks not turned off to hide an error: the Mor3a
  gate stays active; the bridge only makes the name answer honest about a
  real provider (OptiScaler) — dangerous coercion refused, documented ✔
- RDR2.exe/DRM/protections untouched ✔
- no regression in Vulkan upscaling: OptiScaler keeps serving everything; the
  spoof is invisible outside the target caller ✔
- versioned/documented/defensive/fallback shared ABI: `mirror_core`
  with explicit table + tests ✔
- real compiled Windows x64 binary (not fictitious): `zz_ngxbridge.asi`
  built with MinGW 16.2.1 (MSVC x64 ABI), self-tested under Wine ✔
