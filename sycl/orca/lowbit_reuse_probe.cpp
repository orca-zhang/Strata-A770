// Standalone native-layout U4xS4 / U2xS8 screen. No inference dispatch uses this file.
#include <sycl/sycl.hpp>
#include <sycl/ext/intel/esimd.hpp>
#include <sycl/ext/intel/esimd/xmx/dpas.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace es = sycl::ext::intel::esimd;
using P = es::xmx::dpas_argument_type;
constexpr int guard = 32;
int checks = 0;

template<int Bits, int R, int NC, int MR>
sycl::event product(sycl::queue& q, const uint32_t* a, const uint32_t* b,
                    const float* as, const float* bs, const int* sums,
                    float* y, int m, int n, int ng, int awords, int ametas) {
    constexpr int digits = Bits == 4 ? 2 : 1, bw = Bits == 4 ? 64 : 16;
    const int nt = (n + NC * 8 - 1) / (NC * 8);
    const int tiles = (m + R * MR - 1) / (R * MR) * nt;
    return q.parallel_for(sycl::nd_range<1>((tiles + 1) / 2 * 2, 2),
        [=](sycl::nd_item<1> it) SYCL_ESIMD_KERNEL {
#pragma clang fp contract(off)
            const int tile = int(it.get_global_linear_id());
            if (tile >= tiles) return;
            const int row = tile / nt * (R * MR), col = tile % nt * (NC * 8);
            es::simd<float, R * 8> acc[MR][NC];
#pragma unroll
            for (int r = 0; r < MR; ++r)
#pragma unroll
                for (int c = 0; c < NC; ++c) acc[r][c] = 0;
            for (int g = 0; g < ng; ++g) {
                es::simd<uint32_t, R * 8> av[MR][digits];
                es::simd<float, R * 8> sa[MR][digits];
                es::simd<int, R * 8> correction[MR][digits];
#pragma unroll
                for (int r = 0; r < MR; ++r) {
                    const int rr = row + r * R, at = (rr / 8 * ng + g) * 8 + rr % 8;
#pragma unroll
                    for (int d = 0; d < digits; ++d) {
                        av[r][d].copy_from(a + (rr / 8 * ng + g) * 64 + rr % 8 * 8 + d * awords);
                        es::simd<float, R> scale; scale.copy_from(as + at + d * ametas);
                        es::simd<int, R> sum; sum.copy_from(sums + at + d * ametas);
#pragma unroll
                        for (int j = 0; j < R; ++j) {
                            sa[r][d].template select<8, 1>(j * 8) = float(scale[j]);
                            correction[r][d].template select<8, 1>(j * 8) = int(sum[j]) * (Bits == 4 ? 8 : 1);
                        }
                    }
                }
#pragma unroll
                for (int c = 0; c < NC; ++c) {
                    es::simd<uint32_t, bw> bv; bv.copy_from(b + ((col / 8 + c) * ng + g) * bw);
                    es::simd<float, 8> sw; sw.copy_from(bs + ((col / 8 + c) * ng + g) * 8);
#pragma unroll
                    for (int r = 0; r < MR; ++r)
#pragma unroll
                        for (int d = 0; d < digits; ++d) {
                            es::simd<int, R * 8> dot;
                            if constexpr (Bits == 4)
                                dot = es::xmx::dpas<8, R, int, uint32_t, uint32_t, P::u4, P::s4>(bv, av[r][d]);
                            else
                                dot = es::xmx::dpas<8, R, int, uint32_t, uint32_t, P::u2, P::s8>(bv, av[r][d]);
                            es::simd<float, R * 8> fp = es::convert<float>(dot - correction[r][d]);
                            es::simd<float, R * 8> scale = sa[r][d] * sw.template replicate<R>();
                            acc[r][c] = es::simd<float, R * 8>(
                                __builtin_elementwise_fma(fp.data(),scale.data(),acc[r][c].data()));
                        }
                }
            }
            es::simd<uint32_t, R * 8> i(0, 1);
#pragma unroll
            for (int r = 0; r < MR; ++r)
#pragma unroll
                for (int c = 0; c < NC; ++c) {
                    auto rr = row + r * R + i / 8, cc = col + c * 8 + i % 8;
                    es::scatter(y, (rr * (n + 3) + cc) * 4, acc[r][c],
                                (rr < uint32_t(m)) & (cc < uint32_t(n)));
                }
        });
}

template<int Bits>
void shape(sycl::queue& q, int m, int n, int k, bool reverse, int samples) {
#pragma clang fp contract(off)
    constexpr int step = Bits == 4 ? 64 : 32, digits = Bits == 4 ? 2 : 1;
    constexpr int bw = Bits == 4 ? 64 : 16;
    const int mp = (m + 15) / 16 * 16, np = (n + 31) / 32 * 32, ng = k / step;
    const int awords = mp * ng * 8, ametas = mp * ng;
    std::vector<uint32_t> a(digits * awords), b(np / 8 * ng * bw);
    std::vector<float> as(digits * ametas), bs(np * ng);
    std::vector<int> sums(digits * ametas);
    std::vector<int8_t> raw_a(size_t(digits) * m * k);
    std::vector<uint8_t> raw_b(size_t(n) * k);
    std::mt19937 rng(7701104 + Bits + m + n + k);
    for (int d = 0; d < digits; ++d)
        for (int r = 0; r < m; ++r)
            for (int g = 0; g < ng; ++g) {
                const int meta = (r / 8 * ng + g) * 8 + r % 8 + d * ametas;
                as[meta] = std::ldexp(1.f, -6 + ((r + g + d) % 2));
                if (samples == 3) as[meta] *= float(1024 + (r * 137 + g * 53 + d * 211) % 1024) / 1024.f;
                for (int j = 0; j < step; ++j) {
                    const int v = r == 1 ? 0 : int(rng() % (Bits == 4 ? 16 : 256)) - (Bits == 4 ? 8 : 128);
                    raw_a[(d * m + r) * k + g * step + j] = int8_t(v);
                    sums[meta] += v;
                    const int per_word = Bits == 4 ? 8 : 4;
                    a[(r / 8 * ng + g) * 64 + r % 8 * 8 + j / per_word + d * awords] |=
                        (uint32_t(v) & (Bits == 4 ? 15 : 255)) << ((j % per_word) * (Bits == 4 ? 4 : 8));
                }
            }
    for (int c = 0; c < n; ++c)
        for (int g = 0; g < ng; ++g) {
            // U2 retains Q2's group64 weight scale and distinct Q8 group32 activation scales.
            bs[(c / 8 * ng + g) * 8 + c % 8] = std::ldexp(1.f, -5 + ((c + (Bits == 4 ? g : g / 2)) % 2));
            if (samples == 3) bs[(c / 8 * ng + g) * 8 + c % 8] *=
                float(1024 + (c * 89 + (Bits == 4 ? g : g / 2) * 101) % 1024) / 1024.f;
            for (int j = 0; j < step; ++j) {
                const int v = rng() % (1 << Bits);
                raw_b[c * k + g * step + j] = uint8_t(v);
                const int per_word = 32 / Bits;
                b[(c / 8 * ng + g) * bw + j / per_word * 8 + c % 8] |= uint32_t(v) << ((j % per_word) * Bits);
            }
        }
    std::vector<float> expected(size_t(m) * (n + 3) + 2 * guard, -12345.f);
    for (int r = 0; r < m; ++r)
        for (int c = 0; c < n; ++c) {
            float total = 0;
            for (int g = 0; g < ng; ++g)
                for (int d = 0; d < digits; ++d) {
                    int dot = 0;
                    for (int j = 0; j < step; ++j)
                        dot += int(raw_a[(d * m + r) * k + g * step + j]) *
                               (int(raw_b[c * k + g * step + j]) - (Bits == 4 ? 8 : 1));
                    total = std::fma(float(dot), as[(r / 8 * ng + g) * 8 + r % 8 + d * ametas] *
                                          bs[(c / 8 * ng + g) * 8 + c % 8], total);
                }
            expected[guard + r * (n + 3) + c] = total;
        }
    // ponytail: one shape's device buffers; this measures hot native layouts, not streaming/model speed.
    auto upload = [&q]<class T>(const std::vector<T>& v) {
        auto* p = sycl::malloc_device<T>(v.size() + 2 * guard, q);
        if (!p) throw std::bad_alloc();
        q.fill(p, T(17), v.size() + 2 * guard);
        q.memcpy(p + guard, v.data(), v.size() * sizeof(T));
        return p;
    };
    auto* da = upload(a); auto* db = upload(b);
    auto* das = upload(as); auto* dbs = upload(bs); auto* dsums = upload(sums);
    auto* y = sycl::malloc_device<float>(expected.size() * samples, q);
    if (!y) throw std::bad_alloc();
    q.wait_and_throw();
    auto launch = [&](int mode, float* output) {
        // U2's existing DPAS helper already narrows M and computes N8; retain that control geometry.
        if constexpr (Bits == 2) if (mode == 0) mode = 4;
        const auto* ap = da + guard; const auto* bp = db + guard;
        const auto* asp = das + guard; const auto* bsp = dbs + guard;
        const auto* sp = dsums + guard;
#define RUN(R, NC, MR) product<Bits, R, NC, MR>(q, ap, bp, asp, bsp, sp, output + guard, m, n, ng, awords, ametas)
        if (mode == 2) return RUN(8, 4, 1);
        if (mode == 3) return RUN(8, 2, 2);
        if (mode == 1) {
            if (m == 1) return RUN(1, 2, 1);
            if (m <= 2) return RUN(2, 2, 1);
            if (m <= 4) return RUN(4, 2, 1);
        }
        if (mode == 4) {
            if (m == 1) return RUN(1, 1, 1);
            if (m <= 2) return RUN(2, 1, 1);
            if (m <= 4) return RUN(4, 1, 1);
            return RUN(8, 1, 1);
        }
        return RUN(8, 2, 1);
#undef RUN
    };
    auto validate = [&](const float* output) {
        std::vector<float> got(expected.size());
        q.memcpy(got.data(), output, got.size() * sizeof(float)).wait_and_throw();
        if (got != expected) {
            for (size_t i = 0; i < got.size(); ++i)
                if (got[i] != expected[i]) {
                    std::fprintf(stderr, "U%d M%d N%d K%d index%zu got%.9g expected%.9g\n", Bits,m,n,k,i,got[i],expected[i]);
                    break;
                }
            throw std::runtime_error("full CPU integer/FP32 oracle or output guard mismatch");
        }
        ++checks;
    };
    std::vector<int> order = reverse ? std::vector<int>{0,4,0,3,0,2,0,1,0} : std::vector<int>{0,1,0,2,0,3,0,4,0};
    for (size_t phase = 0; phase < order.size(); ++phase) {
        const int mode = order[phase];
        q.fill(y, -12345.f, expected.size());
        for (int i = 0; i < 64; ++i) launch(mode,y);
        q.wait_and_throw(); validate(y);
        q.fill(y, -12345.f, expected.size() * samples);
        // Each queued sample owns its full output and guards; readback cannot hide overwritten samples.
        std::vector<sycl::event> events;
        for (int i = 0; i < samples; ++i) events.push_back(launch(mode,y + expected.size() * i));
        q.wait_and_throw();
        std::vector<double> times;
        for (int i = 0; i < samples; ++i) {
            const auto& event = events[i];
            const auto start = event.template get_profiling_info<sycl::info::event_profiling::command_start>();
            const auto end = event.template get_profiling_info<sycl::info::event_profiling::command_end>();
            if (end <= start) throw std::runtime_error("invalid kernel timestamps");
            times.push_back(double(end - start) / 1000);
            validate(y + expected.size() * i);
        }
        auto sorted = times; std::sort(sorted.begin(), sorted.end());
        std::printf("RESULT {\"bits\":%d,\"M\":%d,\"N\":%d,\"K\":%d,\"phase\":%zu,\"mode\":%d,\"median_us\":%.9g,\"samples_us\":[",Bits,m,n,k,phase,mode,sorted[sorted.size()/2]);
        for (size_t i = 0; i < times.size(); ++i) std::printf("%s%.9g",i ? "," : "",times[i]);
        std::puts("]}"); std::fflush(stdout);
    }
    auto unchanged = [&q]<class T>(T* p, const std::vector<T>& original) {
        std::vector<T> got(original.size() + 2 * guard), want(got.size(), T(17));
        std::copy(original.begin(), original.end(), want.begin() + guard);
        q.memcpy(got.data(), p, got.size() * sizeof(T)).wait_and_throw();
        if (got != want) throw std::runtime_error("input/scale/sum data or guard changed");
        ++checks; sycl::free(p, q);
    };
    unchanged(da,a); unchanged(db,b); unchanged(das,as); unchanged(dbs,bs); unchanged(dsums,sums);
    sycl::free(y,q);
}

int main(int argc, char** argv) try {
    bool smoke = false, reverse = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--smoke") smoke = true;
        else if (std::string(argv[i]) == "--reverse") reverse = true;
        else throw std::invalid_argument("usage: lowbit-reuse [--smoke] [--reverse]");
    }
    sycl::queue q(sycl::gpu_selector_v, [](sycl::exception_list errors) {
        for (auto error : errors) std::rethrow_exception(error);
    }, {sycl::property::queue::in_order{}, sycl::property::queue::enable_profiling{}});
    std::printf("DEVICE %s\nDRIVER %s\n", q.get_device().get_info<sycl::info::device::name>().c_str(),
                q.get_device().get_info<sycl::info::device::driver_version>().c_str());
    const std::vector<std::vector<int>> shapes = smoke ?
        std::vector<std::vector<int>>{{1,13,64},{3,17,192},{9,33,128},{17,9,64}} :
        std::vector<std::vector<int>>{{1,1280,2560},{2,1280,2560},{4,1280,2560},{8,1280,2560},
            {40,1280,2560},{160,1280,2560},{1,2560,640},{4,2560,640},{40,2560,640},{160,2560,640},
            {1,320,10240},{128,320,10240}};
    for (const auto& s : shapes) {
        shape<4>(q,s[0],s[1],s[2],reverse,smoke ? 3 : 31);
        shape<2>(q,s[0],s[1],s[2],reverse,smoke ? 3 : 31);
    }
    std::printf("PASS %d full CPU output/input/guard checks\n",checks);
    return 0;
} catch (const std::exception& error) {
    std::fprintf(stderr,"FAIL %s\n",error.what()); return 1;
}
