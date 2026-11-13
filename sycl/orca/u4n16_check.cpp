// Same original U4 math and real weights: SYCL writes references, both native transports compare them.
#ifdef STRATA_ORCA_NATIVE
#include <dpct/dpct.hpp>
#include "u4n16.hpp"
#else
#include <sycl/sycl.hpp>
namespace strata::l0 {
using Queue=sycl::queue;
using Event=sycl::event;
using sycl::malloc_device;
using sycl::malloc_host;
using sycl::free;
}
#include "u4n16.hpp"
#endif
#include <filesystem>
#include <iostream>

int main(int argc,char** argv) try {
    if(argc!=4)throw std::runtime_error("usage: check images reference-directory write|compare|load|invalid");
    const std::filesystem::path images(argv[1]),out(argv[2]);
    const bool write=std::string(argv[3])=="write";
#ifdef STRATA_ORCA_NATIVE
    strata::l0::Runtime runtime;
    strata::l0::Program program(runtime,images/"elementwise");
    std::ifstream manifest(images/"packs.txt");std::string name;
    if(!manifest)throw std::runtime_error("missing packs");
    while(std::getline(manifest,name))if(name!="elementwise")program.add(images/name);
    strata::l0::Queue q(runtime,program),copy(runtime,program);
    strata::l0::bind_default(q);
    auto allocate=[&](size_t n){return runtime.allocate(n,strata::l0::Memory::device);};
    auto release=[&](void* p){runtime.deallocate(p);};
#else
    sycl::queue q(sycl::gpu_selector_v,sycl::property::queue::in_order{});
    sycl::queue copy(q.get_context(),q.get_device(),sycl::property::queue::in_order{});
    auto allocate=[&](size_t n){return sycl::malloc_device(n,q);};
    auto release=[&](void* p){sycl::free(p,q);};
#endif
    if(std::string(argv[3])=="invalid") {
        model_u4::Cache cache(q,copy,8);
        auto* x=static_cast<uint16_t*>(allocate(8*2560*2));
        auto* gu=static_cast<float*>(allocate(8*1280*4));
        if(!x || !gu)throw std::bad_alloc();
        for(int mask=1;mask<=3;++mask) {
            cache.begin();q.fill(x,uint16_t(0),8*2560);q.fill(gu,0.f,8*1280);
            if(mask&1)q.fill(x,uint16_t(0x7e01),1);
            if(mask&2)q.fill(gu,std::numeric_limits<float>::quiet_NaN(),1);
            u4s4::pack<false,2>(q,x,1,cache.ax);
            grouped_u4::Batch batch;batch.add(cache.device[0],0,1,8);
            grouped_u4::pack_batch<true>(q,gu,batch,cache.ah);
            const std::string expected="U4 expert nonfinite activation: input="+
                std::to_string(mask&1?1:0)+" hidden="+std::to_string(mask&2?1:0);
            bool rejected=false;
            try {cache.end();}catch(const std::runtime_error& error){rejected=error.what()==expected;}
            u4s4::require(rejected,"missing/wrong U4 nonfinite classification");
            cache.begin();cache.end();
            std::printf("PASS U4 invalid mask=%d input=%d hidden=%d reset=1\n",mask,mask&1?1:0,mask&2?1:0);
        }
        release(gu);release(x);
        return 0;
    }
    if(std::string(argv[3])=="load") {
        model_u4::Cache cache(q,copy,512);q.wait_and_throw();
        const auto root=std::filesystem::path(std::getenv("STRATA_PREFILL_U4_EXPERT_DIR"));
        std::vector<char> bytes(1<<20);size_t checked=0;
        for(size_t i=0;i<cache.experts.size();++i) {
            const auto [l,e]=cache.experts[i];
            const auto name="layer"+std::to_string(l)+"-expert"+std::to_string(e);
            for(int part=0;part<2;++part) {
                const size_t size=part?model_u4::DOWN_BYTES:model_u4::GU_BYTES;
                std::ifstream f(root/"weights"/(name+(part?"-down.u4":"-gate_up.u4")),std::ios::binary);f.seekg(32);
                const auto* data=cache.payload(i)+(part?model_u4::GU_BYTES:0);
                for(size_t off=0;off<size;) {
                    const size_t n=std::min(bytes.size(),size-off);f.read(bytes.data(),n);
                    if(!f || std::memcmp(bytes.data(),data+off,n))throw std::runtime_error("loaded U4 payload byte mismatch");
                    checked+=n;off+=n;
                }
            }
        }
        std::printf("PASS U4 parallel load experts=%zu bytes=%zu complete_file_compare=1\n",cache.experts.size(),checked);
        return 0;
    }
    constexpr int T=271,CAP=512,G=32;
    auto* x=static_cast<uint16_t*>(allocate((T*2560+2*G)*2));
    auto* ids=static_cast<int32_t*>(allocate((CAP+2*G)*4));
    auto* gu=static_cast<float*>(allocate((CAP*1280+2*G)*4));
    auto* y=static_cast<float*>(allocate((CAP*2560+2*G)*4));
    if(!x||!ids||!gu||!y)throw std::bad_alloc();
    std::vector<uint16_t> hx(T*2560+2*G,0x7e01);
    for(int i=0;i<T*2560;++i)hx[G+i]=sycl::bit_cast<uint16_t>(sycl::half(float((i*17+i/2560*3)%29-14)/32.f));
    std::vector<int32_t> hi(CAP+2*G,-12345);
    for(int i=0;i<CAP;++i)hi[G+i]=(i*37+11)%T;
    q.memcpy(x,hx.data(),hx.size()*2);q.memcpy(ids,hi.data(),hi.size()*4);
    size_t outputs=0;
    {
        model_u4::Cache cache(q,copy,CAP);
        for(int r:{1,4,7,8,9,17,127,128,129}) {
            cache.begin();q.fill(gu,-12345.f,CAP*1280+2*G);q.fill(y,-12345.f,CAP*2560+2*G);
            u4s4::pack<false,2>(q,x+G,T,cache.ax);
            // Reuse both banks, including shrinking/growing groups and a one-row tail in the second expert.
            for(int iteration=0;iteration<4;++iteration) {
                const int bank=iteration%2*8;
                grouped_u4::Batch batch;
                for(int j=0;j<2;++j) {
                    const auto [layer,expert]=cache.experts[j];
                    cache.stage(layer,expert,bank+j);
                    batch.add(cache.device[bank+j],j*(r+1),r,CAP);
                }
                cache.flush(bank,2);
                shared_u4::run(cache,ids+G,gu+G,y+G,batch,bank);
            }
            cache.end();cache.ax.check();cache.ah.check();q.wait_and_throw();
            for(auto [buffer,n]:{std::pair{gu,size_t(CAP*1280+2*G)},std::pair{y,size_t(CAP*2560+2*G)}}) {
                std::vector<float> actual(n);q.memcpy(actual.data(),buffer,n*4).wait_and_throw();
                const int cols=buffer==gu?1280:2560;
                for(size_t i=0;i<n;++i) {
                    const int row=i>=G?int((i-G)/cols):-1;
                    const bool active=i>=G && i<n-G && (row<r || (row>=r+1 && row<2*r+1));
                    if(active){if(!std::isfinite(actual[i]))throw std::runtime_error("nonfinite output");++outputs;}
                    else if(actual[i]!=-12345.f)throw std::runtime_error("output guard/routed gap changed");
                }
                const auto path=out/(std::to_string(r)+(buffer==gu?"-gu.f32":"-down.f32"));
                if(write) {
                    std::ofstream f(path,std::ios::binary|std::ios::trunc);f.write((char*)actual.data(),n*4);
                    if(!f)throw std::runtime_error("cannot write reference");
                } else {
                    std::vector<float> expected(n);std::ifstream f(path,std::ios::binary);
                    f.read((char*)expected.data(),n*4);
                    if(!f || std::memcmp(actual.data(),expected.data(),n*4))throw std::runtime_error("complete U4 output differs from SYCL reference");
                }
            }
            std::vector<uint16_t> rx(hx.size());std::vector<int32_t> ri(hi.size());
            q.memcpy(rx.data(),x,rx.size()*2);q.memcpy(ri.data(),ids,ri.size()*4);q.wait_and_throw();
            if(rx!=hx || ri!=hi)throw std::runtime_error("input/IDs/guards changed");
            std::printf("PASS rows=%d banks=4 full_output_and_guards=1\n",r);
        }
    }
    release(y);release(gu);release(ids);release(x);
    std::printf("PASS U4 N16 complete outputs=%zu cases=9 copies=72 reference=%s\n",outputs,write?"written":"SYCL-byte-exact");
    return 0;
} catch(const std::exception& e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}
