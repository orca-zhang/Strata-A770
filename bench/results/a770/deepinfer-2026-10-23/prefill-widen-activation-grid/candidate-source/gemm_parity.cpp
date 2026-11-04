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
    bool bench_only=false, large=false, xmx=false, boundary=false;
    for (int i=1;i<argc;++i) {
        if (std::strcmp(argv[i], "--bench")==0) bench_only=true;
        else if (std::strcmp(argv[i], "--large")==0) large=true;
        else if (std::strcmp(argv[i], "--xmx")==0) xmx=true;
        else if (std::strcmp(argv[i], "--boundary")==0) boundary=true;
        else throw std::invalid_argument("expected --bench, --large, --xmx or --boundary");
    }
    auto& q = dpct::get_in_order_queue();
    if (!q.is_in_order()) throw std::runtime_error("in-order queue required");
    std::printf("DEVICE %s\n", q.get_device().get_info<sycl::info::device::name>().c_str());
    dpct::blas::descriptor desc;
    desc.set_queue(&q);
    strata::prefill::Gemm gemm;
    std::string error;
    const char* arena_flag = std::getenv("STRATA_GEMM_SHARED_ARENA");
    const size_t arena_offset = arena_flag && std::strcmp(arena_flag,"4")==0 ? size_t(4)<<30 : 0;
    auto* arena = arena_flag ? sycl::malloc_device<uint8_t>(arena_offset+(512u<<20),q) : nullptr;
    if (std::getenv("STRATA_GEMM_SHARED_ARENA") && !arena) throw std::runtime_error("arena allocation failed");
    if (!(arena ? gemm.init_external(&q,nullptr,0,arena+arena_offset+(64u<<20),(large ? 128u : 32u)<<20,error) : gemm.init(&q,0,error)))
        throw std::runtime_error(error);
    int checks = 0, mismatched_cases = 0;
    for (bool bf16 : {false, true}) {
      for (int padding : boundary ? std::vector<int>{0,8,13} : std::vector<int>{0,13}) {
        if (!bf16 && padding) continue;
        if (bench_only && bf16 && !padding) continue;
       for (int output_padding : {0,13}) {
        if ((bench_only || large) && output_padding) continue;
       for (int output_offset : {0,7}) {
        if (!boundary && output_offset && (bf16 || !output_padding)) continue;
        for (int t : bench_only ? std::vector<int>{4,40,256} : boundary ? std::vector<int>{1,7,8,9,15,16,17} : std::vector<int>{1,4,6,22,64,256}) {
            const auto shapes = boundary ? std::vector<std::pair<int,int>>{{8,16},{16,32},{24,48},{64,64},{72,80}} : large
                ? std::vector<std::pair<int,int>>{{4096,2560},{6144,2560},{10240,2560},{12288,2560},{16384,2560}}
                : std::vector<std::pair<int,int>>{{1280,2560},{640,2560},{2560,640},{17,65},{4096,2560},
                                                  {320,10240},{10240,320},{1,2560}};
            for (auto dims : shapes) {
                const int n = dims.first, k = dims.second;
                if (bench_only && (large ? n!=4096 && n!=12288 : n!=1280)) continue;
                const int ldx = k + (bf16 && n!=4096 ? padding : 0), ldy = n + (n==1 && output_padding ? 1 : output_padding);
                if (size_t(t)*ldx*2 > (16u<<20) || size_t(n)*k*2 > (96u<<20) ||
                    (size_t(t)*ldy+output_offset+17)*4 > (32u<<20)) throw std::runtime_error("test arena slices overlap");
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
                auto* x = arena ? reinterpret_cast<uint16_t*>(arena+arena_offset+(224u<<20)) : sycl::malloc_device<uint16_t>(hx.size(),q);
                auto* w = arena ? reinterpret_cast<uint16_t*>(arena+arena_offset+(240u<<20)) : sycl::malloc_device<uint16_t>(hw.size(),q);
                const size_t output_size = size_t(t)*ldy+output_offset+17;
                auto* a_storage = arena ? reinterpret_cast<float*>(arena+arena_offset+(352u<<20)) : sycl::malloc_device<float>(output_size,q);
                auto* b_storage = arena ? reinterpret_cast<float*>(arena+arena_offset+(416u<<20)) : sycl::malloc_device<float>(output_size,q);
                if (!x || !w || !a_storage || !b_storage) throw std::runtime_error("allocation failed");
                auto* a = a_storage+output_offset;
                auto* b = b_storage+output_offset;
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
                    std::vector<float> initial(output_size,123.5f), ha(output_size), hb(output_size);
                    for (int row=0;row<t;++row) for(int col=0;col<n;++col)
                        initial[output_offset+row*ldy+col] = beta == 0 ? std::numeric_limits<float>::quiet_NaN() : random();
                    q.memcpy(a_storage,initial.data(),initial.size()*4); q.memcpy(b_storage,initial.data(),initial.size()*4);
                    original(beta); widened(beta);
                    q.memcpy(ha.data(),a_storage,ha.size()*4); q.memcpy(hb.data(),b_storage,hb.size()*4).wait_and_throw();
                    int different=0, padding_different=0, original_padding=0; float max_abs=0;
                    for (int row=0;row<t;++row) for(int col=0;col<ldy;++col) {
                        const int i=output_offset+row*ldy+col;
                        if (col>=n) {
                            padding_different += std::memcmp(&ha[i],&hb[i],4)!=0;
                            original_padding += ha[i]!=initial[i];
                            continue;
                        }
                        if (!std::isfinite(ha[i]) || !std::isfinite(hb[i])) throw std::runtime_error("nonfinite result");
                        different += std::memcmp(&ha[i],&hb[i],4)!=0;
                        max_abs=std::max(max_abs,std::abs(ha[i]-hb[i]));
                    }
                    for (size_t i=0;i<initial.size();++i)
                        if ((i<size_t(output_offset) || i>=size_t(output_offset)+size_t(t)*ldy) &&
                            (ha[i]!=initial[i] || hb[i]!=initial[i])) throw std::runtime_error("output guard overwritten");
                    // ponytail: only XMX's reduction order may differ; fallbacks remain byte-exact.
                    const bool eligible=xmx && beta==0.f && ldy==n && n%8==0 && k%16==0;
                    if (xmx) {
                        auto decode=[=](uint16_t bits) { return bf16 ? sycl::bit_cast<float>(uint32_t(bits)<<16)
                            : float(sycl::bit_cast<sycl::half>(bits)); };
                        double max_ref=0;
                        for (int sample=0;sample<8;++sample) {
                            const int row=(sample*13)%t, col=(sample*37)%n;
                            double reference=0;
                            for (int z=0;z<k;++z)
                                reference+=double(decode(hx[row*ldx+z]))*double(decode(hw[col*k+z]));
                            const int index=output_offset+row*ldy+col;
                            if (beta!=0.f) reference+=double(beta)*initial[index];
                            max_ref=std::max(max_ref,std::abs(reference-double(hb[index])));
                        }
                        if (max_ref>.005) throw std::runtime_error("FP64 reference error exceeds 0.005");
                    }
                    mismatched_cases += (eligible ? max_abs>.005f : different!=0) || padding_different!=0;
                    ++checks;
                    std::printf("PARITY {\"bf16\":%s,\"t\":%d,\"n\":%d,\"k\":%d,\"ldx\":%d,\"ldy\":%d,\"output_offset\":%d,\"beta\":%.2f,\"different\":%d,\"padding_different\":%d,\"max_abs\":%.9g,\"original_padding_changed\":%d}\n",
                                bf16?"true":"false",t,n,k,ldx,ldy,output_offset,beta,different,padding_different,max_abs,original_padding);
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
                    int different=0; float max_abs=0;
                    for(int row=0;row<t;++row) for(int col=0;col<n;++col) {
                        const int i=row*ldy+col;
                        different += std::memcmp(&first[i],&observed[i],4)!=0 || std::memcmp(&second[i],&expected[i],4)!=0;
                        if (!std::isfinite(observed[i]) || !std::isfinite(second[i])) throw std::runtime_error("nonfinite chain result");
                        max_abs=std::max(max_abs,std::max(std::abs(first[i]-observed[i]),std::abs(second[i]-expected[i])));
                    }
                    ++checks; mismatched_cases += xmx && ldy==n && n%8==0 && k%16==0 ? max_abs>.005f : different!=0;
                    std::printf("CHAIN {\"bf16\":%s,\"ldx\":%d,\"ldy\":%d,\"output_offset\":%d,\"different\":%d}\n",bf16?"true":"false",ldx,ldy,output_offset,different);
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
                            std::printf("BATCH {\"bf16\":%s,\"batch\":%d,\"submit_us\":%.6f,\"total_us\":%.6f,\"n\":%d,\"k\":%d,\"t\":%d}\n",
                                        bf16?"true":"false",batch,submit.back(),total.back(),n,k,t);
                        }
                        std::sort(submit.begin(),submit.end()); std::sort(total.begin(),total.end());
                        std::printf("BENCH {\"bf16\":%s,\"engine\":%s,\"submit_us\":%.6f,\"total_us\":%.6f,\"n\":%d,\"k\":%d,\"t\":%d}\n",
                                    bf16?"true":"false",widen?"true":"false",submit[2],total[2],n,k,t); std::fflush(stdout);
                    };
                    bench(true);
                }
                q.wait_and_throw();
                void* allocations[] = {x,w,a_storage,b_storage};
                if (!arena) for(void* p : allocations) sycl::free(p,q);
            }
        }
      }
      }
      }
    }
    if (xmx && boundary) {
        // A partial tile needs exactly eight padded rows; rebinding must use the new capacity and input.
        constexpr int t=7,n=8,k=16,guard=16;
        constexpr size_t required=8*k*sizeof(uint16_t);
        auto* storage=sycl::malloc_device<uint16_t>(8*k+2*guard,q);
        auto* x=sycl::malloc_device<uint16_t>(t*k,q);
        auto* w=sycl::malloc_device<uint16_t>(n*k,q);
        auto* y=sycl::malloc_device<float>(t*n+guard,q);
        if (!storage || !x || !w || !y) throw std::bad_alloc();
        strata::prefill::Gemm rebound;
        if (!rebound.init_external(&q,nullptr,0,nullptr,0,error)) throw std::runtime_error(error);
        for (bool bf16 : {false,true}) {
            int index=0;
            for (size_t capacity : {size_t(0),required-1,required,required+1,size_t(1),required}) {
                auto encode=[&](float v) { return bf16 ? uint16_t(sycl::bit_cast<uint32_t>(v)>>16)
                    : sycl::bit_cast<uint16_t>(sycl::half(v)); };
                q.fill(storage,uint16_t(0x55aa),8*k+2*guard);
                q.fill(x,encode(.25f),t*k); q.fill(w,encode(float(++index)/8),n*k);
                q.fill(y,-12345.f,t*n+guard);
                rebound.rebind(nullptr,0,capacity?storage+guard:nullptr,capacity);
                if (bf16) rebound.bf16(x,w,y,t,n,k); else rebound.f16(x,w,y,t,n,k);
                std::vector<float> result(t*n+guard);
                std::vector<uint16_t> contents(8*k+2*guard);
                q.memcpy(result.data(),y,result.size()*4);
                q.memcpy(contents.data(),storage,contents.size()*2).wait_and_throw();
                for (int i=0;i<t*n+guard;++i)
                    if (result[i]!=(i<t*n?float(index)/2:-12345.f)) throw std::runtime_error("XMX rebind result/guard failed");
                for (size_t i=0;i<contents.size();++i)
                    if ((i<guard || i>=guard+capacity/2) && contents[i]!=0x55aa)
                        throw std::runtime_error("XMX workspace capacity/guard failed");
                ++checks;
            }
        }
        rebound.rebind(nullptr,0,nullptr,0);
        sycl::free(y,q); sycl::free(w,q); sycl::free(x,q); sycl::free(storage,q);
    }
    std::printf("SUMMARY {\"checks\":%d,\"mismatched_cases\":%d}\n",checks,mismatched_cases);
    if (arena) sycl::free(arena,q);
    if(mismatched_cases) return 2;
    std::puts("PASS");
} catch(const std::exception& e) { std::fprintf(stderr,"%s\n",e.what()); return 1; }
