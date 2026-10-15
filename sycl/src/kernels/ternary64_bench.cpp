// A770 microbenchmark for the offline ternary64 down-weight prototype.
#include "strata/kernels/native_mmvq.hpp"
#include "strata/kernels/dp4a.hpp"

#include <dpct/dpct.hpp>
#include <sycl/sycl.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr int kIn = 640;
constexpr int kOut = 2560;
constexpr int kBlock = 64;
constexpr int kBlocksPerRow = kIn / kBlock;
constexpr int kQ2Bytes = 18;
constexpr int kTritBytes = 13;
constexpr int kWarps = 4;
constexpr int kWarp = 32;

struct Q81Block {
    sycl::half2 ds;
    int8_t qs[32];
};
static_assert(sizeof(Q81Block) == 36);

float median(std::vector<double> values) {
    std::sort(values.begin(), values.end());
    return (float) values[values.size() / 2];
}

std::vector<uint8_t> read_file(const char* path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) throw std::runtime_error("cannot open Q2_0 expert file");
    const std::streamsize size = in.tellg();
    std::vector<uint8_t> data((size_t) size);
    in.seekg(0);
    if (!in.read((char*) data.data(), size)) throw std::runtime_error("cannot read Q2_0 expert file");
    return data;
}

void make_ternary(const std::vector<uint8_t>& q2, std::vector<uint8_t>& codes,
                  std::vector<sycl::half>& scales) {
    const size_t blocks = (size_t) kOut * kBlocksPerRow;
    if (q2.size() != blocks * kQ2Bytes) throw std::runtime_error("expected one 2560x640 Q2_0 expert");
    codes.resize(blocks * kTritBytes);
    scales.resize(blocks);
    for (size_t b = 0; b < blocks; ++b) {
        uint16_t bits;
        std::memcpy(&bits, q2.data() + b * kQ2Bytes, sizeof(bits));
        const float d = (float) std::bit_cast<sycl::half>(bits);
        int symbols[kBlock], nonzero = 0, abs_sum = 0;
        for (int i = 0; i < kBlock; ++i) {
            const int code = (q2[b * kQ2Bytes + 2 + i / 4] >> (2 * (i % 4))) & 3;
            symbols[i] = code - 1;
            nonzero += symbols[i] != 0;
            abs_sum += std::abs(symbols[i]);
        }
        scales[b] = sycl::half(d * (float) abs_sum / std::max(nonzero, 1));
        for (int byte = 0; byte < kTritBytes; ++byte) {
            int packed = 0, power = 1;
            for (int j = 0; j < 5; ++j) {
                const int i = byte * 5 + j;
                const int trit = i < kBlock ? std::clamp(symbols[i], -1, 1) + 1 : 1;
                packed += trit * power;
                power *= 3;
            }
            codes[b * kTritBytes + byte] = (uint8_t) packed;
        }
    }
}

std::array<uint64_t, 256> make_lut() {
    std::array<uint64_t, 256> lut{};
    for (unsigned p0 = 0; p0 < lut.size(); ++p0) {
        unsigned p = p0;
        uint64_t packed = 0;
        for (int j = 0; j < 5; ++j) {
            const int8_t value = (int8_t) (int(p % 3) - 1);
            packed |= uint64_t((uint8_t) value) << (j * 8);
            p /= 3;
        }
        lut[p0] = packed;
    }
    return lut;
}

void ternary64_mmvq(const uint8_t* codes, const sycl::half* scales, const uint64_t* lut,
                    const Q81Block* x, float* y, int ncols, sycl::queue& q) {
    const size_t groups = (kOut + kWarps - 1) / kWarps;
    q.submit([&](sycl::handler& cgh) {
        cgh.parallel_for(
            sycl::nd_range<1>(groups * kWarps * kWarp, kWarps * kWarp),
            [=](sycl::nd_item<1> item) [[sycl::reqd_sub_group_size(32)]] {
                const int tid = (int) item.get_local_id(0);
                const int lane = tid % kWarp;
                const int row = (int) item.get_group(0) * kWarps + tid / kWarp;
                float partial[4] = {};
                if (row < kOut && lane < kBlocksPerRow) {
                    const size_t block = (size_t) row * kBlocksPerRow + lane;
                    int sums[4][2] = {};
                    for (int byte = 0; byte < kTritBytes; ++byte) {
                        const uint64_t trits = lut[codes[block * kTritBytes + byte]];
#pragma unroll
                        for (int j = 0; j < 5; ++j) {
                            const int i = byte * 5 + j;
                            if (i >= kBlock) break;
                            const int w = (int) (int8_t) (trits >> (j * 8));
#pragma unroll
                            for (int c = 0; c < 4; ++c) {
                                if (c < ncols) {
                                    const Q81Block& xb = x[c * (kIn / 32) + lane * 2 + i / 32];
                                    sums[c][i / 32] += w * (int) xb.qs[i % 32];
                                }
                            }
                        }
                    }
                    const float wd = (float) scales[block];
#pragma unroll
                    for (int c = 0; c < 4; ++c) {
                        if (c < ncols) {
                            const Q81Block* xb = x + c * (kIn / 32) + lane * 2;
                            partial[c] = wd * ((float) xb[0].ds[0] * sums[c][0] +
                                               (float) xb[1].ds[0] * sums[c][1]);
                        }
                    }
                }
                const auto sg = item.get_sub_group();
#pragma unroll
                for (int c = 0; c < 4; ++c) {
                    if (c < ncols) {
#pragma unroll
                        for (int offset = 16; offset; offset >>= 1)
                            partial[c] += sycl::permute_group_by_xor(sg, partial[c], offset);
                        if (lane == 0 && row < kOut) y[(size_t) c * kOut + row] = partial[c];
                    }
                }
            });
    });
}

void q2_rowwarp_mmvq(const uint8_t* weights, const Q81Block* x, float* y, int ncols, sycl::queue& q) {
    const size_t groups = (kOut + kWarps - 1) / kWarps;
    q.submit([&](sycl::handler& cgh) {
        cgh.parallel_for(
            sycl::nd_range<1>(groups * kWarps * kWarp, kWarps * kWarp),
            [=](sycl::nd_item<1> item) [[sycl::reqd_sub_group_size(32)]] {
                const int tid = (int) item.get_local_id(0);
                const int lane = tid % kWarp;
                const int row = (int) item.get_group(0) * kWarps + tid / kWarp;
                float partial[4] = {};
                if (row < kOut && lane < kBlocksPerRow * 2) {
                    const int block_in_row = lane / 2, half = lane % 2;
                    const uint8_t* block = weights + ((size_t) row * kBlocksPerRow + block_in_row) * kQ2Bytes;
                    const float wd = (float) *reinterpret_cast<const sycl::half*>(block);
                    const int16_t* qs = reinterpret_cast<const int16_t*>(block + 2) + half * 4;
#pragma unroll
                    for (int c = 0; c < 4; ++c) {
                        if (c < ncols) {
                            const Q81Block& xb = x[c * (kIn / 32) + block_in_row * 2 + half];
                            const int* q8 = reinterpret_cast<const int*>(xb.qs);
                            int sum = 0;
#pragma unroll
                            for (int j = 0; j < 4; ++j) {
                                const int packed = qs[j];
                                const int even = dpct::byte_level_permute(0x020100ff, 0x020100ff, packed);
                                const int odd = dpct::byte_level_permute(0x020100ff, 0x020100ff, packed >> 2);
                                const int qx = dpct::byte_level_permute(even, odd, 0x5140);
                                const int qy = dpct::byte_level_permute(even, odd, 0x7362);
                                sum = STRATA_DP4A(q8[j * 2], qx, sum);
                                sum = STRATA_DP4A(q8[j * 2 + 1], qy, sum);
                            }
                            partial[c] = wd * (float) xb.ds[0] * sum;
                        }
                    }
                }
                const auto sg = item.get_sub_group();
#pragma unroll
                for (int c = 0; c < 4; ++c) {
                    if (c < ncols) {
#pragma unroll
                        for (int offset = 16; offset; offset >>= 1)
                            partial[c] += sycl::permute_group_by_xor(sg, partial[c], offset);
                        if (lane == 0 && row < kOut) y[(size_t) c * kOut + row] = partial[c];
                    }
                }
            });
    });
}

void check_output(const std::vector<uint8_t>& codes, const std::vector<sycl::half>& scales,
                  const std::array<uint64_t, 256>& lut, const std::vector<Q81Block>& x,
                  const std::vector<float>& got, int ncols) {
    double error = 0.0, reference = 0.0;
    for (int c = 0; c < ncols; ++c) {
        for (int row = 0; row < kOut; ++row) {
            float expected = 0.0f;
            for (int block_in_row = 0; block_in_row < kBlocksPerRow; ++block_in_row) {
                const size_t block = (size_t) row * kBlocksPerRow + block_in_row;
                int sums[2] = {};
                for (int byte = 0; byte < kTritBytes; ++byte) {
                    const uint64_t trits = lut[codes[block * kTritBytes + byte]];
                    for (int j = 0; j < 5; ++j) {
                        const int i = byte * 5 + j;
                        if (i >= kBlock) break;
                        const int w = (int) (int8_t) (trits >> (j * 8));
                        const Q81Block& xb = x[c * (kIn / 32) + block_in_row * 2 + i / 32];
                        sums[i / 32] += w * (int) xb.qs[i % 32];
                    }
                }
                const Q81Block* xb = x.data() + c * (kIn / 32) + block_in_row * 2;
                expected += (float) scales[block] * ((float) xb[0].ds[0] * sums[0] +
                                                     (float) xb[1].ds[0] * sums[1]);
            }
            const double delta = (double) got[(size_t) c * kOut + row] - expected;
            error += delta * delta;
            reference += (double) expected * expected;
        }
    }
    const double relative_l2 = std::sqrt(error / std::max(reference, 1e-30));
    if (!std::isfinite(relative_l2) || relative_l2 > 2e-6)
        throw std::runtime_error("ternary64 GPU output failed CPU check");
    std::printf("parity T=%d: relative L2 %.3e\n", ncols, relative_l2);
}

void check_q2_rowwarp(const std::vector<float>& reference, const std::vector<float>& got, int ncols) {
    double error = 0.0, norm = 0.0;
    size_t different = 0;
    for (size_t i = 0; i < (size_t) ncols * kOut; ++i) {
        different += reference[i] != got[i];
        const double delta = (double) got[i] - reference[i];
        error += delta * delta;
        norm += (double) reference[i] * reference[i];
    }
    const double relative_l2 = std::sqrt(error / std::max(norm, 1e-30));
    if (!std::isfinite(relative_l2) || relative_l2 > 2e-6)
        throw std::runtime_error("rowwarp Q2 output differs from current Q2");
    std::printf("Q2 rowwarp parity T=%d: relative L2 %.3e, %zu/%zu values differ bitwise\n",
                ncols, relative_l2, different, (size_t) ncols * kOut);
}

struct TripleResult { float q2_us; float q2_rowwarp_us; float ternary_us; };

template <class Q2, class Q2Rowwarp, class T64>
TripleResult bench_three(Q2&& q2, Q2Rowwarp&& q2_rowwarp, T64&& t64, int calls, sycl::queue& q) {
    for (int i = 0; i < 64; ++i) { q2(i); q2_rowwarp(i); t64(i); }
    q.wait_and_throw();
    std::vector<double> q2_times, q2_rowwarp_times, t64_times;
    for (int batch = 0; batch < 8; ++batch) {
        auto run = [&](auto&& fn) {
            const auto start = std::chrono::steady_clock::now();
            for (int i = 0; i < calls; ++i) fn(i);
            q.wait_and_throw();
            return std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count() / calls;
        };
        double a, b, c;
        if (batch % 3 == 0) { a = run(q2); b = run(q2_rowwarp); c = run(t64); }
        else if (batch % 3 == 1) { c = run(t64); a = run(q2); b = run(q2_rowwarp); }
        else { b = run(q2_rowwarp); c = run(t64); a = run(q2); }
        if (batch >= 3) { q2_times.push_back(a); q2_rowwarp_times.push_back(b); t64_times.push_back(c); }
    }
    return {median(q2_times), median(q2_rowwarp_times), median(t64_times)};
}

void print_result(const char* memory, const char* reuse, int ncols, TripleResult r,
                  size_t q2_bytes, size_t ternary_bytes) {
    const double logical = (double) kIn * kOut * ncols;
    std::printf("%-6s %-8s T=%d: current Q2 %8.3f us, rowwarp Q2 %8.3f us, ternary64 %8.3f us; "
                "ternary speed %.3fx/current %.3fx/rowwarp; logical %.1f/%.1f/%.1f Gweight/s; bytes %.2f/%.2f/%.2f GB/s\n",
                memory, reuse, ncols, r.q2_us, r.q2_rowwarp_us, r.ternary_us,
                r.q2_us / r.ternary_us, r.q2_rowwarp_us / r.ternary_us,
                logical / r.q2_us / 1e3, logical / r.q2_rowwarp_us / 1e3, logical / r.ternary_us / 1e3,
                q2_bytes / r.q2_us / 1e3, q2_bytes / r.q2_rowwarp_us / 1e3,
                ternary_bytes / r.ternary_us / 1e3);
}

int run(const char* path) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    sycl::queue& q = dpct::get_in_order_queue();
    std::printf("device: %s\n", q.get_device().get_info<sycl::info::device::name>().c_str());
    const std::vector<uint8_t> q2 = read_file(path);
    std::vector<uint8_t> codes;
    std::vector<sycl::half> scales;
    make_ternary(q2, codes, scales);
    const auto lut = make_lut();
    const int copies = std::getenv("STRATA_T64_COPIES") ? std::atoi(std::getenv("STRATA_T64_COPIES")) : 128;
    const int calls = std::getenv("STRATA_T64_CALLS") ? std::atoi(std::getenv("STRATA_T64_CALLS")) : 256;
    if (copies < 1 || calls < 1) throw std::runtime_error("copies and calls must be positive");
    const size_t q2_bytes = q2.size(), code_bytes = codes.size();
    const size_t scale_bytes = scales.size() * sizeof(sycl::half);
    const size_t ternary_bytes = code_bytes + scale_bytes;
    std::printf("shape %dx%d, copies %d: Q2_0 %.3f MiB, ternary64 %.3f MiB (%.2f%% smaller)\n",
                kOut, kIn, copies, q2_bytes / 1048576.0, ternary_bytes / 1048576.0,
                100.0 * (1.0 - (double) ternary_bytes / q2_bytes));

    auto* q2_dev = sycl::malloc_device<uint8_t>(q2_bytes * copies, q);
    auto* code_dev = sycl::malloc_device<uint8_t>(code_bytes * copies, q);
    auto* scale_dev = sycl::malloc_device<sycl::half>(scales.size() * copies, q);
    auto* q2_host = sycl::malloc_host<uint8_t>(q2_bytes * copies, q);
    auto* code_host = sycl::malloc_host<uint8_t>(code_bytes * copies, q);
    auto* scale_host = sycl::malloc_host<sycl::half>(scales.size() * copies, q);
    auto* lut_dev = sycl::malloc_device<uint64_t>(lut.size(), q);
    auto* x = sycl::malloc_device<float>(4 * kIn, q);
    auto* xq = sycl::malloc_device<Q81Block>(4 * (kIn / 32), q);
    auto* y = sycl::malloc_device<float>(4 * kOut, q);
    if (!q2_dev || !code_dev || !scale_dev || !q2_host || !code_host || !scale_host || !lut_dev || !x || !xq || !y)
        throw std::bad_alloc();
    for (int i = 0; i < copies; ++i) {
        std::memcpy(q2_host + (size_t) i * q2_bytes, q2.data(), q2_bytes);
        std::memcpy(code_host + (size_t) i * code_bytes, codes.data(), code_bytes);
        std::memcpy(scale_host + (size_t) i * scales.size(), scales.data(), scale_bytes);
    }
    q.memcpy(q2_dev, q2_host, q2_bytes * copies);
    q.memcpy(code_dev, code_host, code_bytes * copies);
    q.memcpy(scale_dev, scale_host, scale_bytes * copies);
    q.memcpy(lut_dev, lut.data(), lut.size() * sizeof(uint64_t));
    std::vector<float> hx(4 * kIn);
    for (size_t i = 0; i < hx.size(); ++i) hx[i] = (float) ((i * 7919 + 17) % 2001) / 1000.0f - 1.0f;
    q.memcpy(x, hx.data(), hx.size() * sizeof(float)).wait();
    strata::kernels::native_quantize_q8_1(x, xq, kIn, 4, &q);
    q.wait_and_throw();

    std::vector<Q81Block> hxq(4 * (kIn / 32));
    std::vector<float> got(4 * kOut), q2_reference(4 * kOut);
    q.memcpy(hxq.data(), xq, hxq.size() * sizeof(Q81Block)).wait();
    for (int ncols : {1, 4}) {
        ternary64_mmvq(code_dev, scale_dev, lut_dev, xq, y, ncols, q);
        q.memcpy(got.data(), y, (size_t) ncols * kOut * sizeof(float)).wait();
        check_output(codes, scales, lut, hxq, got, ncols);
        strata::kernels::native_mmvq(42, q2_dev, xq, y, kIn, kOut, ncols, &q);
        q.memcpy(q2_reference.data(), y, (size_t) ncols * kOut * sizeof(float)).wait();
        q2_rowwarp_mmvq(q2_dev, xq, y, ncols, q);
        q.memcpy(got.data(), y, (size_t) ncols * kOut * sizeof(float)).wait();
        check_q2_rowwarp(q2_reference, got, ncols);
        for (bool mapped : {false, true}) {
            const uint8_t* q2_base = mapped ? q2_host : q2_dev;
            const uint8_t* code_base = mapped ? code_host : code_dev;
            const sycl::half* scale_base = mapped ? scale_host : scale_dev;
            for (bool stream : {false, true}) {
                auto index = [&](int i) { return stream ? i % copies : 0; };
                auto q2_call = [&](int i) {
                    const int slot = index(i);
                    strata::kernels::native_mmvq(42, q2_base + (size_t) slot * q2_bytes, xq, y,
                                                  kIn, kOut, ncols, &q);
                };
                auto t64_call = [&](int i) {
                    const int slot = index(i);
                    ternary64_mmvq(code_base + (size_t) slot * code_bytes,
                                   scale_base + (size_t) slot * scales.size(), lut_dev, xq, y, ncols, q);
                };
                auto q2_rowwarp_call = [&](int i) {
                    const int slot = index(i);
                    q2_rowwarp_mmvq(q2_base + (size_t) slot * q2_bytes, xq, y, ncols, q);
                };
                print_result(mapped ? "mapped" : "VRAM", stream ? "stream" : "hot", ncols,
                             bench_three(q2_call, q2_rowwarp_call, t64_call, calls, q),
                             q2_bytes, ternary_bytes);
            }
        }
    }
    for (void* p : {(void*) q2_dev, (void*) code_dev, (void*) scale_dev, (void*) q2_host, (void*) code_host,
                    (void*) scale_host, (void*) lut_dev, (void*) x, (void*) xq, (void*) y}) sycl::free(p, q);
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 2) {
            std::fprintf(stderr, "usage: ternary64_bench FIRST_EXPERT_Q2_0.bin\n");
            return 2;
        }
        return run(argv[1]);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "ternary64_bench: %s\n", e.what());
        return 1;
    }
}
