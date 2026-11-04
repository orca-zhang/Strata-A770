// Deterministic residency rejection on real, small allocations; never exhaust VRAM.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winternl.h>
#include <d3dkmthk.h>
#include <algorithm>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
int fault = 0;
unsigned destroyed = 0, destroy_failures = 0;
std::vector<D3DKMT_HANDLE> rejected;
void insist(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
NTSTATUS APIENTRY query_residency(const D3DKMT_QUERYALLOCATIONRESIDENCY* q) {
    const auto result = D3DKMTQueryAllocationResidency(q);
    if (result < 0 || !fault) return result;
    if (fault == 4) return NTSTATUS(0xc0000001u);
    insist(rejected.empty(), "previous rejected allocation leaked");
    insist(std::all_of(q->pResidencyStatus, q->pResidencyStatus + q->AllocationCount,
        [](auto s) { return s == D3DKMT_ALLOCATIONRESIDENCYSTATUS_RESIDENTINGPUMEMORY; }),
        "test allocation was not actually local before injection");
    rejected.assign(q->phAllocationList, q->phAllocationList + q->AllocationCount);
    q->pResidencyStatus[q->AllocationCount - 1] = fault == 2 ?
        D3DKMT_ALLOCATIONRESIDENCYSTATUS_NOTRESIDENT : D3DKMT_ALLOCATIONRESIDENCYSTATUS_RESIDENTINSHAREDMEMORY;
    return result;
}
NTSTATUS APIENTRY destroy_allocation(const D3DKMT_DESTROYALLOCATION2* d) {
    const bool tracked = !rejected.empty() && d->AllocationCount && d->phAllocationList[0] == rejected[0];
    if (tracked) {
        insist(d->Flags.SynchronousDestroy && d->AllocationCount == rejected.size() &&
            std::equal(rejected.begin(), rejected.end(), d->phAllocationList),
            "rollback did not synchronously destroy every part exactly once");
        if (fault == 3 && !destroy_failures++) return NTSTATUS(0xc0000001u);
    }
    const auto result = D3DKMTDestroyAllocation2(d);
    if (tracked && result >= 0) { rejected.clear(); ++destroyed; }
    return result;
}
}

// Compile the actual runtime with test-only call interception; no production hooks.
#define D3DKMTQueryAllocationResidency query_residency
#define D3DKMTDestroyAllocation2 destroy_allocation
#include "../src/wddm/runtime.cpp"
#undef D3DKMTQueryAllocationResidency
#undef D3DKMTDestroyAllocation2

template<class F> void rejects(F f, const char* expected) {
    try { f(); } catch (const std::exception& e) {
        insist(std::string(e.what()).find(expected) != std::string::npos, e.what()); return;
    }
    throw std::runtime_error("expected failure was accepted");
}
int main() try {
    using namespace orca::l0::dg2;
    {
        Runtime runtime;
        auto host = runtime.allocate(65536);
        *static_cast<unsigned*>(host.data) = 0x12345678;
        for (unsigned i = 0; i < 17; ++i) {
            fault = 1 + i % 2;
            const auto bytes = i == 16 ? (size_t{1} << 30) + 65536 : 65536;
            rejects([&] { runtime.allocate_device(bytes); }, "not resident in local GPU memory");
            insist(rejected.empty() && destroyed == i + 1, "residency rejection left live backing");
            fault = 0;
            insist(runtime.memory_info().total > 0, "runtime cannot query memory after rollback");
            auto small = runtime.allocate_device(65536);
            insist(small.gpu && small.device_local && !small.data, "retry changed allocation semantics");
            runtime.release(small);
            runtime.cpu_access(host, false);
            insist(*static_cast<unsigned*>(host.data) == 0x12345678, "rollback damaged an existing allocation");
        }
        runtime.release(host);
    }
    for (int mode : {3, 4}) {
        {
            Runtime runtime;
            fault = mode;
            rejects([&] { runtime.allocate_device(65536); }, mode == 3 ? "rollback" : "query device allocation residency");
            fault = 0;
            rejects([&] { runtime.memory_info(); }, "runtime failure");
            rejects([&] { runtime.allocate_device(65536); }, "failed recording");
        }
        insist(rejected.empty(), "failed rollback lost ownership during teardown");
    }
    insist(destroyed == 18 && destroy_failures == 1, "incorrect rollback or teardown count");
    std::puts("PASS 17 rejected local allocations (one multipart),17 retries,existing data,2 terminal failures,18 synchronous destroys");
} catch (const std::exception& e) { std::fprintf(stderr, "%s\n", e.what()); return 1; }
