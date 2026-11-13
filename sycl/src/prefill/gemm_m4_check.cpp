// Exact dyadic CPU reference covers M4 tails, BF16 strides and untouched guards.
#include <sycl/sycl.hpp>
#include <dpct/dpct.hpp>
#include "strata/prefill/gemm.hpp"
#include <algorithm>
#include <cstdio>
#include <stdexcept>
#include <vector>

int main() try {
    auto& q=dpct::get_in_order_queue();
    constexpr int N=24,K=48,G=16,WS=65536;
    auto* workspace=sycl::malloc_shared<unsigned char>(WS+2*G,q);
    if(!workspace)throw std::bad_alloc();
    std::fill_n(workspace,WS+2*G,0x5a);
    int checks=0;
    {
        strata::prefill::Gemm gemm;std::string error;
        if(!gemm.init_external(&q,nullptr,0,workspace+G,WS,error))throw std::runtime_error(error);
        for(bool bf16:{false,true})for(int t:{1,2,3,4,5,7,8,9,15,16,17,127,128,129})for(int pad:{0,2,7}) {
            if(!bf16 && pad)continue;
            const int stride=K+pad;const size_t nx=size_t(t)*stride,nw=N*K,ny=size_t(t)*N;
            auto* x=sycl::malloc_shared<uint16_t>(nx+2*G,q);
            auto* w=sycl::malloc_shared<uint16_t>(nw+2*G,q);
            auto* y=sycl::malloc_shared<float>(ny+2*G,q);
            if(!x||!w||!y)throw std::bad_alloc();
            std::fill_n(x,nx+2*G,uint16_t(0x7e01));std::fill_n(w,nw+2*G,uint16_t(0x7e01));
            std::fill_n(y,ny+2*G,-12345.f);
            auto encode=[=](float v){return bf16?uint16_t(sycl::bit_cast<uint32_t>(v)>>16):sycl::bit_cast<uint16_t>(sycl::half(v));};
            for(int r=0;r<t;++r)for(int k=0;k<K;++k)x[G+r*stride+k]=encode(float((r*5+k*11)%31-15)/32.f);
            for(int c=0;c<N;++c)for(int k=0;k<K;++k)w[G+c*K+k]=encode(float((c*13+k*7)%29-14)/32.f);
            const std::vector<uint16_t> original_x(x,x+nx+2*G),original_w(w,w+nw+2*G);
            if(bf16)gemm.bf16(x+G,w+G,y+G,t,N,K,0,0.f,stride);
            else gemm.f16(x+G,w+G,y+G,t,N,K);
            q.wait_and_throw();
            for(int r=0;r<t;++r)for(int c=0;c<N;++c) {
                double expected=0;
                for(int k=0;k<K;++k)expected+=double((r*5+k*11)%31-15)*double((c*13+k*7)%29-14)/1024.;
                if(y[G+r*N+c]!=float(expected))throw std::runtime_error("exact CPU result/tail/stride mismatch");
            }
            for(int i=0;i<G;++i)if(y[i]!=-12345.f||y[G+ny+i]!=-12345.f||workspace[i]!=0x5a||workspace[G+WS+i]!=0x5a)
                throw std::runtime_error("output/workspace guard changed");
            if(!std::equal(original_x.begin(),original_x.end(),x)||!std::equal(original_w.begin(),original_w.end(),w))
                throw std::runtime_error("input/weight/guard changed");
            sycl::free(y,q);sycl::free(w,q);sycl::free(x,q);++checks;
        }
    }
    sycl::free(workspace,q);std::printf("PASS FP16/BF16 CPU-exact tails/strides/guards cases=%d\n",checks);
    return 0;
} catch(const std::exception& e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}
