// sycl/src/kernels/mmvq_sg_bench.cpp - SYCL port: the decode mmvq kernels on the Coder's dense shapes and types.
//   mmvq_sg_bench   (run with STRATA_MMVQ_SG=16 and =32; prints us per call and a checksum of the outputs)
#include "strata/kernels/native_mmvq.hpp"
#include <sycl/sycl.hpp>
#include <dpct/dpct.hpp>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#include <algorithm>
#include <cstdlib>

static void head_sg16_env(const char* value) {
#ifdef _WIN32
    _putenv_s("STRATA_IQ4_XS_SG16", value);
#else
    setenv("STRATA_IQ4_XS_SG16", value, 1);
#endif
}

static int head_sg16_check() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    auto& q = dpct::get_in_order_queue();
    std::printf("IQ4_XS SG16 head check: %s\n", q.get_device().get_info<sycl::info::device::name>().c_str());
    constexpr int ni = 2560;
    for (int no : {7, 106299, 248320}) {
        const size_t wb = strata::kernels::native_mmvq_weight_bytes(23, ni, no);
        std::vector<uint8_t> hw(wb);
        for (size_t i = 0; i < wb; ++i) hw[i] = (uint8_t)(i * 2654435761u >> 13) & 0x3b;
        std::vector<float> hx(8 * ni);
        for (size_t i = 0; i < hx.size(); ++i) hx[i] = (float)((i * 7919) % 1000) / 500.f - 1.f;
        auto* w = sycl::malloc_device<uint8_t>(wb, q);
        auto* x = sycl::malloc_device<float>(hx.size(), q);
        auto* xq = sycl::malloc_device<uint8_t>(8 * (ni / 32) * 36, q);
        auto* y = sycl::malloc_device<float>((size_t)8 * no, q);
        if (!w || !x || !xq || !y) { std::fprintf(stderr, "head check allocation failed\n"); return 1; }
        q.memcpy(w, hw.data(), wb).wait();
        q.memcpy(x, hx.data(), hx.size() * sizeof(float)).wait();
        strata::kernels::native_quantize_q8_1(x, xq, ni, 8, &q);
        for (int nc = 1; nc <= 8; ++nc) {
            std::vector<float> ref((size_t)nc * no), got(ref.size());
            double timing[2] = {};
            for (int variant = 0; variant < 2; ++variant) {
                head_sg16_env(variant ? "1" : "0");
                auto run = [&] { strata::kernels::native_mmvq(23, w, xq, y, ni, no, nc, &q); };
                run();
                auto& output = variant ? got : ref;
                q.memcpy(output.data(), y, output.size() * sizeof(float)).wait();
                if (variant && (std::memcmp(ref.data(), got.data(), got.size() * sizeof(float)) ||
                    !std::all_of(got.begin(), got.end(), [](float v) { return std::isfinite(v); }))) {
                    std::fprintf(stderr, "FAIL: no=%d cols=%d\n", no, nc); return 1;
                }
                std::vector<double> times;
                for (int batch = 0; no > 7 && batch < 9; ++batch) {
                    const auto start = std::chrono::steady_clock::now();
                    for (int it = 0; it < 20; ++it) run();
                    q.wait_and_throw();
                    if (batch >= 4) times.push_back(std::chrono::duration<double, std::micro>(
                        std::chrono::steady_clock::now() - start).count() / 20);
                }
                if (!times.empty()) { std::sort(times.begin(), times.end()); timing[variant] = times[2]; }
            }
            std::printf("IQ4_XS %d x %d cols=%d: bitwise equal; SG32 %.3f, SG16 %.3f us/call\n",
                        ni, no, nc, timing[0], timing[1]);
        }
        sycl::free(w, q); sycl::free(x, q); sycl::free(xq, q); sycl::free(y, q);
    }
    std::puts("head SG16: 24 cases, 0 failures");
    return 0;
}

int main(int argc, char** argv) {
    if (argc == 2 && std::strcmp(argv[1], "--head-sg16") == 0) return head_sg16_check();
    sycl::queue* s = &dpct::get_in_order_queue();
    struct T { const char* name; int type, bytes, elems; };   // ggml type, bytes per block of `elems` weights
    const T types[] = {{"q6_k", 14, 210, 256}, {"q5_k", 13, 176, 256}, {"q4_k", 12, 144, 256}, {"iq4_xs", 23, 136, 256},
                        {"q8_0", 8, 34, 32}, {"iq4_nl", 20, 18, 32}, {"mxfp4", 39, 17, 32}, {"nvfp4", 40, 36, 64}};
    const int shapes[][2] = {{2560, 10240}, {2560, 12288}, {6144, 2560}, {2560, 640}, {640, 2560}, {2560, 2560}};
    const int cols[] = {2, 4, 6};
    for (const T& t : types)
        for (auto& sh : shapes) {
            const int n_in = sh[0], n_out = sh[1];
            if (n_in % t.elems != 0) continue;   // 640 wide: the 32-element formats only
            const size_t wbytes = (size_t) n_out * (n_in / t.elems) * t.bytes;
            std::vector<uint8_t> hw(wbytes);
            for (size_t i = 0; i < wbytes; ++i) hw[i] = (uint8_t) (i * 2654435761u >> 13) & 0x3b;   // keeps fp16 fields finite
            if (t.type == 39) for (size_t b = 0; b < wbytes / 17; ++b) hw[b * 17] = 127;             // MXFP4 E8M0 scale
            if (t.type == 40) for (size_t b = 0; b < wbytes / 36; ++b) for (int i = 0; i < 4; ++i) hw[b * 36 + i] = 0x38; // NVFP4 UE4M3 scale
            void* w = sycl::malloc_device(wbytes, *s);
            s->memcpy(w, hw.data(), wbytes).wait();
            for (int nc : cols) {
                std::vector<float> hx((size_t) nc * n_in);
                for (size_t i = 0; i < hx.size(); ++i) hx[i] = (float) ((i * 7919) % 1000) / 500.f - 1.f;
                float* x = sycl::malloc_device<float>(hx.size(), *s);
                void* xq = sycl::malloc_device((size_t) nc * (n_in / 32) * 36, *s);
                float* y = sycl::malloc_device<float>((size_t) nc * n_out, *s);
                s->memcpy(x, hx.data(), hx.size() * 4).wait();
                strata::kernels::native_quantize_q8_1(x, xq, n_in, nc, s);
                auto run = [&] { strata::kernels::native_mmvq(t.type, w, xq, y, n_in, n_out, nc, s); };
                const auto w0 = std::chrono::steady_clock::now();
                while (std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - w0).count() < 150) {
                    for (int i = 0; i < 20; ++i) run();
                    s->wait();
                }
                const int it = 300;
                const auto t0 = std::chrono::steady_clock::now();
                for (int i = 0; i < it; ++i) run();
                s->wait();
                const double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count() / it;
                std::vector<float> hy((size_t) nc * n_out);
                s->memcpy(hy.data(), y, hy.size() * 4).wait();
                double cs = 0; for (float v : hy) cs += std::isfinite(v) ? std::fabs((double) v) : 1e30;
                std::printf("%-7s %5d x %5d  cols %d  %7.1f us  %6.1f GB/s  sum %.6e\n", t.name, n_in, n_out, nc, us,
                            wbytes / us / 1e3, cs);
                sycl::free(x, *s); sycl::free(xq, *s); sycl::free(y, *s);
            }
            sycl::free(w, *s);
        }
    return 0;
}
