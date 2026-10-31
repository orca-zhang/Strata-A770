#include <sycl/sycl.hpp>
#include <dpct/dpct.hpp>
#include "strata/prefill/gemm.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>

int main(int argc,char** argv) try {
    const bool bench=argc==2 && std::strcmp(argv[1],"--bench")==0;
    if(argc>1 && !bench) throw std::invalid_argument("expected --bench");
    auto& q=dpct::get_in_order_queue();
    std::printf("DEVICE %s\n",q.get_device().get_info<sycl::info::device::name>().c_str());
    if(q.get_device().get_info<sycl::info::device::name>().find("A770")==std::string::npos) return 77;
    constexpr int K=2560,N=48,S=96,BANKS=8,G=16;
    constexpr size_t WB=size_t(N)*K;
    auto* ws=sycl::malloc_device<unsigned char>(128u<<20,q);
    auto* w=sycl::malloc_device<uint16_t>(BANKS*2*WB,q);
    if(!ws||!w) throw std::bad_alloc();
    strata::prefill::Gemm gm;std::string error;
    if(!gm.init_external(&q,nullptr,0,ws,128u<<20,error)) throw std::runtime_error(error);
    auto encode=[](float x){return uint16_t(sycl::bit_cast<uint32_t>(x)>>16);};
    auto decode=[](uint16_t x){return sycl::bit_cast<float>(uint32_t(x)<<16);};
    uint32_t state=42;
    auto random=[&]{state=1664525u*state+1013904223u;return state;};
    std::vector<uint16_t> hw(BANKS*2*WB);
    for(auto& v:hw) v=encode(float(int(random()%33)-16)/64);
    q.memcpy(w,hw.data(),hw.size()*2).wait_and_throw();
    int checks=0;double worst_random=0;
    for(int t:{1,4,7,8,9,22,64,255,256,512,1024}) {
        auto* x=sycl::malloc_device<uint16_t>(size_t(t)*K,q);
        auto* y=sycl::malloc_device<float>(size_t(t)*S+N+2*G,q);
        if(!x||!y) throw std::bad_alloc();
        std::vector<uint16_t> hx(size_t(t)*K);
        for(auto& v:hx) v=encode(float(int(random()%33)-16)/64);
        q.memcpy(x,hx.data(),hx.size()*2);
        auto launch=[&](int bank){
            gm.bf16(x,w+size_t(bank)*2*WB,y+G,t,N,K,S);
            gm.bf16(x,w+(size_t(bank)*2+1)*WB,y+G+N,t,N,K,S);
        };
        uint64_t hash=14695981039346656037ull;
        for(int bank:{0,7}) {
            q.fill(y,2.f,size_t(t)*S+N+2*G);
            gm.bf16(x,w+size_t(bank)*2*WB,y+G,t,N,K,S);
            std::vector<float> host(size_t(t)*S+N+2*G);
            q.memcpy(host.data(),y,host.size()*4).wait_and_throw();
            for(int row=0;row<t;++row) for(int col=0;col<N;++col)
                if(host[G+row*S+N+col]!=2.f) throw std::runtime_error("alpha overwrote beta half");
            ++checks;
            gm.bf16(x,w+(size_t(bank)*2+1)*WB,y+G+N,t,N,K,S);
            q.memcpy(host.data(),y,host.size()*4).wait_and_throw();
            for(int row=0;row<t;++row) for(int col=0;col<S;++col) {
                double expected=0;
                for(int k=0;k<K;++k) expected+=double(decode(hx[size_t(row)*K+k]))*decode(hw[(size_t(bank)*2+col/N)*WB+(col%N)*K+k]);
                const float actual=host[G+row*S+col];
                if(double(actual)!=expected) throw std::runtime_error("full alpha/beta CPU64 mismatch");
                hash=(hash^sycl::bit_cast<uint32_t>(actual))*1099511628211ull;
            }
            for(size_t i=0;i<host.size();++i)
                if((i<G||i>=G+size_t(t)*S)&&host[i]!=2.f) throw std::runtime_error("dyadic outer/tail guard");
            ++checks;
        }
        std::vector<double> times;
        if(bench) {
            for(int i=0;i<8;++i) launch(i%BANKS);
            for(int sample=0;sample<7;++sample) {
                auto begin=std::chrono::steady_clock::now();
                for(int i=0;i<4;++i) launch((sample*4+i)%BANKS);
                q.wait_and_throw();
                times.push_back(std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-begin).count()/4);
            }
            std::sort(times.begin(),times.end());
        }
        std::printf("CASE {\"T\":%d,\"N\":48,\"K\":2560,\"ldy\":96,\"us\":%.6f,\"hash\":\"%016llx\",\"weight_pairs\":8}\n",t,bench?times[3]:0.,(unsigned long long)hash);
        std::fflush(stdout);
        for(int pad:{0,13}) {
            const int stride=K+pad;
            auto* rx=sycl::malloc_device<uint16_t>(size_t(t)*stride,q);
            auto* rw=sycl::malloc_device<uint16_t>(2*WB,q);
            if(!rx||!rw) throw std::bad_alloc();
            std::vector<uint16_t> hrx(size_t(t)*stride),hrw(2*WB);
            for(auto& v:hrx) v=encode((double(random())/4294967296.-.5)*2);
            for(auto& v:hrw) v=encode((double(random())/4294967296.-.5)*2);
            q.memcpy(rx,hrx.data(),hrx.size()*2);q.memcpy(rw,hrw.data(),hrw.size()*2);
            q.fill(y,2.f,size_t(t)*S+N+2*G);
            gm.bf16(rx,rw,y+G,t,N,K,S,0.f,stride);
            std::vector<float> host(size_t(t)*S+N+2*G);
            q.memcpy(host.data(),y,host.size()*4).wait_and_throw();
            for(int row=0;row<t;++row) for(int col=0;col<N;++col)
                if(host[G+row*S+N+col]!=2.f) throw std::runtime_error("random alpha overwrote beta");
            ++checks;
            gm.bf16(rx,rw+WB,y+G+N,t,N,K,S,0.f,stride);
            q.memcpy(host.data(),y,host.size()*4).wait_and_throw();
            double max_abs=0;
            for(int row=0;row<t;++row) for(int col=0;col<S;++col) {
                double expected=0;
                for(int k=0;k<K;++k) expected+=double(decode(hrx[size_t(row)*stride+k]))*decode(hrw[(col/N)*WB+(col%N)*K+k]);
                const float actual=host[G+row*S+col];double delta=std::abs(double(actual)-expected);
                if(!std::isfinite(actual)||delta>.005) throw std::runtime_error("random CPU64 error above .005");
                max_abs=std::max(max_abs,delta);
            }
            for(size_t i=0;i<host.size();++i)
                if((i<G||i>=G+size_t(t)*S)&&host[i]!=2.f) throw std::runtime_error("random outer/tail guard");
            ++checks;worst_random=std::max(worst_random,max_abs);
            std::printf("ERROR {\"T\":%d,\"input_padding\":%d,\"max_abs_vs_cpu64\":%.9g}\n",t,pad,max_abs);
            sycl::free(rw,q);sycl::free(rx,q);
        }
        sycl::free(y,q);sycl::free(x,q);
    }
    for(int t:{0,7,8,9,255}) {
        const size_t required=size_t(std::max(t,1))*N*4+(t%8?size_t((t+7)/8)*8*K*2:0);
        const size_t capacity=required+128;
        auto* a=sycl::malloc_device<uint16_t>(capacity/2+2*G,q);
        auto* b=sycl::malloc_device<uint16_t>(capacity/2+2*G,q);
        auto* x=sycl::malloc_device<uint16_t>(size_t(std::max(t,1))*K,q);
        auto* y=sycl::malloc_device<float>(size_t(t)*S+N+2*G,q);
        if(!a||!b||!x||!y) throw std::bad_alloc();
        int binding=0;
        for(size_t bytes:{size_t(0),required-1,required,required+1,required}) {
            auto* selected=(binding++%2)?a:b;
            for(auto* p:{a,b}) q.fill(p,uint16_t(0x55aa),capacity/2+2*G);
            q.fill(x,encode(float(binding)/8),size_t(std::max(t,1))*K);
            q.fill(w,encode(131072.f),WB);q.fill(w+WB,encode(65536.f),WB);
            q.fill(y,-12345.f,size_t(t)*S+N+2*G);
            if(t) q.fill(y+G,sycl::bit_cast<float>(uint32_t(0x7fc00000)),size_t(t)*S);
            gm.rebind(nullptr,0,bytes?selected+G:nullptr,bytes);
            gm.bf16(x,w,y+G,t,N,K,S);gm.bf16(x,w+WB,y+G+N,t,N,K,S);
            std::vector<float> host(size_t(t)*S+N+2*G);
            q.memcpy(host.data(),y,host.size()*4).wait_and_throw();
            for(size_t i=0;i<host.size();++i) {
                float expected=-12345.f;
                if(i>=G&&i<G+size_t(t)*S) expected=float(K)*((i-G)%S<N?131072.f:65536.f)*float(binding)/8;
                if(host[i]!=expected) throw std::runtime_error("rebind/NaN/full-range result or tail guard");
            }
            ++checks;
            for(auto* p:{a,b}) {
                std::vector<uint16_t> contents(capacity/2+2*G);q.memcpy(contents.data(),p,contents.size()*2).wait_and_throw();
                for(size_t i=0;i<contents.size();++i)
                    if((p!=selected||i<G||i>=G+bytes/2)&&contents[i]!=0x55aa) throw std::runtime_error("workspace guard");
            }
            ++checks;
        }
        gm.rebind(nullptr,0,ws,128u<<20);
        sycl::free(y,q);sycl::free(x,q);sycl::free(b,q);sycl::free(a,q);
    }
    struct Shape{int n,k,ldy;float beta;bool bf16;};int index=0;
    for(auto [n,k,ldy,beta,bf16]:{Shape{49,K,49,0,true},{N,K-16,N,0,true},{N,K,100,0,true},
                                {N,K,S,1,true},{N,K,S,-.25f,true},{N,K,N,0,false}}) {
        constexpr int t=9;
        auto* x=sycl::malloc_device<uint16_t>(t*k,q);auto* ww=sycl::malloc_device<uint16_t>(n*k,q);
        auto* y=sycl::malloc_device<float>(t*ldy+2*G,q);
        if(!x||!ww||!y) throw std::bad_alloc();
        q.fill(x,bf16?encode(.125f):sycl::bit_cast<uint16_t>(sycl::half(.125f)),t*k);
        q.fill(ww,bf16?encode(.0625f):sycl::bit_cast<uint16_t>(sycl::half(.0625f)),n*k);
        q.fill(y,2.f,t*ldy+2*G);
        if(bf16) gm.bf16(x,ww,y+G,t,n,k,ldy,beta);else gm.f16(x,ww,y+G,t,n,k,ldy,beta);
        std::vector<float> host(t*ldy+2*G);q.memcpy(host.data(),y,host.size()*4).wait_and_throw();
        int padding_changed=0;
        for(size_t i=0;i<host.size();++i) {
            bool active=i>=G&&i<G+t*ldy&&(i-G)%ldy<size_t(n);bool outside=i<G||i>=G+t*ldy;
            if((active&&host[i]!=float(k)/128+beta*2)||(outside&&host[i]!=2.f)) throw std::runtime_error("fallback active/outer guard");
            if(!active&&!outside&&host[i]!=2.f) ++padding_changed;
        }
        if(beta!=0.f&&padding_changed) throw std::runtime_error("beta correction clobbered adjacent half");
        std::printf("FALLBACK {\"index\":%d,\"checked\":true,\"padding_changed\":%d}\n",index++,padding_changed);checks+=2;
        sycl::free(y,q);sycl::free(ww,q);sycl::free(x,q);
    }
    sycl::free(w,q);sycl::free(ws,q);
    std::printf("SUMMARY {\"checks\":%d,\"worst_random_abs_vs_cpu64\":%.9g}\n",checks,worst_random);
}catch(const std::exception& e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}
