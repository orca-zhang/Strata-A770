// sycl/src/kernels/mmvq_bench.cpp - SYCL port: time the dense native_mmvq kernels on random weights.
//   mmvq_bench [n_in] [n_out] [ncols]      (weights are random bytes: dequant cost is data-independent)
#include "strata/kernels/native_mmvq.hpp"
#include <sycl/sycl.hpp>
#include <dpct/dpct.hpp>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <cmath>
#include <exception>
#include <algorithm>
#include <cstring>

int main_impl(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    const int n_in = argc > 1 ? std::atoi(argv[1]) : 2560, n_out = argc > 2 ? std::atoi(argv[2]) : 2560, ncols = argc > 3 ? std::atoi(argv[3]) : 4;
    sycl::queue* s = &dpct::get_in_order_queue();
    std::printf("device: %s\n", s->get_device().get_info<sycl::info::device::name>().c_str());
    const bool check_grf = argc > 4 && std::strcmp(argv[4], "--check-grf") == 0;
    const auto device = s->get_device();
    if (check_grf && (!device.is_gpu() || device.get_info<sycl::info::device::vendor_id>() != 0x8086 ||
        device.get_info<sycl::ext::oneapi::experimental::info::device::architecture>() !=
            sycl::ext::oneapi::experimental::architecture::intel_gpu_acm_g10)) {
        std::printf("Q6_K GRF check skipped (requires ACM G10)\n");
        return 0;
    }
    // Q6_K: 210 bytes per 256 weights
    const size_t wbytes = (size_t) n_out * (n_in / 256) * 210;
    std::vector<uint8_t> hw(wbytes);
    for (size_t i = 0; i < wbytes; ++i) hw[i] = (uint8_t) (i * 2654435761u >> 13);
    for (size_t b = 0; b < wbytes / 210; ++b) { hw[b * 210 + 208] = 0x00; hw[b * 210 + 209] = 0x3c; }   // d = 1.0 (finite)
    std::vector<float> hx((size_t) ncols * n_in);
    for (size_t i = 0; i < hx.size(); ++i) hx[i] = (float) ((i * 7919) % 1000) / 500.f - 1.f;
    void* w = sycl::malloc_device(wbytes, *s);
    float* x = sycl::malloc_device<float>(hx.size(), *s);
    void* xq = sycl::malloc_device((size_t) ncols * (n_in / 32) * 36, *s);
    float* y = sycl::malloc_device<float>((size_t) ncols * n_out, *s);
    s->memcpy(w, hw.data(), wbytes).wait(); s->memcpy(x, hx.data(), hx.size() * 4).wait();
    std::printf("quantize...\n");
    strata::kernels::native_quantize_q8_1(x, xq, n_in, ncols, s);
    s->wait();
    std::printf("quantize done\n");
    if (check_grf) {
        std::vector<float> ref((size_t) ncols * n_out), got(ref.size());
        for (int large : {0, 1}) {
#ifdef _WIN32
            _putenv_s("STRATA_MMVQ_LARGE_GRF", large ? "1" : "0");
#else
            setenv("STRATA_MMVQ_LARGE_GRF", large ? "1" : "0", 1);
#endif
            strata::kernels::native_q6_k_mmvq(w, xq, y, n_in, n_out, ncols, s);
            auto& out = large ? got : ref;
            s->memcpy(out.data(), y, out.size() * sizeof(float)).wait_and_throw();
        }
        const bool equal = std::memcmp(ref.data(), got.data(), ref.size() * sizeof(float)) == 0 &&
            std::all_of(got.begin(), got.end(), [](float v) { return std::isfinite(v); });
        std::printf("Q6_K GRF 128/256: %s\n", equal ? "bitwise equal" : "FAIL");
        sycl::free(w, *s); sycl::free(x, *s); sycl::free(xq, *s); sycl::free(y, *s);
        return equal ? 0 : 1;
    }
    if (argc > 4 && std::strcmp(argv[4], "--repack224") == 0) {
        std::vector<uint8_t> padded(wbytes / 210 * 224, 0);
        for (size_t b = 0; b < wbytes / 210; ++b) std::memcpy(padded.data() + b * 224, hw.data() + b * 210, 210);
        void* wp = sycl::malloc_device(padded.size(), *s);
        if (!wp) throw std::runtime_error("repack allocation failed");
        s->memcpy(wp, padded.data(), padded.size()).wait();
        std::vector<float> ref((size_t)ncols * n_out), got(ref.size());
        strata::kernels::native_q6_k_mmvq(w, xq, y, n_in, n_out, ncols, s);
        s->memcpy(ref.data(), y, ref.size() * 4).wait();
        strata::kernels::native_q6_k_mmvq_stride224(wp, xq, y, n_in, n_out, ncols, s);
        s->memcpy(got.data(), y, got.size() * 4).wait();
        const bool match = std::memcmp(ref.data(), got.data(), got.size() * 4) == 0 &&
            std::all_of(got.begin(), got.end(), [](float v) { return std::isfinite(v); });
        std::printf("Q6_K stride224 %d x %d cols=%d: %s\n", n_in, n_out, ncols, match ? "bitwise equal" : "FAIL");
        if (!match) return 1;
        for (bool pad : {false, true}) {
            std::vector<double> times;
            for (int batch = 0; batch < 8; ++batch) {
                const auto start = std::chrono::steady_clock::now();
                for (int i = 0; i < 100; ++i) {
                    if (pad) strata::kernels::native_q6_k_mmvq_stride224(wp, xq, y, n_in, n_out, ncols, s);
                    else strata::kernels::native_q6_k_mmvq(w, xq, y, n_in, n_out, ncols, s);
                }
                s->wait_and_throw();
                if (batch >= 3) times.push_back(std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count() / 100);
            }
            std::sort(times.begin(), times.end());
            std::printf("stride %d: %.3f us/call, median of 5 batches\n", pad ? 224 : 210, times[2]);
        }
        sycl::free(wp, *s); sycl::free(w, *s); sycl::free(x, *s); sycl::free(xq, *s); sycl::free(y, *s);
        return 0;
    }
    if (std::getenv("STRATA_MMVQ_CHECK_SHARED") && std::atoi(std::getenv("STRATA_MMVQ_CHECK_SHARED")) != 0) {
        // correctness: the 16-byte kernel against the shared one
        std::vector<float> y0((size_t) ncols * n_out), y1(y0.size());
        std::printf("shared q6_k...\n");
        strata::kernels::native_mmvq_set_q6k_wide(false);
        strata::kernels::native_q6_k_mmvq(w, xq, y, n_in, n_out, ncols, s); s->wait(); s->memcpy(y0.data(), y, y0.size() * 4).wait();
        std::printf("wide q6_k...\n");
        strata::kernels::native_mmvq_set_q6k_wide(true);
        strata::kernels::native_q6_k_mmvq(w, xq, y, n_in, n_out, ncols, s); s->wait(); s->memcpy(y1.data(), y, y1.size() * 4).wait();
        double num = 0, den = 0; for (size_t i = 0; i < y0.size(); ++i) { num += std::fabs((double) y0[i] - y1[i]); den += std::fabs((double) y0[i]); }
        std::printf("wide vs shared: rel %.3e over %zu outputs (first %g vs %g)\n", num / (den > 0 ? den : 1), y0.size(), y0[0], y1[0]);
        strata::kernels::native_mmvq_set_q6k_wide(std::getenv("STRATA_MMVQ_WIDE") == nullptr || std::atoi(std::getenv("STRATA_MMVQ_WIDE")) != 0);
    } else {
        // ponytail: the old shared Q6_K kernel hangs on Windows/A770; the optimized wide path is the measured path.
        std::printf("shared q6_k check skipped\n");
    }
    std::printf("one q6_k launch...\n");
    strata::kernels::native_q6_k_mmvq(w, xq, y, n_in, n_out, ncols, s);
    std::printf("one q6_k wait...\n");
    s->wait();
    std::printf("one q6_k done\n");
    {   // warm up until the clocks are up (~300 ms of work), then time
        const int warmup_ms = std::getenv("STRATA_MMVQ_WARMUP_MS") ? std::atoi(std::getenv("STRATA_MMVQ_WARMUP_MS")) : 300;
        std::printf("warmup...\n");
        const auto w0 = std::chrono::steady_clock::now();
        while (std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - w0).count() < warmup_ms) {
            for (int i = 0; i < 20; ++i) strata::kernels::native_q6_k_mmvq(w, xq, y, n_in, n_out, ncols, s);
            s->wait();
        }
        std::printf("warmup done\n");
    }
    const int it = std::getenv("STRATA_MMVQ_ITERS") ? std::atoi(std::getenv("STRATA_MMVQ_ITERS")) : 400;
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < it; ++i) strata::kernels::native_q6_k_mmvq(w, xq, y, n_in, n_out, ncols, s);
    s->wait();
    const double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count() / it;
    std::vector<float> hy((size_t) ncols * n_out);
    s->memcpy(hy.data(), y, hy.size() * sizeof(float)).wait_and_throw();
    if (!std::all_of(hy.begin(), hy.end(), [](float v) { return std::isfinite(v); })) return 1;
    unsigned long long cs = 1469598103934665603ull;
    for (float v : hy) { uint32_t u; std::memcpy(&u, &v, sizeof(u)); cs = (cs ^ u) * 1099511628211ull; }
    std::printf("q6_k mmvq %d x %d, %d cols: %.1f us  (%.1f GB/s of weights)  checksum %016llx\n", n_in, n_out, ncols, us, wbytes / us / 1e3, cs);
    return 0;
}

int main(int argc, char** argv) {
    try {
        return main_impl(argc, argv);
    } catch (const sycl::exception& e) {
        std::fprintf(stderr, "SYCL exception: %s\n", e.what());
        return 2;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "exception: %s\n", e.what());
        return 2;
    }
}
