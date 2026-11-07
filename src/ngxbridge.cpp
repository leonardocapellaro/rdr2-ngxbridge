// zz_ngxbridge.asi - compatibility bridge between OptiScaler 0.9.4's NGX
// serving path and Mor3a's rdr2-vulkan-frame-generation input capture.
//
// Root cause this addresses (see DIAGNOSIS.md): RDR2 loads "nvngx.dll";
// OptiScaler's EnableDlssInputs intercepts that name and hands the game *its
// own* module handle; the game resolves the real NVSDK_NGX_VULKAN_* functions
// from OptiScaler.asi. Mor3a installs its input-capture wrapper only when the
// module a GetProcAddress came from is named exactly nvngx.dll/_nvngx.dll, so
// its wrapper is never installed and XeFG never receives depth/motion vectors.
//
// Fix, least-invasive form:
//  * Hook kernel32/kernelbase!GetModuleFileNameW/A. ONLY a GetModuleFileNameW
//    call whose caller is inside Mor3a's ASI and whose module argument is
//    OptiScaler's handle receives a spoofed path "...\nvngx.dll". Every other
//    caller in the process (the game, OptiScaler itself, any other mod) is
//    untouched. This is what makes Mor3a's nvngx-whitelist gate
//    (inputs.cpp h_EarlyNgxGetProc / NgxProbeTick) accept OptiScaler's handle
//    and install its h_NgxEvaluate input capture on the real exports.
//  * NO GetProcAddress hook. That entry point has two owners already -
//    OptiScaler (hooks/Kernel_Hooks.cpp:111-146, :257-275) and Mor3a
//    (inputs.cpp:675) - and a third engine patching it is a recursive
//    double-hook (it silently broke the first self-test). Observation of
//    name resolution lives on ntdll!LdrGetProcedureAddress instead (nobody
//    hooks it), purely as logging for the §9 questions.
//  * The vtable mirror is installed by hooking OptiScaler's OWN exported
//    NVSDK_NGX_{VULKAN,D3D12,D3D11}_{AllocateParameters,GetParameters}
//    bodies directly (addresses resolved once from the module handle).
//    Nobody hooks those two exports (Mor3a only Detours Create/Create1/
//    Evaluate after its gate accepts), so there is no engine collision, and
//    this works no matter which resolver path the game used to bind them -
//    kernel32, kernelbase, or a private in-engine loader call.
//  * The mirror records every typed Set on the parameter objects OptiScaler
//    creates and answers typed Get calls (only when OptiScaler's own storage
//    has no answer) with a safely coerced value, so Mor3a's fixed vtable-slot
//    reads find depth/MV regardless of how the game's loader stored them. The
//    Reset() slot is patched too: a provider Reset drops the mirror's memory,
//    so no pre-reset value can ever answer as if it were the current frame.
// No file named nvngx.dll is created, no forwarder DLL, no LdrLoadDll hook.
// Mor3a's and OptiScaler's binaries are untouched.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>
#include "MinHook.h"
#include "mirror_core.h"

#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#define NGXBRIDGE_VERSION "0.1.1"
#define NGX_SUCCESS 0x00000001

// ---------------------------------------------------------------- logging ---
static CRITICAL_SECTION g_logCs;
static FILE* g_logFile = nullptr;
static bool g_logFirstFailLogged = false;

static void LogOpen()
{
    InitializeCriticalSection(&g_logCs);
    wchar_t exe[MAX_PATH]{};
    GetModuleFileNameW(NULL, exe, MAX_PATH);
    std::wstring dir = exe;
    size_t sep = dir.find_last_of(L"\\/");
    if (sep != std::wstring::npos) dir = dir.substr(0, sep);
    std::wstring sub = dir + L"\\RDR2VulkanFG";
    CreateDirectoryW(sub.c_str(), NULL);
    std::wstring path = sub + L"\\ngxbridge.log";
    g_logFile = _wfopen(path.c_str(), L"a");
    if (!g_logFile) g_logFile = _wfopen((dir + L"\\ngxbridge.log").c_str(), L"a");
}

static void LogLine(const char* fmt, ...)
{
    if (!g_logFile) {
        if (!g_logFirstFailLogged) g_logFirstFailLogged = true;
        return; // log-open failure must never affect the game
    }
    EnterCriticalSection(&g_logCs);
    SYSTEMTIME st; GetLocalTime(&st);
    fprintf(g_logFile, "[%02d:%02d:%02d.%03d] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    va_list ap; va_start(ap, fmt);
    vfprintf(g_logFile, fmt, ap);
    va_end(ap);
    fputc('\n', g_logFile);
    fflush(g_logFile);
    LeaveCriticalSection(&g_logCs);
}

// Throttled logging helper: first 3 occurrences + every 600th.
#define LOGT(ctr, ...) do { if (++(ctr) <= 3 || (ctr) % 600 == 1) LogLine(__VA_ARGS__); } while (0)

// Lock-free, allocation-free log queue for code that may run while the LOADER
// LOCK is held (GetModuleFileNameW is called by the loader itself, and
// ntdll!LdrGetProcedureAddress runs under that lock on every DLL load).
// fprintf/fflush allocate from the heap there: a concurrent DLL load holding
// the heap and waiting for the loader lock inverts with us waiting for the
// heap under the loader lock - the classic startup deadlock. Producers here
// only touch preallocated static buffers and vsnprintf (no heap), so nothing
// under the loader lock can block on us; a normal thread drains to the file.
struct LogRec { char msg[256]; };
static LogRec g_ring[128];
static volatile LONG g_ringW = 0;   // total produced
static LONG g_ringT = 0;            // total consumed (flusher thread only)
static void LogQueued(const char* fmt, ...)
{
    LONG w = InterlockedIncrement(&g_ringW);
    LogRec* rec = &g_ring[(w - 1) & 127];
    va_list ap; va_start(ap, fmt);
    vsnprintf(rec->msg, sizeof rec->msg, fmt, ap);
    va_end(ap);
}
#define LOGQT(ctr, ...) do { if (++(ctr) <= 3 || (ctr) % 600 == 1) LogQueued(__VA_ARGS__); } while (0)

static void DrainRing()
{
    LONG w = g_ringW;                 // snapshot; producers only ever append
    if (w - g_ringT > 128) g_ringT = w - 128; // queue wrapped: drop stale slots, count in message below
    while (g_ringT < w) {
        char local[256];
        const char* src = g_ring[g_ringT & 127].msg;
        memcpy(local, src, 255); local[255] = 0; // copy out: producer may reuse the slot
        ++g_ringT;
        LogLine("%s", local);
    }
}

// ------------------------------------------------------------------ config ---
struct Cfg {
    bool enabled = true, spoof = true, mirror = true;
    std::wstring target = L"OptiScaler.asi";
    std::vector<std::string> hints;   // lowercased ascii substrings of caller module path
};
static Cfg g_cfg;

static void WideToLowerA(const wchar_t* w, char* out, size_t cap)
{
    size_t j = 0;
    for (size_t i = 0; w[i] && j + 1 < cap; ++i)
        out[j++] = (w[i] < 128) ? char(w[i]) : '?';
    out[j] = 0;
    for (char* s = out; *s; ++s) if (*s >= 'A' && *s <= 'Z') *s = char(*s + 32);
}

static void LoadConfig(HMODULE self)
{
    wchar_t mod[MAX_PATH]{};
    GetModuleFileNameW(self, mod, MAX_PATH);
    std::wstring dir = mod;
    size_t sep = dir.find_last_of(L"\\/");
    if (sep != std::wstring::npos) dir = dir.substr(0, sep);
    std::wstring ini = dir + L"\\zz_ngxbridge.ini";

    g_cfg.enabled = GetPrivateProfileIntW(L"bridge", L"Enabled", 1, ini.c_str()) != 0;
    g_cfg.spoof   = GetPrivateProfileIntW(L"bridge", L"Spoof",    1, ini.c_str()) != 0;
    g_cfg.mirror  = GetPrivateProfileIntW(L"bridge", L"Mirror",   1, ini.c_str()) != 0;

    wchar_t buf[512]{};
    GetPrivateProfileStringW(L"bridge", L"Target", L"OptiScaler.asi", buf, 512, ini.c_str());
    g_cfg.target = buf;
    GetPrivateProfileStringW(L"bridge", L"CallerHints", L"rdr2vulkanfg", buf, 512, ini.c_str());
    std::wstring list = buf;
    size_t pos = 0;
    while (pos < list.size()) {
        size_t comma = list.find(L',', pos);
        if (comma == std::wstring::npos) comma = list.size();
        std::wstring h = list.substr(pos, comma - pos);
        pos = comma + 1;
        char a[256];
        WideToLowerA(h.c_str(), a, sizeof a);
        size_t b = 0, e = strlen(a);
        while (b < e && isspace((unsigned char)a[b])) ++b;
        while (e > b && isspace((unsigned char)a[e - 1])) --e;
        if (e > b) { a[e] = 0; g_cfg.hints.emplace_back(a + b); }
    }
}

// ------------------------------------------------- module/name utilities ---
static HMODULE g_opti = nullptr;         // OptiScaler's real module handle
static wchar_t g_optiPath[MAX_PATH]{};
static wchar_t g_fakePath[MAX_PATH]{};   // "...\nvngx.dll"

typedef DWORD (WINAPI* GPFNWTP)(HMODULE, LPWSTR, DWORD);
typedef DWORD (WINAPI* GPFNATP)(HMODULE, LPSTR, DWORD);
// originals (captured before any hook is installed); [0]=kernel32 [1]=kernelbase [2]=self-IAT binding
static GPFNWTP pGPFNWOrig[3]{};
static GPFNATP pGPFNAOrig[3]{};

typedef HMODULE (WINAPI* GetModuleHandleExTP)(DWORD, LPCWSTR, HMODULE*);
static GetModuleHandleExTP pGetModuleHandleExW = nullptr;

static bool CallerMatchesHints(void* ra)
{
    if (g_cfg.hints.empty()) return false;
    HMODULE m = nullptr;
    if (!pGetModuleHandleExW ||
        !pGetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                             (LPCWSTR)ra, &m) || !m)
        return false;
    wchar_t path[MAX_PATH]{};
    if ((pGPFNWOrig[1] ? pGPFNWOrig[1] : pGPFNWOrig[0])(m, path, MAX_PATH) == 0) return false;
    char lower[3 * MAX_PATH];
    WideToLowerA(path, lower, sizeof lower);
    for (const auto& h : g_cfg.hints)
        if (strstr(lower, h.c_str())) return true;
    return false;
}

static void ModuleNameOf(HMODULE m, wchar_t* out, size_t cap)
{
    out[0] = 0;
    if (!m) return;
    wchar_t path[MAX_PATH]{};
    if ((pGPFNWOrig[1] ? pGPFNWOrig[1] : pGPFNWOrig[0])(m, path, MAX_PATH) == 0) return;
    const wchar_t* base = wcsrchr(path, L'\\');
    base = base ? base + 1 : path;
    wcsncpy(out, base, cap - 1);
    out[cap - 1] = 0;
}

static void ModuleBaseOf(void* addr, wchar_t* out, size_t cap)
{
    out[0] = 0;
    if (!pGetModuleHandleExW) return;
    HMODULE m = nullptr;
    if (!pGetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                             (LPCWSTR)addr, &m) || !m) return;
    ModuleNameOf(m, out, cap);
}

// Who really asked for a symbol? Inside ntdll!LdrGetProcedureAddress the
// plain return address is ALWAYS kernel32/kernelbase's own GetProcAddress
// implementation (the API always funnels through it), never the caller that
// mattered - the shipped 0.1.0 observer therefore always logged the wrong
// module. Walk the stack instead: skip system modules (ntdll/kernel32/
// kernelbase) and frames that belong to no module (hook trampolines, which
// are heap allocations - e.g. Mor3a's Detours), and report the first frame
// that does. Trampolines being skipped is informative too: an attribution of
// RDR2VulkanFG.asi means the resolution came through Mor3a's own hook.
typedef WORD (WINAPI* RtlCSBTP)(ULONG skip, ULONG capture, PVOID* frames, PULONG hash);
static RtlCSBTP pRtlCSBT = nullptr;
static HMODULE g_ntdll = nullptr, g_k32 = nullptr, g_kb = nullptr, g_self = nullptr;

static void FirstExternalCaller(wchar_t* out, size_t cap)
{
    out[0] = 0;
    // Deliberately no __builtin_return_address(1) fallback: reading frame 1
    // without unwind guarantees can dereference garbage. Without the walker
    // we simply omit attribution (logged as "?").
    if (!pRtlCSBT || !pGetModuleHandleExW) return;
    PVOID frames[24];
    // skip=1: drop this function's own frame; the loop below still skips our
    // hook body (g_self), system forwarders and hook trampolines.
    WORD n = pRtlCSBT(1, 24, frames, nullptr);
    for (WORD i = 0; i < n; ++i) {
        HMODULE m = nullptr;
        if (!pGetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                 (LPCWSTR)frames[i], &m) || !m)
            continue; // trampoline / anonymous executable region
        if (m == g_ntdll || m == g_k32 || m == g_kb || m == g_self) continue;
        ModuleNameOf(m, out, cap);
        return;
    }
}

// loader-level resolver signature (ntdll!LdrGetProcedureAddress). The bridge
// hooks this instead of GetProcAddress (see header) - one owner per function.
typedef struct { unsigned short Length; unsigned short MaximumLength; char* Buffer; } MyANSI;
typedef LONG (NTAPI* LdrGetProcTP)(PVOID, MyANSI*, ULONG, PVOID*);
static LdrGetProcTP pLdrOrig = nullptr;  // real resolver, captured by MH_CreateHook

// ------------------------------------------------- parameter vtable mirror ---
static MirrorCore g_core;
static CRITICAL_SECTION g_mirrorCs;
struct SavedVt { void* orig[SLOTS_TOTAL]; };
static std::vector<std::pair<void*, SavedVt>> g_vtables; // patched vtables

static void* OrigSlot(void* self, int slot)
{
    if (!self) return nullptr;
    void* vt = *(void**)self;
    void* r = nullptr;
    EnterCriticalSection(&g_mirrorCs);
    for (auto& kv : g_vtables)
        if (kv.first == vt) { r = kv.second.orig[slot]; break; }
    LeaveCriticalSection(&g_mirrorCs);
    return r;
}

typedef int (*SlotSetP)(void*, const char*, void*);
typedef int (*SlotSetI)(void*, const char*, int);
typedef int (*SlotSetUI)(void*, const char*, unsigned);
typedef int (*SlotSetD)(void*, const char*, double);
typedef int (*SlotSetF)(void*, const char*, float);
typedef int (*SlotSetULL)(void*, const char*, unsigned long long);
typedef int (*SlotGetP)(void*, const char*, void**);
typedef int (*SlotGetI)(void*, const char*, int*);
typedef int (*SlotGetUI)(void*, const char*, unsigned*);
typedef int (*SlotGetD)(void*, const char*, double*);
typedef int (*SlotGetF)(void*, const char*, float*);
typedef int (*SlotGetULL)(void*, const char*, unsigned long long*);

static unsigned g_coerced = 0, g_coerceMiss = 0;

static uint64_t CoercePtr(void* p) { return (uint64_t)(uintptr_t)p; }

// Slot 16 = Reset(). Patched on purpose: after a provider Reset the object's
// storage is empty, and the mirror's copy of the old values MUST be dropped
// with it - answering a later Get with a pre-reset (possibly already freed)
// Vulkan resource would violate the "never use stale inputs" rule.
typedef int (*SlotResetTP)(void*);
extern "C" int StubReset(void* self) {
    auto f = (SlotResetTP)OrigSlot(self, SLOT_RESET);
    int r = f ? f(self) : 0;
    if (r == NGX_SUCCESS) g_core.ForgetObject(self);
    return r;
}

// Stubs are keyed by the REAL provider layout (see mirror_core.h). Each Set
// stub records what it wrote into the typed side-table, then runs the
// provider's own slot untouched. Each Get stub runs the provider first; the
// typed fallback answers only when the provider has no answer - and for the
// pointer slots, also when the provider answers Success with NULL, which
// OptiScaler's strictly-typed storage returns for a key stored under another
// type (verified in v0.9.4 NVNGX_Parameter.h getT/operator).
extern "C" {
int StubSetULL(void* self, const char* k, unsigned long long v) {
    auto f = (SlotSetULL)OrigSlot(self, SLOT_SET_ULL);
    g_core.NoteSet(self, k, ValueKind::ULL, v);
    return f ? f(self, k, v) : 0;
}
int StubSetF(void* self, const char* k, float v) {
    auto f = (SlotSetF)OrigSlot(self, SLOT_SET_F);
    uint32_t b; memcpy(&b, &v, 4);
    g_core.NoteSet(self, k, ValueKind::F, b);
    return f ? f(self, k, v) : 0;
}
int StubSetD(void* self, const char* k, double v) {
    auto f = (SlotSetD)OrigSlot(self, SLOT_SET_D);
    uint64_t b; memcpy(&b, &v, 8);
    g_core.NoteSet(self, k, ValueKind::D, b);
    return f ? f(self, k, v) : 0;
}
int StubSetUI(void* self, const char* k, unsigned v) {
    auto f = (SlotSetUI)OrigSlot(self, SLOT_SET_UI);
    g_core.NoteSet(self, k, ValueKind::UI, v);
    return f ? f(self, k, v) : 0;
}
int StubSetI(void* self, const char* k, int v) {
    auto f = (SlotSetI)OrigSlot(self, SLOT_SET_I);
    g_core.NoteSet(self, k, ValueKind::I, uint64_t(uint32_t(v)));
    return f ? f(self, k, v) : 0;
}
int StubSetD3D11(void* self, const char* k, void* v) {
    auto f = (SlotSetP)OrigSlot(self, SLOT_SET_D3D11);
    g_core.NoteSet(self, k, ValueKind::Ptr, CoercePtr(v));
    return f ? f(self, k, v) : 0;
}
int StubSetD3D12(void* self, const char* k, void* v) {
    auto f = (SlotSetP)OrigSlot(self, SLOT_SET_D3D12);
    g_core.NoteSet(self, k, ValueKind::Ptr, CoercePtr(v));
    return f ? f(self, k, v) : 0;
}
int StubSetVoid(void* self, const char* k, void* v) {
    auto f = (SlotSetP)OrigSlot(self, SLOT_SET_VOIDPTR);
    g_core.NoteSet(self, k, ValueKind::Ptr, CoercePtr(v));
    return f ? f(self, k, v) : 0;
}

static bool Fallback(void* self, const char* k, ValueKind want, uint64_t* out)
{
    if (!g_core.NoteGet(self, k, want, out)) { LOGT(g_coerceMiss, "mirror: no fallback for '%s'", k ? k : "?"); return false; }
    LOGT(g_coerced, "mirror: coerced typed answer for '%s'", k ? k : "?");
    return true;
}
int StubGetULL(void* self, const char* k, unsigned long long* out) {   // slot 8 - Mor3a reads pointer keys here
    auto f = (SlotGetULL)OrigSlot(self, SLOT_GET_ULL);
    int r = f ? f(self, k, out) : 0;
    uint64_t v;
    if (r == NGX_SUCCESS && out && *out) return r;                       // real answer
    if (Fallback(self, k, ValueKind::ULL, &v)) { if (out) *out = v; return NGX_SUCCESS; }
    return r;
}
int StubGetF(void* self, const char* k, float* out) {
    auto f = (SlotGetF)OrigSlot(self, SLOT_GET_F);
    int r = f ? f(self, k, out) : 0;
    if (r == NGX_SUCCESS) return r;
    uint64_t v; if (Fallback(self, k, ValueKind::F, &v)) { uint32_t b = uint32_t(v); float f; memcpy(&f, &b, 4); if (out) *out = f; return NGX_SUCCESS; }
    return r;
}
int StubGetD(void* self, const char* k, double* out) {
    auto f = (SlotGetD)OrigSlot(self, SLOT_GET_D);
    int r = f ? f(self, k, out) : 0;
    if (r == NGX_SUCCESS) return r;
    uint64_t v; if (Fallback(self, k, ValueKind::D, &v)) { double d; memcpy(&d, &v, 8); if (out) *out = d; return NGX_SUCCESS; }
    return r;
}
int StubGetUI(void* self, const char* k, unsigned* out) {
    auto f = (SlotGetUI)OrigSlot(self, SLOT_GET_UI);
    int r = f ? f(self, k, out) : 0;
    if (r == NGX_SUCCESS) return r;
    uint64_t v; if (Fallback(self, k, ValueKind::UI, &v)) { if (out) *out = unsigned(v); return NGX_SUCCESS; }
    return r;
}
int StubGetI(void* self, const char* k, int* out) {
    auto f = (SlotGetI)OrigSlot(self, SLOT_GET_I);
    int r = f ? f(self, k, out) : 0;
    if (r == NGX_SUCCESS) return r;
    uint64_t v; if (Fallback(self, k, ValueKind::I, &v)) { if (out) *out = int(uint32_t(v)); return NGX_SUCCESS; }
    return r;
}
static int StubGetPtrAny(void* self, const char* k, int slot, void** out) {
    auto f = (SlotGetP)OrigSlot(self, slot);
    int r = f ? f(self, k, out) : 0;
    if (r == NGX_SUCCESS && out && *out) return r;                        // provider has a pointer
    uint64_t v;                                                            // success+null (strict-type miss) or fail
    if (Fallback(self, k, ValueKind::Ptr, &v)) { if (out) *out = (void*)(uintptr_t)v; return NGX_SUCCESS; }
    return r;
}
int StubGetD3D11(void* self, const char* k, void** out) { return StubGetPtrAny(self, k, SLOT_GET_D3D11, out); }
int StubGetD3D12(void* self, const char* k, void** out) { return StubGetPtrAny(self, k, SLOT_GET_D3D12, out); }
int StubGetVoid(void* self, const char* k, void** out) { return StubGetPtrAny(self, k, SLOT_GET_VOIDPTR, out); }
} // extern C

static void MirrorEnsureObject(void* obj, bool isNew)
{
    if (!g_cfg.mirror || !obj) return;
    void* vt = *(void**)obj;
    if (!vt) return;
    // The found-check, the original-slot capture, the in-place vtable write
    // and the registry push MUST be one atomic section: two threads creating
    // the first objects of the same parameter class concurrently could
    // otherwise double-patch and record our own stub pointers as the
    // "original" slots - a self-call loop (stack overflow) or a silently
    // dropped provider call. With the lock held across all of it, any stub
    // that fires mid-patch from another thread simply blocks in OrigSlot
    // until the registry is complete.
    EnterCriticalSection(&g_mirrorCs);
    bool found = false;
    for (auto& kv : g_vtables) if (kv.first == vt) { found = true; break; }
    if (found) {
        LeaveCriticalSection(&g_mirrorCs);
        if (isNew) g_core.ForgetObject(obj);
        return;
    }
    DWORD old;
    if (!VirtualProtect(vt, SLOTS_TOTAL * sizeof(void*), PAGE_READWRITE, &old)) {
        LeaveCriticalSection(&g_mirrorCs);
        LogLine("mirror: VirtualProtect on vtable %p failed (%lu) - mirror disabled", vt, GetLastError());
        return;
    }
    void** v = (void**)vt;
    SavedVt saved;
    for (int i = 0; i < SLOTS_TOTAL; ++i) saved.orig[i] = v[i];
    v[SLOT_SET_VOIDPTR] = (void*)&StubSetVoid;   v[SLOT_SET_D3D12] = (void*)&StubSetD3D12;
    v[SLOT_SET_D3D11]   = (void*)&StubSetD3D11;  v[SLOT_SET_I]     = (void*)&StubSetI;
    v[SLOT_SET_UI]      = (void*)&StubSetUI;     v[SLOT_SET_D]     = (void*)&StubSetD;
    v[SLOT_SET_F]       = (void*)&StubSetF;      v[SLOT_SET_ULL]   = (void*)&StubSetULL;
    v[SLOT_GET_VOIDPTR] = (void*)&StubGetVoid;   v[SLOT_GET_D3D12] = (void*)&StubGetD3D12;
    v[SLOT_GET_D3D11]   = (void*)&StubGetD3D11;  v[SLOT_GET_I]     = (void*)&StubGetI;
    v[SLOT_GET_UI]      = (void*)&StubGetUI;     v[SLOT_GET_D]     = (void*)&StubGetD;
    v[SLOT_GET_F]       = (void*)&StubGetF;      v[SLOT_GET_ULL]   = (void*)&StubGetULL;
    v[SLOT_RESET]       = (void*)&StubReset;
    VirtualProtect(vt, SLOTS_TOTAL * sizeof(void*), old, &old);
    g_vtables.push_back({ vt, saved });
    LeaveCriticalSection(&g_mirrorCs);
    if (isNew) g_core.ForgetObject(obj);
    LogLine("mirror: vtable %p patched (%d typed slots + Reset) - cross-type reads now answered, stale-after-reset guarded",
            vt, SLOT_COUNT);
}

// ------------------------------------------------ GetProcAddress observer ---
enum NameIdx {
    N_EVAL_VK = 0, N_CREATE_VK, N_CREATE1_VK,           // the three Mor3a wraps
    N_ALLOC_VK, N_GETP_VK,                              // parameter-object sources (Vulkan)
    N_ALLOC_D3D12, N_GETP_D3D12, N_ALLOC_D3D11, N_GETP_D3D11,
    N_COUNT
};
static const char* kNames[N_COUNT] = {
    "NVSDK_NGX_VULKAN_EvaluateFeature", "NVSDK_NGX_VULKAN_CreateFeature", "NVSDK_NGX_VULKAN_CreateFeature1",
    "NVSDK_NGX_VULKAN_AllocateParameters", "NVSDK_NGX_VULKAN_GetParameters",
    "NVSDK_NGX_D3D12_AllocateParameters", "NVSDK_NGX_D3D12_GetParameters",
    "NVSDK_NGX_D3D11_AllocateParameters", "NVSDK_NGX_D3D11_GetParameters",
};
static unsigned g_seen[N_COUNT]{};
static unsigned g_spoofs = 0;

// In-place hooks on OptiScaler's exported parameter-object factories.
// Installed once at startup on the addresses resolved from the module handle,
// so the mirror installs regardless of which resolver path bound the export.
// Only these two kinds are touched - Mor3a's own Detours own Create/Create1/
// Evaluate when its gate accepts; the allocation exports are otherwise
// untouched by anyone, so no two hook engines ever patch the same entry.
typedef int (*AllocTP)(void**);
static AllocTP pAllocOrig[N_COUNT]{};
static unsigned g_allocs[N_COUNT]{};

template <int IDX, bool NEWOBJ> static int HookParamFactory(void** out)
{
    AllocTP f = pAllocOrig[IDX];
    int r = f ? f(out) : 0;
    if (++g_allocs[IDX] <= 3 || g_allocs[IDX] % 600 == 1) {
        wchar_t cbase[64]{};
        FirstExternalCaller(cbase, 64);   // game-thread context: direct log is fine
        LogLine("intercepted %s (result=%d obj=%p) by %ls",
                kNames[IDX], r, out ? *out : nullptr, cbase[0] ? cbase : L"?");
    }
    if (r == NGX_SUCCESS && out && *out) MirrorEnsureObject(*out, NEWOBJ);
    return r;
}

static int NameIndex(const char* name)
{
    for (int i = 0; i < N_COUNT; ++i)
        if (strcmp(name, kNames[i]) == 0) return i;
    return -1;
}

// Observer on the loader's own resolver. Every GetProcAddress flavor - the
// game's (kernel32 or kernelbase, whichever its IAT points at), and Mor3a's
// g_realGetProc detour trampoline - funnels through ntdll!LdrGetProcedureAddress,
// so this single hook sees every resolution of the OptiScaler handle without
// touching GetProcAddress (which Mor3a owns via Detours - double-hooking it
// from a second engine would be the recursive hook §6 forbids, and is exactly
// what silently broke the first self-test).
static LONG NTAPI Hook_LdrGPA(PVOID baseDll, MyANSI* name, ULONG ordinal, PVOID* proc)
{
    LONG st = pLdrOrig ? pLdrOrig(baseDll, name, ordinal, proc) : (LONG)0xC0000139;
    if (st != 0 || baseDll != (PVOID)g_opti || ordinal || !name || !name->Buffer || !proc || !*proc) return st;
    int idx = NameIndex(name->Buffer);
    if (idx < 0) return st;

    // queued, never logged here: we run under the loader lock (see LogQueued)
    if (++g_seen[idx] <= 3 || g_seen[idx] % 600 == 1) {
        wchar_t cbase[64]{};
        FirstExternalCaller(cbase, 64);
        LogQueued("resolved '%s' from the OptiScaler handle by %ls -> %p",
                  name->Buffer, cbase[0] ? cbase : L"?", *proc);
    }
    return st;
}

static bool SpoofEligible(HMODULE m, void* ra)
{
    return g_cfg.spoof && m == g_opti && g_fakePath[0] && CallerMatchesHints(ra);
}

static unsigned g_gpfCalls = 0;

static DWORD GPFNWImpl(HMODULE m, LPWSTR buf, DWORD size, GPFNWTP call, void* caller)
{
    // GPFN is called by the loader too (under the loader lock): every log
    // inside this hook must go through the queue, never straight to the file.
    LOGQT(g_gpfCalls, "GPFN[W] hook fired (m=%p)", (void*)m);
    if (SpoofEligible(m, caller)) {
        LOGQT(g_spoofs, "spoofed provider path for capture module: %ls", g_fakePath);
        DWORD need = (DWORD)wcslen(g_fakePath);
        if (size > need) { wcscpy(buf, g_fakePath); return need; }
        if (size) buf[0] = 0;
        return 0;
    }
    return call ? call(m, buf, size) : 0;
}
static DWORD WINAPI Hook_GPFNW_K32(HMODULE m, LPWSTR b, DWORD s) {
    return GPFNWImpl(m, b, s, pGPFNWOrig[0], __builtin_extract_return_addr(__builtin_return_address(0)));
}
static DWORD WINAPI Hook_GPFNW_KB(HMODULE m, LPWSTR b, DWORD s) {
    return GPFNWImpl(m, b, s, pGPFNWOrig[1], __builtin_extract_return_addr(__builtin_return_address(0)));
}

static DWORD GPFNAImpl(HMODULE m, LPSTR buf, DWORD size, GPFNATP call, void* caller)
{
    if (SpoofEligible(m, caller)) {
        LOGQT(g_spoofs, "spoofed provider path (A) for capture module: %ls", g_fakePath);
        int n = WideCharToMultiByte(CP_ACP, 0, g_fakePath, -1, buf, (int)size, NULL, NULL);
        return n > 0 ? DWORD(n - 1) : 0;
    }
    return call ? call(m, buf, size) : 0;
}
static DWORD WINAPI Hook_GPFNA_K32(HMODULE m, LPSTR b, DWORD s) {
    return GPFNAImpl(m, b, s, pGPFNAOrig[0], __builtin_extract_return_addr(__builtin_return_address(0)));
}
static DWORD WINAPI Hook_GPFNA_KB(HMODULE m, LPSTR b, DWORD s) {
    return GPFNAImpl(m, b, s, pGPFNAOrig[1], __builtin_extract_return_addr(__builtin_return_address(0)));
}

// Third pair on this module's own IAT binding of the same API. Name-resolved
// entries can be forwarder stubs that no import-based caller ever executes
// (the real code lives elsewhere); our own binding is guaranteed to be the
// address every importer calls. Usually duplicates one of the two above, in
// which case the create call is skipped.
static DWORD WINAPI Hook_GPFNW_SELF(HMODULE m, LPWSTR b, DWORD s) {
    return GPFNWImpl(m, b, s, pGPFNWOrig[2], __builtin_extract_return_addr(__builtin_return_address(0)));
}
static DWORD WINAPI Hook_GPFNA_SELF(HMODULE m, LPSTR b, DWORD s) {
    return GPFNAImpl(m, b, s, pGPFNAOrig[2], __builtin_extract_return_addr(__builtin_return_address(0)));
}

// ----------------------------------------------------------------- startup ---
static std::vector<void*> g_hooked; // targets this copy already owns
static bool alreadyHooked(void* a)
{
    for (void* x : g_hooked) if (x == a) return true;
    return false;
}

// Mandatory slot = kernel32. Mor3a links GetModuleFileNameW through kernel32;
// if only the kernelbase copy is hooked, kernel32's forwarder tail-jumps into
// our kernelbase hook with a return address INSIDE kernel32 (a system module,
// never matching CallerHints) -> the spoof silently never fires for Mor3a.
// So callers only trust success on kernel32; kernelbase is defense in depth.
static bool HookBoth(HMODULE k32, HMODULE kb, const char* fn, void* d32, void* dkb,
                     void** o32, void** okb, const char* label)
{
    bool ok32 = false, okkb = false;
    void *t32 = nullptr, *tkb = nullptr;
    if (k32) {
        t32 = (void*)GetProcAddress(k32, fn);
        if (t32) {
            MH_STATUS st = MH_CreateHook(t32, d32, o32);
            ok32 = (st == MH_OK);
            if (!ok32) LogLine("hook fail %s!%s kernel32 (%d)", label, fn, (int)st);
        }
    }
    if (kb) {
        tkb = (void*)GetProcAddress(kb, fn);
        if (tkb) {
            MH_STATUS st = MH_CreateHook(tkb, dkb, okb);
            okkb = (st == MH_OK);
            if (!okkb && st != MH_ERROR_ALREADY_CREATED && st != MH_ERROR_ENABLED)
                LogLine("hook fail %s!%s kernelbase (%d)", label, fn, (int)st);
        }
    }
    if (ok32) g_hooked.push_back(t32);
    if (okkb) g_hooked.push_back(tkb);
    LogLine("hook %s: kernel32=%p:%d kernelbase=%p:%d", fn, t32, (int)ok32, tkb, (int)okkb);
    return ok32;
}

static void InstallHooks()
{
    HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
    HMODULE kb  = GetModuleHandleW(L"kernelbase.dll");
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    g_k32 = k32; g_kb = kb; g_ntdll = ntdll;
    pGetModuleHandleExW = (GetModuleHandleExTP)GetProcAddress(k32, "GetModuleHandleExW");
    pRtlCSBT = (RtlCSBTP)GetProcAddress(ntdll, "RtlCaptureStackBackTrace");
    if (!pRtlCSBT) LogLine("note: RtlCaptureStackBackTrace unavailable - caller attribution degraded");

    MH_STATUS st = MH_Initialize();
    if (st != MH_OK) { LogLine("fatal: MH_Initialize=%d", (int)st); return; }
    bool anyW = HookBoth(k32, kb, "GetModuleFileNameW", (void*)&Hook_GPFNW_K32, (void*)&Hook_GPFNW_KB,
                         (void**)&pGPFNWOrig[0], (void**)&pGPFNWOrig[1], "spoof");
    HookBoth(k32, kb, "GetModuleFileNameA", (void*)&Hook_GPFNA_K32, (void*)&Hook_GPFNA_KB,
             (void**)&pGPFNAOrig[0], (void**)&pGPFNAOrig[1], "spoof");
    // Self-IAT bindings too (see Hook_GPFNW_SELF): name-resolved entries can be
    // forwarder stubs nobody imports; skip when they are the same address.
    if (void* sw = (void*)&GetModuleFileNameW; g_cfg.spoof && sw && !alreadyHooked(sw)) {
        st = MH_CreateHook(sw, (void*)&Hook_GPFNW_SELF, (void**)&pGPFNWOrig[2]);
        LogLine("hook spoof self-binding GetModuleFileNameW=%p -> %d", sw, (int)st);
    }
    if (void* sa = (void*)&GetModuleFileNameA; g_cfg.spoof && sa && !alreadyHooked(sa)) {
        st = MH_CreateHook(sa, (void*)&Hook_GPFNA_SELF, (void**)&pGPFNAOrig[2]);
        LogLine("hook spoof self-binding GetModuleFileNameA=%p -> %d", sa, (int)st);
    }
    // Single hook on the loader resolver (never GetProcAddress itself, which
    // Mor3a owns - see header).
    bool anyL = false;
    if (ntdll) {
        void* t = (void*)GetProcAddress(ntdll, "LdrGetProcedureAddress");
        if (t) {
            st = MH_CreateHook(t, (void*)&Hook_LdrGPA, (void**)&pLdrOrig);
            anyL = (st == MH_OK);
            LogLine("hook observer: ntdll!LdrGetProcedureAddress=%p -> %d", t, (int)st);
        }
    }
    // Direct in-place hooks on OptiScaler's parameter-factory exports (see
    // header): mirror installation independent of which resolver path the game
    // used to bind them. No other component hooks these two exports.
    struct FactoryHook { int idx; void* detour; };
    const FactoryHook factories[] = {
        { N_ALLOC_VK,    (void*)&HookParamFactory<N_ALLOC_VK, true>    },
        { N_GETP_VK,     (void*)&HookParamFactory<N_GETP_VK, false>    },
        { N_ALLOC_D3D12, (void*)&HookParamFactory<N_ALLOC_D3D12, true>  },
        { N_GETP_D3D12,  (void*)&HookParamFactory<N_GETP_D3D12, false>  },
        { N_ALLOC_D3D11, (void*)&HookParamFactory<N_ALLOC_D3D11, true>  },
        { N_GETP_D3D11,  (void*)&HookParamFactory<N_GETP_D3D11, false>  },
    };
    int hooked = 0;
    for (const auto& fh : factories) {
        void* t = (void*)GetProcAddress(g_opti, kNames[fh.idx]);
        if (!t) { LogLine("mirror: export %s not present in target", kNames[fh.idx]); continue; }
        st = MH_CreateHook(t, fh.detour, (void**)&pAllocOrig[fh.idx]);
        if (st == MH_OK) ++hooked;
        else LogLine("mirror: hook %s@%p failed (%d)", kNames[fh.idx], t, (int)st);
    }
    LogLine("mirror: %d/6 parameter-factory export hooks created", hooked);
    if (g_cfg.mirror && !hooked) LogLine("warning: mirror has no installation point - capture may see raw provider misses");

    // Only the SPOOF is load-bearing (it is the whole fix). The observer and
    // mirror are diagnostics/degradation-tolerant: their absence must never
    // switch the fix off, and the fix must never pretend to be armed.
    if (g_cfg.spoof && !anyW)
        LogLine("CRITICAL: kernel32!GetModuleFileNameW not hookable - the spoof CANNOT reach the capture module; "
                "check for a conflicting hook engine in another .asi (bridge stays armed for diagnostics only)");
    if (!anyL) LogLine("note: LdrGetProcedureAddress observer unavailable - resolution diagnostics off");
    if ((st = MH_EnableHook(MH_ALL_HOOKS)) != MH_OK) { LogLine("fatal: MH_EnableHook=%d", (int)st); return; }
    LogLine("armed: spoof=%d (kernel32 %s), observer=%d, factory hooks=%d/6",
            (int)(g_cfg.spoof && anyW), anyW ? "ok" : "MISSING", (int)anyL, hooked);
}

static void FindTarget()
{
    // Poll for as long as the process lives: a 30s deadline could permanently
    // disable the bridge on a heavily-modded boot (or if OptiScaler is
    // enabled later); one GetModuleHandleW per second costs nothing.
    bool warned = false;
    for (unsigned i = 0; !g_opti; ++i) {
        g_opti = GetModuleHandleW(g_cfg.target.c_str());
        if (g_opti) break;
        if (i == 30 && !warned) {
            warned = true;
            LogLine("waiting for target '%S' - still polling (check ini name / that OptiScaler is enabled)",
                    g_cfg.target.c_str());
        }
        DrainRing();
        Sleep(1000);
    }
    GetModuleFileNameW(g_opti, g_optiPath, MAX_PATH);
    LogLine("target found: %S at %p", g_optiPath, (void*)g_opti);
    std::wstring p = g_optiPath;
    size_t sep = p.find_last_of(L"\\/");
    if (sep != std::wstring::npos) p = p.substr(0, sep + 1);
    std::wstring fake = p + L"nvngx.dll";
    if (fake.size() + 1 > MAX_PATH) {
        // never overflow g_fakePath[MAX_PATH]; a too-long path simply means
        // no spoof, loudly logged - the game keeps working, upscale intact.
        g_fakePath[0] = 0;
        LogLine("warning: target directory too long for the spoof path (%zu) - SPOOF DISABLED", fake.size());
    } else {
        wcscpy(g_fakePath, fake.c_str());
    }
}

static DWORD WINAPI InitThread(LPVOID)
{
    FindTarget();
    InstallHooks();
    // log-queue flusher for messages produced under the loader lock
    for (;;) { DrainRing(); Sleep(500); }
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h);
        g_self = h;
        LogOpen();
        InitializeCriticalSection(&g_mirrorCs);
        LoadConfig(h);
        LogLine("== ngxbridge %s == enabled=%d spoof=%d mirror=%d target=%S hints=%u",
                NGXBRIDGE_VERSION, (int)g_cfg.enabled, (int)g_cfg.spoof, (int)g_cfg.mirror,
                g_cfg.target.c_str(), (unsigned)g_cfg.hints.size());
        if (!g_cfg.enabled) { LogLine("disabled by ini - doing nothing"); return TRUE; }
        // Real work off the loader lock. The ASI name sorts after Opti*/RDR2*,
        // and RDR2 loads nvngx.dll seconds after init, so this thread wins the
        // race in practice; FindTarget's retry covers other load orders.
        HANDLE t = CreateThread(NULL, 0, InitThread, NULL, 0, NULL);
        if (t) CloseHandle(t);
    }
    return TRUE;
}
