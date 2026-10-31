#define NOMINMAX
#include <windows.h>
#include <sycl/sycl.hpp>
#include <dpct/dpct.hpp>
#include "strata/kernels/verify_kernels.hpp"
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace strata::kernels {
bool gdn_commit_baseline_esimd(float*,float*,const float*,const float*,int,const float*,const float*,int,int,int,const int32_t*,void*);
void gdn_step_norm_baseline_multi(float*,const float*,int,const float*,const float*,const float*,const float*,float,float*,int,int,int,const int32_t*,void*,int,void*);
}
uint64_t fingerprint(const std::vector<float>& v) {
    uint64_t hash=14695981039346656037ull;
    for(float x:v){hash^=sycl::bit_cast<uint32_t>(x);hash*=1099511628211ull;}
    return hash;
}
int main(int argc,char** argv) try {
    using namespace strata::kernels;
    constexpr int S=128,HK=16,HV=48,C=10240,L=40,MAXT=6;
    int PAD=32;
    constexpr size_t SS=size_t(S)*HV*S,HS=C*3;
    const bool bench=argc==3&&std::string(argv[1])=="--bench";
    const int mode=argc==3?std::atoi(argv[2]):1;
    if(mode!=1&&mode!=2)throw std::runtime_error("invalid API");
    if(!SetPriorityClass(GetCurrentProcess(),ABOVE_NORMAL_PRIORITY_CLASS) ||
       !SetThreadAffinityMask(GetCurrentThread(),DWORD_PTR(1)<<4))throw std::runtime_error("scheduling failed");
    auto& q=dpct::get_in_order_queue();
    std::printf("device=%s driver=%s\n",q.get_device().get_info<sycl::info::device::name>().c_str(),q.get_device().get_info<sycl::info::device::driver_version>().c_str());
    std::mt19937 rng(2937);std::normal_distribution<float> random(0.f,.03f);
    auto values=[&](size_t n){std::vector<float> v(n);for(auto& x:v)x=random(rng);return v;};
    auto initial=values(L*SS+66),history=values(L*HS+66),input=values(size_t(L)*MAXT*C),conv=values(input.size());
    auto blank=values(size_t(MAXT)*S*HV),gamma=values(S);
    std::vector<float> gate(L*MAXT*HV),beta(gate.size());
    for(size_t i=0;i<gate.size();++i){gate[i]=-.03125f*(i%321);beta[i]=float(i%33)/32.f;}
    std::vector<void*> allocations;
    auto copy=[&](const std::vector<float>& v){auto* p=sycl::malloc_device<float>(v.size(),q);
        if(!p)throw std::bad_alloc();allocations.push_back(p);q.memcpy(p,v.data(),v.size()*4).wait_and_throw();return p;};
    auto *state=copy(initial),*hist=copy(history),*h=copy(input),*qkv=copy(conv),*g=copy(gate),*b=copy(beta);
    auto *y=copy(blank),*z=copy(blank),*gn=copy(gamma);
    auto* keep_d=sycl::malloc_device<int32_t>(1,q);if(!keep_d)throw std::bad_alloc();
    auto launch=[&](bool candidate,int T,int first,int ord0,int count){
        for(int l=0;l<count;++l){
            const int dst=first-ord0+l,src=first+l;
            float* st=state+PAD+size_t(dst)*SS;
            if(mode==1){
                const auto call=candidate?gdn_commit_esimd:gdn_commit_baseline_esimd;
                if(!call(st,hist+PAD+size_t(dst)*HS,qkv+size_t(src)*T*C,h+size_t(src)*T*C,C,
                    g+size_t(src)*T*HV,b+size_t(src)*T*HV,HK,HV,T,keep_d,&q))throw std::runtime_error("commit refused");
            }else{
                const auto call=candidate?gdn_step_norm_multi:gdn_step_norm_baseline_multi;
                call(st,h+size_t(src)*T*C,C,g+size_t(src)*T*HV,b+size_t(src)*T*HV,
                    z,gn,1e-6f,y,HK,HV,T,keep_d,&q,T,nullptr);
            }
        }
    };
    auto read=[&](float* p,size_t n){std::vector<float> v(n);q.memcpy(v.data(),p,n*4).wait_and_throw();return v;};
    auto get=[&]{auto v=read(state,initial.size()),w=read(hist,history.size());v.insert(v.end(),w.begin(),w.end());return v;};
    auto reset=[&](int keep){q.memcpy(state,initial.data(),initial.size()*4);q.memcpy(hist,history.data(),history.size()*4);q.memcpy(keep_d,&keep,4).wait_and_throw();};
    int checks=0;
    auto check=[&](int T,int keep,int first,int ord0,int count,bool graph){
        reset(keep);launch(false,T,first,ord0,count);auto want=get();reset(keep);
        if(graph){
            using namespace sycl::ext::oneapi::experimental;
            command_graph recorded(q.get_context(),q.get_device());recorded.begin_recording(q);
            launch(true,T,first,ord0,count);recorded.end_recording(q);auto executable=recorded.finalize();
            for(int n:{0,1,2,T-1,T,0}){
                reset(n);launch(false,T,first,ord0,count);auto ref=get();reset(n);
                q.ext_oneapi_graph(executable).wait_and_throw();auto got=get();
                if(std::memcmp(ref.data(),got.data(),ref.size()*4))throw std::runtime_error("graph mismatch");++checks;
            }
            reset(keep);q.ext_oneapi_graph(executable).wait_and_throw();
        }else launch(true,T,first,ord0,count);
        auto got=get();
        if(std::memcmp(want.data(),got.data(),got.size()*4))throw std::runtime_error("bitwise parity mismatch");
        for(int l=0;l<L;++l){
            const int local=first-ord0;
            if(keep<=0||l<local||l>=local+count){
                if(std::memcmp(got.data()+PAD+l*SS,initial.data()+PAD+l*SS,SS*4))throw std::runtime_error("outside state write");
            }
            for(int c=0;c<C;++c)for(int j=0;j<3;++j){
                const size_t dst=PAD+l*HS+c*3+j;
                float want=history[dst];
                if(mode==1&&keep>0&&l>=local&&l<local+count){
                    const int src=keep+j;want=src<3?history[dst-j+src]:conv[(size_t(l+ord0)*T+src-3)*C+c];
                }
                if(got[initial.size()+dst]!=want)throw std::runtime_error("CPU history mismatch");
            }
        }
        for(auto entry:{std::pair{size_t(0),SS},std::pair{initial.size(),HS}}){
            const auto& ref=entry.first?history:initial;
            if(std::memcmp(got.data()+entry.first,ref.data(),PAD*4)||
               std::memcmp(got.data()+entry.first+PAD+L*entry.second,ref.data()+PAD+L*entry.second,(ref.size()-PAD-L*entry.second)*4))
                throw std::runtime_error("guard changed");
        }
        ++checks;
        std::printf("check T=%d keep=%d first=%d ord0=%d layers=%d graph=%d hash=%016llx\n",T,keep,first,ord0,count,graph,(unsigned long long)fingerprint(got));std::fflush(stdout);
    };
    if(!bench){
        for(int T:{4,6})for(int keep=-1;keep<=T;++keep){
            check(T,keep,0,0,1,false);check(T,keep,5,3,3,false);check(T,keep,3,3,36,false);
        }
        for(int T:{4,6})check(T,T,5,3,3,true);
        PAD=33;
        for(int T:{4,6})for(int keep=-1;keep<=T;++keep)check(T,keep,5,3,3,false);
        PAD=32;reset(2);
        if(mode==1){
            for(int T:{0,1,2,3,5,7,8}){
                if(gdn_commit_esimd(state+PAD,hist+PAD,qkv,h,C,g,b,HK,HV,T,keep_d,&q))throw std::runtime_error("invalid T accepted");++checks;
            }
            if(gdn_commit_esimd(state+PAD,hist+PAD,qkv,h,C,g,b,HK,HV,6,nullptr,&q))throw std::runtime_error("null keep accepted");++checks;
            if(gdn_commit_esimd(state+PAD,hist+PAD,qkv,h,C-1,g,b,HK,HV,6,keep_d,&q))throw std::runtime_error("invalid C accepted");++checks;
        }
        auto unchanged=initial;unchanged.insert(unchanged.end(),history.begin(),history.end());
        if(get()!=unchanged)throw std::runtime_error("refused call wrote state");
        for(auto pair:{std::pair{h,&input},std::pair{qkv,&conv},std::pair{g,&gate},std::pair{b,&beta},
                       std::pair{y,&blank},std::pair{z,&blank},std::pair{gn,&gamma}}){
            if(read(pair.first,pair.second->size())!=*pair.second)throw std::runtime_error("input/output modified");++checks;
        }
        std::printf("PASS checks=%d (bitwise states, separate CPU history, offsets, guards, graph, inputs, untouched output)\n",checks);
    }else{
        for(int T:{4,6})for(int keep=1;keep<=T;++keep){
            std::vector<float> oracle;
            for(int arm=0;arm<5;++arm){
                reset(keep);launch(arm%2,T,0,0,36);auto got=get();
                if(!arm)oracle=got;else if(std::memcmp(got.data(),oracle.data(),got.size()*4))throw std::runtime_error("benchmark parity mismatch");
                std::printf("fingerprint T=%d keep=%d arm=%d hash=%016llx\n",T,keep,arm,(unsigned long long)fingerprint(got));
                auto run=[&](int n){for(int i=0;i<n;++i)launch(arm%2,T,0,0,36);q.wait_and_throw();};
                for(int i=0;i<3;++i)run(64);
                for(int sample=0;sample<9;++sample){
                    auto start=std::chrono::steady_clock::now();run(128);
                    const double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()/128;
                    std::printf("sample T=%d keep=%d arm=%d index=%d us=%.6f\n",T,keep,arm,sample,us);
                }
                std::fflush(stdout);
            }
        }
    }
    for(auto* p:allocations)sycl::free(p,q);sycl::free(keep_d,q);return 0;
}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}
