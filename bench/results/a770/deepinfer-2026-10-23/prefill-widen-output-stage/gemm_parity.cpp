// Compare the engine with the original oneMKL mixed-input call; no model or test framework.
#define DPCT_PROFILING_ENABLED
#include <sycl/sycl.hpp>
#include <dpct/dpct.hpp>
#include <dpct/blas_utils.hpp>
#include "strata/prefill/gemm.hpp"
#include <cstdlib>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>

using Clock = std::chrono::steady_clock;
double us(Clock::time_point t) { return std::chrono::duration<double, std::micro>(Clock::now()-t).count(); }

int main(int argc, char** argv) try {
    const bool bench_only = argc > 1 && std::strcmp(argv[1], "--bench") == 0;
    auto& q = dpct::get_in_order_queue();
    if (!q.is_in_order()) throw std::runtime_error("in-order queue required");
    std::printf("DEVICE %s\n", q.get_device().get_info<sycl::info::device::name>().c_str());
    dpct::blas::descriptor desc;
    desc.set_queue(&q);
    strata::prefill::Gemm gemm;
    std::string error;
    const char* arena_flag = std::getenv("STRATA_GEMM_SHARED_ARENA");
    const size_t arena_offset = arena_flag && std::strcmp(arena_flag,"4")==0 ? size_t(4)<<30 : 0;
    auto* arena = arena_flag ? sycl::malloc_device<uint8_t>(arena_offset+(160u<<20),q) : nullptr;
    if (std::getenv("STRATA_GEMM_SHARED_ARENA") && !arena) throw std::runtime_error("arena allocation failed");
    if (!(arena ? gemm.init_external(&q,nullptr,0,arena+arena_offset+(64u<<20),32u<<20,error) : gemm.init(&q,0,error)))
        throw std::runtime_error(error);
    const char* flag = std::getenv("STRATA_PREFILL_GPU_WIDEN");
    const bool enabled = flag && std::strcmp(flag,"1")==0;
    int checks = 0, mismatched_cases = 0;
    for (bool bf16 : {false, true}) {
      for (int padding : {0,13}) {
        if (!bf16 && padding) continue;
        if (bench_only && bf16 && !padding) continue;
        for (int t : {1, 4, 6, 22}) {
            if (bench_only && t!=4) continue;
            for (auto dims : {std::pair{1280,2560}, std::pair{640,2560}, std::pair{2560,640}, std::pair{17,65}, std::pair{4096,2560},
                              std::pair{320,10240}, std::pair{10240,320}}) {
                const int n = dims.first, k = dims.second;
                if (bench_only && n!=1280) continue;
                const int ldx = k + (bf16 && n!=4096 ? padding : 0), ldy = n;
                std::vector<uint16_t> hx(t*ldx), hw(n*k);
                uint32_t state = 42;
                auto random = [&] {
                    state = state*1664525u + 1013904223u;
                    return (static_cast<int>(state >> 8)-8388608)/8388608.f;
                };
                auto encode = [&](float v) {
                    if (bf16) return static_cast<uint16_t>(sycl::bit_cast<uint32_t>(v) >> 16);
                    return sycl::bit_cast<uint16_t>(sycl::half(v));
                };
                for (auto& v : hx) v = encode(random());
                for (auto& v : hw) v = encode(random());
                hx[0]=1; hx[1]=0x8000; hx[2]=0x8001;
                auto* x = arena ? reinterpret_cast<uint16_t*>(arena+arena_offset+(112u<<20)) : sycl::malloc_device<uint16_t>(hx.size(),q);
                auto* w = arena ? reinterpret_cast<uint16_t*>(arena+arena_offset+(114u<<20)) : sycl::malloc_device<uint16_t>(hw.size(),q);
                auto* a = arena ? reinterpret_cast<float*>(arena+arena_offset+(144u<<20)) : sycl::malloc_device<float>(t*ldy+17,q);
                auto* b = arena ? reinterpret_cast<float*>(arena+arena_offset+(152u<<20)) : sycl::malloc_device<float>(t*ldy+17,q);
                if (!x || !w || !a || !b) throw std::runtime_error("allocation failed");
                q.memcpy(x,hx.data(),hx.size()*2); q.memcpy(w,hw.data(),hw.size()*2).wait();
                const float alpha = 1;
                auto original = [&](float beta) {
                    const auto ty = bf16 ? dpct::library_data_t::real_bfloat16 : dpct::library_data_t::real_half;
                    dpct::blas::gemm(&desc,oneapi::mkl::transpose::trans,oneapi::mkl::transpose::nontrans,
                        n,t,k,&alpha,w,ty,k,x,ty,ldx,&beta,a,dpct::library_data_t::real_float,ldy,dpct::compute_type::f32);
                };
                auto widened = [&](float beta) {
                    if (bf16) gemm.bf16(x,w,b,t,n,k,ldy,beta,ldx);
                    else gemm.f16(x,w,b,t,n,k,ldy,beta);
                };
                for (float beta : {0.f, 1.f, -.25f}) {
                    std::vector<float> initial(t*ldy+17,123.5f), ha(t*ldy+17), hb(t*ldy+17);
                    for (int row=0;row<t;++row) for(int col=0;col<n;++col)
                        initial[row*ldy+col] = beta == 0 ? std::numeric_limits<float>::quiet_NaN() : random();
                    q.memcpy(a,initial.data(),initial.size()*4); q.memcpy(b,initial.data(),initial.size()*4);
                    original(beta); widened(beta);
                    q.memcpy(ha.data(),a,ha.size()*4); q.memcpy(hb.data(),b,hb.size()*4).wait_and_throw();
                    int different=0, original_padding=0; float max_abs=0;
                    for (int row=0;row<t;++row) for(int col=0;col<ldy;++col) {
                        const int i=row*ldy+col;
                        if (col>=n) {
                            if(enabled && !bf16 && hb[i]!=initial[i]) throw std::runtime_error("GPU-widened padding overwritten");
                            original_padding += ha[i]!=initial[i];
                            continue;
                        }
                        if (!std::isfinite(ha[i]) || !std::isfinite(hb[i])) throw std::runtime_error("nonfinite result");
                        different += std::memcmp(&ha[i],&hb[i],4)!=0;
                        max_abs=std::max(max_abs,std::abs(ha[i]-hb[i]));
                    }
                    for (size_t i=t*ldy;i<initial.size();++i)
                        if (ha[i]!=initial[i] || hb[i]!=initial[i]) throw std::runtime_error("output guard overwritten");
                    mismatched_cases += different!=0;
                    ++checks;
                    std::printf("PARITY {\"bf16\":%s,\"t\":%d,\"n\":%d,\"k\":%d,\"ldx\":%d,\"ldy\":%d,\"beta\":%.2f,\"different\":%d,\"max_abs\":%.9g,\"original_padding_changed\":%d}\n",
                                bf16?"true":"false",t,n,k,ldx,ldy,beta,different,max_abs,original_padding);
                    std::fflush(stdout);
                }
                if (!bench_only && t==4 && n==1280) {
                    // A changed input and an intervening copy must not reuse the workspace before the first GEMM.
                    auto* saved = sycl::malloc_device<float>(t*ldy,q);
                    if (!saved) throw std::runtime_error("chain allocation failed");
                    std::vector<float> first(t*ldy), observed(t*ldy), second(t*ldy), expected(t*ldy);
                    original(0); q.memcpy(first.data(),a,first.size()*4).wait();
                    widened(0);
                    q.memcpy(saved,b,first.size()*4);
                    q.fill(w,encode(.0625f),hw.size());
                    widened(0);
                    q.memcpy(observed.data(),saved,first.size()*4);
                    q.memcpy(second.data(),b,second.size()*4).wait_and_throw();
                    original(0); q.memcpy(expected.data(),a,first.size()*4).wait();
                    int different=0;
                    for(int row=0;row<t;++row) for(int col=0;col<n;++col) {
                        const int i=row*ldy+col;
                        different += std::memcmp(&first[i],&observed[i],4)!=0 || std::memcmp(&second[i],&expected[i],4)!=0;
                    }
                    ++checks; mismatched_cases += different!=0;
                    std::printf("CHAIN {\"bf16\":%s,\"ldx\":%d,\"different\":%d}\n",bf16?"true":"false",ldx,different);
                    std::fflush(stdout);
                    sycl::free(saved,q);
                    q.memcpy(w,hw.data(),hw.size()*2).wait();
                }
                if (bench_only) {
                    auto bench = [&](bool widen) {
                        for(int i=0;i<4;++i) { if(widen) widened(0); else original(0); }
                        q.wait_and_throw();
                        std::vector<double> submit,total;
                        for(int batch=0;batch<5;++batch) {
                            auto t0=Clock::now();
                            for(int i=0;i<16;++i) { if(widen) widened(0); else original(0); }
                            submit.push_back(us(t0)/16); q.wait_and_throw(); total.push_back(us(t0)/16);
                            std::printf("BATCH {\"bf16\":%s,\"batch\":%d,\"submit_us\":%.6f,\"total_us\":%.6f}\n",
                                        bf16?"true":"false",batch,submit.back(),total.back());
                        }
                        std::sort(submit.begin(),submit.end()); std::sort(total.begin(),total.end());
                        std::printf("BENCH {\"bf16\":%s,\"engine\":%s,\"submit_us\":%.6f,\"total_us\":%.6f}\n",
                                    bf16?"true":"false",widen?"true":"false",submit[2],total[2]); std::fflush(stdout);
                    };
                    bench(true);
                }
                q.wait_and_throw();
                void* allocations[] = {x,w,a,b};
                if (!arena) for(void* p : allocations) sycl::free(p,q);
            }
        }
      }
    }
    std::printf("SUMMARY {\"checks\":%d,\"mismatched_cases\":%d}\n",checks,mismatched_cases);
    if (arena) sycl::free(arena,q);
    if(mismatched_cases) return 2;
    std::puts("PASS");
} catch(const std::exception& e) { std::fprintf(stderr,"%s\n",e.what()); return 1; }
