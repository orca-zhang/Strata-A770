// Native-half safety checks and an optional engine-level speed/error sweep; no model needed.
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
    if (argc>1 && !bench) throw std::invalid_argument("expected no arguments or --bench");
    auto& q=dpct::get_in_order_queue();
    std::printf("DEVICE %s\n",q.get_device().get_info<sycl::info::device::name>().c_str());
    auto* workspace=sycl::malloc_device<unsigned char>(128u<<20,q);
    if (!workspace) throw std::bad_alloc();
    strata::prefill::Gemm gemm;
    std::string error;
    if (!gemm.init_external(&q,nullptr,0,workspace,128u<<20,error)) throw std::runtime_error(error);
    int checks=0;
    for (bool bf16:{false,true}) {
        struct Shape { int n,k,t; };
        const Shape shapes[]={{17,65,4},{1280,2560,40},{2560,640,40},{6144,2560,4},
            {6144,2560,256},{10240,2560,256},{12288,2560,256},{320,10240,22}};
        for (auto [n,k,t]:shapes) {
            if (!bench && n!=17) continue;
            const size_t count=size_t(n)*t;
            auto* w=sycl::malloc_device<uint16_t>(size_t(n)*k,q);
            auto* x=sycl::malloc_device<uint16_t>(size_t(t)*k,q);
            auto* y=sycl::malloc_device<float>(count+17,q);
            if (!w || !x || !y) throw std::bad_alloc();
            q.parallel_for(sycl::range<1>(size_t(n)*k),[=](sycl::id<1> p) {
                const float value=float((p[0]*17+7)%23)/32-9.f/32;
                w[p]=bf16?uint16_t(sycl::bit_cast<uint32_t>(value)>>16):sycl::bit_cast<uint16_t>(sycl::half(value));
            });
            q.parallel_for(sycl::range<1>(size_t(t)*k),[=](sycl::id<1> p) {
                const float value=float((p[0]*13+3)%19)/32-5.f/32;
                x[p]=bf16?uint16_t(sycl::bit_cast<uint32_t>(value)>>16):sycl::bit_cast<uint16_t>(sycl::half(value));
            });
            q.fill(y,-12345.f,count+17);
            auto launch=[&] {
                if (bf16) gemm.bf16(x,w,y,t,n,k);
                else gemm.f16(x,w,y,t,n,k);
            };
            // Biased dyadic inputs are exact in both formats; periodicity makes the full CPU reference cheap.
            float reference[23][19];
            for (int i=0;i<23;++i) for (int j=0;j<19;++j) {
                float sum=0;
                for (int z=0;z<k;++z) sum+=float(((i*k+z)*17+7)%23-9)*float(((j*k+z)*13+3)%19-5)/1024;
                reference[i][j]=sum;
            }
            std::vector<float> host(count+17);
            double max_abs=0,rms=0;
            uint64_t hash=0;
            auto check=[&] {
                q.memcpy(host.data(),y,host.size()*sizeof(float)).wait_and_throw();
                max_abs=0;rms=0;hash=14695981039346656037ull;
                for (int row=0;row<t;++row) for (int col=0;col<n;++col) {
                    const float actual=host[size_t(row)*n+col];
                    if (!std::isfinite(actual)) throw std::runtime_error("nonfinite normal output");
                    const double delta=double(actual)-reference[col%23][row%19];
                    max_abs=std::max(max_abs,std::abs(delta));rms+=delta*delta;
                    const uint32_t bits=sycl::bit_cast<uint32_t>(actual);
                    for (int byte=0;byte<4;++byte) hash=(hash^((bits>>(byte*8))&255))*1099511628211ull;
                }
                rms=std::sqrt(rms/count);
                for (size_t i=count;i<host.size();++i)
                    if (host[i]!=-12345.f) throw std::runtime_error("normal output guard overwritten");
                ++checks;
            };
            launch();check();
            std::vector<double> times;
            if (bench) {
                for (int i=0;i<3;++i) launch();
                q.wait_and_throw();
                for (int batch=0;batch<7;++batch) {
                    const auto start=std::chrono::steady_clock::now();
                    for (int repeat=0;repeat<3;++repeat) launch();
                    q.wait_and_throw();
                    times.push_back(std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()/3);
                }
                std::sort(times.begin(),times.end());
            }
            check();
            std::printf("CASE {\"bf16\":%s,\"N\":%d,\"K\":%d,\"T\":%d,\"us\":%.6f,\"max_abs_error\":%.9g,\"rms_error\":%.9g,\"output_hash\":\"%016llx\",\"checked_elements\":%llu}\n",
                bf16?"true":"false",n,k,t,bench?times[3]:0.,max_abs,rms,(unsigned long long)hash,(unsigned long long)(count+17));
            std::fflush(stdout);
            sycl::free(y,q);sycl::free(x,q);sycl::free(w,q);
        }
        // These exact integer sums cover whole-output overflow, partial overflow and BF16 conversion overflow.
        struct Range { float w,x,expected; };
        const Range ranges[]={{1,32,81920},{1,128,327680},{131072,.125f,41943040},{131072,0,0}};
        for (auto [wv,xv,expected]:ranges) {
            if (!bf16 && wv>65504) continue;
            constexpr int n=16,k=2560,t=4;
            auto* w=sycl::malloc_device<uint16_t>(n*k,q);
            auto* x=sycl::malloc_device<uint16_t>(t*k,q);
            auto* y=sycl::malloc_device<float>(n*t+17,q);
            if (!w || !x || !y) throw std::bad_alloc();
            auto encode=[&](float v) {return bf16?uint16_t(sycl::bit_cast<uint32_t>(v)>>16):sycl::bit_cast<uint16_t>(sycl::half(v));};
            q.fill(w,encode(wv),n*k);q.fill(x,encode(xv),t*k);q.fill(y,-12345.f,n*t+17);
            if (bf16) gemm.bf16(x,w,y,t,n,k); else gemm.f16(x,w,y,t,n,k);
            std::vector<float> host(n*t+17);
            q.memcpy(host.data(),y,host.size()*sizeof(float)).wait_and_throw();
            for (int i=0;i<n*t+17;++i)
                if (host[i]!=(i<n*t?expected:-12345.f)) throw std::runtime_error("range result/guard failed");
            ++checks;
            std::printf("RANGE {\"bf16\":%s,\"w\":%.9g,\"x\":%.9g,\"expected\":%.9g,\"outputs\":64,\"guards\":17}\n",bf16?"true":"false",wv,xv,expected);
            sycl::free(y,q);sycl::free(x,q);sycl::free(w,q);
        }
    }
    sycl::free(workspace,q);
    std::printf("SUMMARY {\"checks\":%d}\n",checks);
} catch(const std::exception& e) {std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}
