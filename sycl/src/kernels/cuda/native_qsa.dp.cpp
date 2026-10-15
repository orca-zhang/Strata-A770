// Adapted from llama.cpp 3cf03257f219afbe7334045ff7c6a06ac68c627d:
// ggml/src/ggml-cuda/{norm.cu,common.cuh,unary.cu}.
//
// MIT License
// Copyright (c) 2023-2026 The ggml authors
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#define DPCT_PROFILING_ENABLED
#include <sycl/sycl.hpp>
#include <dpct/dpct.hpp>
#include "strata/sycl_queue.hpp"
#include "strata/kernels/native_qsa.hpp"
#include "strata/kernels/q8_1_finite.hpp"
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

namespace strata::kernels {
namespace {
std::atomic<bool> enabled{false};
__dpct_inline__ float warp_sum(float value) {
#pragma unroll
    for (int offset = 16; offset; offset >>= 1)
        /*
        DPCT1108: '__shfl_xor_sync' was migrated with the experimental
        feature masked sub_group function which may not be supported by all
        compilers or runtimes. You may need to adjust the code.
        */
        value += dpct::experimental::permute_sub_group_by_xor(
            0xffffffffu, sycl::ext::oneapi::this_work_item::get_sub_group(),
            value, offset);
    return value;
}
template <int BlockSize>
__dpct_inline__ void norm(const float *input, const float *__restrict__ gamma,
                          float *output, int n_cols, float epsilon) {
    auto item_ct1 = sycl::ext::oneapi::this_work_item::get_nd_item<3>();
    const int tid = item_ct1.get_local_id(2);
    const std::size_t row_offset = std::size_t(item_ct1.get_group(2)) * n_cols;
    input += row_offset; output += row_offset;
    float partial = 0.0f;
    for (std::size_t col = tid; col < std::size_t(n_cols); col += BlockSize) {
        const float x = input[col];
        partial += x * x;
    }
    auto &sums =
        *sycl::ext::oneapi::group_local_memory_for_overwrite<float[32]>(
            sycl::ext::oneapi::this_work_item::get_work_group<3>());
    partial = warp_sum(partial);
    const int lane = tid % 32;
    if (lane == 0) sums[tid / 32] = partial;
    // All reads of input for the reduction precede this barrier. Afterwards,
    // each thread reads/writes only its own elements, allowing exact in-place use.
    item_ct1.barrier(sycl::access::fence_space::local_space);
    partial = lane < BlockSize / 32 ? sums[lane] : 0.0f;
    partial = warp_sum(partial);
    const float mean = partial / n_cols;
    const float scale = sycl::rsqrt(mean + epsilon);
#pragma unroll
    for (std::size_t col = tid; col < std::size_t(n_cols); col += BlockSize)
        output[col] = scale * input[col] * gamma[col];
}
template <int BlockSize>
__dpct_inline__ void norm_grouped(const float *input, const float *__restrict__ gamma,
                                  float *output, int n_cols, int groups, float epsilon) {
    auto item_ct1 = sycl::ext::oneapi::this_work_item::get_nd_item<3>();
    const int tid = item_ct1.get_local_id(2);
    const std::size_t row = item_ct1.get_group(2);
    input += row * n_cols;
    output += row * n_cols;
    gamma += (row % unsigned(groups)) * n_cols;
    float partial = 0.0f;
    for (std::size_t col = tid; col < std::size_t(n_cols); col += BlockSize) {
        const float x = input[col];
        partial += x * x;
    }
    auto &sums = *sycl::ext::oneapi::group_local_memory_for_overwrite<float[32]>(
        sycl::ext::oneapi::this_work_item::get_work_group<3>());
    partial = warp_sum(partial);
    const int lane = tid % 32;
    if (lane == 0) sums[tid / 32] = partial;
    item_ct1.barrier(sycl::access::fence_space::local_space);
    partial = lane < BlockSize / 32 ? sums[lane] : 0.0f;
    partial = warp_sum(partial);
    const float scale = sycl::rsqrt(partial / n_cols + epsilon);
    for (std::size_t col = tid; col < std::size_t(n_cols); col += BlockSize)
        output[col] = scale * input[col] * gamma[col];
}
struct QsaQ81 {
    sycl::half2 ds;
    int8_t qs[32];
};

template <bool FusedQ8>
__dpct_inline__ void gate_kernel(const float *attn, const float *__restrict__ q_full,
                                 float *output, QsaQ81 *xq_out,
                                 int n_head, int head_dim) {
    auto item_ct1 = sycl::ext::oneapi::this_work_item::get_nd_item<3>();
    const std::size_t i =
        std::size_t(item_ct1.get_group(2)) * item_ct1.get_local_range(2) +
        item_ct1.get_local_id(2);
    const bool active = (i < std::size_t(n_head) * head_dim);
    float val = 0.0f;
    if (active) {
        const std::size_t head = i / head_dim, channel = i % head_dim;
        const float raw = q_full[head * 2 * head_dim + head_dim + channel];
        const float sigmoid = 1.0f / (1.0f + sycl::native::exp(-raw));
        val = attn[i] * sigmoid;
        if (output != nullptr) {
            output[i] = val;
        }
    }
    if constexpr (FusedQ8) {
        if (xq_out != nullptr) {
            auto sg = item_ct1.get_sub_group();
            float amax = active ? sycl::fabs(val) : 0.0f;
            float sum = active ? val : 0.0f;
#pragma unroll
            for (int offset = 16; offset > 0; offset >>= 1) {
                amax = sycl::fmax(amax, sycl::permute_group_by_xor(sg, amax, offset));
                sum += sycl::permute_group_by_xor(sg, sum, offset);
            }
            if (active) {
                const float d = q8_1_finite(amax / 127.0f);
                const int lane = item_ct1.get_local_id(2) % 32;
                const size_t block_idx = i / 32;
                xq_out[block_idx].qs[lane] = q8_1_quant(val, d, amax);
                if (lane == 0) {
                    xq_out[block_idx].ds = q8_1_ds(d, sum);
                }
            }
        }
    }
}
std::size_t elements(int cols, int rows) {
    if (cols <= 0 || rows <= 0 || std::uint64_t(cols) * rows > std::uint64_t(std::numeric_limits<int>::max()))
        throw std::invalid_argument("native QSA requires positive bounded dimensions");
    return std::size_t(cols) * rows;
}
bool valid(const void* ptr, std::size_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(ptr);
    return ptr && address % 4 == 0 && bytes <= UINTPTR_MAX - address;
}
bool overlap(const void* a, std::size_t an, const void* b, std::size_t bn) {
    const auto ap = reinterpret_cast<std::uintptr_t>(a), bp = reinterpret_cast<std::uintptr_t>(b);
    return ap < bp + bn && bp < ap + an;
}
void buffers(const float* input, std::size_t in_bytes, const float* weight, std::size_t weight_bytes,
             float* output, void* stream) {
    if (!stream || !valid(input, in_bytes) || !valid(weight, weight_bytes) || !valid(output, in_bytes) ||
        overlap(input, in_bytes, weight, weight_bytes) || overlap(output, in_bytes, weight, weight_bytes) ||
        (input != output && overlap(input, in_bytes, output, in_bytes)))
        throw std::invalid_argument("native QSA requires a stream, aligned spans, and disjoint buffers or exact input/output alias");
}
void check_launch() {
    /*
    DPCT1010: SYCL uses exceptions to report errors and does not use the
    error codes. The cudaGetLastError function call was replaced with 0. You
    need to rewrite this code.
    */
    const auto result = 0;
    /*
    DPCT1000: Error handling if-stmt was detected but could not be
    rewritten.
    */
    if (result != 0)
        /*
        DPCT1009: SYCL reports errors using exceptions and does not use
        error codes. Please replace the "get_error_string_dummy(...)" with a
        real error-handling function.
        */
        /*
        DPCT1001: The statement could not be removed.
        */
        throw std::runtime_error(std::string("native QSA launch: ") +
                                 dpct::get_error_string_dummy(result));
}
} // namespace

void native_qsa_set_enabled(bool value) { enabled.store(value, std::memory_order_relaxed); }
bool native_qsa_enabled() { return enabled.load(std::memory_order_relaxed); }

void native_qsa_rms_norm_weighted(const float* input, const float* gamma, float* output,
                                  int n_cols, int n_rows, float epsilon, void* stream) {
    const auto count = elements(n_cols, n_rows);
    if (!std::isfinite(epsilon) || epsilon < 0.0f)
        throw std::invalid_argument("native QSA requires finite nonnegative epsilon");
    buffers(input, count * 4, gamma, std::size_t(n_cols) * 4, output, stream);
    if (n_cols < 1024)
    {
        auto exp_props = sycl::ext::oneapi::experimental::properties{
            sycl::ext::oneapi::experimental::use_root_sync};

        ((sycl::queue *)(strata::q_of(stream)))
            ->parallel_for<
                dpct_kernel_name<class norm_bffbae, dpct_kernel_scalar<256>>>(
                sycl::nd_range<3>(sycl::range(1, 1, unsigned(n_rows)) *
                                      sycl::range(1, 1, 256),
                                  sycl::range(1, 1, 256)),
                exp_props,
                [=](sycl::nd_item<3> item_ct1)
                    [[sycl::reqd_sub_group_size(32)]] {
                        norm<256>(input, gamma, output, n_cols, epsilon);
                    });
    } else
    /*
    DPCT1049: The work-group size passed to the SYCL kernel may exceed
    the limit. To get the device limit, query
    info::device::max_work_group_size. Adjust the work-group size if needed.
    */
    {
        auto exp_props = sycl::ext::oneapi::experimental::properties{
            sycl::ext::oneapi::experimental::use_root_sync};

        ((sycl::queue *)(strata::q_of(stream)))
            ->parallel_for<
                dpct_kernel_name<class norm_de4c8c, dpct_kernel_scalar<1024>>>(
                sycl::nd_range<3>(sycl::range(1, 1, unsigned(n_rows)) *
                                      sycl::range(1, 1, 1024),
                                  sycl::range(1, 1, 1024)),
                exp_props,
                [=](sycl::nd_item<3> item_ct1)
                    [[sycl::reqd_sub_group_size(32)]] {
                        norm<1024>(input, gamma, output, n_cols, epsilon);
                    });
    }
    check_launch();
}
void native_qsa_rms_norm_grouped(const float* input, const float* gamma, float* output,
                                 int n_cols, int groups, int n_rows, float epsilon, void* stream) {
    if (groups < 1) throw std::invalid_argument("native QSA requires positive bounded dimensions");
    const auto count = elements(n_cols, n_rows);
    if (!std::isfinite(epsilon) || epsilon < 0.0f)
        throw std::invalid_argument("native QSA requires finite nonnegative epsilon");
    buffers(input, count * 4, gamma, std::size_t(n_cols) * groups * 4, output, stream);
    auto props = sycl::ext::oneapi::experimental::properties{
        sycl::ext::oneapi::experimental::use_root_sync};
    auto* queue = (sycl::queue*) strata::q_of(stream);
    if (n_cols < 1024)
        queue->parallel_for<dpct_kernel_name<class norm_grouped_256, dpct_kernel_scalar<256>>>(
            sycl::nd_range<3>(sycl::range(1, 1, unsigned(n_rows)) * sycl::range(1, 1, 256),
                              sycl::range(1, 1, 256)), props,
            [=](sycl::nd_item<3>) [[sycl::reqd_sub_group_size(32)]] {
                norm_grouped<256>(input, gamma, output, n_cols, groups, epsilon);
            });
    else
        queue->parallel_for<dpct_kernel_name<class norm_grouped_1024, dpct_kernel_scalar<1024>>>(
            sycl::nd_range<3>(sycl::range(1, 1, unsigned(n_rows)) * sycl::range(1, 1, 1024),
                              sycl::range(1, 1, 1024)), props,
            [=](sycl::nd_item<3>) [[sycl::reqd_sub_group_size(32)]] {
                norm_grouped<1024>(input, gamma, output, n_cols, groups, epsilon);
            });
    check_launch();
}
void native_qsa_gate_apply(const float* attn, const float* q_full, float* output,
                           int n_head, int head_dim, void* stream, void* xq_out) {
    if (n_head < 1 || head_dim < 1)
        throw std::invalid_argument("native QSA requires positive bounded dimensions");
    if (!attn || !q_full || (!output && !xq_out))
        throw std::invalid_argument("native QSA requires valid buffers");
    const auto count = elements(head_dim, n_head);
    if (output) {
        buffers(attn, count * 4, q_full, count * 8, output, stream);
    } else {
        if (!stream || !valid(attn, count * 4) || !valid(q_full, count * 8) ||
            overlap(attn, count * 4, q_full, count * 8))
            throw std::invalid_argument("native QSA requires a stream, aligned spans, and disjoint buffers");
    }
    if (xq_out) {
        const size_t q8_bytes = (count / 32) * sizeof(QsaQ81);
        if (!valid(xq_out, q8_bytes) || overlap(attn, count * 4, xq_out, q8_bytes) ||
            overlap(q_full, count * 8, xq_out, q8_bytes) ||
            (output && overlap(output, count * 4, xq_out, q8_bytes)))
            throw std::invalid_argument("native QSA requires disjoint Q8 output buffer");
    }
    auto exp_props = sycl::ext::oneapi::experimental::properties{
        sycl::ext::oneapi::experimental::use_root_sync};
    auto* queue = (sycl::queue*)(strata::q_of(stream));
    if (xq_out != nullptr) {
        queue->parallel_for<dpct_kernel_name<class gate_q8_tail_fused>>(
            sycl::nd_range<3>(
                sycl::range(1, 1, unsigned((count + 255) / 256)) *
                    sycl::range(1, 1, 256),
                sycl::range(1, 1, 256)),
            exp_props, [=](sycl::nd_item<3> item_ct1) [[sycl::reqd_sub_group_size(32)]] {
                gate_kernel<true>(attn, q_full, output, (QsaQ81*) xq_out, n_head, head_dim);
            });
    } else {
        queue->parallel_for<dpct_kernel_name<class gate_148967>>(
            sycl::nd_range<3>(
                sycl::range(1, 1, unsigned((count + 255) / 256)) *
                    sycl::range(1, 1, 256),
                sycl::range(1, 1, 256)),
            exp_props, [=](sycl::nd_item<3> item_ct1) {
                gate_kernel<false>(attn, q_full, output, nullptr, n_head, head_dim);
            });
    }
    check_launch();
}

void native_qsa_gate_apply(const float* attn, const float* q_full, float* output,
                           int n_head, int head_dim, void* stream) {
    native_qsa_gate_apply(attn, q_full, output, n_head, head_dim, stream, nullptr);
}
} // namespace strata::kernels
