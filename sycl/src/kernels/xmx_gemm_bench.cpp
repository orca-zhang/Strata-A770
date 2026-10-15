// xmx_gemm_bench: the XMX quantized GEMM against dequant + oneMKL on random experts.
//   xmx_gemm_bench [ty=18] [M=96] [mode=gu|down] [reps=50] [variant=0..3] [ldy_pad=0]
// reps=0 runs correctness only. Variants must match packed XMX bitwise and preserve output padding.
#include "strata/kernels/iq_kernels.hpp"
#include "strata/kernels/xmx_gemm.hpp"
#include <sycl/sycl.hpp>
#include <dpct/dpct.hpp>
#include <dpct/blas_utils.hpp>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <random>
#include <string>
#include <vector>
static uint16_t f2h(float f) { return sycl::bit_cast<uint16_t>(sycl::half(f)); }
int main(int argc, char** argv) try {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    const int ty = argc > 1 ? std::atoi(argv[1]) : 18;
    const int M = argc > 2 ? std::atoi(argv[2]) : 96;
    const std::string mode = argc > 3 ? argv[3] : "gu";
    const int reps = argc > 4 ? std::atoi(argv[4]) : 50;
    const int variant = argc > 5 ? std::atoi(argv[5]) : 0;
    const int pad = argc > 6 ? std::atoi(argv[6]) : 0;
    if (M <= 0 || M > 8192 || reps < 0 || reps > 100000 || variant < 0 || variant > 3 || pad < 0 || pad > 256 ||
        (mode != "gu" && mode != "down") ||
        (ty != 16 && ty != 17 && ty != 18 && ty != 20 && ty != 21 && ty != 22 && ty != 29 && ty != 42)) {
        std::fprintf(stderr, "invalid benchmark arguments\n"); return 2;
    }
    const bool gu = mode == "gu";
    const int64_t K = gu ? 2560 : 640, n_out = gu ? 1280 : 2560, n_ff = 640;
    const int64_t ldy = n_out + pad;
    const int blk_elems = (ty == 20) ? 32 : (ty == 42) ? 64 : 256;
    const size_t blk_bytes = strata::kernels::iq_row_bytes(ty, blk_elems);
    const size_t row_bytes = strata::kernels::iq_row_bytes(ty, K);
    if (row_bytes == 0) { std::printf("type %d not supported\n", ty); return 2; }
    const int64_t rows_per_blob = gu ? n_ff : n_out;
    std::mt19937 rng(7);
    auto blob = [&]() {
        std::vector<uint8_t> b(rows_per_blob * row_bytes);
        for (auto& x : b) x = (uint8_t) rng();
        for (size_t o = 0; o + 1 < b.size(); o += blk_bytes) {
            if (ty == 29) {   // IQ1_M distributes its FP16 scale across four high nibbles.
                const int nibble[] = {0, 0, 12, 3};
                for (int i = 0; i < 4; ++i) {
                    const size_t j = o + blk_bytes - 8 + 2 * i + 1;
                    b[j] = (b[j] & 15) | (nibble[i] << 4);
                }
            } else { b[o] = 0x00; b[o + 1] = 0x3C; }   // d = 1.0 in every block
        }
        return b;
    };
    std::vector<uint8_t> hg = blob(), hu = gu ? blob() : std::vector<uint8_t>();
    std::vector<uint16_t> hx((size_t) M * K);
    std::normal_distribution<float> nd(0.f, 1.f);
    for (auto& x : hx) x = f2h(nd(rng));
    std::printf("queue...\n");
    sycl::queue* q = &dpct::get_in_order_queue();
    std::printf("device: %s\n", q->get_device().get_info<sycl::info::device::name>().c_str());
    std::printf("malloc...\n");
    uint8_t* dg = sycl::malloc_device<uint8_t>(hg.size(), *q);
    uint8_t* du = gu ? sycl::malloc_device<uint8_t>(hu.size(), *q) : nullptr;
    uint16_t* dx = sycl::malloc_device<uint16_t>(hx.size(), *q);
    uint16_t* dw = sycl::malloc_device<uint16_t>((size_t) n_out * K, *q);
    float* y_ref = sycl::malloc_device<float>((size_t) M * ldy, *q);
    float* y_xmx = sycl::malloc_device<float>((size_t) M * ldy, *q);
    float* y_packed = sycl::malloc_device<float>((size_t) M * ldy, *q);
    if (!dg || (gu && !du) || !dx || !dw || !y_ref || !y_xmx || !y_packed) {
        std::fprintf(stderr, "device allocation failed\n"); return 2;
    }
    std::printf("copy...\n");
    q->memcpy(dg, hg.data(), hg.size()).wait();
    if (gu) q->memcpy(du, hu.data(), hu.size()).wait();
    q->memcpy(dx, hx.data(), hx.size() * 2).wait();
    constexpr float sentinel = -987654.f;
    q->fill(y_ref, sentinel, (size_t) M * ldy);
    q->fill(y_xmx, sentinel, (size_t) M * ldy);
    q->fill(y_packed, sentinel, (size_t) M * ldy);
    // the reference path: the engine's dequant kernels + oneMKL (what prefill.cpp runs today)
    dpct::blas::descriptor_ptr h = new dpct::blas::descriptor();
    h->set_queue(q);
    auto dequant = [&]() {
        if (gu) strata::kernels::iq_dequant_gu_f16(ty, dg, du, n_ff, K, dw, q);
        else strata::kernels::iq_dequant_f16(ty, dg, n_out * K, dw, q);
    };
    auto mkl = [&]() {
        const float alpha = 1.0f, beta = 0.0f;
        dpct::blas::gemm(h, oneapi::mkl::transpose::trans, oneapi::mkl::transpose::nontrans, (int) n_out, M, (int) K, &alpha, dw,
                         dpct::library_data_t::real_half, (int) K, dx, dpct::library_data_t::real_half, (int) K, &beta, y_ref,
                         dpct::library_data_t::real_float, (int) ldy, dpct::compute_type::f32);
    };
    auto xmx = [&](int v, float* out) {
        return strata::kernels::xmx_gemm_iq_variant(ty, dg, du, K, (int) n_out, dx, M, out, ldy, q, v);
    };
    std::printf("reference dequant...\n");
    dequant(); mkl(); q->wait();
    auto time = [&](auto&& fn) {
        for (int i = 0; i < 3; ++i) fn(); q->wait();
        double samples[5];
        for (double& ms : samples) {
            const auto t0 = std::chrono::steady_clock::now();
            for (int i = 0; i < reps; ++i) fn();
            q->wait();
            ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() / reps;
        }
        std::sort(samples, samples + 5);
        return samples[2];
    };
    std::printf("xmx launch...\n");
    if (!xmx(0, y_packed) || !xmx(variant, y_xmx)) {
        std::printf("xmx_gemm_iq refused\n");
        return 3;
    }
    std::printf("xmx wait...\n");
    q->wait();
    std::printf("readback...\n");
    std::vector<float> r((size_t) M * ldy), x(r.size()), packed(r.size());
    q->memcpy(r.data(), y_ref, r.size() * 4).wait();
    q->memcpy(x.data(), y_xmx, x.size() * 4).wait();
    q->memcpy(packed.data(), y_packed, packed.size() * 4).wait();
    double maxref = 0, maxerr = 0, sumerr = 0; size_t bad = 0, nonfinite = 0, mismatches = 0, padding_bad = 0;
    for (size_t i = 0; i < r.size(); ++i) {
        if (i % ldy >= (size_t) n_out) {
            padding_bad += r[i] != sentinel || x[i] != sentinel || packed[i] != sentinel;
            continue;
        }
        if (!std::isfinite(r[i]) || !std::isfinite(x[i]) || !std::isfinite(packed[i])) { ++nonfinite; continue; }
        mismatches += sycl::bit_cast<uint32_t>(x[i]) != sycl::bit_cast<uint32_t>(packed[i]);
        maxref = std::max(maxref, (double) std::fabs(r[i]));
        const double e = std::fabs((double) r[i] - x[i]);
        maxerr = std::max(maxerr, e); sumerr += e;
        if (e > 1e-2 * (1.0 + std::fabs(r[i]))) ++bad;
    }
    const size_t count = (size_t) M * n_out;
    std::printf("ty %d %s M %d variant %d: max |ref| %.3g, max err %.3g, mean err %.3g, %zu of %zu outside 1%% "
                "(nonfinite %zu, packed mismatches %zu, padding errors %zu)\n", ty,
                mode.c_str(), M, variant, maxref, maxerr, sumerr / count, bad, count, nonfinite, mismatches, padding_bad);
    if (nonfinite || bad > count / 1000 || mismatches || padding_bad) return 1;
    if (reps == 0) return 0;
    const double t_dq = time([&] { dequant(); }), t_mkl = time([&] { mkl(); }), t_both = time([&] { dequant(); mkl(); });
    const double t_xmx = time([&] { xmx(variant, y_xmx); });
    const double flop = 2.0 * M * n_out * K;
    std::printf("dequant %.3f ms + mkl %.3f ms = %.3f ms (%.0f GFLOP/s on the GEMM)  |  xmx %.3f ms (%.0f GFLOP/s, %.2fx)\n", t_dq,
                t_mkl, t_both, flop / t_mkl / 1e6, t_xmx, flop / t_xmx / 1e6, t_both / t_xmx);
    std::printf("RESULT {\"ty\":%d,\"mode\":\"%s\",\"M\":%d,\"variant\":%d,\"reps\":%d,\"pad\":%d,"
                "\"dequant_ms\":%.6f,\"mkl_ms\":%.6f,\"baseline_ms\":%.6f,\"xmx_ms\":%.6f,"
                "\"max_error\":%.9g,\"packed_mismatches\":%zu,\"nonfinite\":%zu,\"padding_errors\":%zu}\n",
                ty, mode.c_str(), M, variant, reps, pad, t_dq, t_mkl, t_both, t_xmx, maxerr, mismatches, nonfinite, padding_bad);
    delete h;
    sycl::free(dg, *q); if (du) sycl::free(du, *q);
    sycl::free(dx, *q); sycl::free(dw, *q);
    sycl::free(y_ref, *q); sycl::free(y_xmx, *q); sycl::free(y_packed, *q);
    return 0;
} catch (const sycl::exception& e) {
    std::fprintf(stderr, "SYCL exception: %s\n", e.what());
    return 10;
} catch (const std::exception& e) {
    std::fprintf(stderr, "exception: %s\n", e.what());
    return 11;
}
