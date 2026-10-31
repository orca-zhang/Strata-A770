#define NOMINMAX
#include <windows.h>
#include <sycl/sycl.hpp>
#include <dpct/dpct.hpp>
#include "strata/kernels/verify_kernels.hpp"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace strata::kernels {
void gdn_step_norm_multi_baseline(float*,const float*,int,const float*,const float*,const float*,const float*,float,float*,int,int,int,const int32_t*,void*,int,void*);
void gdn_step_norm_multi_grf_only(float*,const float*,int,const float*,const float*,const float*,const float*,float,float*,int,int,int,const int32_t*,void*,int,void*);
void gdn_step_norm_multi_replay_grf(float*,const float*,int,const float*,const float*,const float*,const float*,float,float*,int,int,int,const int32_t*,void*,int,void*);

void gdn_sync_variant(int mode,float* state,const float* h,int C,const float* gate,const float* beta,
    const float* z,const float* gamma,float eps,float* y,int hk,int hv,int T,const int32_t* keep,void* stream,int begin,void* xq) {
#ifdef GDN_FINAL_BUILD
    auto fn=mode?gdn_step_norm_multi:gdn_step_norm_multi_baseline;
#else
    auto fn=mode==1?gdn_step_norm_multi_grf_only:(mode==2?gdn_step_norm_multi_replay_grf:gdn_step_norm_multi_baseline);
#endif
    fn(state,h,C,gate,beta,z,gamma,eps,y,hk,hv,T,keep,stream,begin,xq);
}
}

uint64_t hash_bytes(const void* data,size_t n) {
    uint64_t h=14695981039346656037ull;
    auto* p=static_cast<const uint8_t*>(data);
    for(size_t i=0;i<n;++i){h^=p[i];h*=1099511628211ull;}return h;
}
int main(int argc,char** argv) try {
    if(argc!=3)throw std::runtime_error("use --check/--bench mode");
    const bool bench=std::string(argv[1]).starts_with("--bench");
    const int mode=std::stoi(argv[2]);
    if(!SetPriorityClass(GetCurrentProcess(),ABOVE_NORMAL_PRIORITY_CLASS)||
       !SetThreadAffinityMask(GetCurrentThread(),DWORD_PTR(1)<<4))throw std::runtime_error("scheduling failed");
    using namespace strata::kernels;
    constexpr int S=128,HK=16,HV=48,C=10240,V=S*HV,PAD=33,QP=32,L=36;
    constexpr size_t SS=size_t(S)*HV*S,QB=size_t(8)*V/32*36;
    auto& q=dpct::get_in_order_queue();
    std::printf("device=%s driver=%s mode=%d\n",q.get_device().get_info<sycl::info::device::name>().c_str(),q.get_device().get_info<sycl::info::device::driver_version>().c_str(),mode);
    std::mt19937 rng(712);std::normal_distribution<float> random(0.f,.1f);
    auto values=[&](size_t n){std::vector<float> v(n);for(auto& x:v)x=random(rng);return v;};
    auto initial=values(SS+2*PAD),input=values(8*C),zs=values(8*V);
    std::vector<float> gs(8*HV),bs(8*HV),gam(S,1.f),blank(8*V+2*PAD,42.f);
    for(size_t i=0;i<gs.size();++i){gs[i]=-.03125f*(i%321);bs[i]=float(i%33)/32.f;}
    std::vector<void*> allocations;
    auto copy=[&](const std::vector<float>& v){auto* p=sycl::malloc_device<float>(v.size(),q);if(!p)throw std::bad_alloc();
        allocations.push_back(p);q.memcpy(p,v.data(),v.size()*4).wait_and_throw();return p;};
    auto *state=copy(initial),*h=copy(input),*z=copy(zs),*g=copy(gs),*b=copy(bs),*gamma=copy(gam),*y=copy(blank);
    auto* xq=sycl::malloc_device<uint8_t>(QB+2*QP,q);auto* kd=sycl::malloc_device<int32_t>(1,q);
    if(!xq||!kd)throw std::bad_alloc();allocations.push_back(xq);allocations.push_back(kd);
    std::vector<uint8_t> qblank(QB+2*QP,0xa5);
    struct Result { std::vector<float> s,y;std::vector<uint8_t> q; };
    auto reset=[&](int keep){q.memcpy(state,initial.data(),initial.size()*4);q.memcpy(y,blank.data(),blank.size()*4);
        q.memcpy(xq,qblank.data(),qblank.size());q.memcpy(kd,&keep,4).wait_and_throw();};
    auto launch=[&](int m,int T,int begin,int keep,bool fused,float* st=nullptr){
        gdn_sync_variant(m,st?st:state+PAD,h,C,g,b,z,gamma,1e-6f,y+PAD,HK,HV,T,keep<0?nullptr:kd,&q,begin,fused?xq+QP:nullptr);
    };
    auto get=[&]{Result r{std::vector<float>(initial.size()),std::vector<float>(blank.size()),std::vector<uint8_t>(qblank.size())};
        q.memcpy(r.s.data(),state,r.s.size()*4);q.memcpy(r.y.data(),y,r.y.size()*4);q.memcpy(r.q.data(),xq,r.q.size()).wait_and_throw();return r;};
    auto equal=[&](const Result& a,const Result& c){return !std::memcmp(a.s.data(),c.s.data(),a.s.size()*4)&&
        !std::memcmp(a.y.data(),c.y.data(),a.y.size()*4)&&a.q==c.q;};
    int checks=0;
    auto compare=[&](int T,int begin,int keep,bool fused,bool graph){
        reset(keep);launch(0,T,begin,keep,fused);auto a=get();reset(keep);
        if(graph){using namespace sycl::ext::oneapi::experimental;
            command_graph record(q.get_context(),q.get_device());record.begin_recording(q);
            launch(mode,T,begin,keep,fused);record.end_recording(q);auto executable=record.finalize();
            for(int k:{0,1,T,0}){reset(k);launch(0,T,begin,k,fused);auto ref=get();reset(k);q.ext_oneapi_graph(executable).wait_and_throw();
                if(!equal(ref,get()))throw std::runtime_error("graph parity mismatch");++checks;}
            reset(keep);q.ext_oneapi_graph(executable).wait_and_throw();
        }else launch(mode,T,begin,keep,fused);
        auto c=get();if(!equal(a,c))throw std::runtime_error("state/output/Q8 parity mismatch T="+std::to_string(T)+" begin="+std::to_string(begin)+" keep="+std::to_string(keep));
        if(std::memcmp(c.s.data(),initial.data(),PAD*4)||std::memcmp(c.s.data()+PAD+SS,initial.data()+PAD+SS,PAD*4)||
           std::memcmp(c.y.data(),blank.data(),PAD*4)||std::memcmp(c.y.data()+PAD+8*V,blank.data()+PAD+8*V,PAD*4)||
           std::memcmp(c.q.data(),qblank.data(),QP)||std::memcmp(c.q.data()+QP+QB,qblank.data()+QP+QB,QP))throw std::runtime_error("guard modified");
        ++checks;
    };
    if(!bench){
        for(int sg:{32,16}){_putenv_s("STRATA_GDN_SG16",sg==16?"1":"0");
            for(int T=1;T<=8;++T)for(int keep=-1;keep<=T;++keep)for(int begin:{0,T/2,T})for(bool fused:{false,true})
                compare(T,begin,keep,fused,false);
            for(int T:{4,6})compare(T,T/2,T,true,true);
        }
        for(auto pair:{std::pair{h,&input},std::pair{z,&zs},std::pair{g,&gs},std::pair{b,&bs},std::pair{gamma,&gam}}){
            std::vector<float> out(pair.second->size());q.memcpy(out.data(),pair.first,out.size()*4).wait_and_throw();
            if(out!=*pair.second)throw std::runtime_error("input modified");++checks;
        }
        std::printf("PASS checks=%d (bitwise state/output/Q8, SG16/32, windows, guards, graph, inputs)\n",checks);
    }else{
        _putenv_s("STRATA_GDN_SG16",std::string(argv[1])=="--bench16"?"1":"0");
        auto* states=sycl::malloc_device<float>(L*SS,q);if(!states)throw std::bad_alloc();allocations.push_back(states);
        for(int l=0;l<L;++l)q.memcpy(states+l*SS,initial.data()+PAD,SS*4);q.wait_and_throw();
        for(auto shape:{std::pair{1,0},{2,0},{3,0},{4,0},{6,0},{8,0},{4,2},{6,3},{8,4}}){
            auto [T,begin]=shape;uint64_t oracle=0;
            for(int arm=0;arm<5;++arm){
                reset(-1);launch(arm%2?mode:0,T,begin,-1,true);auto r=get();
                uint64_t fp=hash_bytes(r.s.data(),r.s.size()*4)^hash_bytes(r.y.data(),r.y.size()*4)^hash_bytes(r.q.data(),r.q.size());
                if(!arm)oracle=fp;else if(fp!=oracle)throw std::runtime_error("benchmark fingerprint mismatch");
                std::printf("fingerprint T=%d begin=%d arm=%d hash=%016llx\n",T,begin,arm,(unsigned long long)fp);
                auto sweep=[&](int n){for(int i=0;i<n;++i)for(int l=0;l<L;++l)launch(arm%2?mode:0,T,begin,-1,true,states+l*SS);q.wait_and_throw();};
                for(int i=0;i<3;++i)sweep(8);
                for(int sample=0;sample<9;++sample){auto start=std::chrono::steady_clock::now();sweep(8);
                    double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()/(8*L);
                    std::printf("sample T=%d begin=%d arm=%d index=%d us=%.6f\n",T,begin,arm,sample,us);}
                std::fflush(stdout);
            }
        }
    }
    for(auto* p:allocations)sycl::free(p,q);return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}
