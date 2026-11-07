// Mock stand-in for RDR2's RAGE NGX loader. Self-test only.
// Flow mirroring production:
//   OptiScaler returns its own handle for "nvngx.dll" (here: we resolve
//   directly from mock_optiscaler.dll's handle), the game resolves the NGX
//   functions through kernel32!GetProcAddress (bridge observer + Mor3a-mock
//   gate run), allocates a parameter object (bridge thunk + vtable mirror get
//   installed), stores frame inputs, then Creates and Evaluates (Mor3a-mock
//   wrapper must see them).
//
// Deliberate worst case: Depth is stored as a 64-bit int (Set ULL) while the
// reader asks for it through a slot whose declared type is a different one.
// Without the bridge's typed mirror the flags read (cross-type miss) fails;
// with it the read must succeed (OBSERVER-OK + flags result=1).
#include <windows.h>
#include <cstdarg>
#include <cstdio>

extern "C" void* __imp_GetProcAddress; // the game's IAT slot for GetProcAddress

static char depthRes[256] = "DEPTH-SNAPSHOT";
static char mvRes[256] = "MOTIONVECTORS-SNAPSHOT";

static void Q(const char* fmt, ...)
{
    char buf[512];
    va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof buf, fmt, ap); va_end(ap);
    printf("[GAME   ] %s\n", buf);
    fflush(stdout);
}

int main()
{
    Q("== mock game: RDR2 NGX loader simulation ==");
    // load order mirrors reality: OptiScaler, then Mor3a's ASI, then the
    // zz_-named bridge (zz sorts last), so the bridge's hooks attach last and
    // observe everything the game resolves.
    HMODULE ho = LoadLibraryA("mock_optiscaler.dll");
    HMODULE hm = LoadLibraryA("mock_mor3a.dll");
    HMODULE hz = LoadLibraryA("zz_ngxbridge.asi");
    Q("modules: opti=%p mor3a=%p bridge=%p", (void*)ho, (void*)hm, (void*)hz);
    if (!ho || !hm || !hz) { Q("FATAL: module load failed"); return 2; }

    Sleep(2000); // let both hook threads install (production gap is seconds)

    // evidence: the gate has redirected this process's GetProcAddress import
    Q("game IAT slot addr=%p value=%p (mock_mor3a base=%p - gate lives inside it)",
      (void*)&__imp_GetProcAddress, *(void* volatile*)&__imp_GetProcAddress, (void*)hm);

    typedef int (*AllocTP)(void**);
    typedef int (*CreateTP)(void*, int, void*, void**);
    typedef int (*EvalTP)(void*, void*, void*, void*);
    // Volatile slot read per call: the compiler would otherwise hoist
    // __imp_GetProcAddress into a register at main entry, BEFORE the gate
    // (loaded via LoadLibrary below) can redirect it. A real game binds NGX
    // seconds after Mor3a's ASI hooks the API, so no such race exists there.
    #define GPA(h, n) ((FARPROC (WINAPI*)(HMODULE, LPCSTR))*(void* volatile*)&__imp_GetProcAddress)(h, n)
    AllocTP alloc = (AllocTP)GPA(ho, "NVSDK_NGX_VULKAN_AllocateParameters");
    CreateTP create = (CreateTP)GPA(ho, "NVSDK_NGX_VULKAN_CreateFeature");
    EvalTP eval = (EvalTP)GPA(ho, "NVSDK_NGX_VULKAN_EvaluateFeature");
    Q("resolved alloc=%p create=%p eval=%p", (void*)alloc, (void*)create, (void*)eval);
    if (!alloc || !create || !eval) { Q("FATAL: GetProcAddress failed"); return 3; }

    void* params = nullptr;
    if (alloc(&params) != 1 || !params) { Q("FATAL: AllocateParameters failed"); return 4; }

    // Deliberate worst case, using the REAL slot layout (see mirror_core.h):
    //   Depth        stored as 64-bit int via Set ULL   (slot 0)
    //   MotionVectors stored as pointer via Set void*  (slot 7)
    //   Flags        stored as int via Set I           (slot 4)
    // Mor3a's reader asks pointer keys through slot 8 (which its reversed map
    // calls GetVoidPointer but is really Get ULL* - works via conversion),
    // and flags through slot 11 (really Get unsigned* - a cross-type miss the
    // mirror must rescue).
    void** vt = *(void***)params;   // vptr first, then index the table
    if (HMODULE mo = GetModuleHandleA("mock_optiscaler.dll")) {
        typedef void (*DumpTP)(void*);
        ((DumpTP)GPA(mo, "mock_dump"))(params);
    }
    typedef int (*SetULLTP)(void*, const char*, unsigned long long);
    typedef int (*SetPTP)(void*, const char*, void*);
    typedef int (*SetITP)(void*, const char*, int);
    ((SetULLTP)vt[0])(params, "Depth", (unsigned long long)(uintptr_t)depthRes);
    ((SetPTP)vt[7])(params, "MotionVectors", mvRes);
    ((SetULLTP)vt[0])(params, "Reset", 1);
    ((SetITP)vt[4])(params, "DLSS.Feature.Create.Flags", 1);

    void* handle = nullptr;
    Q("CreateFeature result=%d", create(nullptr, 1, params, &handle));

    for (int frame = 1; frame <= 2; ++frame) {
        int r = eval(nullptr, handle, params, nullptr);
        Q("frame %d evaluate result=%d", frame, r);
    }

    // Stale guard: the game's own Reset() (REAL vtable slot 16) empties the
    // provider's storage. The mirror's copy MUST be dropped with it - a later
    // cross-type read must fail again, never answer with the pre-reset value
    // ("never treat old frames as valid"). If the bridge forgot this, this
    // line would print STALE-LEAK.
    typedef int (*ResetTP)(void*);
    typedef int (*GetUIP)(void*, const char*, unsigned*);
    int rres = ((ResetTP)vt[16])(params);
    unsigned flags2 = 0xFFFFFFFFu;
    int r2 = ((GetUIP)vt[11])(params, "DLSS.Feature.Create.Flags", &flags2);
    Q("post-Reset provider=%d flags read result=%d %s", rres, r2,
      (r2 == 0) ? "STALE-GUARD-OK" : "STALE-LEAK!!");

    Sleep(300);
    Q("== mock game done ==");
    Sleep(200); // let other threads flush their logs
    return 0;
}
