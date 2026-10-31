#include <sycl/sycl.hpp>
#include <dpct/dpct.hpp>
#include "strata/kernels/iq_kernels.hpp"
#include "strata/prefill/gemm.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <stdexcept>
#include <vector>

namespace k=strata::kernels;
constexpr int GUARD=32,N=2560,F=640,BANKS=8;
constexpr size_t WG=size_t(2)*N*F,WD=size_t(N)*F;
int checks=0;

std::vector<uint8_t> weights(int ty,size_t n,std::mt19937& rng) {
    const size_t bs=k::iq_row_bytes(ty,ty==42?64:256);
    std::vector<uint8_t> v(k::iq_row_bytes(ty,n));
    for (auto& b:v) b=uint8_t(rng());
    for (size_t i=0;i<v.size();i+=bs) {
        const uint16_t h=uint16_t(((2+rng()%5)<<10)|(rng()%1024)|((rng()%4==0)?0x8000:0));
        std::memcpy(v.data()+i,&h,2);
    }
    return v;
}
uint64_t mix(uint64_t h,const void* p,size_t bytes) {
    auto* b=(const uint8_t*)p;
    for (size_t i=0;i<bytes;++i) h=(h^b[i])*1099511628211ull;
    return h;
}
template<class T> std::vector<T> read(sycl::queue& q,T* p,size_t n) {
    std::vector<T> v(n);q.memcpy(v.data(),p,n*sizeof(T)).wait_and_throw();return v;
}
template<class T> void equal(const std::vector<T>& a,const std::vector<T>& b) {
    if (a.size()!=b.size() || std::memcmp(a.data(),b.data(),a.size()*sizeof(T)))
        throw std::runtime_error("full byte comparison");
    ++checks;
}
void dq(sycl::queue& q,bool fused,int gt,int dt,const uint8_t* raw,int nf,int ne,uint16_t* gu,uint16_t* d) {
    const size_t half=k::iq_row_bytes(gt,size_t(nf)*ne);
    if (fused) {
        if (!k::iq_dequant_expert_f16(gt,dt,raw,raw+half,raw+2*half,nf,ne,gu,d,&q))
            throw std::runtime_error("expected fused dispatch");
    } else {
        k::iq_dequant_gu_f16(gt,raw,raw+half,nf,ne,gu,&q);
        k::iq_dequant_f16(dt,raw+2*half,size_t(nf)*ne,d,&q);
    }
}
int main(int argc,char** argv) try {
    const bool bench=argc==2 && std::strcmp(argv[1],"--bench")==0;
    if (argc>1 && !bench) throw std::invalid_argument("expected --bench");
    const bool enabled=std::getenv("STRATA_PREFILL_FUSED_DEQUANT") &&
        std::strcmp(std::getenv("STRATA_PREFILL_FUSED_DEQUANT"),"1")==0;
    auto& q=dpct::get_in_order_queue();
    const auto name=q.get_device().get_info<sycl::info::device::name>();
    std::printf("DEVICE %s\n",name.c_str());
    if (name.find("A770")==std::string::npos) return 77;
    struct PolicyCase {int gt,dt;int64_t tokens,nf,ne;bool expected;};
    for (const auto& v:std::vector<PolicyCase>{
        {16,42,1,640,2560,true},{16,42,16,640,2560,true},{16,42,17,640,2560,false},
        {22,42,1,640,2560,true},{22,42,9,640,2560,true},{22,42,10,640,2560,false},
        {17,42,1,640,2560,false},{29,42,1,640,2560,false},{42,42,1,640,2560,false},
        {16,16,1,640,2560,false},{16,42,0,640,2560,false},{16,42,-1,640,2560,false},
        {16,42,INT64_MAX,640,2560,false},{16,42,8,639,2560,false},{16,42,8,640,2559,false},
        {16,42,8,641,2560,false},{22,42,9,640,2561,false},{22,42,0,640,2560,false}}) {
        if (k::iq_fused_dequant_eligible(v.gt,v.dt,v.tokens,v.nf,v.ne)!=v.expected)
            throw std::runtime_error("dispatch boundary");
        ++checks;
    }
    std::mt19937 rng(41024);
    // Both supported format pairs, interleaving boundaries and complete guards.
    for (int ne:{256,768,2560}) for (int nf:{1,3,640})
    for (int gt:{16,22}) for (int dt:{42}) {
        const size_t n=size_t(nf)*ne;
        auto a=weights(gt,2*n,rng),b=weights(dt,n,rng);a.insert(a.end(),b.begin(),b.end());
        auto* raw=sycl::malloc_device<uint8_t>(a.size(),q);
        auto* gu=sycl::malloc_device<uint16_t>(2*n+2*GUARD,q);
        auto* d=sycl::malloc_device<uint16_t>(n+2*GUARD,q);
        if (!raw || !gu || !d) throw std::bad_alloc();
        q.memcpy(raw,a.data(),a.size());
        q.fill(gu,uint16_t(0x55aa),2*n+2*GUARD);q.fill(d,uint16_t(0x55aa),n+2*GUARD);
        dq(q,false,gt,dt,raw,nf,ne,gu+GUARD,d+GUARD);
        auto rg=read(q,gu,2*n+2*GUARD),rd=read(q,d,n+2*GUARD);
        q.fill(gu,uint16_t(0x55aa),2*n+2*GUARD);q.fill(d,uint16_t(0x55aa),n+2*GUARD);
        dq(q,true,gt,dt,raw,nf,ne,gu+GUARD,d+GUARD);
        auto fg=read(q,gu,rg.size()),fd=read(q,d,rd.size());equal(rg,fg);equal(rd,fd);
        for (int i=0;i<GUARD;++i)
            if (fg[i]!=0x55aa || fd[i]!=0x55aa || fg[fg.size()-1-i]!=0x55aa || fd[fd.size()-1-i]!=0x55aa)
                throw std::runtime_error("dequant guard");
        for (size_t i=GUARD;i<fg.size()-GUARD;++i)
            if ((fg[i]&0x7c00)==0x7c00) throw std::runtime_error("nonfinite dequant GU");
        for (size_t i=GUARD;i<fd.size()-GUARD;++i)
            if ((fd[i]&0x7c00)==0x7c00) throw std::runtime_error("nonfinite dequant D");
        sycl::free(d,q);sycl::free(gu,q);sycl::free(raw,q);
    }
    for (auto dims:{std::pair<int64_t,int64_t>{0,256},{1,0},{-1,256},{1,255},{INT64_MAX,256},{INT64_MAX/256,256}}) {
        if (k::iq_dequant_expert_f16(16,42,nullptr,nullptr,nullptr,dims.first,dims.second,nullptr,nullptr,&q))
            throw std::runtime_error("invalid dimensions accepted");
        ++checks;
    }
    for (auto types:{std::pair{0,42},{16,0},{30,42},{16,30},{11,42},{16,20},{17,42},{42,42},{16,16},{22,22}}) {
        if (k::iq_dequant_expert_f16(types.first,types.second,nullptr,nullptr,nullptr,640,2560,nullptr,nullptr,&q))
            throw std::runtime_error("unsupported pair accepted");
        ++checks;
    }
    auto* ws=sycl::malloc_device<uint8_t>(128u<<20,q);
    auto* gu=sycl::malloc_device<uint16_t>(2*WG,q);
    auto* d=sycl::malloc_device<uint16_t>(2*WD,q);
    auto* x=sycl::malloc_device<uint16_t>(256*N,q);
    auto* outg=sycl::malloc_device<float>(256*2*F+2*GUARD,q);
    auto* outh=sycl::malloc_device<uint16_t>(256*F+2*GUARD,q);
    auto* outy=sycl::malloc_device<float>(256*N+2*GUARD,q);
    if (!ws || !gu || !d || !x || !outg || !outh || !outy) throw std::bad_alloc();
    q.parallel_for(sycl::range<1>(256*N),[=](sycl::id<1> i) {
        x[i]=sycl::bit_cast<uint16_t>(sycl::half(float(int((i[0]*11+5)%29)-14)/32));
    });
    strata::prefill::Gemm gm;std::string err;
    if (!gm.init_external(&q,nullptr,0,ws,128u<<20,err)) throw std::runtime_error(err);
    for (int gt:{16,22}) {
        const int dt=42;
        std::vector<uint8_t> host;
        for (int i=0;i<BANKS;++i) {
            auto a=weights(gt,WG,rng),b=weights(dt,WD,rng);
            host.insert(host.end(),a.begin(),a.end());host.insert(host.end(),b.begin(),b.end());
        }
        const size_t stride=host.size()/BANKS;
        auto* raw=sycl::malloc_device<uint8_t>(host.size(),q);
        if (!raw) throw std::bad_alloc();
        q.memcpy(raw,host.data(),host.size()).wait_and_throw();
        for (int t:{1,2,4,6,7,8,9,10,16,17,22,32,40,64,65,128,256}) {
            const size_t ng=size_t(t)*2*F+2*GUARD,nh=size_t(t)*F+2*GUARD,ny=size_t(t)*N+2*GUARD;
            auto launch=[&](int i,bool fused,bool chain,bool finish) {
                const int slot=i%2;
                dq(q,fused && k::iq_fused_dequant_eligible(gt,dt,t,F,N),gt,dt,raw+(i%BANKS)*stride,F,N,gu+slot*WG,d+slot*WD);
                if (chain && !gm.f16_expert(x,gu+slot*WG,d+slot*WD,outg+GUARD,outh+GUARD,outy+GUARD,t,finish))
                    throw std::runtime_error("expected expert chain");
            };
            uint64_t hash=14695981039346656037ull;
            for (int i=0;i<BANKS;++i) {
                q.fill(outg,-12345.f,ng);q.fill(outh,uint16_t(0x55aa),nh);q.fill(outy,-12345.f,ny);
                launch(i,false,true,true);
                auto rg=read(q,outg,ng),ry=read(q,outy,ny);auto rh=read(q,outh,nh);
                launch(i,true,true,true);
                auto fg=read(q,outg,ng),fy=read(q,outy,ny);auto fh=read(q,outh,nh);
                equal(rg,fg);equal(ry,fy);equal(rh,fh);
                for (int j=0;j<GUARD;++j)
                    if (fg[j]!=-12345.f || fy[j]!=-12345.f || fh[j]!=0x55aa ||
                        fg[ng-1-j]!=-12345.f || fy[ny-1-j]!=-12345.f || fh[nh-1-j]!=0x55aa)
                        throw std::runtime_error("chain guard");
                for (float f:fy) if (!std::isfinite(f)) throw std::runtime_error("nonfinite chain");
                hash=mix(hash,fg.data(),fg.size()*4);hash=mix(hash,fy.data(),fy.size()*4);
                hash=mix(hash,fh.data(),fh.size()*2);
            }
            if (bench) {
                for (bool fused:{false,true}) for (int i=0;i<128;++i)
                    launch(i,fused,true,(i+1)%8==0);
                q.wait_and_throw();
                for (int cycle=0;cycle<12;++cycle) {
                    double us[4];
                    for (int arm=0;arm<4;++arm) {
                        const bool fused=arm==1 || arm==2;
                        const auto start=std::chrono::steady_clock::now();
                        for (int i=0;i<128;++i) launch(i,fused,true,(i+1)%8==0);
                        q.wait_and_throw();
                        us[arm]=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()/128;
                    }
                    std::printf("PAIR {\"gu\":%d,\"down\":42,\"T\":%d,\"cycle\":%d,\"a0_us\":%.6f,\"b0_us\":%.6f,\"b1_us\":%.6f,\"a1_us\":%.6f}\n",
                        gt,t,cycle,us[0],us[1],us[2],us[3]);
                }
            }
            std::printf("CASE {\"gu\":%d,\"down\":42,\"T\":%d,\"hash\":\"%016llx\"}\n",gt,t,(unsigned long long)hash);
            std::fflush(stdout);
        }
        // Alternate two dequant buffers through mixed shapes and unfinished batches.
        for (int calls:{1,2,3,7,8,9,16,17}) {
            constexpr size_t cg=128*2*F+2*GUARD,ch=128*F+2*GUARD,cy=128*N+2*GUARD;
            auto* bg=sycl::malloc_device<float>(calls*cg,q);
            auto* bh=sycl::malloc_device<uint16_t>(calls*ch,q);
            auto* by=sycl::malloc_device<float>(calls*cy,q);
            if (!bg || !bh || !by) throw std::bad_alloc();
            std::vector<float> rg,ry;std::vector<uint16_t> rh;
            const int shapes[]={1,7,8,9,10,16,17,65,128};
            for (bool fused:{false,true}) {
                q.fill(bg,-12345.f,calls*cg);q.fill(by,-12345.f,calls*cy);
                q.fill(bh,uint16_t(0x55aa),calls*ch);
                for (int i=0;i<calls;++i) {
                    const int slot=i%2,t=shapes[i%9];
                    dq(q,fused && k::iq_fused_dequant_eligible(gt,dt,t,F,N),gt,dt,raw+(i%BANKS)*stride,F,N,gu+slot*WG,d+slot*WD);
                    if (!gm.f16_expert(x,gu+slot*WG,d+slot*WD,bg+i*cg+GUARD,bh+i*ch+GUARD,by+i*cy+GUARD,
                                      t,!fused || (i+1)%8==0 || i+1==calls))
                        throw std::runtime_error("mixed expert dispatch");
                }
                auto fg=read(q,bg,calls*cg),fy=read(q,by,calls*cy);auto fh=read(q,bh,calls*ch);
                if (!fused) {rg=fg;ry=fy;rh=fh;}
                else {equal(rg,fg);equal(ry,fy);equal(rh,fh);}
            }
            sycl::free(by,q);sycl::free(bh,q);sycl::free(bg,q);
        }
        sycl::free(raw,q);
    }
    sycl::free(outy,q);sycl::free(outh,q);sycl::free(outg,q);sycl::free(x,q);
    sycl::free(d,q);sycl::free(gu,q);sycl::free(ws,q);
    std::printf("SUMMARY {\"checks\":%d}\n",checks);
} catch (const std::exception& e) {std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}
