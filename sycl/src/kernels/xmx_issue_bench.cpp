// Register-resident XMX issue throughput. This is not a GEMM or model benchmark.
// xmx_issue_bench [fp16|bf16|int8] [dpas|dpasw] [chains=8] [local=8] [iterations=8192] [reps=9] [items=4096]
// Packed integers (u8s8/u4s8/u2s8/u4s4/u2s2) use DPAS, 8 chains and local=2.
// --selftest checks every output against exact CPU dot products, including odd DPAS work-groups.
#include <sycl/sycl.hpp>
#include <sycl/ext/intel/esimd.hpp>
#include <sycl/ext/intel/esimd/xmx/dpas.hpp>
#include <sycl/ext/oneapi/bfloat16.hpp>
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace es = sycl::ext::intel::esimd;
using bf16 = sycl::ext::oneapi::bfloat16;
template<class Input, bool Wide, int Chains> class XmxIssue;

template<class Input, bool Wide, int Chains>
void measure(sycl::queue& q, int local, int iterations, int reps, int items, bool report = true) {
    using Acc = std::conditional_t<std::is_same_v<Input, int8_t>, int32_t, float>;
    constexpr int K = 32 / sizeof(Input), AN = (Wide ? 4 : 8) * K, BN = 8 * K;
    constexpr float scale = std::is_same_v<Input, int8_t> ? 1.f : 1.f / 32;
    auto* a = sycl::malloc_shared<Input>(AN, q);
    auto* b = sycl::malloc_shared<Input>(BN, q);
    const size_t count = size_t(items) * Chains * 64;
    auto* out = sycl::malloc_shared<Acc>(count, q);
    if (!a || !b || !out) throw std::runtime_error("USM allocation failed");
    // Repeated A rows also make the two halves consumed by DPASW identical.
    // Nonuniform K/column values check VNNI packing and both input precisions.
    for (int i = 0; i < AN; ++i) a[i] = Input(((i % K) % 7 - 3) * scale);
    for (int k = 0; k < K; ++k)
        for (int n = 0; n < 8; ++n)
            b[(k / (4 / sizeof(Input))) * (8 * (4 / sizeof(Input))) + n * (4 / sizeof(Input)) + k % (4 / sizeof(Input))] =
                Input(((k + n * 3) % 5 - 2) * scale);
    auto launch = [&] {
        return q.parallel_for<XmxIssue<Input, Wide, Chains>>(
            sycl::nd_range<1>(items, local), [=](sycl::nd_item<1> it) SYCL_ESIMD_KERNEL {
                es::simd<Input, AN> av; av.copy_from(a);
                es::simd<Input, BN> bv; bv.copy_from(b);
                es::simd<Acc, 64> c[Chains];
                #pragma unroll
                for (int j = 0; j < Chains; ++j) c[j] = Acc(j);
                // ponytail: runtime loop and distinct live accumulators; inspect native code before interpreting rates.
                for (int i = 0; i < iterations; ++i) {
                    #pragma unroll
                    for (int j = 0; j < Chains; ++j) {
                        if constexpr (Wide) c[j] = es::xmx::dpasw<8, 8, Acc>(c[j], bv, av);
                        else c[j] = es::xmx::dpas<8, 8, Acc>(c[j], bv, av);
                    }
                }
                #pragma unroll
                for (int j = 0; j < Chains; ++j)
                    c[j].copy_to(out + (it.get_global_id(0) * Chains + j) * 64);
            });
    };
    double dots[8]{};
    for (int n = 0; n < 8; ++n)
        for (int k = 0; k < K; ++k)
            dots[n] += double((k % 7 - 3) * scale) * double(((k + n * 3) % 5 - 2) * scale);
    auto check = [&] {
        for (size_t i = 0; i < count; ++i) {
            const int n = i % 8, chain = (i / 64) % Chains;
            const Acc expected = Acc(chain + iterations * dots[n]);
            if (out[i] != expected) {
                std::fprintf(stderr, "mismatch at %zu: %.9g != %.9g\n", i, double(out[i]), double(expected));
                throw std::runtime_error("XMX output differs from CPU dot product");
            }
        }
    };
    launch().wait_and_throw();
    check();
    std::vector<double> samples;
    for (int r = 0; r < reps; ++r) {
        auto event = launch();
        event.wait_and_throw();
        const auto start = event.template get_profiling_info<sycl::info::event_profiling::command_start>();
        const auto end = event.template get_profiling_info<sycl::info::event_profiling::command_end>();
        if (end <= start) throw std::runtime_error("nonpositive device duration");
        samples.push_back(double(end - start) / 1e6);
    }
    check();
    auto ordered = samples;
    std::sort(ordered.begin(), ordered.end());
    if (report) {
        const double ms = (ordered[(ordered.size() - 1) / 2] + ordered[ordered.size() / 2]) / 2;
        const double ops = double(items) * Chains * iterations * 2 * 8 * 8 * K;
        std::printf("RESULT {\"type\":\"%s\",\"instruction\":\"%s\",\"chains\":%d,\"local\":%d,"
                    "\"iterations\":%d,\"items\":%d,\"operations\":%.0f,\"median_ms\":%.9g,\"tops\":%.9g,"
                    "\"checked_outputs\":%zu,\"samples_ms\":[",
                    std::is_same_v<Input, sycl::half> ? "fp16" : std::is_same_v<Input, bf16> ? "bf16" : "int8",
                    Wide ? "dpasw" : "dpas", Chains, local, iterations, items, ops, ms, ops / ms / 1e9, count * 2);
        for (size_t i = 0; i < samples.size(); ++i) std::printf("%s%.9g", i ? "," : "", samples[i]);
        std::puts("]}");
    }
    sycl::free(a, q); sycl::free(b, q); sycl::free(out, q);
}

template<class Input>
void dispatch(sycl::queue& q, bool wide, int chains, int local, int iterations, int reps, int items, bool report = true) {
    if (wide && chains == 8) measure<Input, true, 8>(q, local, iterations, reps, items, report);
    else if (wide) measure<Input, true, 1>(q, local, iterations, reps, items, report);
    else if (chains == 8) measure<Input, false, 8>(q, local, iterations, reps, items, report);
    else measure<Input, false, 1>(q, local, iterations, reps, items, report);
}

int number(const char* text, int lo, int hi) {
    int n = 0;
    const auto [end, error] = std::from_chars(text, text + std::strlen(text), n);
    if (error != std::errc{} || *end || n < lo || n > hi) throw std::runtime_error("invalid numeric argument");
    return n;
}

using P=es::xmx::dpas_argument_type;
template<int WB,int AB> class Lowbit;

template<int WB,int AB>
void measure_lowbit(sycl::queue& q,int iterations,int items,int reps,bool report=true) {
    constexpr int K=(WB==8 || AB==8)?32:64, AN=8*K*AB/32, BN=8*K*WB/32;
    constexpr P WP=WB==8?P::u8:WB==4?P::u4:P::u2, AP=AB==8?P::s8:AB==4?P::s4:P::s2;
    auto* a=sycl::malloc_shared<uint32_t>(AN,q);
    auto* b=sycl::malloc_shared<uint32_t>(BN,q);
    auto* out=sycl::malloc_shared<int32_t>(size_t(items)*8*64,q);
    if (!a || !b || !out) throw std::bad_alloc();
    std::fill_n(a,AN,0);std::fill_n(b,BN,0);
    auto avalue=[](int r,int k) {return (k+r*2)%(1<<AB)-(1<<(AB-1));};
    auto bvalue=[](int k,int n) {return (k+n*3)%(1<<WB);};
    for (int r=0;r<8;++r) for (int k=0;k<K;++k) {
        int bit=(r*K+k)*AB;
        a[bit/32]|=uint32_t(avalue(r,k)&((1<<AB)-1))<<(bit%32);
    }
    for (int k=0;k<K;++k) for (int n=0;n<8;++n)
        b[(k/(32/WB))*8+n]|=uint32_t(bvalue(k,n))<<((k%(32/WB))*WB);
    int32_t dots[64]{};
    for (int r=0;r<8;++r) for (int n=0;n<8;++n) for (int k=0;k<K;++k)
        dots[r*8+n]+=avalue(r,k)*bvalue(k,n);
    for (int32_t dot:dots)
        if (int64_t(dot)*iterations<INT32_MIN || int64_t(dot)*iterations+7>INT32_MAX)
            throw std::invalid_argument("iteration count overflows low-bit INT32 accumulator");
    auto launch=[&] {
        return q.parallel_for<Lowbit<WB,AB>>(sycl::nd_range<1>(items,2),[=](sycl::nd_item<1> it) SYCL_ESIMD_KERNEL {
            es::simd<uint32_t,AN> aa;aa.copy_from(a);
            es::simd<uint32_t,BN> bb;bb.copy_from(b);
            es::simd<int32_t,64> c[8];
            #pragma unroll
            for (int j=0;j<8;++j) c[j]=j;
            for (int i=0;i<iterations;++i) {
                #pragma unroll
                for (int j=0;j<8;++j) c[j]=es::xmx::dpas<8,8,int32_t,int32_t,uint32_t,uint32_t,WP,AP>(c[j],bb,aa);
            }
            #pragma unroll
            for (int j=0;j<8;++j) c[j].copy_to(out+(it.get_global_id(0)*8+j)*64);
        });
    };
    auto check=[&] {
        for (size_t i=0;i<size_t(items)*8*64;++i) {
            const int64_t expected=int64_t(dots[i%64])*iterations+int((i/64)%8);
            if (expected<INT32_MIN || expected>INT32_MAX || out[i]!=expected) {
                std::fprintf(stderr,"mismatch %zu: %d != %lld\n",i,out[i],(long long)expected);
                throw std::runtime_error("lowbit CPU reference");
            }
        }
    };
    launch().wait_and_throw();check();
    std::vector<double> samples;
    for (int r=0;r<reps;++r) {
        auto e=launch();e.wait_and_throw();
        auto begin=e.template get_profiling_info<sycl::info::event_profiling::command_start>();
        auto end=e.template get_profiling_info<sycl::info::event_profiling::command_end>();
        if (end<=begin) throw std::runtime_error("invalid GPU duration");
        samples.push_back(double(end-begin)/1e6);
    }
    check();auto ordered=samples;std::sort(ordered.begin(),ordered.end());
    const double ms=(ordered[(ordered.size()-1)/2]+ordered[ordered.size()/2])/2,ops=double(items)*8*iterations*2*8*8*K;
    if (report) {
        std::printf("RESULT {\"type\":\"u%ds%d\",\"instruction\":\"dpas\",\"chains\":8,\"local\":2,\"K\":%d,\"iterations\":%d,\"items\":%d,\"operations\":%.0f,\"median_ms\":%.9g,\"tops\":%.9g,\"checked_outputs\":%zu,\"samples_ms\":[",WB,AB,K,iterations,items,ops,ms,ops/ms/1e9,size_t(items)*8*64*2);
        for (size_t i=0;i<samples.size();++i) std::printf("%s%.9g",i?",":"",samples[i]);
        std::puts("]}");
    }
    sycl::free(out,q);sycl::free(b,q);sycl::free(a,q);
}

int main(int argc, char** argv) try {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    const bool selftest = argc == 2 && std::strcmp(argv[1], "--selftest") == 0;
    const std::string type = argc > 1 && !selftest ? argv[1] : "fp16";
    const bool packed = type == "u8s8" || type == "u4s8" || type == "u2s8" || type == "u4s4" || type == "u2s2";
    const std::string instruction = argc > 2 ? argv[2] : "dpas";
    const int chains = argc > 3 ? number(argv[3], 1, 8) : 8;
    const int local = argc > 4 ? number(argv[4], 1, 16) : (packed ? 2 : 8);
    const int iterations = argc > 5 ? number(argv[5], 1, 32768) : (type == "u4s4" || type == "u2s2" ? 4096 : 8192);
    const int reps = argc > 6 ? number(argv[6], 1, 31) : 9;
    const int items = argc > 7 ? number(argv[7], 16, 16384) : 4096;
    if (argc > 8 || (!packed && type != "fp16" && type != "bf16" && type != "int8") ||
        (instruction != "dpas" && instruction != "dpasw") || (chains != 1 && chains != 8) ||
        (packed && (instruction != "dpas" || chains != 8 || local != 2)) || items % local || (instruction == "dpasw" && (local % 2 && (local != 1 || items % 2))))
        throw std::runtime_error("usage: xmx_issue_bench [fp16|bf16|int8|u8s8|u4s8|u2s8|u4s4|u2s2] [dpas|dpasw] [chains=1|8] [local=1..16] [iterations=1..32768] [reps=1..31] [items=16..16384], or --selftest; packed integers require dpas/chains=8/local=2");
    sycl::queue q(sycl::gpu_selector_v, {sycl::property::queue::in_order{}, sycl::property::queue::enable_profiling{}});
    if (q.get_device().get_info<sycl::info::device::vendor_id>() != 0x8086)
        throw std::runtime_error("Intel XMX GPU required");
    std::printf("DEVICE %s DRIVER %s\n", q.get_device().get_info<sycl::info::device::name>().c_str(),
                q.get_device().get_info<sycl::info::device::driver_version>().c_str());
    if (selftest) {
        int cases = 0;
        for (bool wide : {false, true}) for (int c : {1, 8}) for (int l : {1, 2, 3, 8}) {
            if (wide && l == 3) continue; // Preserve the driver's DPAS/EU-fusion workaround.
            for (int count : {1, 37, 257}) {
                dispatch<sycl::half>(q, wide, c, l, count, 1, 48, false);
                dispatch<bf16>(q, wide, c, l, count, 1, 48, false);
                dispatch<int8_t>(q, wide, c, l, count, 1, 48, false);
                cases += 3;
            }
        }
        measure_lowbit<8,8>(q,37,48,1,false); measure_lowbit<4,8>(q,37,48,1,false);
        measure_lowbit<2,8>(q,37,48,1,false); measure_lowbit<4,4>(q,37,48,1,false);
        measure_lowbit<2,2>(q,37,48,1,false); cases += 5;
        std::printf("PASS XMX CPU-reference cases=%d\n", cases);
    } else if (type == "fp16") dispatch<sycl::half>(q, instruction == "dpasw", chains, local, iterations, reps, items);
    else if (type == "bf16") dispatch<bf16>(q, instruction == "dpasw", chains, local, iterations, reps, items);
    else if (type == "int8") dispatch<int8_t>(q, instruction == "dpasw", chains, local, iterations, reps, items);
    else if (type == "u8s8") measure_lowbit<8,8>(q,iterations,items,reps);
    else if (type == "u4s8") measure_lowbit<4,8>(q,iterations,items,reps);
    else if (type == "u2s8") measure_lowbit<2,8>(q,iterations,items,reps);
    else if (type == "u4s4") measure_lowbit<4,4>(q,iterations,items,reps);
    else measure_lowbit<2,2>(q,iterations,items,reps);
    return 0;
} catch (const std::exception& e) {
    std::fprintf(stderr, "FAIL %s\n", e.what());
    return 1;
}
