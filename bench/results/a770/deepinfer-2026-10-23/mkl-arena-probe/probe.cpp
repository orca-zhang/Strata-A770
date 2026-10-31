#define DPCT_PROFILING_ENABLED
#include <sycl/sycl.hpp>
#include <dpct/dpct.hpp>
#include <oneapi/mkl/blas.hpp>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

int main() try {
    auto& q=dpct::get_in_order_queue();
    int checks=0,failed=0;
    for(int n:{640,1280}) for(int t:{4,22}) for(bool shared:{false,true}) {
        constexpr int k=2560;
        auto* arena=sycl::malloc_device<uint8_t>(64u<<20,q);
        auto* wf=reinterpret_cast<float*>(arena+(8u<<20));
        auto* xf=reinterpret_cast<float*>(arena+(24u<<20));
        auto* out=shared ? reinterpret_cast<float*>(arena+(40u<<20)) : sycl::malloc_device<float>(n*t,q);
        auto* ref=sycl::malloc_device<float>(n*t,q);
        auto* x=sycl::malloc_device<sycl::half>(t*k,q);
        auto* w=sycl::malloc_device<sycl::half>(n*k,q);
        std::vector<sycl::half> hx(t*k),hw(n*k);
        uint32_t rng=42;
        auto next=[&] { rng=rng*1664525u+1013904223u;return sycl::half((int(rng>>8)-8388608)/8388608.f); };
        for(auto& v:hx) v=next(); for(auto& v:hw) v=next();
        q.memcpy(x,hx.data(),hx.size()*2);q.memcpy(w,hw.data(),hw.size()*2);
        q.parallel_for(sycl::range<1>(hw.size()),[=](sycl::id<1> i){wf[i]=float(w[i]);});
        q.parallel_for(sycl::range<1>(hx.size()),[=](sycl::id<1> i){xf[i]=float(x[i]);});
        q.wait_and_throw();
        oneapi::mkl::blas::column_major::gemm(q,oneapi::mkl::transpose::trans,oneapi::mkl::transpose::nontrans,
            n,t,k,1.f,w,k,x,k,0.f,ref,n,oneapi::mkl::blas::compute_mode::unset).wait_and_throw();
        oneapi::mkl::blas::column_major::gemm(q,oneapi::mkl::transpose::trans,oneapi::mkl::transpose::nontrans,
            n,t,k,1.f,wf,k,xf,k,0.f,out,n,oneapi::mkl::blas::compute_mode::unset).wait_and_throw();
        std::vector<float> actual(n*t),expected(n*t),aw(n*k),ax(t*k);
        q.memcpy(actual.data(),out,actual.size()*4);q.memcpy(expected.data(),ref,expected.size()*4);
        q.memcpy(aw.data(),wf,aw.size()*4);q.memcpy(ax.data(),xf,ax.size()*4).wait_and_throw();
        int different=0,bad=0,conversion=0;
        for(size_t i=0;i<aw.size();++i) conversion+=aw[i]!=float(hw[i]);
        for(size_t i=0;i<ax.size();++i) conversion+=ax[i]!=float(hx[i]);
        for(size_t i=0;i<actual.size();++i) {different+=std::memcmp(&actual[i],&expected[i],4)!=0;bad+=!std::isfinite(actual[i]);}
        ++checks;failed+=different!=0 || bad!=0 || conversion!=0;
        std::printf("PARITY {\"n\":%d,\"t\":%d,\"shared_output\":%s,\"different\":%d,\"nonfinite\":%d,\"conversion_errors\":%d}\n",n,t,shared?"true":"false",different,bad,conversion);
        std::fflush(stdout);
        if(!shared) sycl::free(out,q);
        sycl::free(arena,q);sycl::free(ref,q);sycl::free(x,q);sycl::free(w,q);
    }
    std::printf("SUMMARY {\"checks\":%d,\"mismatched_cases\":%d}\n",checks,failed);
    return failed ? 2 : 0;
} catch(const std::exception& e) {std::fprintf(stderr,"%s\n",e.what());return 1;}
