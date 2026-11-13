// Include the actual candidate runtime; observe ownership and inject one-shot failures.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winternl.h>
#include <d3dkmthk.h>
#include <cstdio>
#include <cstring>
#include <map>
#include <stdexcept>
#include <vector>
#include <cstdint>

namespace {
int fault = 0; unsigned map_calls = 0, created = 0, destroyed = 0;
struct Span { uintptr_t address; size_t bytes; };
std::map<D3DKMT_HANDLE, Span> live;
void insist(bool v,const char* text) { if(!v)throw std::runtime_error(text); }
bool fail(int stage) { if(fault!=stage)return false; fault=0;return true; }
NTSTATUS APIENTRY create_pages(D3DKMT_CREATEALLOCATION* p) {
    const bool imported = p->NumAllocations && p->pAllocationInfo2[0].pSystemMem;
    if(imported) {
        unsigned flags=0;std::memcpy(&flags,&p->Flags,sizeof(flags));
        insist(!flags && !p->pStandardAllocation && !p->PrivateDriverDataSize && !p->PrivateRuntimeDataSize,
            "caller pages still use standard/shared allocation");
        for(unsigned i=0;i<p->NumAllocations;++i) {
            const auto& a=p->pAllocationInfo2[i];
            insist(a.pSystemMem && a.pPrivateDriverData && a.PrivateDriverDataSize==2376 && !a.Flags.Value,
                "invalid caller-page descriptor");
            uint64_t bytes=0;std::memcpy(&bytes,static_cast<const uint8_t*>(a.pPrivateDriverData)+0x38,8);
            insist(bytes && bytes<=(uint64_t{2}<<30) && !(bytes&65535),"caller-page part exceeds codec boundary");
            for(size_t offset:{size_t{0x750},size_t{0x758},size_t{0x760}}) {
                uint64_t address=0;std::memcpy(&address,static_cast<const uint8_t*>(a.pPrivateDriverData)+offset,8);
                insist(address==reinterpret_cast<uintptr_t>(a.pSystemMem),"caller pointer fields differ");
            }
        }
        if(fail(7))return NTSTATUS(0xc0000001u);
    }
    const auto status=D3DKMTCreateAllocation2(p);
    if(imported && status>=0)for(unsigned i=0;i<p->NumAllocations;++i) {
        const auto& a=p->pAllocationInfo2[i];uint64_t bytes=0;
        std::memcpy(&bytes,static_cast<const uint8_t*>(a.pPrivateDriverData)+0x38,8);
        insist(live.emplace(a.hAllocation,Span{reinterpret_cast<uintptr_t>(a.pSystemMem),size_t(bytes)}).second,"duplicate imported handle");
        ++created;
    }
    return status;
}
NTSTATUS APIENTRY resident_pages(D3DDDI_MAKERESIDENT* p) {
    if(fail(8))return NTSTATUS(0xc0000001u);
    return D3DKMTMakeResident(p);
}
NTSTATUS APIENTRY reserve_pages(D3DDDI_RESERVEGPUVIRTUALADDRESS* p) {
    if(fail(9))return NTSTATUS(0xc0000001u);
    return D3DKMTReserveGpuVirtualAddress(p);
}
NTSTATUS APIENTRY map_pages(D3DDDI_MAPGPUVIRTUALADDRESS* p) {
    if(live.count(p->hAllocation) && ++map_calls==2 && fail(10))return NTSTATUS(0xc0000001u);
    return D3DKMTMapGpuVirtualAddress(p);
}
NTSTATUS APIENTRY destroy_pages(const D3DKMT_DESTROYALLOCATION2* p) {
    bool tracked=false;
    for(unsigned i=0;i<p->AllocationCount;++i)tracked|=live.count(p->phAllocationList[i])!=0;
    if(tracked) {
        insist(p->Flags.SynchronousDestroy,"caller pages must unpin synchronously");
        if(fail(11))return NTSTATUS(0xc0000001u);
    }
    const auto status=D3DKMTDestroyAllocation2(p);
    if(status>=0)for(unsigned i=0;i<p->AllocationCount;++i)destroyed+=unsigned(live.erase(p->phAllocationList[i]));
    return status;
}
BOOL WINAPI free_pages(void* p,SIZE_T bytes,DWORD flags) {
    const auto address=reinterpret_cast<uintptr_t>(p);
    for(const auto& [handle,span]:live)
        insist(address<span.address || address-span.address>=span.bytes,"caller pages freed before unpin");
    return VirtualFree(p,bytes,flags);
}
struct Pages {
    size_t bytes; uint8_t* data;
    explicit Pages(size_t size):bytes(size),data(static_cast<uint8_t*>(VirtualAlloc(nullptr,size,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE))) {
        insist(data!=nullptr,"allocate caller pages");
    }
    ~Pages() { if(data)free_pages(data,0,MEM_RELEASE); }
};
uint8_t pattern(size_t i,unsigned seed) { return uint8_t(i*19u ^ (i>>12)*23u ^ (i>>28)*37u ^ seed); }
template<class F>void rejects(F f) { bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}insist(rejected,"expected failure"); }
void owned(const uint8_t* data) {
    MEMORY_BASIC_INFORMATION info{};
    insist(VirtualQuery(data,&info,sizeof(info))==sizeof(info) && info.State==MEM_COMMIT &&
        !(info.Protect&(PAGE_NOCACHE|PAGE_WRITECOMBINE|PAGE_NOACCESS|PAGE_GUARD)),"caller memory lost or cache policy changed");
}
}
#define D3DKMTCreateAllocation2 create_pages
#define D3DKMTMakeResident resident_pages
#define D3DKMTReserveGpuVirtualAddress reserve_pages
#define D3DKMTMapGpuVirtualAddress map_pages
#define D3DKMTDestroyAllocation2 destroy_pages
#define VirtualFree free_pages
#include "../src/wddm/runtime.cpp"
#undef D3DKMTCreateAllocation2
#undef D3DKMTMakeResident
#undef D3DKMTReserveGpuVirtualAddress
#undef D3DKMTMapGpuVirtualAddress
#undef D3DKMTDestroyAllocation2
#undef VirtualFree
#include "runtime_dependencies.hpp"

int main(int argc,char** argv) try {
    using namespace orca::l0::dg2;
    insist(argc==2,"pass memory.zebin");
    constexpr size_t part=size_t{2}<<30, large=2*part+65536;
    {
        Pages pages(large);
        Runtime r; r.set_memory_module(argv[1]);
        auto imported=r.import_host(pages.data,pages.bytes);
        insist(imported.imported && imported.data==pages.data && imported.bytes==pages.bytes,"import identity changed");
        insist(live.size()==3,"large import not split into three bounded parts");
        const auto output=r.allocate(6*65536);
        const size_t offsets[]={0,65529,part-35,part+33,2*part-35,2*part+33};
        const uint8_t fill=0x3c;
        for(unsigned i=0;i<6;++i)r.copy(output.gpu+size_t(i)*65536+16,imported.gpu+offsets[i],257);
        r.fill(imported.gpu+part-64,&fill,1,257);
        r.fill(imported.gpu+2*part-64,&fill,1,257);
        r.close();
        for(unsigned phase=0;phase<2;++phase) {
            for(size_t i=0;i<pages.bytes;++i)pages.data[i]=pattern(i,17+phase);
            std::memset(output.data,0xa5,output.bytes);
            r.submit();r.wait();r.cpu_access(imported,false);r.cpu_access(output,false);
            for(size_t i=0;i<output.bytes;++i) {
                const auto block=i/65536,local=i%65536;
                const auto expected=block<6 && local>=16 && local<16+257 ? pattern(offsets[block]+local-16,17+phase):uint8_t{0xa5};
                insist(static_cast<const uint8_t*>(output.data)[i]==expected,"multipart GPU read/output guard mismatch");
            }
            for(size_t i=0;i<pages.bytes;++i) {
                const bool written=(i>=part-64 && i<part-64+257)||(i>=2*part-64 && i<2*part-64+257);
                insist(pages.data[i]==(written?fill:pattern(i,17+phase)),"multipart GPU write/input guard mismatch");
            }
        }
        r.reset();r.release(imported);owned(pages.data);
        insist(live.empty(),"normal imported ownership leaked");
    }
    {
        Runtime r;auto a=r.allocate(large);
        insist(!a.imported && a.data && live.size()==3,"large owned Host fallback is not native multipart");
        for(size_t i=0;i<a.bytes;++i)static_cast<uint8_t*>(a.data)[i]=pattern(i,29);
        for(size_t i=0;i<a.bytes;++i)insist(static_cast<const uint8_t*>(a.data)[i]==pattern(i,29),"large owned Host bytes mismatch");
        r.release(a);insist(live.empty(),"large owned native Host teardown leaked");
    }
    for(int stage=7;stage<=11;++stage) {
        Pages pages(part+65536);pages.data[0]=0x53;pages.data[pages.bytes-1]=0x7a;
        {
            Runtime r;map_calls=0;
            if(stage<11) { fault=stage;rejects([&]{r.import_host(pages.data,pages.bytes);}); }
            else {auto a=r.import_host(pages.data,pages.bytes);fault=stage;rejects([&]{r.release(a);});}
            insist(!fault,"import fault not exercised");
            rejects([&]{r.allocate(65536);});
        }
        insist(live.empty(),"import fault cleanup leaked allocation");
        owned(pages.data);insist(pages.data[0]==0x53 && pages.data[pages.bytes-1]==0x7a,"fault path damaged caller pages");
    }
    insist(created==destroyed && created>=14,"native import lifecycle imbalance");
    check_runtime_dependencies(true);
    std::printf("PASS4GiB+64KiB full owned/imported bytes;three native parts;two GPU read/write replays across2GiB/4GiB with full guards;five injected import faults;created=%u destroyed=%u;caller ownership;no vendor UMD/GMM\n",created,destroyed);
} catch(const std::exception& e) {std::fprintf(stderr,"%s\n",e.what());return 1;}
