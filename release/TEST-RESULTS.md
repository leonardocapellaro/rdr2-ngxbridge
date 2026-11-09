# TEST-RESULTS — test evidence (development environment)

Date: 2026-11-07. Environment: Fedora 44 (kernel 7.2.13), `x86_64-w64-mingw32-gcc
(GCC) 16.2.1 (Fedora MinGW 16.2.1-1.fc44)`, `g++ (GCC) 16.2.1 (Red Hat)`,
`wine-11.20 (Staging)`. **No RDR2 and no Intel GPU here** — the tests below
prove the compatibility chain with mocks faithful to the real behavior of
OptiScaler and RDR2VulkanFG (behaviors verified directly in the source
code of both codebases), not the game.

## T1 — real Windows x64 build

```
$ bash build.sh
...
OK (0 failures)          <- T2 now runs embedded
BUILD OK
-rwxr-xr-x. ...   292040 build/mock_game.exe
-rwxr-xr-x. ...   341272 build/mock_mor3a.dll
-rwxr-xr-x. ...  2810574 build/mock_optiscaler.dll
-rwxr-xr-x. ...  3822004 build/zz_ngxbridge.asi
```

Artifact sanity (imports; nothing exotic, normal DLL):

```
$ x86_64-w64-mingw32-objdump -p build/zz_ngxbridge.asi | grep 'DLL Name'
        DLL Name: KERNEL32.dll
        DLL Name: msvcrt.dll
```

## T2 — unit tests of the shared ABI (native Linux)

```
$ g++ -O2 -std=c++17 -Isrc tests/test_mirror.cpp src/mirror_core.cpp -o build/test_mirror -pthread
$ ./build/test_mirror
... (cases: Ptr<->ULL, I<->UI, ULL->I range, F->Ptr refused, missing key,
     Success+null-pointer treated as absence, Reset, idempotent getObject)
OK (0 failures)
```

## T3 — end-to-end self-test under Wine (the briefing §7 mandatory test)

```
$ bash selftest/run_wine.sh
```

Covers (§7): create/eval NGX, OptiScaler's "original" vs. fake handle,
module path, ASI load order, valid depth/MV, resolution by name,
vtable mirroring, cross-type read, missing input (nonexistent
key → original Fail), reentrancy (bridge thread vs. loader), and above all the required one:
**"mock NGX demonstrating that the observer receives the necessary
resources even when OptiScaler provides the upscaling
implementation"** — `OBSERVER-OK` with real pointers coming from the OptiScaler mock
with no actual NVSDK in the process.

Full transcription (reproducible; only addresses/timestamps change):

```
----- mock_game stdout -----
[GAME   ] == mock game: RDR2 NGX loader simulation ==
[OPTI-MOCK] loaded
[MOR3A-MOCK] loaded
[GAME   ] modules: opti=... mor3a=... bridge=...
[MOR3A-MOCK] gate installed (game IAT GetProcAddress redirect); waiting for NVSDK_NGX_VULKAN_* resolutions
[MOR3A-MOCK] gate name=...CreateFeature provider='Z:\...\build\selftest\nvngx.dll'
[MOR3A-MOCK] RESULT: ACCEPT - installing input-capture wrapper
[MOR3A-MOCK] gate name=...EvaluateFeature provider='Z:\...\build\selftest\nvngx.dll'
[MOR3A-MOCK] RESULT: ACCEPT - installing input-capture wrapper
[GAME   ] resolved alloc=... create=... eval=...
[OPTI-MOCK] AllocateParameters -> param=00000000007c5370
[OPTI-MOCK] dump(obj=00000000007c5370) vptr=... (off=+0x32140)
    slot[ 0..15]=stubs externos   <- mirrored vtable
[MOR3A-MOCK] CreateFeature: SR flags result=1 value=1 MIRROR-CROSS-TYPE-OK
[GAME   ] CreateFeature result=1
[MOR3A-MOCK] OBSERVER depth(result=1 ptr=0000000140009100) motionvectors(result=1 ptr=0000000140009000) OBSERVER-OK
[GAME   ] frame 1 evaluate result=1
[MOR3A-MOCK] OBSERVER depth(result=1 ptr=0000000140009100) motionvectors(result=1 ptr=0000000140009000) OBSERVER-OK
[GAME   ] frame 2 evaluate result=1
[GAME   ] post-Reset provider=1 flags read result=0 STALE-GUARD-OK
----- bridge log -----
== ngxbridge 0.1.1 == enabled=1 spoof=1 mirror=1 target=mock_optiscaler.dll hints=2
target found: Z:\...\build\selftest\mock_optiscaler.dll at ...
hook GetModuleFileNameW: kernel32=...:1 kernelbase=...:1
hook GetModuleFileNameA: kernel32=...:1 kernelbase=...:1
hook observer: ntdll!LdrGetProcedureAddress=... -> 0
mirror: 2/6 parameter-factory export hooks created   (mock only has the Vulkan ones; the real one has all 6 - verified in the code)
armed: spoof=1 (kernel32 ok), observer=1, factory hooks=2/6
intercepted NVSDK_NGX_VULKAN_AllocateParameters (result=1 obj=00000000007c5370) by mock_game.exe
mirror: vtable ... patched (16 typed slots + Reset) - cross-type reads now answered, stale-after-reset guarded
mirror: coerced typed answer for 'DLSS.Feature.Create.Flags'
mirror: no fallback for 'DLSS.Feature.Create.Flags'    <- post-Reset: correct refusal of the stale value
resolved 'NVSDK_NGX_VULKAN_AllocateParameters' from the OptiScaler handle by mock_mor3a.dll -> ...
resolved 'NVSDK_NGX_VULKAN_CreateFeature' from the OptiScaler handle by mock_mor3a.dll -> ...
spoofed provider path for capture module: Z:\...\build\selftest\nvngx.dll
resolved 'NVSDK_NGX_VULKAN_EvaluateFeature' from the OptiScaler handle by mock_mor3a.dll -> ...
spoofed provider path for capture module: Z:\...\build\selftest\nvngx.dll
----- verdict -----
SELFTEST: PASS
```

What each line proves:

| Line | Proof |
|---|---|
| `RESULT: ACCEPT` ×2 | with the spoof, the Mor3a gate accepts (without the bridge it REJECTS — regression mode below) |
| `spoofed provider path` | the spoof fires only for the target caller, exactly 2× (Create+Evaluate) |
| `OBSERVER-OK` ×2 frames | the wrapper's observer sees **non-null** Depth+MotionVectors from the OptiScaler mock on every frame |
| `MIRROR-CROSS-TYPE-OK` + `coerced typed answer` | the cross-type read (stored int read by the "unsigned" slot) only exists because of the mirror — the pure provider would fail there (verified with Mirror=0: `MIRROR-MISSING(flags)` + invalid `input`) |
| `2/6 ... created` | graceful degradation when an export doesn't exist (the mock only has the 2 Vulkan ones; the real one has all 6, verified in the 0.9.4 code) |
| `STALE-GUARD-OK` + `no fallback ...` | after the provider's `Reset()` (slot 16), the mirror **refuses** the pre-reset value — the "no stale frame is valid" invariant has its own test |
| `by mock_game.exe` / `by mock_mor3a.dll` | correct attribution of who created the object (the game) and who forwarded the resolution (Mor3a's global hook; in production it will show `RDR2VulkanFG.asi` — it's exactly the signal DIAGNOSIS §6.1 looks for) |

## T4 — self-test on Windows

`release/selftest-windows/run_selftest.bat` runs the same chain on native Windows
x64 (no Wine) and prints `SELFTEST: PASS/FAIL`. Run it on the B390 **before**
installing into the game. No runtime dependencies (everything static).

## Regressions verified during development

- `Mirror=0` → `MIRROR-MISSING(flags)`: the mirror is genuinely necessary,
  not decorative.
- `Spoof=0` → `RESULT: REJECT` and no `OBSERVER` line: reproduces
  production FG-109 (current behavior without the bridge).
- Third inline engine in `GetProcAddress` (discarded by design): silences
  the previous layers — which is why the final design doesn't hook GPA.

## Honest limits

Wine ≠ production Windows for very specific loader semantics; that's
why T4 exists. And no mock replaces the real RDR2: the hypothesis that only
the observer log on the B390 can settle is the caller module of
`LdrGetProcedureAddress`/`GetProcAddress` in the game (DIAGNOSIS §6). **Nothing here claims "it works in
RDR2"** — it claims the proven root cause was eliminated and measurable on the
real machine.
