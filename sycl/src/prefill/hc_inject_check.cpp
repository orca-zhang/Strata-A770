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
    const bool scalar=argc==2 && std::strcmp(argv[1],"--scalar")==0;
    if (argc>2 || (argc>1 && !bench && !scalar)) throw std::invalid_argument("expected --bench or --scalar");
    auto& q=dpct::get_in_order_queue();
    std::printf("DEVICE %s\n",q.get_device().get_info<sycl::info::device::name>().c_str());
    if (q.get_device().get_info<sycl::info::device::name>().find("A770")==std::string::npos) return 77;
    const int K=scalar?2560:10240,N=scalar?1:4;
    constexpr int BANKS=8,GUARD=16;
    const size_t WB=size_t(N)*K;
    auto* workspace=sycl::malloc_device<unsigned char>(32u<<20,q);
    auto* weights=sycl::malloc_device<uint16_t>(BANKS*WB,q);
    if (!workspace || !weights) throw std::bad_alloc();
    strata::prefill::Gemm gm;
    std::string error;
    if (!gm.init_external(&q,nullptr,0,workspace,32u<<20,error)) throw std::runtime_error(error);
    uint32_t state=42;
    auto random=[&] {state=1664525u*state+1013904223u;return state;};
    auto encode=[](float value) {return uint16_t(sycl::bit_cast<uint32_t>(value)>>16);};
    auto decode=[](uint16_t bits) {return sycl::bit_cast<float>(uint32_t(bits)<<16);};
    std::vector<uint16_t> hw(BANKS*WB);
    for (auto& v:hw) v=encode(float(int(random()%33)-16)/64);
    q.memcpy(weights,hw.data(),hw.size()*2).wait_and_throw();
    int checks=0;
    double worst_random=0;
    for (int t:{1,4,7,8,9,22,64,255,256,512,1024,1038,2069}) {
        auto* x=sycl::malloc_device<uint16_t>(size_t(t)*K,q);
        auto* y=sycl::malloc_device<float>(size_t(t)*N+2*GUARD,q);
        if (!x || !y) throw std::bad_alloc();
        std::vector<uint16_t> hx(size_t(t)*K);
        for (auto& v:hx) v=encode(float(int(random()%33)-16)/64);
        q.memcpy(x,hx.data(),hx.size()*2);q.fill(y,-12345.f,size_t(t)*N+2*GUARD);
        auto launch=[&](int bank) {gm.bf16(x,weights+size_t(bank)*WB,y+GUARD,t,N,K);};
        uint64_t hash=14695981039346656037ull;
        for (int bank:{0,7}) {
            launch(bank);
            std::vector<float> host(size_t(t)*N+2*GUARD);
            q.memcpy(host.data(),y,host.size()*4).wait_and_throw();
            for (int row=0;row<t;++row) for (int col=0;col<N;++col) {
                double expected=0;
                for (int k=0;k<K;++k) expected+=double(decode(hx[size_t(row)*K+k]))*decode(hw[size_t(bank)*WB+col*K+k]);
                const float actual=host[GUARD+row*N+col];
                if (double(actual)!=expected) throw std::runtime_error("dyadic full-output CPU64 mismatch");
                hash=(hash^sycl::bit_cast<uint32_t>(actual))*1099511628211ull;
            }
            for (int i=0;i<GUARD;++i)
                if (host[i]!=-12345.f || host[GUARD+size_t(t)*N+i]!=-12345.f) throw std::runtime_error("output guard");
            checks+=2;
        }
        std::vector<double> times;
        if (bench) {
            for (int i=0;i<8;++i) launch(i%BANKS);
            for (int sample=0;sample<7;++sample) {
                const auto begin=std::chrono::steady_clock::now();
                for (int i=0;i<4;++i) launch((sample*4+i)%BANKS);
                q.wait_and_throw();
                times.push_back(std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-begin).count()/4);
            }
            std::sort(times.begin(),times.end());
        }
        std::printf("CASE {\"T\":%d,\"N\":%d,\"K\":%d,\"us\":%.6f,\"hash\":\"%016llx\",\"weight_banks\":8}\n",
            t,N,K,bench?times[3]:0.,(unsigned long long)hash);
        std::fflush(stdout);
        // Non-dyadic BF16 values, both contiguous and padded input, checked in full against CPU double.
        for (int pad:{0,13}) {
            const int stride=K+pad;
            auto* rx=sycl::malloc_device<uint16_t>(size_t(t)*stride,q);
            auto* rw=sycl::malloc_device<uint16_t>(WB,q);
            if (!rx || !rw) throw std::bad_alloc();
            std::vector<uint16_t> hrx(size_t(t)*stride),hrw(WB);
            for (auto& v:hrx) v=encode((double(random())/4294967296.-.5)*2);
            for (auto& v:hrw) v=encode((double(random())/4294967296.-.5)*2);
            q.memcpy(rx,hrx.data(),hrx.size()*2);q.memcpy(rw,hrw.data(),hrw.size()*2);
            gm.bf16(rx,rw,y+GUARD,t,N,K,N,0.f,stride);
            std::vector<float> host(size_t(t)*N+2*GUARD);
            q.memcpy(host.data(),y,host.size()*4).wait_and_throw();
            double max_abs=0;
            for (int row=0;row<t;++row) for (int col=0;col<N;++col) {
                double expected=0;
                for (int k=0;k<K;++k) expected+=double(decode(hrx[size_t(row)*stride+k]))*decode(hrw[col*K+k]);
                const double delta=std::abs(double(host[GUARD+row*N+col])-expected);
                if (!std::isfinite(host[GUARD+row*N+col]) || delta>.005) throw std::runtime_error("random CPU64 error above .005");
                max_abs=std::max(max_abs,delta);
            }
            for (int i=0;i<GUARD;++i)
                if (host[i]!=-12345.f || host[GUARD+size_t(t)*N+i]!=-12345.f) throw std::runtime_error("random output guard");
            worst_random=std::max(worst_random,max_abs);checks+=2;
            std::printf("ERROR {\"T\":%d,\"input_padding\":%d,\"max_abs_vs_cpu64\":%.9g}\n",t,pad,max_abs);
            sycl::free(rw,q);sycl::free(rx,q);
        }
        sycl::free(y,q);sycl::free(x,q);
    }
    // Exact/short/moved workspace, beta=0 NaN destination, changed inputs and large finite BF16 values.
    for (int t:{0,7,8,9,255,1038,2069}) for (int stride:{K,K+13}) {
        const size_t rows=stride%8?size_t((t+7)/8)*8:t%8?8:0;
        const size_t required=8*K*2+size_t(t)*8*4+rows*K*2;
        const size_t capacity=required+128;
        auto* a=sycl::malloc_device<uint16_t>(capacity/2+2*GUARD,q);
        auto* b=sycl::malloc_device<uint16_t>(capacity/2+2*GUARD,q);
        auto* x=sycl::malloc_device<uint16_t>(size_t(std::max(t,1))*stride,q);
        auto* y=sycl::malloc_device<float>(size_t(t)*N+2*GUARD,q);
        if (!a || !b || !x || !y) throw std::bad_alloc();
        int binding=0;
        for (size_t bytes:{size_t(0),required-1,required,required+1,required}) {
            auto* selected=(binding++%2)?a:b;
            for (auto* p:{a,b}) q.fill(p,uint16_t(0x55aa),capacity/2+2*GUARD);
            q.fill(x,encode(float(binding)/8),size_t(std::max(t,1))*stride);
            q.fill(weights,encode(131072.f),WB);
            q.fill(y,-12345.f,size_t(t)*N+2*GUARD);
            if (t) q.fill(y+GUARD,sycl::bit_cast<float>(uint32_t(0x7fc00000)),size_t(t)*N);
            gm.rebind(nullptr,0,bytes?selected+GUARD:nullptr,bytes);
            gm.bf16(x,weights,y+GUARD,t,N,K,N,0.f,stride);
            std::vector<float> host(size_t(t)*N+2*GUARD);
            q.memcpy(host.data(),y,host.size()*4).wait_and_throw();
            for (size_t i=0;i<host.size();++i)
                if (host[i]!=(i<GUARD || i>=GUARD+size_t(t)*N?-12345.f:float(K)*131072.f*float(binding)/8))
                    throw std::runtime_error("rebind/NaN-beta0/full-range result");
            for (auto* p:{a,b}) {
                std::vector<uint16_t> contents(capacity/2+2*GUARD);
                q.memcpy(contents.data(),p,contents.size()*2).wait_and_throw();
                for (size_t i=0;i<contents.size();++i)
                    if ((p!=selected || i<GUARD || i>=GUARD+bytes/2) && contents[i]!=0x55aa)
                        throw std::runtime_error("workspace guard");
            }
            checks+=2;
        }
        gm.rebind(nullptr,0,workspace,32u<<20);
        sycl::free(y,q);sycl::free(x,q);sycl::free(b,q);sycl::free(a,q);
    }
    // Ineligible N/K/ldy/beta/FP16 must preserve the existing path, including its output stride.
    struct Shape {int n,k,ldy;float beta;bool bf16;};
    int fallback_index=0;
    for (auto [n,k,ldy,beta,bf16]:{Shape{5,K,5,0,true},{4,K-16,4,0,true},{4,K,7,0,true},
                                  {4,K,4,1,true},{4,K,4,-.25f,true},{4,K,4,0,false}}) {
        constexpr int t=9;
        auto* x=sycl::malloc_device<uint16_t>(size_t(t)*k,q);
        auto* w=sycl::malloc_device<uint16_t>(size_t(n)*k,q);
        auto* y=sycl::malloc_device<float>(size_t(t)*ldy+2*GUARD,q);
        if (!x || !w || !y) throw std::bad_alloc();
        const uint16_t xb=bf16?encode(.125f):sycl::bit_cast<uint16_t>(sycl::half(.125f));
        const uint16_t wb=bf16?encode(.0625f):sycl::bit_cast<uint16_t>(sycl::half(.0625f));
        q.fill(x,xb,size_t(t)*k);q.fill(w,wb,size_t(n)*k);q.fill(y,2.f,size_t(t)*ldy+2*GUARD);
        if (bf16) gm.bf16(x,w,y+GUARD,t,n,k,ldy,beta);else gm.f16(x,w,y+GUARD,t,n,k,ldy,beta);
        std::vector<float> host(size_t(t)*ldy+2*GUARD);
        q.memcpy(host.data(),y,host.size()*4).wait_and_throw();
        int padding_changed=0;
        for (size_t i=0;i<host.size();++i) {
            const bool output=i>=GUARD && i<GUARD+size_t(t)*ldy && (i-GUARD)%ldy<size_t(n);
            const bool outside=i<GUARD || i>=GUARD+size_t(t)*ldy;
            if ((output && host[i]!=float(k)/128+beta*2) || (outside && host[i]!=2.f))
                throw std::runtime_error("fallback result/outer guard");
            if (!output && !outside && host[i]!=2.f) ++padding_changed;
        }
        std::printf("FALLBACK {\"index\":%d,\"checked\":true,\"padding_changed\":%d}\n",fallback_index++,padding_changed);checks+=2;
        sycl::free(y,q);sycl::free(w,q);sycl::free(x,q);
    }
    sycl::free(weights,q);sycl::free(workspace,q);
    std::printf("SUMMARY {\"checks\":%d,\"worst_random_abs_vs_cpu64\":%.9g}\n",checks,worst_random);
} catch (const std::exception& e) {std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}
