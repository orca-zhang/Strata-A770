// Real small allocations plus one-shot fault injection; include the actual runtime.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winternl.h>
#include <d3dkmthk.h>
#include <cstdio>
#include <stdexcept>
#include <map>
#include <set>
#include <cstring>
#include <string>
#include <algorithm>
#include <cstdint>
#include <vector>
#include <memory>

namespace {
int fault = 0;
unsigned native_creates = 0, native_destroys = 0, unlocks = 0;
std::map<D3DKMT_HANDLE, void*> live;
std::set<void*> driver_pointers;
void insist(bool v, const char* text) { if (!v) throw std::runtime_error(text); }
bool fail(int stage) { if (fault != stage) return false; fault = 0; return true; }
NTSTATUS APIENTRY observe_create(D3DKMT_CREATEALLOCATION* p) {
    const bool native = !p->Flags.StandardAllocation && p->NumAllocations == 1 &&
        !p->pAllocationInfo2[0].pSystemMem &&
        p->pAllocationInfo2[0].pPrivateDriverData &&
        (static_cast<const uint8_t*>(p->pAllocationInfo2[0].pPrivateDriverData)[0x27] & 2);
    if (native) {
        unsigned flags=0; std::memcpy(&flags,&p->Flags,sizeof(flags));
        insist(!flags && !p->pAllocationInfo2[0].pSystemMem, "native Host unexpectedly uses shared/imported pages");
        if (fail(1)) return NTSTATUS(0xc0000001u);
    }
    const auto result = D3DKMTCreateAllocation2(p);
    if (native && result >= 0) { live.emplace(p->pAllocationInfo2[0].hAllocation, nullptr); ++native_creates; }
    return result;
}
NTSTATUS APIENTRY observe_lock(D3DKMT_LOCK2* p) {
    insist(live.count(p->hAllocation) && !p->Flags.Value, "unexpected CPU mapping contract");
    if (fail(2)) return NTSTATUS(0xc0000001u);
    const auto result = D3DKMTLock2(p);
    if (result >= 0) { live.at(p->hAllocation) = p->pData; driver_pointers.insert(p->pData); }
    return result;
}
NTSTATUS APIENTRY observe_unlock(const D3DKMT_UNLOCK2* p) {
    insist(live.count(p->hAllocation), "unlock lost native owner");
    if (fail(5)) return NTSTATUS(0xc0000001u);
    const auto result = D3DKMTUnlock2(p);
    if (result >= 0) ++unlocks;
    return result;
}
NTSTATUS APIENTRY observe_resident(D3DDDI_MAKERESIDENT* p) {
    if (fail(3)) return NTSTATUS(0xc0000001u);
    return D3DKMTMakeResident(p);
}
NTSTATUS APIENTRY observe_map(D3DDDI_MAPGPUVIRTUALADDRESS* p) {
    if (fail(4)) return NTSTATUS(0xc0000001u);
    return D3DKMTMapGpuVirtualAddress(p);
}
NTSTATUS APIENTRY observe_destroy(const D3DKMT_DESTROYALLOCATION2* p) {
    const bool tracked = p->AllocationCount == 1 && live.count(*p->phAllocationList);
    if (tracked && fail(6)) return NTSTATUS(0xc0000001u);
    const auto result = D3DKMTDestroyAllocation2(p);
    if (tracked && result >= 0) { insist(p->Flags.SynchronousDestroy, "Host cleanup must finish synchronously"); live.erase(*p->phAllocationList); ++native_destroys; }
    return result;
}
BOOL WINAPI free_pages(void* p, SIZE_T bytes, DWORD flags) {
    insist(!driver_pointers.count(p), "VirtualFree called on a driver-owned CPU mapping");
    return VirtualFree(p, bytes, flags);
}
}
#define D3DKMTCreateAllocation2 observe_create
#define D3DKMTLock2 observe_lock
#define D3DKMTUnlock2 observe_unlock
#define D3DKMTMakeResident observe_resident
#define D3DKMTMapGpuVirtualAddress observe_map
#define D3DKMTDestroyAllocation2 observe_destroy
#define VirtualFree free_pages
#include "../src/wddm/runtime.cpp"
#undef D3DKMTCreateAllocation2
#undef D3DKMTLock2
#undef D3DKMTUnlock2
#undef D3DKMTMakeResident
#undef D3DKMTMapGpuVirtualAddress
#undef D3DKMTDestroyAllocation2
#undef VirtualFree
#include "runtime_dependencies.hpp"

template<class F> void rejects(F f) { bool rejected = false; try { f(); } catch (const std::exception&) { rejected = true; } insist(rejected, "expected rejection"); }
int main() try {
    using namespace orca::l0::dg2;
    {
        Runtime r;
        for (size_t size : {size_t{65536}, size_t{2162688}, size_t{16777216}, size_t{2} << 30}) {
            auto a = r.allocate(size);
            insist(a.data && a.gpu && !a.imported && !a.device_local && a.bytes >= size,
                "native Host identity/alignment/ownership changed");
            MEMORY_BASIC_INFORMATION info{};
            insist(VirtualQuery(a.data, &info, sizeof(info)) == sizeof(info) && !(info.Protect & (PAGE_NOCACHE | PAGE_WRITECOMBINE)), "native Host is not CPU WB");
            auto* data = static_cast<uint8_t*>(a.data);
            for (size_t i = 0; i < a.bytes; ++i) data[i] = uint8_t(i * 19 + 7);
            r.cpu_access(a, false);
            for (size_t i = 0; i < a.bytes; ++i) insist(data[i] == uint8_t(i * 19 + 7), "native Host full content mismatch");
            r.release(a); insist(!a.data && !a.handle, "release did not clear allocation");
        }
        void* pages = VirtualAlloc(nullptr, 65536, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        insist(pages != nullptr, "import pages");
        driver_pointers.erase(pages); // A freed driver mapping address may be reused by caller pages.
        *static_cast<unsigned*>(pages) = 0x12345678;
        auto imported = r.import_host(pages, 65536);
        insist(imported.imported && imported.data == pages, "import semantics changed");
        r.release(imported);
        insist(*static_cast<unsigned*>(pages) == 0x12345678, "imported caller pages freed or damaged");
        insist(VirtualFree(pages, 0, MEM_RELEASE), "free caller pages");
    }
    insist(live.empty(), "normal lifecycle leaked driver allocation");
    for (int stage = 1; stage <= 6; ++stage) {
        {
            Runtime r;
            if (stage <= 4) { fault = stage; rejects([&] { r.allocate(65536); }); }
            else { auto a = r.allocate(65536); fault = stage; rejects([&] { r.release(a); }); }
            insist(!fault, "failure path was not exercised");
            rejects([&] { r.allocate(65536); });
        }
        insist(live.empty(), "failure cleanup leaked driver allocation");
    }
    insist(native_creates == native_destroys && unlocks, "native create/destroy imbalance");
    check_runtime_dependencies(true);
    std::printf("PASS full Host bytes/CPU WB/independent GPU VA/import ownership;6 fault stages;created=%u destroyed=%u unlocks=%u;no vendor UMD/GMM\n", native_creates, native_destroys, unlocks);
} catch (const std::exception& e) { std::fprintf(stderr, "%s\n", e.what()); return 1; }
