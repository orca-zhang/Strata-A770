#include <sycl/sycl.hpp>
#include <dpct/dpct.hpp>
#include "strata/prefill/gemm.hpp"
#include "strata/prefill/kernels.hpp"
#include "strata/prefill/fp16_pair.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <vector>

int main(int argc, char** argv) try {
    const bool bench=argc==2 && std::strcmp(argv[1],"--bench")==0;
    if (argc>1 && !bench) throw std::invalid_argument("expected --bench");
    const bool candidate=std::getenv("STRATA_PREFILL_XMX_EXPERT_CHAIN") &&
        std::strcmp(std::getenv("STRATA_PREFILL_XMX_EXPERT_CHAIN"),"1")==0;
    const bool batch=std::getenv("STRATA_PREFILL_XMX_EXPERT_BATCH") &&
        std::strcmp(std::getenv("STRATA_PREFILL_XMX_EXPERT_BATCH"),"1")==0;
    auto& q=dpct::get_in_order_queue();
    std::printf("DEVICE %s\n",q.get_device().get_info<sycl::info::device::name>().c_str());
    if (q.get_device().get_info<sycl::info::device::name>().find("A770")==std::string::npos) {
        std::puts("SKIP A770-specific expert-chain calibration"); return 77;
    }
    constexpr int N=2560,F=640,G=1280,BANKS=8,GUARD=16;
    constexpr size_t WG=size_t(G)*N,WD=size_t(N)*F;
    auto* ws=sycl::malloc_device<unsigned char>(128u<<20,q);
    auto* wg=sycl::malloc_device<uint16_t>(BANKS*WG,q);
    auto* wd=sycl::malloc_device<uint16_t>(BANKS*WD,q);
    if (!ws || !wg || !wd) throw std::bad_alloc();
    strata::prefill::Gemm gm;
    std::string error;
    if (!gm.init_external(&q,nullptr,0,ws,128u<<20,error)) throw std::runtime_error(error);
    q.parallel_for(sycl::range<1>(BANKS*WG),[=](sycl::id<1> i) {
        const float v=float(int((i[0]*17+3)%37)-18)/128;
        wg[i]=sycl::bit_cast<uint16_t>(sycl::half(v));
    });
    q.parallel_for(sycl::range<1>(BANKS*WD),[=](sycl::id<1> i) {
        const float v=float(int((i[0]*13+7)%31)-15)/128;
        wd[i]=sycl::bit_cast<uint16_t>(sycl::half(v));
    });
    int checks=0;
    for (int t : {1,2,4,6,7,8,9,16,22,32,40,64,65,128,256}) {
        const size_t cg=size_t(t)*G,ch=size_t(t)*F,cy=size_t(t)*N;
        auto* x=sycl::malloc_device<uint16_t>(size_t(t)*N,q);
        auto* gu0=sycl::malloc_device<float>(cg+2*GUARD,q);
        auto* h0=sycl::malloc_device<uint16_t>(ch+2*GUARD,q);
        auto* y0=sycl::malloc_device<float>(cy+2*GUARD,q);
        auto* gu1=sycl::malloc_device<float>(cg+2*GUARD,q);
        auto* h1=sycl::malloc_device<uint16_t>(ch+2*GUARD,q);
        auto* y1=sycl::malloc_device<float>(cy+2*GUARD,q);
        if (!x || !gu0 || !h0 || !y0 || !gu1 || !h1 || !y1) throw std::bad_alloc();
        q.parallel_for(sycl::range<1>(size_t(t)*N),[=](sycl::id<1> i) {
            x[i]=sycl::bit_cast<uint16_t>(sycl::half(float(int((i[0]*11+5)%29)-14)/32));
        });
        for (auto* p : {gu0,gu1}) q.fill(p,-12345.f,cg+2*GUARD);
        for (auto* p : {h0,h1}) q.fill(p,uint16_t(0x55aa),ch+2*GUARD);
        for (auto* p : {y0,y1}) q.fill(p,-12345.f,cy+2*GUARD);
        auto original=[&](int bank,float* gu,uint16_t* h,float* y,int rows=-1,const uint16_t* input=nullptr) {
            const int count=rows<0?t:rows;
            gm.f16(input?input:x,wg+size_t(bank)*WG,gu+GUARD,count,G,N);
            strata::prefill::swiglu_interleaved(gu+GUARD,h+GUARD,count,&q);
            gm.f16(h+GUARD,wd+size_t(bank)*WD,y+GUARD,count,N,F);
        };
        auto chain=[&](int bank, bool finish=true) {
            if (!gm.f16_expert(x,wg+size_t(bank)*WG,wd+size_t(bank)*WD,gu1+GUARD,h1+GUARD,y1+GUARD,t,finish))
                throw std::runtime_error("expected chain dispatch");
        };
        uint64_t hash=14695981039346656037ull;
        for (int bank=0;bank<BANKS;++bank) {
            original(bank,gu0,h0,y0); chain(bank);
            for (auto pointers : {std::pair{gu0,gu1},std::pair{y0,y1}}) {
                const size_t count=pointers.first==gu0?cg:cy;
                std::vector<float> a(count+2*GUARD),b(a.size());
                q.memcpy(a.data(),pointers.first,a.size()*4);
                q.memcpy(b.data(),pointers.second,b.size()*4).wait_and_throw();
                if (std::memcmp(a.data(),b.data(),a.size()*4)) throw std::runtime_error("full FP32 output mismatch");
                for (size_t i=0;i<a.size();++i) {
                    if (!std::isfinite(a[i])) throw std::runtime_error("nonfinite output");
                    if ((i<GUARD || i>=GUARD+count) && a[i]!=-12345.f) throw std::runtime_error("FP32 guard");
                    const uint32_t bits=sycl::bit_cast<uint32_t>(a[i]);
                    hash=(hash^bits)*1099511628211ull;
                }
                ++checks;
            }
            std::vector<uint16_t> a(ch+2*GUARD),b(a.size());
            q.memcpy(a.data(),h0,a.size()*2);q.memcpy(b.data(),h1,b.size()*2).wait_and_throw();
            if (a!=b) throw std::runtime_error("full H mismatch");
            for (size_t i=0;i<a.size();++i)
                if ((i<GUARD || i>=GUARD+ch) && a[i]!=0x55aa) throw std::runtime_error("H guard");
            ++checks;
        }
        // Distinct weights/inputs and unequal rows share the existing GEMM workspace.
        const int second_rows=std::max(1,t-1);
        const auto* second_x=t>1?x+N:x;
        auto reset_pair=[&] {
            for (auto* p : {gu0,gu1}) q.fill(p,-12345.f,cg+2*GUARD);
            for (auto* p : {h0,h1}) q.fill(p,uint16_t(0x55aa),ch+2*GUARD);
            for (auto* p : {y0,y1}) q.fill(p,-12345.f,cy+2*GUARD);
        };
        std::vector<float> reference_g[2],reference_y[2];
        std::vector<uint16_t> reference_h[2];
        auto pair_outputs=[&](bool save) {
            int bank=0;
            for (auto pointers : {std::pair{gu0,y0},std::pair{gu1,y1}}) {
                std::vector<float> g(cg+2*GUARD),y(cy+2*GUARD);
                std::vector<uint16_t> h(ch+2*GUARD);
                q.memcpy(g.data(),pointers.first,g.size()*4);
                q.memcpy(y.data(),pointers.second,y.size()*4);
                q.memcpy(h.data(),bank?h1:h0,h.size()*2).wait_and_throw();
                if (save) {reference_g[bank]=g;reference_y[bank]=y;reference_h[bank]=h;}
                else {
                    if (std::memcmp(g.data(),reference_g[bank].data(),g.size()*4) ||
                        std::memcmp(y.data(),reference_y[bank].data(),y.size()*4) || h!=reference_h[bank])
                        throw std::runtime_error("FP16 pair complete output/guard/refusal mismatch");
                    checks+=3;
                }
                ++bank;
            }
        };
        reset_pair();
        original(0,gu0,h0,y0);
        original(1,gu1,h1,y1,second_rows,second_x);
        pair_outputs(true);
        reset_pair();
        const fp16_pair::Expert first{x,wg,wd,gu0+GUARD,h0+GUARD,y0+GUARD,t};
        const fp16_pair::Expert second{second_x,wg+WG,wd+WD,gu1+GUARD,h1+GUARD,y1+GUARD,second_rows};
        constexpr size_t pair_bytes=2*8*N*sizeof(uint16_t);
        auto* padding=reinterpret_cast<uint16_t*>(ws)+GUARD;
        q.fill(reinterpret_cast<uint16_t*>(ws),uint16_t(0x55aa),pair_bytes/2+2*GUARD);
        if (!fp16_pair::execute(q,first,second,padding,pair_bytes)) throw std::runtime_error("FP16 pair dispatch");
        pair_outputs(false);
        std::vector<uint16_t> pad(pair_bytes/2+2*GUARD);
        q.memcpy(pad.data(),ws,pad.size()*2).wait_and_throw();
        for (size_t i=0;i<pad.size();++i)
            if ((i<GUARD || i>=GUARD+pair_bytes/2) && pad[i]!=0x55aa)
                throw std::runtime_error("FP16 pair workspace guard");
        for (int invalid : {0,1,2,3,4}) {
            reset_pair();pair_outputs(true);
            auto rejected=first;
            if (invalid==0) rejected.rows=0;
            if (invalid==1) rejected.x=nullptr;
            if (invalid==4) rejected.down_weight=nullptr;
            if (fp16_pair::execute(q,rejected,second,invalid==2?nullptr:padding,
                                   invalid==3?pair_bytes-1:pair_bytes))
                throw std::runtime_error("invalid FP16 pair accepted");
            pair_outputs(false);
        }
        auto launch=[&](int i) {
            if (candidate) chain(i%BANKS,!batch || (i+1)%8==0);
            else original(i%BANKS,gu1,h1,y1);
        };
        std::vector<double> times;
        if (bench) {
            for (int i=0;i<16;++i) launch(i);
            for (int batch=0;batch<7;++batch) {
                const auto begin=std::chrono::steady_clock::now();
                for (int i=0;i<16;++i) launch(i);
                q.wait_and_throw();
                times.push_back(std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-begin).count()/16);
            }
            std::sort(times.begin(),times.end());
        }
        std::printf("CASE {\"T\":%d,\"us\":%.6f,\"hash\":\"%016llx\",\"weight_banks\":8}\n",
            t,bench?times[3]:0.,(unsigned long long)hash);
        std::fflush(stdout);
        sycl::free(y1,q);sycl::free(h1,q);sycl::free(gu1,q);
        sycl::free(y0,q);sycl::free(h0,q);sycl::free(gu0,q);sycl::free(x,q);
    }
    // Changed weights/inputs and exact/short/moved workspaces; refusal must enqueue no output writes.
    for (int t : {0,7,8,9}) {
        const size_t required=t%8?size_t(8)*N*2:0;
        const size_t capacity=std::max(required,size_t(128));
        auto* a=sycl::malloc_device<uint16_t>(capacity/2+2*GUARD,q);
        auto* b=sycl::malloc_device<uint16_t>(capacity/2+2*GUARD,q);
        auto* x=sycl::malloc_device<uint16_t>(size_t(std::max(t,1))*N,q);
        auto* gu=sycl::malloc_device<float>(size_t(t)*G+2*GUARD,q);
        auto* h=sycl::malloc_device<uint16_t>(size_t(t)*F+2*GUARD,q);
        auto* y=sycl::malloc_device<float>(size_t(t)*N+2*GUARD,q);
        if (!a || !b || !x || !gu || !h || !y) throw std::bad_alloc();
        int binding=0;
        for (size_t bytes : {size_t(0),required?required-1:0,required,required+1,required}) {
            auto* selected=(binding++%2)?a:b;
            for (auto* p : {a,b}) q.fill(p,uint16_t(0x55aa),capacity/2+2*GUARD);
            q.fill(gu,-12345.f,size_t(t)*G+2*GUARD);q.fill(y,-12345.f,size_t(t)*N+2*GUARD);
            q.fill(h,uint16_t(0x55aa),size_t(t)*F+2*GUARD);
            q.fill(x,sycl::bit_cast<uint16_t>(sycl::half(.25f)),size_t(std::max(t,1))*N);
            q.fill(wg,sycl::bit_cast<uint16_t>(sycl::half(float(binding)/1024)),WG);
            q.fill(wd,sycl::bit_cast<uint16_t>(sycl::half(.0625f)),WD);
            gm.rebind(nullptr,0,bytes?selected+GUARD:nullptr,bytes);
            const bool used=gm.f16_expert(x,wg,wd,gu+GUARD,h+GUARD,y+GUARD,t);
            if (used!=(t>0 && (!required || bytes>=required))) throw std::runtime_error("capacity dispatch");
            std::vector<float> host(size_t(t)*N+2*GUARD);
            std::vector<float> host_gu(size_t(t)*G+2*GUARD);
            std::vector<uint16_t> host_h(size_t(t)*F+2*GUARD);
            q.memcpy(host.data(),y,host.size()*4);q.memcpy(host_gu.data(),gu,host_gu.size()*4);
            q.memcpy(host_h.data(),h,host_h.size()*2).wait_and_throw();
            for (size_t i=0;i<host_gu.size();++i)
                if ((!used || i<GUARD || i>=GUARD+size_t(t)*G) && host_gu[i]!=-12345.f)
                    throw std::runtime_error("refusal/GU guard");
            for (size_t i=0;i<host_h.size();++i)
                if ((!used || i<GUARD || i>=GUARD+size_t(t)*F) && host_h[i]!=0x55aa)
                    throw std::runtime_error("refusal/H guard");
            for (size_t i=0;i<host.size();++i) {
                if ((!used || i<GUARD || i>=GUARD+size_t(t)*N) && host[i]!=-12345.f)
                    throw std::runtime_error("refusal/Y guard");
                if (used && i>=GUARD && i<GUARD+size_t(t)*N && !std::isfinite(host[i]))
                    throw std::runtime_error("nonfinite rebound output");
            }
            if (used) {
                gm.rebind(nullptr,0,ws,128u<<20);
                gm.f16(x,wg,gu+GUARD,t,G,N);
                strata::prefill::swiglu_interleaved(gu+GUARD,h+GUARD,t,&q);
                gm.f16(h+GUARD,wd,y+GUARD,t,N,F);
                std::vector<float> reference(host.size());
                q.memcpy(reference.data(),y,reference.size()*4).wait_and_throw();
                if (reference!=host) throw std::runtime_error("rebound result");
            }
            for (auto* buffer : {a,b}) {
                std::vector<uint16_t> contents(capacity/2+2*GUARD);
                q.memcpy(contents.data(),buffer,contents.size()*2).wait_and_throw();
                const size_t allowed=buffer==selected?bytes/2:0;
                for (size_t i=0;i<contents.size();++i)
                    if ((i<GUARD || i>=GUARD+allowed) && contents[i]!=0x55aa) throw std::runtime_error("workspace guard");
            }
            ++checks;
        }
        gm.rebind(nullptr,0,ws,128u<<20);
        sycl::free(y,q);sycl::free(h,q);sycl::free(gu,q);sycl::free(x,q);sycl::free(b,q);sycl::free(a,q);
    }

    // Mixed shapes, distinct outputs, one padding workspace and two reused dequant buffers.
    // Queue copies stand in for dequant writes, so reuse before the previous expert completes is checked.
    constexpr int MAXT=128,MAXCALLS=65;
    const int shapes[]={1,7,8,9,2,16,65,128,6};
    const size_t gx=size_t(MAXT)*N,gg=size_t(MAXT)*G+2*GUARD;
    const size_t gh=size_t(MAXT)*F+2*GUARD,gy=gx+2*GUARD;
    auto* bx=sycl::malloc_device<uint16_t>(gx,q);
    auto* bg=sycl::malloc_device<float>(MAXCALLS*gg,q);
    auto* bh=sycl::malloc_device<uint16_t>(MAXCALLS*gh,q);
    auto* by=sycl::malloc_device<float>(MAXCALLS*gy,q);
    auto* dg=sycl::malloc_device<uint16_t>(2*WG,q);
    auto* dd=sycl::malloc_device<uint16_t>(2*WD,q);
    if (!bx || !bg || !bh || !by || !dg || !dd) throw std::bad_alloc();
    for (int calls : {1,2,3,4,7,8,9,16,17,31,32,33,64,65}) {
        std::vector<float> reference_g(calls*gg),reference_y(calls*gy);
        std::vector<uint16_t> reference_h(calls*gh);
        for (int group : {1,8,32}) {
            q.fill(bg,-12345.f,calls*gg);q.fill(by,-12345.f,calls*gy);
            q.fill(bh,uint16_t(0x55aa),calls*gh);
            for (int i=0;i<calls;++i) {
                const int t=shapes[i%9],slot=i%2,bank=i%BANKS;
                q.parallel_for(sycl::range<1>(gx),[=](sycl::id<1> j) {
                    bx[j]=sycl::bit_cast<uint16_t>(sycl::half(float(int((j[0]*11+i*3)%29)-14)/32));
                });
                q.memcpy(dg+slot*WG,wg+bank*WG,WG*2);
                q.memcpy(dd+slot*WD,wd+bank*WD,WD*2);
                if (!gm.f16_expert(bx,dg+slot*WG,dd+slot*WD,bg+i*gg+GUARD,bh+i*gh+GUARD,by+i*gy+GUARD,
                                  t,(i+1)%group==0 || i+1==calls))
                    throw std::runtime_error("mixed batch dispatch");
            }
            q.wait_and_throw();
            std::vector<float> host_g(calls*gg),host_y(calls*gy);
            std::vector<uint16_t> host_h(calls*gh);
            q.memcpy(host_g.data(),bg,host_g.size()*4);q.memcpy(host_y.data(),by,host_y.size()*4);
            q.memcpy(host_h.data(),bh,host_h.size()*2).wait_and_throw();
            for (int i=0;i<calls;++i) {
                const size_t t=shapes[i%9];
                for (size_t j=0;j<gg;++j)
                    if ((j<GUARD || j>=GUARD+t*G) && host_g[i*gg+j]!=-12345.f)
                        throw std::runtime_error("mixed GU guard");
                for (size_t j=0;j<gy;++j)
                    if ((j<GUARD || j>=GUARD+t*N) && host_y[i*gy+j]!=-12345.f)
                        throw std::runtime_error("mixed Y guard");
                for (size_t j=0;j<gh;++j)
                    if ((j<GUARD || j>=GUARD+t*F) && host_h[i*gh+j]!=0x55aa)
                        throw std::runtime_error("mixed H guard");
            }
            if (group==1) {reference_g=host_g;reference_y=host_y;reference_h=host_h;}
            else {
                if (std::memcmp(reference_g.data(),host_g.data(),host_g.size()*4) ||
                    std::memcmp(reference_y.data(),host_y.data(),host_y.size()*4) || reference_h!=host_h)
                    throw std::runtime_error("mixed batch full output mismatch");
                checks+=3;
            }
        }
    }
    sycl::free(dd,q);sycl::free(dg,q);sycl::free(by,q);sycl::free(bh,q);sycl::free(bg,q);sycl::free(bx,q);
    sycl::free(wd,q);sycl::free(wg,q);sycl::free(ws,q);
    std::printf("SUMMARY {\"checks\":%d}\n",checks);
} catch (const std::exception& e) { std::fprintf(stderr,"FAIL %s\n",e.what());return 1; }
