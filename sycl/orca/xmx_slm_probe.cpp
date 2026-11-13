// FP16 XMX kernel experiment, not an Orca/model speed claim.
#include <sycl/sycl.hpp>
#include <sycl/ext/oneapi/matrix/matrix.hpp>
#include <sycl/ext/intel/experimental/grf_size_properties.hpp>
#include <sycl/ext/oneapi/experimental/prefetch.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>

namespace jm = sycl::ext::oneapi::experimental::matrix;
template<int Mode> class SlmProbe;

template<int Mode>
sycl::event launch(sycl::queue& q, const sycl::half* x, const sycl::half* w, float* y, int m, int n, int k) {
    constexpr int SG = 8, NSG = 8, WG = SG * NSG, BK = 64, STRIDE = BK + 8;
    constexpr bool small = Mode < 2 || Mode == 5;
    constexpr int MB = small ? 4 : 16, NB = 64;
    constexpr int STAGE = (16 + 64) * STRIDE, STAGES = Mode == 3 ? 2 : 1;
    return q.submit([&](sycl::handler& h) {
        sycl::local_accessor<sycl::half, 1> local(sycl::range<1>(Mode == 2 || Mode == 3 ? STAGE * STAGES : 1), h);
        sycl::local_accessor<float, 1> tail(sycl::range<1>(NSG * 4 * 8), h);
        h.parallel_for<SlmProbe<Mode>>(sycl::nd_range<3>(
            sycl::range<3>((m + MB - 1) / MB, n / NB, WG), sycl::range<3>(1, 1, WG)),
            sycl::ext::oneapi::experimental::properties{sycl::ext::intel::experimental::grf_size<256>,
                sycl::ext::oneapi::experimental::sub_group_size<SG>},
            [=](sycl::nd_item<3> it) {
                auto sg = it.get_sub_group();
                const int sid = int(sg.get_group_id()[0]), lane = int(sg.get_local_id()[0]);
                const int tid = int(it.get_local_id(2));
                const int m0 = int(it.get_group(0)) * MB, n0 = int(it.get_group(1)) * NB;
                const int row = m0 + (small ? 0 : sid / 4 * 8);
                const int col = n0 + (small ? sid * 8 : sid % 4 * 16);
                auto gp = [](auto* p) { return sycl::address_space_cast<sycl::access::address_space::global_space,
                    sycl::access::decorated::no>(p); };
                auto lp = [](auto* p) { return sycl::address_space_cast<sycl::access::address_space::local_space,
                    sycl::access::decorated::no>(p); };
                jm::joint_matrix<sycl::sub_group, float, jm::use::accumulator, 4, 8> acc;
                jm::joint_matrix_fill(sg, acc, 0.f);
                if constexpr (Mode == 0 || Mode == 5) {
                    for (int z = 0; z < k; z += 16) {
                        if constexpr (Mode == 5) if ((z & 63) == 0 && z + 64 < k) {
                            namespace ex = sycl::ext::oneapi::experimental;
                            ex::prefetch(w + size_t(col + lane) * k + z + 64, 64,
                                ex::properties{ex::prefetch_hint_L2});
                            if (sid == 0 && lane < 4)
                                ex::prefetch(x + size_t(row + lane) * k + z + 64, 64,
                                    ex::properties{ex::prefetch_hint_L2});
                        }
                        jm::joint_matrix<sycl::sub_group, sycl::half, jm::use::a, 4, 16, jm::layout::row_major> a;
                        jm::joint_matrix<sycl::sub_group, sycl::half, jm::use::b, 16, 8, jm::layout::col_major> b;
                        jm::joint_matrix_load(sg, a, gp(x + size_t(row) * k + z), size_t(k));
                        jm::joint_matrix_load(sg, b, gp(w + size_t(col) * k + z), size_t(k));
                        jm::joint_matrix_mad(sg, acc, a, b, acc);
                    }
                } else if constexpr (Mode == 1) {
                    jm::joint_matrix<sycl::sub_group, sycl::half, jm::use::a, 4, 16, jm::layout::row_major> a0, a1;
                    jm::joint_matrix<sycl::sub_group, sycl::half, jm::use::b, 16, 8, jm::layout::col_major> b0, b1;
                    jm::joint_matrix_load(sg, a0, gp(x + size_t(row) * k), size_t(k));
                    jm::joint_matrix_load(sg, b0, gp(w + size_t(col) * k), size_t(k));
                    for (int z = 0; z < k; z += 32) {
                        jm::joint_matrix_load(sg, a1, gp(x + size_t(row) * k + z + 16), size_t(k));
                        jm::joint_matrix_load(sg, b1, gp(w + size_t(col) * k + z + 16), size_t(k));
                        jm::joint_matrix_mad(sg, acc, a0, b0, acc);
                        if (z + 32 < k) {
                            jm::joint_matrix_load(sg, a0, gp(x + size_t(row) * k + z + 32), size_t(k));
                            jm::joint_matrix_load(sg, b0, gp(w + size_t(col) * k + z + 32), size_t(k));
                        }
                        jm::joint_matrix_mad(sg, acc, a1, b1, acc);
                    }
                } else {
                    auto* sm = local.template get_multi_ptr<sycl::access::decorated::no>().get();
                    using A = jm::joint_matrix<sycl::sub_group, sycl::half, jm::use::a, 4, 16, jm::layout::row_major>;
                    using B = jm::joint_matrix<sycl::sub_group, sycl::half, jm::use::b, 16, 8, jm::layout::col_major>;
                    jm::joint_matrix<sycl::sub_group, float, jm::use::accumulator, 4, 8> tile[2][2];
                    #pragma unroll
                    for (int r = 0; r < 2; ++r)
                        #pragma unroll
                        for (int c = 0; c < 2; ++c) jm::joint_matrix_fill(sg, tile[r][c], 0.f);
                    const auto stage = [&](int bank, int z) {
                        auto* a = sm + bank * STAGE;
                        auto* b = a + 16 * STRIDE;
                        for (int i = tid; i < 16 * BK; i += WG) {
                            const int r = m0 + i / BK;
                            a[(i / BK) * STRIDE + i % BK] = r < m ? x[size_t(r) * k + z + i % BK] : sycl::half(0);
                        }
                        for (int i = tid; i < 64 * BK; i += WG)
                            b[(i / BK) * STRIDE + i % BK] = w[size_t(n0 + i / BK) * k + z + i % BK];
                    };
                    if constexpr (Mode == 3) {
                        stage(0, 0);
                        it.barrier(sycl::access::fence_space::local_space);
                    }
                    for (int z = 0; z < k; z += BK) {
                        const int bank = Mode == 3 ? (z / BK) & 1 : 0;
                        sycl::half next_a[16], next_b[64];
                        if constexpr (Mode == 2) {
                            stage(bank, z);
                            it.barrier(sycl::access::fence_space::local_space);
                        } else if constexpr (Mode == 3) { if (z + BK < k) {
                            // Ordinary UGM loads into registers; this is not a direct global-to-SLM DMA.
                            #pragma unroll
                            for (int j = 0; j < 16; ++j) {
                                const int i = tid + j * WG, r = m0 + i / BK;
                                next_a[j] = r < m ? x[size_t(r) * k + z + BK + i % BK] : sycl::half(0);
                            }
                            #pragma unroll
                            for (int j = 0; j < 64; ++j) {
                                const int i = tid + j * WG;
                                next_b[j] = w[size_t(n0 + i / BK) * k + z + BK + i % BK];
                            }
                        } }
                        auto* a = sm + bank * STAGE + (row - m0) * STRIDE;
                        auto* b = sm + bank * STAGE + 16 * STRIDE + (col - n0) * STRIDE;
                        #pragma unroll
                        for (int j = 0; j < BK; j += 16) {
                            A aa[2]; B bb[2];
                            #pragma unroll
                            for (int r = 0; r < 2; ++r) {
                                if constexpr (Mode == 4) {
                                    jm::joint_matrix_load(sg, aa[r], gp(x + size_t(row + r * 4) * k + z + j), size_t(k));
                                    jm::joint_matrix_load(sg, bb[r], gp(w + size_t(col + r * 8) * k + z + j), size_t(k));
                                } else {
                                    jm::joint_matrix_load(sg, aa[r], lp(a + r * 4 * STRIDE + j), STRIDE);
                                    jm::joint_matrix_load(sg, bb[r], lp(b + r * 8 * STRIDE + j), STRIDE);
                                }
                            }
                            #pragma unroll
                            for (int r = 0; r < 2; ++r)
                                #pragma unroll
                                for (int c = 0; c < 2; ++c)
                                    jm::joint_matrix_mad(sg, tile[r][c], aa[r], bb[c], tile[r][c]);
                        }
                        if constexpr (Mode == 3) if (z + BK < k) {
                            auto* na = sm + (bank ^ 1) * STAGE;
                            auto* nb = na + 16 * STRIDE;
                            #pragma unroll
                            for (int j = 0; j < 16; ++j) {
                                const int i = tid + j * WG;
                                na[(i / BK) * STRIDE + i % BK] = next_a[j];
                            }
                            #pragma unroll
                            for (int j = 0; j < 64; ++j) {
                                const int i = tid + j * WG;
                                nb[(i / BK) * STRIDE + i % BK] = next_b[j];
                            }
                        }
                        if constexpr (Mode != 4) it.barrier(sycl::access::fence_space::local_space);
                    }
                    #pragma unroll
                    for (int r = 0; r < 2; ++r)
                        #pragma unroll
                        for (int c = 0; c < 2; ++c) {
                            const int rr = row + r * 4, cc = col + c * 8;
                            if (rr + 4 <= m)
                                jm::joint_matrix_store(sg, tile[r][c], gp(y + size_t(rr) * n + cc), size_t(n), jm::layout::row_major);
                            else {
                                auto* tmp = tail.template get_multi_ptr<sycl::access::decorated::no>().get() + sid * 32;
                                jm::joint_matrix_store(sg, tile[r][c], lp(tmp), 8, jm::layout::row_major);
                                sycl::group_barrier(sg);
                                for (int i = lane; i < 32; i += SG)
                                    if (rr + i / 8 < m) y[size_t(rr + i / 8) * n + cc + i % 8] = tmp[i];
                                sycl::group_barrier(sg);
                            }
                        }
                }
                if constexpr (small) {
                if (row + 4 <= m) jm::joint_matrix_store(sg, acc, gp(y + size_t(row) * n + col), size_t(n), jm::layout::row_major);
                else {
                    auto* tmp = tail.template get_multi_ptr<sycl::access::decorated::no>().get() + sid * 32;
                    jm::joint_matrix_store(sg, acc, lp(tmp), 8, jm::layout::row_major);
                    sycl::group_barrier(sg);
                    for (int i = lane; i < 32; i += SG)
                        if (row + i / 8 < m) y[size_t(row + i / 8) * n + col + i % 8] = tmp[i];
                }
                }
            });
    });
}

int main(int argc, char** argv) try {
    if (argc > 2 || (argc == 2 && std::strcmp(argv[1], "--smoke"))) throw std::invalid_argument("expected --smoke or no arguments");
    const bool smoke = argc == 2;
    sycl::queue q(sycl::gpu_selector_v, {sycl::property::queue::enable_profiling(), sycl::property::queue::in_order()});
    const auto name = q.get_device().get_info<sycl::info::device::name>();
    if (name.find("A770") == std::string::npos) throw std::runtime_error("this experiment requires A770");
    std::printf("DEVICE %s;all variants request256 GRFs;kernel timestamps only;no model claim\n", name.c_str());
    struct Shape { int m, n, k; };
    std::vector<Shape> shapes = smoke ? std::vector<Shape>{{13, 64, 64}} :
        std::vector<Shape>{{4,1280,2560},{16,1280,2560},{40,1280,2560},{160,1280,2560},
                           {40,2560,640},{160,2560,640},{2048,6144,2560}};
    constexpr size_t guard = 64;
    size_t checks = 0;
    for (const auto [m, n, k] : shapes) {
        if (m < 1 || n % 64 || k % 64) throw std::invalid_argument("invalid probe shape");
        const size_t padded_m = size_t((m + 15) / 16 * 16), cells = size_t(m) * n;
        std::vector<sycl::half> hx(padded_m * k + 2 * guard, sycl::half(123)), hw(size_t(n) * k + 2 * guard, sycl::half(123));
        std::fill(hx.begin() + guard, hx.end() - guard, sycl::half(0));
        for (size_t i = 0; i < size_t(m) * k; ++i) hx[guard + i] = sycl::half(float(int((i * 13 + 3) % 19) - 5) / 32);
        for (size_t i = 0; i < size_t(n) * k; ++i) hw[guard + i] = sycl::half(float(int((i * 17 + 7) % 23) - 9) / 32);
        float reference[23][19]{};
        for (int a = 0; a < 23; ++a) for (int b = 0; b < 19; ++b)
            for (int z = 0; z < k; ++z)
                reference[a][b] += float(((a * k + z) * 17 + 7) % 23 - 9) * float(((b * k + z) * 13 + 3) % 19 - 5) / 1024;
        auto* dx = sycl::malloc_device<sycl::half>(hx.size(), q);
        auto* dw = sycl::malloc_device<sycl::half>(hw.size(), q);
        auto* dy = sycl::malloc_device<float>(cells + 2 * guard, q);
        if (!dx || !dw || !dy) throw std::bad_alloc();
        std::vector<float> actual(cells + 2 * guard, -12345.f);
        q.memcpy(dx, hx.data(), hx.size() * 2);
        q.memcpy(dw, hw.data(), hw.size() * 2);
        q.memcpy(dy, actual.data(), actual.size() * 4).wait_and_throw();
        const auto run = [&](int mode) {
            if (mode == 0) return launch<0>(q, dx + guard, dw + guard, dy + guard, m, n, k);
            if (mode == 1) return launch<1>(q, dx + guard, dw + guard, dy + guard, m, n, k);
            if (mode == 2) return launch<2>(q, dx + guard, dw + guard, dy + guard, m, n, k);
            if (mode == 3) return launch<3>(q, dx + guard, dw + guard, dy + guard, m, n, k);
            if (mode == 4) return launch<4>(q, dx + guard, dw + guard, dy + guard, m, n, k);
            return launch<5>(q, dx + guard, dw + guard, dy + guard, m, n, k);
        };
        const auto check = [&] {
            q.memcpy(actual.data(), dy, actual.size() * 4).wait_and_throw();
            for (size_t i = 0; i < actual.size(); ++i) {
                const float expected = i < guard || i >= guard + cells ? -12345.f : reference[(i - guard) % n % 23][((i - guard) / n) % 19];
                if (!std::isfinite(actual[i]) || actual[i] != expected) {
                    std::fprintf(stderr, "MISMATCH m%d n%d k%d i%zu actual%.9g expected%.9g\n", m, n, k, i, actual[i], expected);
                    throw std::runtime_error("XMX output/guard mismatch");
                }
            }
            ++checks;
        };
        // Warm every JIT variant before measuring; poison each output so skipped stores cannot pass.
        for (int mode = 0; mode < 6; ++mode) {
            q.fill(dy + guard, std::numeric_limits<float>::quiet_NaN(), cells);
            run(mode).wait_and_throw();
            check();
        }
        for (int phase = 0; phase < 11; ++phase) {
            const int order[]{0,1,0,4,0,2,0,3,0,5,0}, mode = order[phase];
            q.fill(dy + guard, std::numeric_limits<float>::quiet_NaN(), cells);
            for (int warm = 0; warm < 3; ++warm) run(mode);
            q.wait_and_throw();
            check();
            std::vector<double> times;
            for (int repeat = 0; repeat < (smoke ? 3 : 9); ++repeat) {
                auto e = run(mode); e.wait_and_throw();
                const auto a = e.get_profiling_info<sycl::info::event_profiling::command_start>();
                const auto z = e.get_profiling_info<sycl::info::event_profiling::command_end>();
                if (z <= a) throw std::runtime_error("nonpositive kernel duration");
                times.push_back(double(z - a) / 1000);
            }
            check();
            auto ordered = times; std::sort(ordered.begin(), ordered.end());
            std::printf("RESULT {\"M\":%d,\"N\":%d,\"K\":%d,\"phase\":%d,\"mode\":%d,\"median_us\":%.9g,\"samples_us\":[", m,n,k,phase,mode,ordered[ordered.size()/2]);
            for (size_t i = 0; i < times.size(); ++i) std::printf("%s%.9g", i ? "," : "", times[i]);
            std::puts("]}"); std::fflush(stdout);
        }
        std::vector<sycl::half> rx(hx.size()), rw(hw.size());
        q.memcpy(rx.data(), dx, rx.size() * 2); q.memcpy(rw.data(), dw, rw.size() * 2).wait_and_throw();
        if (std::memcmp(rx.data(), hx.data(), rx.size() * 2) || std::memcmp(rw.data(), hw.data(), rw.size() * 2))
            throw std::runtime_error("input/weight/guard was modified");
        sycl::free(dy, q); sycl::free(dw, q); sycl::free(dx, q);
    }
    std::printf("PASS %zu full-output/guard checks and exact operands;6 variants;standalone SYCL kernel screen only\n", checks);
} catch (const std::exception& e) { std::fprintf(stderr, "FAIL %s\n", e.what()); return 1; }
