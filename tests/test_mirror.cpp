// Native (no Windows) unit tests for the mirror's coercion + bookkeeping.
#include "mirror_core.h"
#include <cassert>
#include <cstdio>
#include <cmath>

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { ++failures; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } } while (0)

int main()
{
    uint64_t out = 0;

    // ptr -> ull, ull -> ptr
    MirrorValue v{ValueKind::Ptr, 0x1122334455667788ULL};
    CHECK(MirrorCoerce(v, ValueKind::ULL, &out) && out == 0x1122334455667788ULL);
    MirrorValue u{ValueKind::ULL, 0x7FF012345678ULL};
    CHECK(MirrorCoerce(u, ValueKind::Ptr, &out) && out == 0x7FF012345678ULL);

    // ull -> i range checks
    MirrorValue big{ValueKind::ULL, 0xFFFFFFFF1000ULL};
    CHECK(!MirrorCoerce(big, ValueKind::I, &out));
    MirrorValue small{ValueKind::ULL, 7};
    CHECK(MirrorCoerce(small, ValueKind::I, &out) && out == 7);

    // i <-> ui sign rules
    MirrorValue neg{ValueKind::I, uint64_t(uint32_t(-5))};
    CHECK(!MirrorCoerce(neg, ValueKind::UI, &out));
    MirrorValue pos{ValueKind::I, uint64_t(uint32_t(42))};
    CHECK(MirrorCoerce(pos, ValueKind::UI, &out) && out == 42);

    // ptr must NEVER narrow to int-like types
    CHECK(!MirrorCoerce(v, ValueKind::I, &out));
    CHECK(!MirrorCoerce(v, ValueKind::UI, &out));

    // float: integral-only when presented as integer; a real jitter stays float-typed
    float jit = 0.35f; uint32_t jb; memcpy(&jb, &jit, 4);
    MirrorValue f{ValueKind::F, jb};
    CHECK(!MirrorCoerce(f, ValueKind::I, &out));   // jitter must not become a "flag"
    float one = 1.0f; uint32_t oneb; memcpy(&oneb, &one, 4);
    MirrorValue f1{ValueKind::F, oneb};
    CHECK(MirrorCoerce(f1, ValueKind::I, &out) && out == 1);
    CHECK(MirrorCoerce(f1, ValueKind::D, &out) && std::abs(*(double*)&out - 1.0) < 1e-9);

    // core bookkeeping: set, get, forget
    MirrorCore core;
    int obj = 0, obj2 = 0;
    core.NoteSet(&obj, "Depth", ValueKind::ULL, 0x1000);
    CHECK(core.NoteGet(&obj, "Depth", ValueKind::Ptr, &out) && out == 0x1000);
    CHECK(!core.NoteGet(&obj, "MotionVectors", ValueKind::Ptr, &out));
    CHECK(!core.NoteGet(&obj2, "Depth", ValueKind::Ptr, &out));
    // address reuse: a fresh allocation at the same address must not see stale data
    core.NoteSet(&obj2, "Depth", ValueKind::Ptr, 0x2000);
    core.ForgetObject(&obj2);
    CHECK(!core.NoteGet(&obj2, "Depth", ValueKind::Ptr, &out));

    printf("%s (%d failure%s)\n", failures ? "FAILED" : "OK", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
