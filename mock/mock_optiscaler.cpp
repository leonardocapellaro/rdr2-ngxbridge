// Mock stand-in for OptiScaler.asi's NGX surface. Self-test only - never
// installed next to the game. It reproduces the two behaviors the bridge
// design depends on: (1) the game resolves real NVSDK_NGX_VULKAN_* functions
// from THIS module's handle (OptiScaler's EnableDlssInputs hands out its own
// handle for the name "nvngx.dll"), and (2) its parameter object stores
// values STRICTLY typed: a value set as ULL is not returned by Get(void*),
// which is exactly the worst case the typed mirror defends against.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>

static void M(const char* fmt, ...)
{
    char buf[512];
    va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof buf, fmt, ap); va_end(ap);
    printf("[OPTI-MOCK] %s\n", buf);
    fflush(stdout);
}

enum { K_PTR = 0, K_I, K_UI, K_D, K_F, K_ULL };

// Declaration order == vtable slot order (verified against v0.9.4's vendored
// nvsdk_ngx_params.h): ULL,F,D,UI,I,D3D11,D3D12,void per group, then Reset.
// Read semantics mirror OptiScaler's own storage (NVNGX_Parameter.h getT):
//  * a typed read of an EXISTING key returns Success; pointer slots write
//    NULL when the stored type cannot yield one (this is the verified
//    "SetULL(Depth) then Get(void**) -> Success + nullptr" behavior),
//  * void* <-> unsigned long long convert,
//  * missing key -> Fail.
// Scalar slots keep strict mismatch->Fail here (a documented conservative
// divergence: real OptiScaler may numeric-convert scalars; strict keeps the
// mock exercising the bridge's mirror fallback instead of assuming OptiScaler
// converts, so the self-test proves the mirror works either way).
struct MockParam {
    std::map<std::string, std::pair<int, uint64_t>> store;

    int ReadPtr(const char* k, void** o)
    {
        auto it = store.find(k);
        if (it == store.end()) return 0;                       // missing -> Fail
        uint64_t v = 0;
        if (it->second.first == K_PTR || it->second.first == K_ULL) v = it->second.second;
        if (o) *o = (void*)(uintptr_t)v;                        // may be null (Success+null)
        return 1;
    }
    int ReadULL(const char* k, unsigned long long* o)
    {
        auto it = store.find(k);
        if (it == store.end()) return 0;
        if (it->second.first != K_ULL && it->second.first != K_PTR) return 0; // documented divergence
        if (o) *o = it->second.second;
        return 1;
    }
    int ReadStrict(const char* k, int kind, uint64_t* o)
    {
        auto it = store.find(k);
        if (it == store.end() || it->second.first != kind) return 0;
        if (o) *o = it->second.second;
        return 1;
    }

    virtual int SetULL(const char* k, unsigned long long v) { store[k] = { K_ULL, v }; return 1; }          // slot 0
    virtual int SetF(const char* k, float v) { uint32_t b; memcpy(&b, &v, 4); store[k] = { K_F, b }; return 1; }  // 1
    virtual int SetD(const char* k, double v) { uint64_t b; memcpy(&b, &v, 8); store[k] = { K_D, b }; return 1; } // 2
    virtual int SetUI(const char* k, unsigned v) { store[k] = { K_UI, v }; return 1; }                      // 3
    virtual int SetI(const char* k, int v) { store[k] = { K_I, (uint64_t)(uint32_t)v }; return 1; }         // 4
    virtual int SetD3D11Resource(const char* k, void* v) { store[k] = { K_PTR, (uint64_t)(uintptr_t)v }; return 1; } // 5
    virtual int SetD3D12Resource(const char* k, void* v) { store[k] = { K_PTR, (uint64_t)(uintptr_t)v }; return 1; } // 6
    virtual int SetVoidPointer(const char* k, void* v) { store[k] = { K_PTR, (uint64_t)(uintptr_t)v }; return 1; }   // 7
    virtual int GetULL(const char* k, unsigned long long* o) { return ReadULL(k, o); }                      // 8
    virtual int GetF(const char* k, float* o) { uint64_t v; int r = ReadStrict(k, K_F, &v); if (r == 1 && o) { uint32_t b = (uint32_t)v; float f; memcpy(&f, &b, 4); *o = f; } return r; } // 9
    virtual int GetD(const char* k, double* o) { uint64_t v; int r = ReadStrict(k, K_D, &v); if (r == 1 && o) { double d; memcpy(&d, &v, 8); *o = d; } return r; } // 10
    virtual int GetUI(const char* k, unsigned* o) { uint64_t v; int r = ReadStrict(k, K_UI, &v); if (r == 1 && o) *o = (unsigned)v; return r; } // 11
    virtual int GetI(const char* k, int* o) { uint64_t v; int r = ReadStrict(k, K_I, &v); if (r == 1 && o) *o = (int)(int32_t)(uint32_t)v; return r; } // 12
    virtual int GetD3D11Resource(const char* k, void** o) { return ReadPtr(k, o); }   // 13
    virtual int GetD3D12Resource(const char* k, void** o) { return ReadPtr(k, o); }   // 14
    virtual int GetVoidPointer(const char* k, void** o) { return ReadPtr(k, o); }     // 15
    virtual int Reset() { store.clear(); return 1; }                                   // 16
};

// --------------------------------------------------- game-facing exports ---
extern "C" {
__declspec(dllexport) int NVSDK_NGX_VULKAN_AllocateParameters(void** out) {
    if (!out) return 0;
    *out = (void*)new MockParam();
    M("AllocateParameters -> param=%p", *out);
    return 1;
}
__declspec(dllexport) int NVSDK_NGX_VULKAN_GetParameters(void** out) {
    static MockParam* single = nullptr;
    if (!single) single = new MockParam();
    if (out) { *out = (void*)single; M("GetParameters -> singleton param=%p", (void*)single); }
    return 1;
}
__declspec(dllexport) int NVSDK_NGX_VULKAN_CreateFeature(void* cmd, int feat, void* p, void** h) {
    M("CreateFeature(feature=%d params=%p) - upscaling implementation would run here", feat, p);
    if (h) *h = (void*)0xC0FFEE;
    return 1;
}
__declspec(dllexport) int NVSDK_NGX_VULKAN_CreateFeature1(void* inst, void* dev, int feat, void* p, void** h) {
    (void)inst; (void)dev;
    return NVSDK_NGX_VULKAN_CreateFeature(nullptr, feat, p, h);
}
__declspec(dllexport) int NVSDK_NGX_VULKAN_EvaluateFeature(void* cmd, const void* h, void* p, void* cb) {
    M("EvaluateFeature IMPL ran (params=%p) - provider saw the frame inputs", p);
    return 1;
}
__declspec(dllexport) void mock_dump(void* obj) {
    uintptr_t base = (uintptr_t)GetModuleHandleA("mock_optiscaler.dll");
    M("dump(obj=%p) vptr=%p (off=+0x%llx)", obj, obj ? *(void**)obj : nullptr,
      obj ? (uintptr_t)(*(void**)obj) - base : 0ULL);
    if (!obj) return;
    void** v = *(void***)obj;
    for (int i = 0; i < 16; ++i) {
        uintptr_t s = (uintptr_t)v[i];
        M("  slot[%2d]=%p %s", i, v[i],
          (s - base) < 0x200000 ? "(in mock dll)" : (s == (uintptr_t)-1 ? "(-1!!)" : "(external)"));
    }
}
__declspec(dllexport) int NVSDK_NGX_VULKAN_Init(void* a, void* b, void* c, void* d, void* e) { M("Init"); return 1; }
__declspec(dllexport) int NVSDK_NGX_VULKAN_Shutdown() { M("Shutdown"); return 1; }
__declspec(dllexport) int NVSDK_NGX_VULKAN_Shutdown1(void* h) { return 1; }
__declspec(dllexport) int NVSDK_NGX_VULKAN_ReleaseFeature(void* h) { return 1; }
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) { DisableThreadLibraryCalls(h); M("loaded"); }
    return TRUE;
}
