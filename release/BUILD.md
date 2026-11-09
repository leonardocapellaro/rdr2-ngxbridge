# BUILD — how `zz_ngxbridge.asi` was (and can be) compiled

## On Fedora (the environment used) — MinGW-w64

```
sudo dnf install mingw64-gcc mingw64-gcc-c++ mingw64-binutils wine
# wine deps: sudo dnf install --assumeyes wine
```

```
cd case/ngxbridge
bash build.sh          # Windows-x64 cross build + unit tests (native g++)
bash selftest/run_wine.sh
```

Outputs: `build/zz_ngxbridge.asi` (the release artifact), the self-test
mocks, `build/test_mirror`, `build/zz_ngxbridge.imports.txt`.

Toolchain notes:

- **MinGW 16.2 emits the MSVC x64 ABI** — nothing MSVC is mandatory; the briefing
  asked for CI that *actually compiles*, and this is the pipeline used. The code is
  clean C++17 (no SEH, no `__try`, no `<excpt.h>`; `__declspec(dllexport)`
  is not used in the bridge; the FARPROC↔typed casts are the only points
  that require `-Wno-cast-function-type`), so opening the solution in MSVC later and
  compiling `src/*.cpp` as an x64 DLL with MinHook also works — but the
  **delivered** binary is the one from this script, not a "would-compile".
- `mirror_core` is compiled in both worlds (MinGW and native g++ for the tests).
- MinHook 1.3.3 **vendored** in `case/vendor/minhook-1.3.3` (MIT, license
  included in `THIRD-PARTY-NOTICES.txt`); static link — no extra DLL.

## Source files

| File | Contents |
|---|---|
| `src/ngxbridge.cpp` | DllMain → config → init thread (finds the target) → hooks (GPFN W/A k32+kb+self-IAT, LdrGPA observer, 6 parameter factories) + per-object mirror |
| `src/mirror_core.{h,cpp}` | versioned ABI (`MIRROR_ABI_VERSION`), real vs. reverse slot map, defensive `MirrorCoerce`, mutex'd mirror |
| `tests/test_mirror.cpp` | native unit tests |
| `mock/*`, `selftest/*` | self-test (never installed in the game) |

## Optional CI (GitHub Actions, sketch)

```yaml
- name: Build (MinGW on ubuntu)
  run: |
    sudo apt-get update && sudo apt-get install -y mingw-w64
    bash build.sh
- name: Native unit tests
  run: ./build/test_mirror
```

(The Wine self-test doesn't run on the Ubuntu runner without `--enable-interactive`; use
`selftest-windows/run_selftest.bat` on a Windows runner/VM for native
coverage — that's why the Windows bundle exists.)

## Reproducibility

The binaries in `release/` are exactly the ones produced by `build.sh` on the date
in `SHA-256SUMS.txt`. There is no precompiled blob without matching source code
in the repository.
