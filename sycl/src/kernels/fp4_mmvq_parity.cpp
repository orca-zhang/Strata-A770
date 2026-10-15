// sycl/src/kernels/fp4_mmvq_parity.cpp - MXFP4/NVFP4 native_mmvq against a tiny CPU oracle.
#include "strata/kernels/native_mmvq.hpp"
#include <sycl/sycl.hpp>
#include <dpct/dpct.hpp>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <exception>
#include <vector>

namespace {

struct Q81Host {
    unsigned short ds[2];
    int8_t qs[32];
};
static_assert(sizeof(Q81Host) == 36);

constexpr int8_t kFp4[16] = {0, 1, 2, 3, 4, 6, 8, 12, 0, -1, -2, -3, -4, -6, -8, -12};

float f32_from_bits(uint32_t bits) {
    float f;
    std::memcpy(&f, &bits, sizeof f);
    return f;
}

float e8m0_half(uint8_t x) {
    return f32_from_bits(x < 2 ? (0x00200000u << x) : ((uint32_t(x) - 1u) << 23));
}

float ue4m3_half(uint8_t x) {
    if (x == 0 || x == 0x7f) return 0.0f;
    const int exp = (x >> 3) & 0x0f;
    const int man = x & 0x07;
    return exp == 0 ? std::ldexp(float(man), -10)
                    : std::ldexp(1.0f + float(man) * 0.125f, exp - 8);
}

float h2f(unsigned short bits) {
    return (float) sycl::bit_cast<sycl::half, unsigned short>(bits);
}

void fill_weights(int type, std::vector<uint8_t>& w, int n_in, int n_out) {
    const int elems = type == 39 ? 32 : 64;
    const int bytes = type == 39 ? 17 : 36;
    const int blocks = n_in / elems;
    for (int r = 0; r < n_out; ++r) {
        for (int b = 0; b < blocks; ++b) {
            uint8_t* p = w.data() + (std::size_t(r) * blocks + b) * bytes;
            if (type == 39) {
                p[0] = uint8_t(124 + ((r + b) & 7));
                for (int i = 0; i < 16; ++i) p[1 + i] = uint8_t(((i + r + 3 * b) & 15) | (((11 * i + r + b) & 15) << 4));
            } else {
                for (int s = 0; s < 4; ++s) p[s] = uint8_t(0x34 + ((r + b + s) & 7));
                for (int i = 0; i < 32; ++i) p[4 + i] = uint8_t(((5 * i + r + b) & 15) | (((7 * i + 3 * r + b) & 15) << 4));
            }
        }
    }
}

float weight_at(int type, const std::vector<uint8_t>& w, int n_in, int row, int k) {
    if (type == 39) {
        const int blocks = n_in / 32;
        const int kb = k / 32, i = k % 32;
        const uint8_t* p = w.data() + (std::size_t(row) * blocks + kb) * 17;
        const int q = p[1 + (i & 15)];
        return e8m0_half(p[0]) * float(kFp4[i < 16 ? (q & 15) : (q >> 4)]);
    }
    const int blocks = n_in / 64;
    const int kb = k / 64, i = k % 64, sub = i / 16, j = i % 16;
    const uint8_t* p = w.data() + (std::size_t(row) * blocks + kb) * 36;
    const int q = p[4 + sub * 8 + (j & 7)];
    return ue4m3_half(p[sub]) * float(kFp4[j < 8 ? (q & 15) : (q >> 4)]);
}

int check_case(const char* name, int type, int n_in, int n_out, int ncols, sycl::queue* s) {
    const std::size_t wbytes = strata::kernels::native_mmvq_weight_bytes(type, n_in, n_out);
    std::vector<uint8_t> hw(wbytes);
    fill_weights(type, hw, n_in, n_out);
    std::vector<float> hx(std::size_t(ncols) * n_in);
    for (std::size_t i = 0; i < hx.size(); ++i) hx[i] = float(int((i * 37) % 101) - 50) / 17.0f;

    void* dw = sycl::malloc_device(wbytes, *s);
    float* dx = sycl::malloc_device<float>(hx.size(), *s);
    void* xq = sycl::malloc_device(strata::kernels::native_q8_1_bytes(n_in, ncols), *s);
    float* dy = sycl::malloc_device<float>(std::size_t(ncols) * n_out, *s);
    s->memcpy(dw, hw.data(), hw.size()).wait();
    s->memcpy(dx, hx.data(), hx.size() * sizeof(float)).wait();
    strata::kernels::native_quantize_q8_1(dx, xq, n_in, ncols, s);
    strata::kernels::native_mmvq(type, dw, xq, dy, n_in, n_out, ncols, s);
    s->wait();

    std::vector<Q81Host> hq(std::size_t(ncols) * (n_in / 32));
    std::vector<float> hy(std::size_t(ncols) * n_out), single(hy.size());
    s->memcpy(hq.data(), xq, hq.size() * sizeof(Q81Host)).wait();
    s->memcpy(hy.data(), dy, hy.size() * sizeof(float)).wait();
    for (int c = 0; c < ncols; ++c) {
        strata::kernels::native_mmvq(type, dw, static_cast<const uint8_t*>(xq) + std::size_t(c) * (n_in / 32) * 36,
                                     dy + std::size_t(c) * n_out, n_in, n_out, 1, s);
    }
    s->wait();
    s->memcpy(single.data(), dy, single.size() * sizeof(float)).wait();

    double num = 0.0, den = 0.0;
    int nonfinite = 0;
    for (int c = 0; c < ncols; ++c) {
        for (int r = 0; r < n_out; ++r) {
            double ref = 0.0;
            for (int k = 0; k < n_in; ++k) {
                const Q81Host& q = hq[std::size_t(c) * (n_in / 32) + k / 32];
                ref += double(weight_at(type, hw, n_in, r, k)) * double(q.qs[k & 31]) * double(h2f(q.ds[0]));
            }
            const float got = hy[std::size_t(c) * n_out + r];
            if (!std::isfinite(got)) ++nonfinite;
            num += std::fabs(double(got) - ref);
            den += std::fabs(ref);
        }
    }
    const double rel = num / (den + 1e-30);
    const bool multi_ok = !strata::kernels::native_mmvq_multi_exact() ||
                          std::memcmp(hy.data(), single.data(), hy.size() * sizeof(float)) == 0;
    const bool ok = nonfinite == 0 && rel < 1e-4 && multi_ok;
    std::printf("%-6s type %2d %4d x %4d cols %d  rel %.3e  multi %s  %s\n",
                name, type, n_out, n_in, ncols, rel, multi_ok ? "ok" : "DIFF", ok ? "ok" : "FAIL");

    sycl::free(dw, *s);
    sycl::free(dx, *s);
    sycl::free(xq, *s);
    sycl::free(dy, *s);
    return ok ? 0 : 1;
}

} // namespace

int main() {
    try {
        sycl::queue* s = &dpct::get_in_order_queue();
        int failures = 0;
        failures += check_case("mxfp4", 39, 64, 7, 3, s);
        failures += check_case("nvfp4", 40, 64, 7, 3, s);
        std::printf("fp4_mmvq_parity: %d failures\n", failures);
        return failures ? 1 : 0;
    } catch (const sycl::exception& e) {
        std::fprintf(stderr, "SYCL exception: %s\n", e.what());
        return 2;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "exception: %s\n", e.what());
        return 2;
    }
}
