# Diagnosis — why XeFG never turned on in RDR2 Vulkan (and what this bridge does)

Target machine: Win11, Intel Arc B390, RDR2 Epic 1.0.1491.50, 3440×1440,
**native Vulkan** renderer, OptiScaler 0.9.4 + OptiPatcher +
rdr2-vulkan-frame-generation (Mor3a 1.0 beta, production binary
`RDR2VulkanFG.asi`) + dinput8 ASI loader + fakenvapi.

## 1. What the machine's logs showed

Two files in `case_files/` (watchdog and RDR2VulkanFG log):

- `wd: evals=0` — OptiScaler's `EvaluateFeature` counter never incremented
  for the inputs wrapper;
- `Early NGX export routing commit=0` — **this is not an error**: `commit=0` is the
  `NO_ERROR` of `DetourTransactionCommit` (inputs.cpp:677). The
  `GetProcAddress` hook of RDR2VulkanFG was installed successfully. What's missing is
  what it should have seen pass through;
- telemetry `input=0 requested=0 enabled=0 displayed=1` — RDR2VulkanFG
  never received SR input; FG stayed disabled → code **FG-109**;
- OSD `Vulkan | FSR 2.2.1 | Input: DLSS` — explained in section 4.

## 2. Root cause (proven by code + logs)

The game's NGX chain with OptiScaler active:

1. RDR2 calls `LoadLibrary("nvngx.dll")`. OptiScaler hooks
   `LdrLoadDll`/`NtLoadDll` and **answers with its own HMODULE** — a "fake load", no
   `nvngx.dll` ever gets mapped
   (`OptiScaler/OptiScaler/hooks/LibraryLoad_Hooks.cpp:66-75`,
   `EnableDlssInputs`).
2. The game resolves `NVSDK_NGX_VULKAN_*` with `GetProcAddress(eseloghandle, ...)`.
   OptiScaler hooks `GetProcAddress` (kernel32: `Kernel_Hooks.cpp:111-146`;
   kernelbase: `:257-275`) and serves its own implementations. The
   **upscaling works** — that's why the game runs with SR.
3. RDR2VulkanFG also hooks `GetProcAddress` (via MS Detours,
   `rdr2-vulkan-frame-generation/src/inputs.cpp:675`). In its gate
   `h_EarlyNgxGetProc` (inputs.cpp:638-669), when someone resolves
   `NVSDK_NGX_VULKAN_{CreateFeature,CreateFeature1,EvaluateFeature}`, it
   asks **which file the provider module comes from** with
   `GetModuleFileNameW(handle)` and only installs the inputs-capture wrapper
   if the answer is exactly `nvngx.dll` or `_nvngx.dll` (the same
   check is repeated in `NgxProbeTick`, inputs.cpp:696-732).
4. Since the handle returned in step 1 is the **OptiScaler.asi** one, the answer is
   `...\OptiScaler.asi` → **silent REJECT** → no wrapper is installed
   → `evals=0` → no `Depth`/`MotionVectors`/flags reach the XeFG →
   `input=0 enabled=0` → FG-109.

Point 4 is the root cause. The logs close the circuit: the RDR2VulkanFG hook
is alive (`commit=0`), but it never sees the three names being resolved — because
the gate it would apply would reject them, and in fact it never fires
(no `NGX hooks: module=...` / `FG input pipeline v2: accepted SR handle=`
appears in the case logs).

### 2.1 The "nvngx.dll" module discrepancy (asked to investigate without assuming)

Resolved and it's **not another cause**: OptiPatcher (verified in the source code)
is a pure *pattern-patcher* binary — `VirtualProtect`+`memcpy`, zero hooks,
zero injection; it only NOPs vendor gates inside RDR2.exe in memory and
OptiScaler runs it as a plugin via the ASI loader. It does **not** interfere with
the game's `LoadLibrary("nvngx.dll")`: what returns the fake handle is OptiScaler's own
`LdrLoadDll` hook. So there are no two "nvngx" fighting;
there is a single provider (OptiScaler) whose file-name answer the
RDR2VulkanFG gate does not recognize.

## 3. Why OptiScaler "already serves DLSS" yet FG doesn't turn on — and the reverse ABI

Even if the gate accepted, there's a second ABI abyss the bridge also
resolves. RDR2VulkanFG reads the NGX parameters object through a **fixed vtable slot
map with inverted groups** (inputs.cpp:90-103:
`GetVoidPointer→slot 8`, `GetF→14`, `GetUI→12`, `GetI→11`, `SetI→3`,
`SetF→6`). The real vtable (`nvsdk_ngx_params.h` v0.9.4 header) is:

```
Set: ULL=0 F=1 D=2 UI=3 I=4 D3D11=5 D3D12=6 void*=7
Get: ULL=8 F=9 D=10 UI=11 I=12 D3D11=13 D3D12=14 void*=15   Reset=16
```

- slot 8: the reverse map calls `Get(void**)`; the real slot is `Get(ULL*)`.
  It goes through because OptiScaler converts `void*↔ULL` in both directions — the
  **bridge guarantees the answer even when a slot refuses strictly**;
- slots 11/12: `int` ↔ `unsigned` — cross-type read that the provider refuses
  (`DLSS.Feature.Create.Flags`, `Width/Height/OutWidth/OutHeight`);
- slot 14: the reverse map calls `GetF`; the real slot is `Get(ID3D12Resource**)`
  — **known limitation** (section 6).

## 4. The `Vulkan | FSR 2.2.1 | Input: DLSS`

It's not a contradiction nor secret DX12:

- `Input: DLSS` = OptiScaler recorded that the **NGX input bridge** was
  used (the game spoke the DLSS-NGX API; OptiScaler is what serves it). It does not indicate
  the active upscaler;
- `FSR 2.2.1`: on the Vulkan path, OptiScaler tries XeSS-SR **exactly**
  via a `libxess.dll` containing the `xessVK*` exports (a single file;
  `XeSS_Proxy.h:212-250`, `DllNames.h`). If the file isn't there/doesn't export
  `xessVK*`, it silently falls back to the embedded FSR 2.2.1
  (`NVNGX_DLSS_Vk.cpp:722-741` + provider fallback). The correct key is
  `[Upscalers] VulkanUpscaler=xess` — in `[General]` it is **ignored**
  (the sample `OptiScaler.ini` has no `[General]` section at all).
- For real XeSS-SR on the B390: place a `libxess.dll` **from your own
  Intel system/package** (with `xessVK*` exports) in the game folder. We do not
  redistribute that file (Intel license). XeFG's FG is independent of this —
  with FSR 2.2.1 as SR, the XeFG 2x can turn on normally (it's the
  "FSR-SR + XeFG" mode accepted in the briefing as an intermediate).

## 5. What the bridge (zz_ngxbridge.asi) does — and what it does NOT do

Three mechanisms, each on the single function that didn't yet have an owner in the stack:

1. **Provider path spoof (Spoof=1)** — hook of
   `GetModuleFileNameW/A` (kernel32 + kernelbase + the bridge's own IAT).
   It only answers differently when (a) the handle being asked is OptiScaler's **and**
   (b) the asking code belongs to a module whose path contains one of the
   `CallerHints` (default `rdr2vulkanfg`). It answers
   `<game folder>\nvngx.dll` — a **fictitious path, no file is
   loaded or needs to exist**. It's just enough for the
   RDR2VulkanFG gate to accept; nothing else changes in the game's or
   OptiScaler's behavior. Questions from any other module pass untouched (counter +
   log).
2. **Typed vtable mirror (Mirror=1)** — hook of OptiScaler's **6 parameter-factory
   exporters** (`NVSDK_NGX_{VULKAN,D3D12,D3D11}_
   {AllocateParameters,GetParameters}`; all 6 names verified in the 0.9.4
   source code). When the game creates the object, the bridge patches the 16 slots of
   its vtable (VirtualProtect + writes stubs). Each stub: (a) calls the provider's
   original slot; (b) if the provider refuses (or returns Success with
   a null pointer in a pointer slot), applies a **safe typed fallback**:
   `void*↔ULL`, `int↔unsigned`, `ULL→int` with range check, `int→float`
   only if finite/integral. It **never** fabricates a pointer from a float,
   never invents a value, never answers with a stale frame: if no valid value
   from the current frame exists, it returns the same Fail the provider would.
3. **Observer (log) of name resolution** — hook of
   `ntdll!LdrGetProcedureAddress`, which **nobody else hooks** in the stack
   (OptiScaler hooks LdrLoadDll/NtLoadDll/GetProcAddress; Mor3a hooks
   GetProcAddress + the NGX exports; OptiPatcher hooks nothing). It records
   which module asked which NGX name from the OptiScaler handle — on the
   real machine it answers the last open hypothesis (section 6).

**Does not do:** `GetProcAddress` hook (it already belongs to OptiScaler **and** Mor3a;
a third engine at the same address — measured — silences both), `LdrLoadDll`
hook (not needed), an `nvngx.dll` file on disk (not needed nor
desirable), patching RDR2.exe/DRM (forbidden and unnecessary), altering
OptiScaler/Mor3a/OptiPatcher (not a single byte touched). All hooks use a
single static MinHook; the bridge exposes no surface for others to hook.

## 6. What is proven here, what only the B390 proves

**Proven in this environment (Fedora 44 + MinGW + Wine 11.20, no RDR2):**
the full chain of events with faithful mocks (see `TEST-RESULTS.md`): spoof
delivered → gate accepts → wrapper installed → observer sees real `Depth`/
`MotionVectors` in the `EvaluateFeature` of both frames → cross-type read
saved by the mirror (`MIRROR-CROSS-TYPE-OK`). Unit tests of the
coercion logic: green. Real Windows x64 compilation of the `.asi`.

**To prove on the B390 (no honest shortcut):**
1. *Game resolution path:* Mor3a's gate only exists if the game's
   `GetProcAddress` resolution goes through its hook. The observer records
   the stack of each resolution (`resolved '...' by <module>` = the first non-system
   module on the stack of whoever asked for the symbol — hook trampolines and
   kernel32/kernelbase forwarders are skipped). Reading:
   `by RDR2VulkanFG.asi` = the resolution went through Mor3a's global hook → the
   gate sees the event and the spoof fires (best case, it's what the test chain
   demonstrates); `by RDR2.exe` = the game resolved bypassing Mor3a's
   hook → the gate would never see the three names and the fix would need another
   injection point (the log proves which and where). Until this line exists in the real
   log, **we do not claim
   "it works in RDR2"**, only "the root cause was eliminated at the proven
   point".
2. *Jitter:* Mor3a's reverse `GetF` (slot 14 = real `Get(D3D12**)`) cannot
   read the `Jitter.Offset.X/Y` floats stored by the game in the real float
   slot — the mirror **refuses** to answer a float as a pointer
   (it would be a legitimate value delivered in a wrong type; risky for
   any other reader of the slot). Expected consequence: FG turns on with
   correct depth+MV+flags, but `jitter=(0.0000,0.0000)` in Mor3a's log
   (reprojection without jitter compensation — quality may fall short of
   ideal). Safe and small follow-up: allow this coercion **only** for
   the two known jitter keys, with an ini flag. Documented, not
   implemented (the minimum needed to get FG turning on first).
3. Final performance/stability validation on the B390 (out of scope for this
   development environment, by construction).

## 7. Success criterion on the real machine

In order, in the logs:

1. `RDR2VulkanFG\ngxbridge.log`: `spoofed provider path for capture module: ...nvngx.dll`;
2. `RDR2VulkanFG\RDR2VulkanFG.log`: `NGX hooks: module=... commit=0 ...`
   (the probe accepted; before it never appeared);
3. `FG input pipeline v2: accepted SR handle=... flags=0x... render=... output=...`;
4. `NGX inputs first valid: depth ... mvec ... jitter ... mvScale ...`;
5. telemetry: `input=1 ... enabled=1`; `wd: evals>0`; FG-109 disappears; 2x
   real frames in the XeFG OSD.
