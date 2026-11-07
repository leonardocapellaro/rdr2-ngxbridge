#include "mirror_core.h"
#include <cmath>
#include <limits>

static float bitsToFloat(uint64_t r) { float f; std::memcpy(&f, &r, 4); return f; }
static double bitsToDouble(uint64_t r) { double d; std::memcpy(&d, &r, 8); return d; }
static uint64_t floatToBits(float f) { uint32_t u; std::memcpy(&u, &f, 4); return u; }
static uint64_t doubleToBits(double d) { uint64_t u; std::memcpy(&u, &d, 8); return u; }

bool MirrorCoerce(const MirrorValue& v, ValueKind want, uint64_t* out)
{
    if (v.kind == want) { *out = v.raw; return true; }
    switch (v.kind) {
    case ValueKind::Ptr:
        if (want == ValueKind::ULL) { *out = v.raw; return true; }   // ptr -> ull
        return false;                                                 // never narrow a pointer
    case ValueKind::ULL:
        switch (want) {
        case ValueKind::Ptr: *out = v.raw; return true;               // game convention: store ptr as ULL64
        case ValueKind::I:
            if (v.raw <= uint64_t(std::numeric_limits<int32_t>::max())) { *out = v.raw; return true; }
            return false;
        case ValueKind::UI:
            if (v.raw <= uint64_t(std::numeric_limits<uint32_t>::max())) { *out = v.raw; return true; }
            return false;
        case ValueKind::F:  *out = floatToBits(float(v.raw)); return true;
        case ValueKind::D:  *out = doubleToBits(double(v.raw)); return true;
        default: return false;
        }
    case ValueKind::I: {
        // SetI stores the low 32 bits (zero-extended); recover the signed value.
        int32_t i32 = int32_t(uint32_t(v.raw));
        int64_t s = i32;
        switch (want) {
        case ValueKind::UI:  if (s >= 0) { *out = uint64_t(s); return true; } return false;
        case ValueKind::ULL: if (s >= 0) { *out = uint64_t(s); return true; } return false;
        case ValueKind::F:   *out = floatToBits(float(s)); return true;
        case ValueKind::D:   *out = doubleToBits(double(s)); return true;
        default: return false;
        }
    }
    case ValueKind::UI: {
        uint32_t u = uint32_t(v.raw);
        switch (want) {
        case ValueKind::I:   if (u <= uint32_t(std::numeric_limits<int32_t>::max())) { *out = u; return true; } return false;
        case ValueKind::ULL: *out = u; return true;
        case ValueKind::F:   *out = floatToBits(float(u)); return true;
        case ValueKind::D:   *out = doubleToBits(double(u)); return true;
        default: return false;
        }
    }
    case ValueKind::F: {
        float f = bitsToFloat(v.raw);
        switch (want) {
        case ValueKind::D:   *out = doubleToBits(double(f)); return true;
        case ValueKind::I: {
            // only present an integral float as integer; a jitter offset must never
            // silently become a truncated "flag" value.
            if (std::isfinite(f) && f >= float(std::numeric_limits<int32_t>::min()) &&
                f < 2147483648.0f && f == std::floor(f)) { *out = uint64_t(uint32_t(int32_t(f))); return true; }
            return false;
        }
        case ValueKind::UI: {
            if (std::isfinite(f) && f >= 0.0f && f < 4294967296.0f && f == std::floor(f)) { *out = uint64_t(uint32_t(f)); return true; }
            return false;
        }
        case ValueKind::ULL: {
            if (std::isfinite(f) && f >= 0.0f && f < 18446744073709551616.0f && f == std::floor(f)) { *out = uint64_t(f); return true; }
            return false;
        }
        default: return false;
        }
    }
    case ValueKind::D: {
        double d = bitsToDouble(v.raw);
        switch (want) {
        case ValueKind::F: *out = floatToBits(float(d)); return true;
        case ValueKind::I: {
            if (std::isfinite(d) && d >= double(std::numeric_limits<int32_t>::min()) &&
                d < 2147483648.0 && d == std::floor(d)) { *out = uint64_t(uint32_t(int32_t(d))); return true; }
            return false;
        }
        case ValueKind::UI: {
            if (std::isfinite(d) && d >= 0.0 && d < 4294967296.0 && d == std::floor(d)) { *out = uint64_t(uint32_t(d)); return true; }
            return false;
        }
        case ValueKind::ULL: {
            if (std::isfinite(d) && d >= 0.0 && d < 18446744073709551616.0 && d == std::floor(d)) { *out = uint64_t(d); return true; }
            return false;
        }
        default: return false;
        }
    }
    default: return false;
    }
}

void MirrorCore::NoteSet(void* self, const char* key, ValueKind kind, uint64_t raw)
{
    if (!self || !key) return;
    std::lock_guard<std::mutex> lk(mu_);
    if (++sets_ == 0) return; // saturation only; counter wraps harmlessly
    objs_[self][key] = MirrorValue{kind, raw};
}

bool MirrorCore::NoteGet(void* self, const char* key, ValueKind want, uint64_t* out)
{
    if (!self || !key || !out) return false;
    std::lock_guard<std::mutex> lk(mu_);
    auto it = objs_.find(self);
    if (it == objs_.end()) { ++misses_; return false; }
    auto kit = it->second.find(key);
    if (kit == it->second.end()) { ++misses_; return false; }
    if (!MirrorCoerce(kit->second, want, out)) { ++misses_; return false; }
    ++hits_;
    return true;
}

void MirrorCore::ForgetObject(void* self)
{
    if (!self) return;
    std::lock_guard<std::mutex> lk(mu_);
    objs_.erase(self);
}
