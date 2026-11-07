// Mock stand-in for Mor3a's rdr2-vulkan-frame-generation ASI (production
// binary is RDR2VulkanFG.asi). Self-test only - never installed next to the
// game. It replicates the decisive part of inputs.cpp h_EarlyNgxGetProc: when
// the game resolves one of the three NVSDK_NGX_VULKAN_{CreateFeature,
// CreateFeature1,EvaluateFeature} names, ask which file the provider module
// handle belongs to (this GetModuleFileNameW call is where the zz bridge
// intervenes) and only install the input-capture wrapper if the answer is
// nvngx.dll/_nvngx.dll. The wrapper's ReadFrameInputs replica then reads
// Depth/MotionVectors and the create flags through Mor3a's hardcoded vtable
// slots - exactly what feeds XeFG in production.
// Interception point: production Mor3a uses a process-wide Detours hook on
// GetProcAddress; the mock redirects the GAME's own GetProcAddress IAT slot
// instead (see redirectGameGPA). Same observed event, no shared entry point.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cwchar>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cstdint>

static void* origEval = nullptr;
static void* origCreate = nullptr;
static int evalsSeen = 0;
static int rejects = 0;

static void M3(const char* fmt, ...)
{
    char buf[512];
    va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof buf, fmt, ap); va_end(ap);
    printf("[MOR3A-MOCK] %s\n", buf);
    fflush(stdout);
}

typedef int (*GetP)(void*, const char*, void**);
typedef int (*GetITP)(void*, const char*, int*);

// Faithful replica of inputs.cpp h_NgxEvaluate / RegisterSrFeature reading the
// parameter object through Mor3a's HARDCODED (group-reversed) vtable map, on
// top of the provider's TRUE layout. This is why the mirror exists:
//   vt[8]  Mor3a calls GetVoidPointer; the real slot is Get(unsigned long long*).
//          It still finds Depth/MV because OptiScaler converts void*<->ULL
//          both directions, and the bridge mirror guarantees a non-null answer
//          even for a slot the provider strictly rejects.
//   vt[11] Mor3a calls GetI; the real slot is Get(unsigned*). A value set with
//          SetI is a STRICT MISMATCH there (provider answers Fail) - only the
//          bridge's typed fallback (I->UI) answers, proving the mirror is load-
//          bearing. If the mirror were off, this line reads result=0 value=0.
static int wrappedEval(void* cmd, const void* h, void* p, void* cb)
{
    void** vt = *(void***)p; // vptr first, then index the table
    GetP gp = (GetP)vt[8];   // Mor3a's "GetVoidPointer" slot
    void* depth = nullptr; void* mv = nullptr;
    int rd = gp ? gp(p, "Depth", &depth) : 0;
    int rm = gp ? gp(p, "MotionVectors", &mv) : 0;
    bool ok = (rd == 1 && depth != nullptr && rm == 1 && mv != nullptr);
    M3("OBSERVER depth(result=%d ptr=%p) motionvectors(result=%d ptr=%p) %s",
       rd, depth, rm, mv, ok ? "OBSERVER-OK" : "OBSERVER-MISSING");
    if (ok) ++evalsSeen;
    typedef int (*EvalTP)(void*, const void*, void*, void*);
    return ((EvalTP)origEval)(cmd, h, p, cb);
}

static int wrappedCreate(void* cmd, int feat, void* p, void** h)
{
    void** vt = *(void***)p;
    GetITP gi = (GetITP)vt[11]; // Mor3a's "GetI" slot (really Get unsigned*)
    int flags = 0; int r = gi ? gi(p, "DLSS.Feature.Create.Flags", &flags) : 0;
    M3("CreateFeature: SR flags result=%d value=%d %s",
       r, flags, (r == 1 && flags == 1) ? "MIRROR-CROSS-TYPE-OK" : "MIRROR-MISSING(flags)");
    typedef int (*CreateTP)(void*, int, void*, void**);
    return ((CreateTP)origCreate)(cmd, feat, p, h);
}

// Gate: intercepts the GAME's own GetProcAddress import (IAT redirection on
// mock_game.exe). Production Mor3a gates one level up - a process-wide
// Detours on GetProcAddress (inputs.cpp:675). We cannot reproduce that here:
// on this wine host inline patches of kernel32!GetProcAddress silently never
// land (byte dumps prove it), and patching a function the bridge also owns
// would be the recursive double-hook §6 forbids (measured: it silences both
// hooks). An IAT redirect observes exactly the same event (handle + name +
// resolution) through the game's single import table entry, touches no shared
// function, and mirrors what the bridge will verify on the real machine: that
// the resolution Mor3a's production hook sees is answered with the spoofed
// provider path, and that accepting installs the capture.
static FARPROC (WINAPI* origGPA)(HMODULE, LPCSTR) = nullptr;

static unsigned gateCalls = 0;

static FARPROC WINAPI gateIAT(HMODULE m, LPCSTR name)
{
    if (++gateCalls <= 10)
        M3("gateIAT #%u m=%p name=%s", gateCalls, (void*)m,
           ((INT_PTR)name < 0x10000) ? "#ordinal" : (const char*)name);
    FARPROC orig = origGPA ? origGPA(m, name) : nullptr;
    // Ordinal bindings encode a small integer in the name; real import names
    // are string POINTERS (values far above 0x10000) - pass those through.
    if (!m || (INT_PTR)name < 0x10000) return orig;
    bool eval = strcmp(name, "NVSDK_NGX_VULKAN_EvaluateFeature") == 0;
    bool create = strcmp(name, "NVSDK_NGX_VULKAN_CreateFeature") == 0;
    bool create1 = strcmp(name, "NVSDK_NGX_VULKAN_CreateFeature1") == 0;
    if (!eval && !create && !create1) return orig;

    wchar_t provider[MAX_PATH]{};
    GetModuleFileNameW(m, provider, MAX_PATH); // the call the bridge answers
    const wchar_t* base = wcsrchr(provider, L'\\');
    base = base ? base + 1 : provider;
    M3("gate name=%s provider='%S'", name, provider);
    if (_wcsicmp(base, L"_nvngx.dll") != 0 && _wcsicmp(base, L"nvngx.dll") != 0) {
        ++rejects;
        M3("RESULT: REJECT - provider name not on the nvngx whitelist; capture NOT installed (%d rejects)", rejects);
        return orig;
    }
    M3("RESULT: ACCEPT - installing input-capture wrapper");
    if (eval)   { origEval = (void*)orig;   return (FARPROC)&wrappedEval; }
    if (create) { origCreate = (void*)orig; return (FARPROC)&wrappedCreate; }
    return orig; // CreateFeature1: pass through, same as production
}

static bool redirectGameGPA(void)
{
    HMODULE exe = GetModuleHandleW(NULL);
    if (!exe) return false;
    char* base = (char*)exe;
    auto* dh = (IMAGE_DOS_HEADER*)base;
    if (dh->e_magic != IMAGE_DOS_SIGNATURE) return false;
    auto* nt = (IMAGE_NT_HEADERS*)(base + dh->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;
    auto& impDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!impDir.VirtualAddress) return false;
    auto* desc = (IMAGE_IMPORT_DESCRIPTOR*)(base + impDir.VirtualAddress);
    for (; desc->Name; ++desc) {
        const char* mod = base + desc->Name;
        if (_stricmp(mod, "kernel32.dll") != 0) continue;
        auto* names = (IMAGE_THUNK_DATA*)(base + (desc->OriginalFirstThunk ? desc->OriginalFirstThunk : desc->FirstThunk));
        auto* slots = (IMAGE_THUNK_DATA*)(base + desc->FirstThunk);
        for (; names->u1.AddressOfData; ++names, ++slots) {
            if (IMAGE_SNAP_BY_ORDINAL64(names->u1.Ordinal)) continue;
            auto* iname = (IMAGE_IMPORT_BY_NAME*)(base + names->u1.AddressOfData);
            if (strcmp(iname->Name, "GetProcAddress") != 0) continue;
            DWORD old;
            if (!VirtualProtect(&slots->u1.Function, sizeof(void*), PAGE_READWRITE, &old)) return false;
            origGPA = (FARPROC (WINAPI*)(HMODULE, LPCSTR))(uintptr_t)slots->u1.Function;
            slots->u1.Function = (ULONGLONG)&gateIAT;
            VirtualProtect(&slots->u1.Function, sizeof(void*), old, &old);
            return true;
        }
    }
    return false;
}

static DWORD WINAPI initThread(LPVOID)
{
    if (!redirectGameGPA()) { M3("FATAL: could not redirect game's GetProcAddress import"); return 0; }
    M3("gate installed (game IAT GetProcAddress redirect); waiting for NVSDK_NGX_VULKAN_* resolutions");
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h);
        M3("loaded");
        HANDLE t = CreateThread(NULL, 0, initThread, NULL, 0, NULL);
        if (t) CloseHandle(t);
    }
    return TRUE;
}
