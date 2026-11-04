// src/kernels/native_expert_parity.cpp - plan v0.3 P6: one native expert three ways, on real GGUF rows.
//
//     build/native_expert_parity <shard1.gguf> [layer ...]
//
// (a) float reference: ggml's own dequantizer (`to_float`) and a float SwiGLU expert, (b) the CPU path
// (ggml-cpu vec_dot with its quantized activations), (c) the GPU path (`native_expert_grouped`, q8_1
// activations).  (b) and (c) each differ from (a) by their activation rounding only (a few 1e-3 relative).
#include "strata/artifact/gguf_reader.hpp"
#include "strata/kernels/cpu/native_expert.hpp"
#include "strata/kernels/cpu/pool.hpp"
#include "strata/kernels/cpu/expert.hpp"
#include "strata/kernels/cpu/iq_avx512.hpp"
#include "strata/kernels/cpu/iq_avx2.hpp"
#include "strata/kernels/cpu/expert_layout.hpp"
#include "ggml-cpu.h"
#include "strata/kernels/iq_kernels.hpp"
#ifdef STRATA_EZPP
#include "strata/kernels/q8_1_finite.hpp"
#include "strata/kernels/xmx_gemm.hpp"
#include "ezpp.hpp"
#undef int64_t
#endif

#include "ggml.h"

#include <sycl/sycl.hpp>
#include <dpct/dpct.hpp>   // SYCL port: hand-ported (dpct needs ggml-cpu.h to parse this file)

#include <chrono>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace cpu = strata::kernels::cpu;

static double rel(const std::vector<float>& a, const std::vector<float>& b) {
    double n = 0, d = 0;
    for (size_t i = 0; i < a.size(); ++i) { n += std::fabs((double) a[i] - b[i]); d += std::fabs((double) b[i]); }
    return n / (d + 1e-30);
}

#ifdef STRATA_EZPP
struct alignas(4) BenchQ81 {
    sycl::half2 ds;
    int8_t qs[32];
};
static_assert(sizeof(BenchQ81) == 36);

static bool xmx_full_cost(const cpu::NativeFmt& f, const std::vector<uint8_t>& blob,
                          const std::vector<float>& x, sycl::queue* s) {
    if (f.gu_type != 29 || f.d_type != 42) return true;
    constexpr int max_t = 4, reps = 100;
    const int H = (int) f.n_embd, FF = (int) f.n_ff;
    const auto L = strata::kernels::native_expert_layout(f.gu_type, f.d_type, H, FF);
    auto* dblob = sycl::malloc_device<uint8_t>(blob.size(), *s);
    auto* dx = sycl::malloc_device<float>((size_t) max_t * H, *s);
    auto* xh = sycl::malloc_device<uint16_t>((size_t) max_t * H, *s);
    auto* xq = sycl::malloc_device<uint8_t>((size_t) max_t * H / 32 * 36, *s);
    auto* gu = sycl::malloc_device<float>((size_t) max_t * 2 * FF, *s);
    auto* hq = sycl::malloc_device<BenchQ81>((size_t) max_t * FF / 32, *s);
    auto* scratch = sycl::malloc_device<uint8_t>(strata::kernels::native_expert_scratch_bytes(max_t, FF), *s);
    auto* base = sycl::malloc_device<float>((size_t) max_t * H, *s);
    auto* cand = sycl::malloc_device<float>((size_t) max_t * H, *s);
    auto* ptr = sycl::malloc_device<unsigned long long>(1, *s);
    auto* start = sycl::malloc_device<int32_t>(2, *s);
    auto* ng = sycl::malloc_device<int32_t>(1, *s);
    auto* dst = sycl::malloc_device<int32_t>(max_t, *s);
    auto* tok = sycl::malloc_device<int32_t>(max_t, *s);
    if (!dblob || !dx || !xh || !xq || !gu || !hq || !scratch || !base || !cand || !ptr || !start || !ng || !dst || !tok)
        throw std::bad_alloc();
    s->memcpy(dblob, blob.data(), blob.size());
    s->memcpy(dx, x.data(), (size_t) max_t * H * sizeof(float));
    const unsigned long long p = (unsigned long long) dblob;
    const int32_t one = 1, indices[max_t] = {0, 1, 2, 3};
    s->memcpy(ptr, &p, sizeof(p));
    s->memcpy(ng, &one, sizeof(one));
    s->memcpy(dst, indices, sizeof(indices));
    s->memcpy(tok, indices, sizeof(indices)).wait();

    auto half = [&](int m) {
        const size_t n = (size_t) m * H;
        s->parallel_for(sycl::range<1>(n), [=](sycl::id<1> i) {
            xh[i] = sycl::bit_cast<uint16_t>(sycl::half(dx[i]));
        });
    };
    auto swiglu_q8 = [&](int m) {
        const size_t n = (size_t) m * FF;
        s->parallel_for(sycl::nd_range<1>(sycl::range<1>((n + 255) / 256 * 256), sycl::range<1>(256)),
                        [=](sycl::nd_item<1> item) [[sycl::reqd_sub_group_size(32)]] {
            const size_t i = item.get_global_linear_id();
            if (i >= n) return;
            const float g = gu[2 * i];
            const float h = (g / (1.0f + sycl::native::exp(-g))) * gu[2 * i + 1];
            const auto sg = item.get_sub_group();
            const float amax = sycl::reduce_over_group(sg, sycl::fabs(h), sycl::maximum<float>());
            const float sum = sycl::reduce_over_group(sg, h, sycl::plus<float>());
            const float d = strata::kernels::q8_1_finite(amax / 127.0f);
            const size_t block = i / 32, lane = i % 32;
            hq[block].qs[lane] = strata::kernels::q8_1_quant(h, d, amax);
            if (lane == 0) hq[block].ds = strata::kernels::q8_1_ds(d, sum);
        });
    };
    auto current = [&](int m) {
        strata::kernels::quantize_q8_1_rows(dx, m, H, xq, s);
        strata::kernels::native_expert_grouped(L, ptr, start, ng, dst, tok, 1, m, xq, scratch, base, s);
    };
    auto xmx = [&](int m) {
        half(m);
        if (!strata::kernels::xmx_gemm_iq_variant(29, dblob, dblob + f.up_off, H, 2 * FF, xh, m,
                                                  gu, 2 * FF, s, 3))
            throw std::runtime_error("IQ1_M XMX variant 3 refused the real expert");
        swiglu_q8(m);
        strata::kernels::iq_mmvq(42, dblob + f.down_off, hq, cand, FF, H, m, s);
    };
    auto median = [&](auto&& run) {
        for (int i = 0; i < 3; ++i) run();
        s->wait();
        double samples[5];
        for (double& ms : samples) {
            const auto begin = std::chrono::steady_clock::now();
            for (int i = 0; i < reps; ++i) run();
            s->wait();
            ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count() / reps;
        }
        std::sort(samples, samples + 5);
        return samples[2];
    };

    bool ok = true;
    for (int m : {1, 2, 4}) {
        const int32_t starts[2] = {0, m};
        s->memcpy(start, starts, sizeof(starts)).wait();
        current(m); xmx(m); s->wait();
        std::vector<float> b((size_t) m * H), c(b.size());
        s->memcpy(b.data(), base, b.size() * sizeof(float));
        s->memcpy(c.data(), cand, c.size() * sizeof(float)).wait();
        const double error = rel(c, b);
        const bool case_ok = std::isfinite(error) && error < 3e-2;
        ok = ok && case_ok;
        double current_ms, xmx_ms, half_ms, gu_ms, swiglu_ms, down_ms;
        {
            EZPP_EX("iq1m.current.full");
            current_ms = median([&] { current(m); });
        }
        {
            EZPP_EX("iq1m.xmx.full");
            xmx_ms = median([&] { xmx(m); });
        }
        {
            EZPP_EX("iq1m.xmx.input_fp16");
            half_ms = median([&] { half(m); });
        }
        {
            EZPP_EX("iq1m.xmx.gu");
            gu_ms = median([&] {
                strata::kernels::xmx_gemm_iq_variant(29, dblob, dblob + f.up_off, H, 2 * FF, xh, m,
                                                     gu, 2 * FF, s, 3);
            });
        }
        {
            EZPP_EX("iq1m.xmx.swiglu_q8");
            swiglu_ms = median([&] { swiglu_q8(m); });
        }
        {
            EZPP_EX("iq1m.xmx.down");
            down_ms = median([&] { strata::kernels::iq_mmvq(42, dblob + f.down_off, hq, cand, FF, H, m, s); });
        }
        std::printf("XMX_REAL {\"T\":%d,\"current_ms\":%.6f,\"xmx_full_ms\":%.6f,\"speedup\":%.6f,"
                    "\"input_fp16_ms\":%.6f,\"xmx_gu_ms\":%.6f,\"swiglu_q8_ms\":%.6f,\"down_ms\":%.6f,"
                    "\"relative_error\":%.9g,\"ok\":%s}\n",
                    m, current_ms, xmx_ms, current_ms / xmx_ms, half_ms, gu_ms, swiglu_ms, down_ms, error,
                    case_ok ? "true" : "false");
    }
    sycl::free(dblob, *s); sycl::free(dx, *s); sycl::free(xh, *s); sycl::free(xq, *s); sycl::free(gu, *s);
    sycl::free(hq, *s); sycl::free(scratch, *s); sycl::free(base, *s); sycl::free(cand, *s); sycl::free(ptr, *s);
    sycl::free(start, *s); sycl::free(ng, *s); sycl::free(dst, *s); sycl::free(tok, *s);
    return ok;
}
#endif

static bool pool_cost(const cpu::NativeFmt& f, const std::vector<uint8_t>& blob,
                      const std::vector<float>& x, const std::vector<float>& ref, int nt) {
    using Clock = std::chrono::steady_clock;
    constexpr int groups = 8, layers = 48;
    // ponytail: reuse the real pool and a bank larger than LLC. GPU handoff/sync is excluded;
    // this is a CPU lower bound, an end-to-end hybrid trial is needed if it wins.
    std::vector<uint8_t> bank(blob.size() * groups * layers);
    for (size_t offset = 0; offset < bank.size(); offset += blob.size())
        std::memcpy(bank.data() + offset, blob.data(), blob.size());
    std::vector<uint8_t> act((size_t) nt * cpu::kNativeActBytes);
    for (int t = 0; t < nt; ++t)
        cpu::native_quant_act(f, x.data() + (size_t) t * f.n_embd,
                              act.data() + (size_t) t * cpu::kNativeActBytes);
    std::vector<float> out((size_t) groups * nt * f.n_embd);
    cpu::ExpertJobMulti jobs[groups];
    for (int g = 0; g < groups; ++g) {
        jobs[g].nt = nt;
        for (int t = 0; t < nt; ++t) {
            jobs[g].nact[t] = act.data() + (size_t) t * cpu::kNativeActBytes;
            jobs[g].out[t] = out.data() + ((size_t) g * nt + t) * f.n_embd;
        }
    }
    bool ok = true;
    for (int workers : {1, 0}) {
        cpu::ExpertPool pool(workers, true, workers == 0);
        auto run = [&] {
            for (int l = 0; l < layers; ++l) {
                for (int g = 0; g < groups; ++g)
                    jobs[g].blob = bank.data() + ((size_t) l * groups + g) * blob.size();
                pool.run_split_multi_native(f, jobs, groups);
            }
        };
        run();
        double error = 0;
        for (int g = 0; g < groups; ++g) {
            const auto begin = out.begin() + (size_t) g * ref.size();
            const std::vector<float> row(begin, begin + ref.size());
            const double e = rel(row, ref);
            if (!std::isfinite(e) || e >= 3e-2) ok = false;
            error = std::max(error, e);
        }
        if (!ok) { std::fprintf(stderr, "CPU pool float-reference check failed\n"); return false; }
        std::vector<double> times;
        for (int i = 0; i < 5; ++i) {
            const auto started = Clock::now();
            run();
            times.push_back(std::chrono::duration<double, std::milli>(Clock::now() - started).count());
        }
        std::sort(times.begin(), times.end());
        std::printf("CPU cost GU=%d/down=%d T=%d: %d workers + host=%d, %d layers x %d experts, %.3f ms/window, %.3f ms/layer, rel %.2e\n",
                    f.gu_type, f.d_type, nt, pool.workers(), pool.host_works(), layers, groups,
                    times[2], times[2] / layers, error);
    }
    return ok;
}

int main(int argc, char** argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);   // keep the trail on a crash
#ifdef STRATA_EZPP
    const bool xmx_bench = std::getenv("NATIVE_XMX_BENCH") != nullptr;
    EZPP_ADD_OPTION(xmx_bench ? EZPP_OPT_FORCE_ENABLE : EZPP_OPT_FORCE_DISABLE);
#endif
    if (argc < 2) { std::fprintf(stderr, "usage: native_expert_parity <shard1.gguf> [layer ...]\n"); return 2; }
    strata::GgufFile gguf(argv[1]);
    std::vector<int> layers;
    for (int i = 2; i < argc; ++i) layers.push_back(std::atoi(argv[i]));
    if (layers.empty()) layers = {0, 1, 2, 3, 20, 47};
    const int NT = 3, E = 7;
    const int64_t H = 2560, FF = 640;
    int failures = 0;
    sycl::queue* s = &dpct::get_in_order_queue();
    for (int l : layers) {
        const strata::TensorInfo* t[3] = {};
        const char* roles[3] = {"gate", "up", "down"};
        for (const auto& ti : gguf.tensors())
            for (int r = 0; r < 3; ++r)
                if (ti.name == "blk." + std::to_string(l) + ".ffn_" + roles[r] + "_exps.weight") t[r] = &ti;
        if (!t[0] || !t[1] || !t[2]) { std::printf("layer %d: no expert tensors\n", l); ++failures; continue; }
        cpu::NativeFmt f;
        std::string err;
        if (!cpu::native_fmt((int) t[0]->type, (int) t[2]->type, H, FF, f, err)) {
            std::printf("layer %d: %s\n", l, err.c_str()); ++failures; continue;
        }
        std::vector<uint8_t> blob(f.bytes);
        std::memcpy(blob.data(), gguf.tensor_data(*t[0]) + (size_t) E * f.up_off, f.up_off);
        std::memcpy(blob.data() + f.up_off, gguf.tensor_data(*t[1]) + (size_t) E * f.up_off, f.up_off);
        std::memcpy(blob.data() + f.down_off, gguf.tensor_data(*t[2]) + (size_t) E * (f.bytes - f.down_off),
                    f.bytes - f.down_off);
        // (a) the float reference
        const auto* tg = ggml_get_type_traits((ggml_type) f.gu_type);
        const auto* td = ggml_get_type_traits((ggml_type) f.d_type);
        std::vector<float> G((size_t) FF * H), U((size_t) FF * H), D((size_t) H * FF);
        for (int64_t r = 0; r < FF; ++r) {
            tg->to_float(blob.data() + r * f.gu_row, G.data() + r * H, H);
            tg->to_float(blob.data() + f.up_off + r * f.gu_row, U.data() + r * H, H);
        }
        for (int64_t r = 0; r < H; ++r) td->to_float(blob.data() + f.down_off + r * f.d_row, D.data() + r * FF, FF);
        std::mt19937 rng(11 + l);
        std::normal_distribution<float> nd(0.f, 1.f);
        std::vector<float> x((size_t) NT * H);
        for (auto& v : x) v = nd(rng);
#ifdef STRATA_EZPP
        if (xmx_bench && f.gu_type == 29 && f.d_type == 42) {
            std::vector<float> x4((size_t) 4 * H);
            for (auto& v : x4) v = nd(rng);
            if (!xmx_full_cost(f, blob, x4, s)) ++failures;
        }
#endif
        std::vector<float> ref((size_t) NT * H), got_c((size_t) NT * H), got_g((size_t) NT * H);
        for (int k = 0; k < NT; ++k) {
            std::vector<float> h(FF);
            for (int64_t r = 0; r < FF; ++r) {
                double g = 0, u = 0;
                for (int64_t i = 0; i < H; ++i) { g += (double) G[r * H + i] * x[k * H + i]; u += (double) U[r * H + i] * x[k * H + i]; }
                h[r] = (float) (g / (1.0 + std::exp(-g)) * u);
            }
            for (int64_t r = 0; r < H; ++r) {
                double o = 0;
                for (int64_t i = 0; i < FF; ++i) o += (double) D[r * FF + i] * h[i];
                ref[k * H + r] = (float) o;
            }
        }
        // (b) the CPU
        {
            std::vector<std::vector<uint8_t>> act(NT, std::vector<uint8_t>(cpu::kNativeActBytes));
            std::vector<std::vector<uint8_t>> hq(NT, std::vector<uint8_t>(cpu::kNativeHBytes));
            std::vector<std::vector<float>> ff(NT, std::vector<float>(FF));
            const void* a[NT];
            float* ffp[NT];
            const void* hp[NT];
            float* op[NT];
            for (int k = 0; k < NT; ++k) {
                cpu::native_quant_act(f, x.data() + k * H, act[k].data());
                a[k] = act[k].data();
                ffp[k] = ff[k].data();
            }
            cpu::native_gu_rows(f, blob.data(), a, NT, ffp, 0, (int) FF);
            if (cpu::iq512_supported(f.gu_type)) {
                // ggml's own vec_dot, same Q8_K activations: the reference for both multi-token kernels
                // (float-order differences only)
                const auto* tc = ggml_get_type_traits_cpu((ggml_type) f.gu_type);
                std::vector<float> gref((size_t) NT * FF);
                for (int k = 0; k < NT; ++k)
                    for (int64_t r = 0; r < FF; ++r)
                        tc->vec_dot((int) H, &gref[k * FF + r], 0, blob.data() + r * f.gu_row, 0, a[k], 0, 1);
                double usg = 0.0;
                {   // ggml single-token throughput, the weights cache-resident (compute bound)
                    const int it = 50;
                    auto t0 = std::chrono::steady_clock::now();
                    for (int i = 0; i < it; ++i)
                        for (int64_t r = 0; r < FF; ++r) {
                            float gg, uu;
                            tc->vec_dot((int) H, &gg, 0, blob.data() + r * f.gu_row, 0, a[0], 0, 1);
                            tc->vec_dot((int) H, &uu, 0, blob.data() + f.up_off + r * f.gu_row, 0, a[0], 0, 1);
                        }
                    auto t1 = std::chrono::steady_clock::now();
                    usg = std::chrono::duration<double, std::micro>(t1 - t0).count() / it;
                }
                // one multi-token kernel: parity against ggml's vec_dot, then one-token and NT-token throughput
                auto check = [&](const char* tag, bool is512) {
                    std::vector<float> g((size_t) NT * FF);
                    float* gp[NT];
                    for (int k = 0; k < NT; ++k) gp[k] = g.data() + k * FF;
                    (is512 ? cpu::iq512_rows : cpu::iq256_rows)(f.gu_type, blob.data(), f.gu_row, (int) H, a, NT, gp, 0, (int) FF);
                    const double rg = rel(g, gref);
                    std::printf("          %s %s gate rows vs ggml vec_dot: rel %.2e\n",
                                ggml_type_name((ggml_type) f.gu_type), tag, rg);
                    if (rg > 1e-5) {   // ggml's own vec_dot is the reference: above 1e-5 it is a real mismatch,
                        std::printf("          %s %s gate rows MISMATCH (rel %.2e > 1e-5)\n",
                                    ggml_type_name((ggml_type) f.gu_type), tag, rg);   // not rounding (measured 3e-8)
                        ++failures;
                    }
                    float* f1[1] = {ffp[0]};
                    const int it = 50;
                    const auto gu = is512 ? cpu::iq512_gu_rows : cpu::iq256_gu_rows;
                    auto t0 = std::chrono::steady_clock::now();
                    for (int i = 0; i < it; ++i) gu(f.gu_type, blob.data(), f.gu_row, f.up_off, (int) H, a, 1, f1, 0, (int) FF);
                    auto t1 = std::chrono::steady_clock::now();
                    for (int i = 0; i < it; ++i) gu(f.gu_type, blob.data(), f.gu_row, f.up_off, (int) H, a, NT, ffp, 0, (int) FF);
                    auto t2 = std::chrono::steady_clock::now();
                    const double us1 = std::chrono::duration<double, std::micro>(t1 - t0).count() / it;
                    const double usn = std::chrono::duration<double, std::micro>(t2 - t1).count() / it;
                    std::printf("          gate+up one token one thread: %s %.0f us (%.2f GB/s), ggml %.0f us (%.2f GB/s)\n",
                                tag, us1, 2.0 * f.up_off / us1 / 1e3, usg, 2.0 * f.up_off / usg / 1e3);
                    std::printf("          gate+up %d tokens one thread: %s %.0f us vs ggml %d x %.0f us\n", NT, tag, usn, NT, usg);
                };
                if (cpu::cpu_avx512_ok()) check("avx512", true);   // guarded: the binary runs on AVX-2 CPUs too
                check("avx2", false);
            }
            for (int k = 0; k < NT; ++k) {
                cpu::native_quant_h(f, ff[k].data(), hq[k].data());
                hp[k] = hq[k].data();
                op[k] = got_c.data() + k * H;
            }
            cpu::native_down_rows(f, blob.data(), hp, NT, op, 0, (int) H);
            if (f.d_type == 42) {
                // (b2) the GGUF-layout Q2_0 kernel the pool uses for Q2_0 down projections - the AVX-512 one
                // where the CPU has it, the AVX-2 one (q2_avx2.cpp) where it does not.  Calling the AVX-512
                // kernel unconditionally faults on a Zen 2/3 CPU.
                std::vector<cpu::ActQ> a2(NT);
                const cpu::ActQ* ap[NT];
                std::vector<float> alt((size_t) NT * H);
                float* altp[NT];
                for (int k = 0; k < NT; ++k) {
                    cpu::act_quant_any(ff[k].data(), (int) FF, a2[k]);   // gated: AVX-512 or AVX-2 per CPU
                    ap[k] = &a2[k];
                    altp[k] = alt.data() + k * H;
                }
                cpu::q2_rows_any(blob.data() + f.down_off, f.d_row, (int) (FF / 64), ap, NT, altp, 0, (int) H);
                std::printf("          q2_0 %s down vs ggml down: rel %.2e\n",
                            cpu::cpu_avx512_ok() ? "AVX-512" : "AVX-2", rel(alt, got_c));
            }
            if (f.d_type == 20) {
                // (b3) the IQ4_NL multi-token AVX-2 kernel the pool now uses for IQ4_NL down projections,
                // against ggml-cpu's single-token vec_dot on the SAME Q8_0 activations (h), plus timing.
                std::vector<float> alt((size_t) NT * H), refd((size_t) NT * H);
                float* altp[NT];
                for (int k = 0; k < NT; ++k) altp[k] = alt.data() + k * H;
                cpu::iq4nl256_down_rows(blob.data() + f.down_off, f.d_row, (int) FF, hp, NT, altp, 0, (int) H);
                const auto* tdc = ggml_get_type_traits_cpu((ggml_type) f.d_type);
                for (int k = 0; k < NT; ++k)
                    for (int64_t r = 0; r < H; ++r)
                        tdc->vec_dot((int) FF, refd.data() + k * H + r, 0,
                                     blob.data() + f.down_off + (size_t) r * f.d_row, 0, hp[k], 0, 1);
                const double rdn = rel(alt, refd);
                std::printf("          iq4_nl AVX-2 multi-token down vs ggml down: rel %.2e\n", rdn);
                if (rdn > 1e-5) {   // measured 1e-7; same reasoning as the gate/up rows above
                    std::printf("          iq4_nl down MISMATCH (rel %.2e > 1e-5)\n", rdn);
                    ++failures;
                }
                const int it = 50;
                const auto t0 = std::chrono::steady_clock::now();
                for (int i = 0; i < it; ++i)
                    cpu::iq4nl256_down_rows(blob.data() + f.down_off, f.d_row, (int) FF, hp, NT, altp, 0, (int) H);
                const auto t1 = std::chrono::steady_clock::now();
                for (int i = 0; i < it; ++i)
                    for (int k = 0; k < NT; ++k)
                        for (int64_t r = 0; r < H; ++r)
                            tdc->vec_dot((int) FF, refd.data() + k * H + r, 0,
                                        blob.data() + f.down_off + (size_t) r * f.d_row, 0, hp[k], 0, 1);
                const auto t2 = std::chrono::steady_clock::now();
                const double usn = std::chrono::duration<double, std::micro>(t1 - t0).count() / it;
                const double usg = std::chrono::duration<double, std::micro>(t2 - t1).count() / it;
                std::printf("          down %d tokens one thread: avx2 %.0f us (%.2f GB/s) vs ggml %.0f us (%.2f GB/s)\n",
                            NT, usn, (double) (f.bytes - f.down_off) / usn / 1e3, usg,
                            (double) (f.bytes - f.down_off) / usg / 1e3);
            }
        }
        if (std::getenv("NATIVE_POOL_BENCH") && !pool_cost(f, blob, x, ref, NT)) ++failures;
        // (c) the GPU: one group holding the NT entries
        {
            const auto L = strata::kernels::native_expert_layout(f.gu_type, f.d_type, H, FF);
            void *dblob, *dx, *dxq, *dscr;
            float* dout;
            unsigned long long* dptr;
            int32_t *dstart, *dn, *ddst, *dtok;
            dblob = (decltype(dblob)) sycl::malloc_device(blob.size(), *s);
            dx = (decltype(dx)) sycl::malloc_device(x.size() * 4, *s);
            dxq = (decltype(dxq)) sycl::malloc_device((size_t) NT * H / 32 * 36, *s);
            dscr = (decltype(dscr)) sycl::malloc_device(strata::kernels::native_expert_scratch_bytes(NT, FF), *s);
            dout = (decltype(dout)) sycl::malloc_device((size_t) NT * H * 4, *s);
            dptr = (decltype(dptr)) sycl::malloc_device(8, *s);
            dstart = (decltype(dstart)) sycl::malloc_device(8, *s);
            dn = (decltype(dn)) sycl::malloc_device(4, *s);
            ddst = (decltype(ddst)) sycl::malloc_device(NT * 4, *s);
            dtok = (decltype(dtok)) sycl::malloc_device(NT * 4, *s);
            s->memcpy(dblob, blob.data(), blob.size()).wait();
            s->memcpy(dx, x.data(), x.size() * 4).wait();
            const unsigned long long p = (unsigned long long) dblob;
            const int32_t st[2] = {0, NT}, one = 1, idx[NT] = {0, 1, 2};
            s->memcpy(dptr, &p, 8).wait();
            s->memcpy(dstart, st, 8).wait();
            s->memcpy(dn, &one, 4).wait();
            s->memcpy(ddst, idx, NT * 4).wait();
            s->memcpy(dtok, idx, NT * 4).wait();
            strata::kernels::quantize_q8_1_rows((const float*) dx, NT, H, dxq, s);
            strata::kernels::native_expert_grouped(L, dptr, dstart, dn, ddst, dtok, 1, NT, dxq, dscr, dout, s);
            s->wait();
            s->memcpy(got_g.data(), dout, got_g.size() * 4).wait();
            if (std::getenv("NATIVE_BENCH") != nullptr) {   // SYCL port: one layer's worth of hits, timed
                // G distinct experts (copies of this blob at different addresses), E entries each = a T-token window
                const bool access_bench = std::getenv("NATIVE_POOL_BENCH") != nullptr;
                const int G = access_bench ? 8 : 10, E = access_bench ? NT : 4, NE = G * E, iters = 20;
                std::vector<void*> blobs((size_t) G);
                std::vector<unsigned long long> ptrs((size_t) G);
                for (int g = 0; g < G; ++g) {
                    blobs[(size_t) g] = sycl::malloc_device(blob.size(), *s);
                    s->memcpy(blobs[(size_t) g], blob.data(), blob.size()).wait();
                    ptrs[(size_t) g] = (unsigned long long) blobs[(size_t) g];
                }
                std::vector<int32_t> starts((size_t) G + 1), dst((size_t) NE), tokv((size_t) NE);
                for (int g = 0; g <= G; ++g) starts[(size_t) g] = g * E;
                for (int e = 0; e < NE; ++e) { dst[(size_t) e] = e; tokv[(size_t) e] = e % NT; }
                unsigned long long* bptr = sycl::malloc_device<unsigned long long>((size_t) G, *s);
                int32_t* bstart = sycl::malloc_device<int32_t>((size_t) G + 1, *s);
                int32_t* bn = sycl::malloc_device<int32_t>(1, *s);
                int32_t* bdst = sycl::malloc_device<int32_t>((size_t) NE, *s);
                int32_t* btok = sycl::malloc_device<int32_t>((size_t) NE, *s);
                void* bscr = sycl::malloc_device(strata::kernels::native_expert_scratch_bytes(NE, FF), *s);
                float* bout = sycl::malloc_device<float>((size_t) NE * H, *s);
                const int32_t gn = G;
                s->memcpy(bptr, ptrs.data(), (size_t) G * 8).wait();
                s->memcpy(bstart, starts.data(), ((size_t) G + 1) * 4).wait();
                s->memcpy(bn, &gn, 4).wait();
                s->memcpy(bdst, dst.data(), (size_t) NE * 4).wait();
                s->memcpy(btok, tokv.data(), (size_t) NE * 4).wait();
                strata::kernels::native_expert_grouped(L, bptr, bstart, bn, bdst, btok, G, NE, dxq, bscr, bout, s);
                s->wait();
                const auto b0 = std::chrono::steady_clock::now();
                for (int i = 0; i < iters; ++i)
                    strata::kernels::native_expert_grouped(L, bptr, bstart, bn, bdst, btok, G, NE, dxq, bscr, bout, s);
                s->wait();
                const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - b0).count() / iters;
                std::printf("          GPU grouped: %d experts x %d entries: %.3f ms per layer call  (%.1f GB/s of expert bytes)\n",
                            G, E, ms, (double) G * (double) blob.size() / ms / 1e6);
                if (access_bench) {
                    // ponytail: match the CPU's 48 distinct banks and 8 x T inputs. Quantization,
                    // routing and result handoff remain excluded; this screens execution costs only.
                    constexpr int layers = 48;
                    const size_t layer_bytes = (size_t) G * blob.size(), bank_bytes = layers * layer_bytes;
                    auto* host = sycl::malloc_host<uint8_t>(bank_bytes, *s);
                    auto* arena = sycl::malloc_device<uint8_t>(bank_bytes, *s);
                    auto* stage = sycl::malloc_device<uint8_t>(layer_bytes, *s);
                    auto* plan = sycl::malloc_device<unsigned long long>((size_t) layers * G, *s);
                    if (!host || !arena || !stage || !plan) throw std::bad_alloc();
                    for (size_t offset = 0; offset < bank_bytes; offset += blob.size())
                        std::memcpy(host + offset, blob.data(), blob.size());
                    s->memcpy(arena, host, bank_bytes).wait();
                    std::vector<unsigned long long> pointers((size_t) layers * G);
                    std::vector<float> baseline((size_t) NE * H), result(baseline.size());
                    for (int mode = 0; mode < 3; ++mode) {
                        for (int i = 0; i < layers * G; ++i) {
                            auto* base = mode == 0 ? arena : mode == 1 ? host : stage;
                            const int index = mode == 2 ? i % G : i;
                            pointers[(size_t) i] = (unsigned long long) (base + (size_t) index * blob.size());
                        }
                        s->memcpy(plan, pointers.data(), pointers.size() * sizeof(pointers[0])).wait();
                        auto run = [&] {
                            for (int layer = 0; layer < layers; ++layer) {
                                if (mode == 2) s->memcpy(stage, host + (size_t) layer * layer_bytes, layer_bytes);
                                strata::kernels::native_expert_grouped(L, plan + (size_t) layer * G,
                                    bstart, bn, bdst, btok, G, NE, dxq, bscr, bout, s);
                                s->wait();
                            }
                        };
                        run();
                        s->memcpy(result.data(), bout, result.size() * sizeof(float)).wait();
                        double error = 0;
                        bool ok = true;
                        for (int g = 0; g < G; ++g) {
                            const auto begin = result.begin() + (size_t) g * ref.size();
                            const std::vector<float> row(begin, begin + ref.size());
                            const double e = rel(row, ref);
                            ok = ok && std::isfinite(e) && e < 3e-2;
                            error = std::max(error, e);
                        }
                        if (mode == 0) baseline = result;
                        else ok = ok && std::memcmp(result.data(), baseline.data(), result.size() * sizeof(float)) == 0;
                        if (!ok) { std::fprintf(stderr, "GPU access/reference check failed, mode %d\n", mode); ++failures; }
                        std::vector<double> times;
                        for (int i = 0; i < 5; ++i) {
                            const auto started = std::chrono::steady_clock::now();
                            run();
                            times.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count());
                        }
                        std::sort(times.begin(), times.end());
                        std::printf("GPU access GU=%d/down=%d T=%d: %s, %d layers x %d experts, %.3f ms/window, %.3f ms/layer, rel %.2e, %s\n",
                            f.gu_type, f.d_type, NT, mode == 0 ? "VRAM" : mode == 1 ? "mapped" : "DMA+compute",
                            layers, G, times[2], times[2] / layers, error, ok ? "ok" : "FAIL");
                    }
                    sycl::free(host, *s); sycl::free(arena, *s); sycl::free(stage, *s); sycl::free(plan, *s);
                }
                for (int g = 0; g < G; ++g) sycl::free(blobs[(size_t) g], *s);
                sycl::free(bptr, *s); sycl::free(bstart, *s); sycl::free(bn, *s); sycl::free(bdst, *s); sycl::free(btok, *s);
                sycl::free(bscr, *s); sycl::free(bout, *s);
            }
            sycl::free(dblob, *s); sycl::free(dx, *s); sycl::free(dxq, *s); sycl::free(dscr, *s); sycl::free(dout, *s); sycl::free(dptr, *s);
            sycl::free(dstart, *s); sycl::free(dn, *s); sycl::free(ddst, *s); sycl::free(dtok, *s);
        }
        const double ec = rel(got_c, ref), eg = rel(got_g, ref), ecg = rel(got_c, got_g);
        const bool ok = ec < 3e-2 && eg < 3e-2 && std::isfinite(ec) && std::isfinite(eg);
        std::printf("layer %2d  %-8s/%-7s blob %8zu  cpu rel %.2e  gpu rel %.2e  cpu-gpu %.2e  %s\n", l,
                    ggml_type_name((ggml_type) f.gu_type), ggml_type_name((ggml_type) f.d_type), f.bytes, ec, eg, ecg,
                    ok ? "ok" : "FAIL");
        if (!ok) ++failures;
    }
#ifdef STRATA_EZPP
    if (xmx_bench) {
        const char* report = std::getenv("NATIVE_XMX_EZPP_OUT");
        EZPP_SAVE(report && *report ? report : "native-xmx-ezpp.txt");
    }
#endif
    std::printf("native_expert_parity: %d failures\n", failures);
    return failures ? 1 : 0;
}
