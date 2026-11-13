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
bool budget_fault = false;
int global_fault = 0;
unsigned adapter_queries = 0, segment_queries = 0;
unsigned destroyed = 0, destroy_failures = 0;
std::vector<D3DKMT_HANDLE> rejected;
void insist(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
NTSTATUS APIENTRY query_budget(D3DKMT_QUERYVIDEOMEMORYINFO* info) {
    return budget_fault ? NTSTATUS(0xc0000001u) : D3DKMTQueryVideoMemoryInfo(info);
}
NTSTATUS APIENTRY query_statistics(const D3DKMT_QUERYSTATISTICS* info) {
    if (info->Type == D3DKMT_QUERYSTATISTICS_ADAPTER) ++adapter_queries;
    if (info->Type == D3DKMT_QUERYSTATISTICS_SEGMENT) ++segment_queries;
    if (global_fault == 1) return NTSTATUS(0xc0000001u);
    const auto result = D3DKMTQueryStatistics(info);
    if (result >= 0 && global_fault && info->Type == D3DKMT_QUERYSTATISTICS_SEGMENT) {
        auto& memory = const_cast<D3DKMT_QUERYSTATISTICS*>(info)->QueryResult.SegmentInformation;
        if (!memory.Aperture && !memory.SegmentProperties.SystemMemory) {
            if (global_fault == 2) memory.BytesResident = memory.CommitLimit + 1;
            else if (global_fault == 3) memory.Aperture = 1;
        }
    }
    return result;
}
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
#define D3DKMTQueryVideoMemoryInfo query_budget
#define D3DKMTQueryStatistics query_statistics
#define D3DKMTDestroyAllocation2 destroy_allocation
#include "../src/wddm/runtime.cpp"
#undef D3DKMTQueryAllocationResidency
#undef D3DKMTQueryVideoMemoryInfo
#undef D3DKMTQueryStatistics
#undef D3DKMTDestroyAllocation2

template<class F> std::string rejects(F f, const char* expected) {
    try { f(); } catch (const std::exception& e) {
        insist(std::string(e.what()).find(expected) != std::string::npos, e.what()); return e.what();
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
            budget_fault = i == 7;
            global_fault = i == 8 ? 1 : 0;
            const auto bytes = i == 16 ? (size_t{1} << 30) + 65536 : 65536;
            const auto message = rejects([&] { runtime.allocate_device(bytes); }, "not resident in local GPU memory");
            insist(message.find("bytes=" + std::to_string(bytes) + (i == 16 ? " parts=2 local=1" : " parts=1 local=0")) != std::string::npos &&
                message.find(fault == 1 ? " shared=1 not_resident=0" : " shared=0 not_resident=1") != std::string::npos &&
                message.find(" budget_bytes=") != std::string::npos && message.find(" usage_bytes=") != std::string::npos &&
                message.find(" budget_status=0x") != std::string::npos && message.find(" global_free_bytes=") != std::string::npos,
                "residency diagnostic lost size, parts, status or memory counters");
            if (budget_fault) insist(message.find("budget_status=0xc0000001") != std::string::npos,
                "budget query failure hid the original residency rejection");
            if (global_fault) insist(message.find("global_free_bytes=18446744073709551615") != std::string::npos,
                "global query failure hid the original residency rejection");
            insist(rejected.empty() && destroyed == i + 1, "residency rejection left live backing");
            fault = 0; budget_fault = false; global_fault = 0;
            const auto memory = runtime.memory_info();
            insist(memory.total > 0 && memory.available <= memory.total && memory.process_budget && memory.global_availability,
                "runtime lost global memory availability after rollback");
            auto small = runtime.allocate_device(65536);
            insist(small.gpu && small.device_local && !small.data, "retry changed allocation semantics");
            runtime.release(small);
            runtime.cpu_access(host, false);
            insist(*static_cast<unsigned*>(host.data) == 0x12345678, "rollback damaged an existing allocation");
        }
        const auto topology_queries = adapter_queries, old_segment_queries = segment_queries;
        runtime.memory_info(); runtime.memory_info();
        insist(adapter_queries == topology_queries && segment_queries > old_segment_queries, "memory query did not cache topology and refresh residency");
        for (int mode : {1, 2, 3}) {
            global_fault = mode;
            rejects([&] { runtime.memory_info(); }, mode == 1 ? "global local-memory residency" : "global local-memory counters");
            global_fault = 0;
            insist(runtime.memory_info().global_availability, "runtime did not recover from a failed global query");
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
    std::puts("PASS cached topology/live residency,3 global-query faults/recovery and nonmasking residency diagnostics");
} catch (const std::exception& e) { std::fprintf(stderr, "%s\n", e.what()); return 1; }
