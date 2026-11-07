// Typed side-table for NVSDK_NGX_Parameter vtable slot mirroring.
// Portable C++ (no Win32) so the logic is unit-testable on any platform.
#pragma once
#include <cstdint>
#include <cstring>
#include <map>
#include <mutex>
#include <string>

// NVSDK_NGX_Parameter virtual slot layout (MSVC x64 ABI, single inheritance):
// slot N == Nth virtual as DECLARED in external/nvngx_dlss_sdk/nvsdk_ngx_params.h
// (verified line-by-line against OptiScaler v0.9.4's vendored SDK copy).
// Declaration order is ULL,F,D,UI,I,D3D11,D3D12,void* per group; no dtor slot.
//
// NOTE: Mor3a's inputs.cpp assumes the groups reversed (void*=0). Its reads of
// the pointer keys still work against the real layout by conversion (its slot 8
// hits the real Get(unsigned long long*), which converts stored pointers back),
// but its float reads (slot 14 = real Get(ID3D12Resource**)) cannot. The mirror
// is therefore keyed by the REAL layout: it records exactly what the provider's
// real slot types store and answers provider misses for that same slot type.
enum NgxParamSlot : int {
    SLOT_SET_ULL = 0,    SLOT_SET_F = 1,   SLOT_SET_D = 2,   SLOT_SET_UI = 3,
    SLOT_SET_I = 4,      SLOT_SET_D3D11 = 5, SLOT_SET_D3D12 = 6, SLOT_SET_VOIDPTR = 7,
    SLOT_GET_ULL = 8,    SLOT_GET_F = 9,   SLOT_GET_D = 10,  SLOT_GET_UI = 11,
    SLOT_GET_I = 12,     SLOT_GET_D3D11 = 13, SLOT_GET_D3D12 = 14, SLOT_GET_VOIDPTR = 15,
    SLOT_COUNT = 16,
    SLOT_RESET = 16,     // Reset() IS patched: provider Reset must also drop
                         // mirror memory, so no value from before the reset can
                         // ever answer a Get (never-use-stale-inputs invariant).
    SLOTS_TOTAL = 17,
};

enum class ValueKind : uint8_t { None = 0, Ptr, I, UI, ULL, F, D };

struct MirrorValue {
    ValueKind kind = ValueKind::None;
    uint64_t  raw  = 0;   // bit pattern: pointer, or bits of the scalar type
};

// Safe reinterpretation of a stored value into the requested type.
// Returns false when the conversion is not representable (e.g. a 64-bit
// pointer out of int32 range, or a non-integral float presented as int).
bool MirrorCoerce(const MirrorValue& v, ValueKind want, uint64_t* out);

class MirrorCore {
public:
    void NoteSet(void* self, const char* key, ValueKind kind, uint64_t raw);
    // Called only when the provider's own Get failed for (self,key).
    // Returns true and writes *out when a safe coerced fallback exists.
    bool NoteGet(void* self, const char* key, ValueKind want, uint64_t* out);
    // Object address is being (re)allocated: drop stale entries so a reused
    // address can never resurrect freed Vulkan resources (use-after-free guard).
    void ForgetObject(void* self);

    unsigned sets()  const { return sets_; }
    unsigned getHits()   const { return hits_; }
    unsigned getMiss()   const { return misses_; }

private:
    std::mutex mu_;
    std::map<void*, std::map<std::string, MirrorValue>> objs_;
    unsigned sets_ = 0, hits_ = 0, misses_ = 0;
};
