// src/kernels/cuda/verify_kernels.cu - see include/strata/kernels/verify_kernels.hpp.
//
// The per-token arithmetic of every kernel here is transcribed from its single-token original (fused_gdn.cu,
// elementwise.cu) with the same operation order, so a verify window reproduces plain decode bit for bit.
#define DPCT_PROFILING_ENABLED
#include <sycl/sycl.hpp>
#include <sycl/ext/intel/esimd.hpp>
#include <sycl/ext/oneapi/experimental/device_architecture.hpp>
#include <sycl/ext/intel/experimental/grf_size_properties.hpp>
#include <dpct/dpct.hpp>
#include "strata/sycl_queue.hpp"
#include "strata/sycl_doorbell.hpp"
#include "strata/kernels/verify_kernels.hpp"
#include "strata/kernels/resident_plan_mirror.hpp"
#include "strata/kernels/dp4a.hpp"
#include "strata/kernels/q8_1_finite.hpp"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>

#ifndef STRATA_PLAN_LOCAL
#define STRATA_PLAN_LOCAL 1   // 0: the original one-thread plan kernel (A/B)
#endif

namespace strata::kernels {
namespace {
constexpr int S=128, RG=4, RPG=32;
struct GdnQ81 {
    sycl::half2 ds;
    int8_t qs[32];
};
static_assert(sizeof(GdnQ81) == 36);

template <int SG, bool Q>
__dpct_inline__ void gdn_step_norm_multi_kernel(
    float *__restrict__ state, const float *__restrict__ hbuf, int C,
    const float *__restrict__ gate, const float *__restrict__ beta,
    const float *__restrict__ z, const float *__restrict__ gamma, float eps,
    float *__restrict__ y, int h_k, int h_v, int T,
    const int32_t *__restrict__ n_keep, int t_out_begin,
    GdnQ81 *__restrict__ xq) {
    auto item_ct1 = sycl::ext::oneapi::this_work_item::get_nd_item<3>();
auto &sk = *sycl::ext::oneapi::group_local_memory_for_overwrite<float[S]>(
    sycl::ext::oneapi::this_work_item::get_work_group<3>());
    auto &sq = *sycl::ext::oneapi::group_local_memory_for_overwrite<float[S]>(
        sycl::ext::oneapi::this_work_item::get_work_group<3>());
    auto &red =
        *sycl::ext::oneapi::group_local_memory_for_overwrite<float[RG][S]>(
            sycl::ext::oneapi::this_work_item::get_work_group<3>());
    auto &wsum = *sycl::ext::oneapi::group_local_memory_for_overwrite<
        float[S * RG / 32]>(
        sycl::ext::oneapi::this_work_item::get_work_group<3>());
    const int head = item_ct1.get_group(2);
    const int col = item_ct1.get_local_id(2);
    const int rg = item_ct1.get_local_id(1);
    const int tid = rg * S + col;
    const int qh = head % h_k;
    const int qk = S * h_k;             // q at [0, qk), k at [qk, 2qk), v at [2qk, ...)
    const int value_dim = S * h_v;
    const int n = n_keep ? *n_keep : T;
    float s[RPG];
    float* base = state + ((size_t) (rg * RPG) * h_v + head) * S + col;
    const size_t row_stride = (size_t) h_v * S;
#pragma unroll
    for (int r = 0; r < RPG; ++r) s[r] = base[r * row_stride];
    for (int t = 0; t < n; ++t) {
        const float* ht = hbuf + (size_t) t * C;
        /*
        DPCT1118: SYCL group functions and algorithms must be encountered in
        converged control flow. You may need to adjust the code.
        */
        /*
        DPCT1065: Consider replacing sycl::nd_item::barrier() with
        sycl::nd_item::barrier(sycl::access::fence_space::local_space) for
        better performance if there is no access to global memory.
        */
        item_ct1.barrier(); // the previous token is done with sk/sq/red/wsum
        if (tid < S) { sk[tid] = ht[qk + qh * S + tid]; sq[tid] = ht[qh * S + tid]; }
        /*
        DPCT1118: SYCL group functions and algorithms must be encountered in
        converged control flow. You may need to adjust the code.
        */
        /*
        DPCT1065: Consider replacing sycl::nd_item::barrier() with
        sycl::nd_item::barrier(sycl::access::fence_space::local_space) for
        better performance if there is no access to global memory.
        */
        item_ct1.barrier();
        const float g = sycl::native::exp(gate[(size_t)t * h_v + head]);
        float kv = 0.0f;
#pragma unroll
        for (int r = 0; r < RPG; ++r)
            kv = sycl::fma(s[r], sk[rg * RPG + r], kv);
        red[rg][col] = kv;
        /*
        DPCT1118: SYCL group functions and algorithms must be encountered in
        converged control flow. You may need to adjust the code.
        */
        /*
        DPCT1065: Consider replacing sycl::nd_item::barrier() with
        sycl::nd_item::barrier(sycl::access::fence_space::local_space) for
        better performance if there is no access to global memory.
        */
        item_ct1.barrier();
        const float kv_col = red[0][col] + red[1][col] + red[2][col] + red[3][col];
        const float delta = (ht[2 * qk + head * S + col] - g * kv_col) * beta[(size_t) t * h_v + head];
        float o = 0.0f;
#pragma unroll
        for (int r = 0; r < RPG; ++r) {
            s[r] = sycl::fma((float)g, s[r], sk[rg * RPG + r] * delta);
            o = sycl::fma(s[r], sq[rg * RPG + r], o);
        }
        /*
        DPCT1118: SYCL group functions and algorithms must be encountered in
        converged control flow. You may need to adjust the code.
        */
        /*
        DPCT1065: Consider replacing sycl::nd_item::barrier() with
        sycl::nd_item::barrier(sycl::access::fence_space::local_space) for
        better performance if there is no access to global memory.
        */
        item_ct1.barrier();
        red[rg][col] = o;
        /*
        DPCT1118: SYCL group functions and algorithms must be encountered in
        converged control flow. You may need to adjust the code.
        */
        /*
        DPCT1065: Consider replacing sycl::nd_item::barrier() with
        sycl::nd_item::barrier(sycl::access::fence_space::local_space) for
        better performance if there is no access to global memory.
        */
        item_ct1.barrier();
        float oc = 0.0f, sq_part = 0.0f;
        if (rg == 0) {
            oc = (red[0][col] + red[1][col] + red[2][col] + red[3][col]) *
                 sycl::rsqrt((float)S);
            sq_part = oc * oc;
        }
        if (t < t_out_begin) continue;   // a replayed token: its state update is needed, its output is not
        /*
DPCT1108: '__shfl_xor_sync' was migrated with the experimental feature
masked sub_group function which may not be supported by all compilers or
runtimes. You may need to adjust the code.
*/
        if constexpr (SG == 16) {
            // Preserve the SIMD32 XOR tree across two SIMD16 subgroups, with fewer live GRFs per subgroup.
            item_ct1.barrier(sycl::access::fence_space::local_space);
            red[rg][col] = sq_part;
            item_ct1.barrier(sycl::access::fence_space::local_space);
            sq_part += red[rg][col ^ 16];
        } else {
            sq_part += dpct::experimental::permute_sub_group_by_xor(
                0xffffffffu, item_ct1.get_sub_group(), sq_part, 16);
        }
#pragma unroll
        for (int o2 = 8; o2 > 0; o2 >>= 1) sq_part +=
            dpct::experimental::permute_sub_group_by_xor(
                0xffffffffu, sycl::ext::oneapi::this_work_item::get_sub_group(),
                sq_part, o2);
        if ((tid & 31) == 0) wsum[tid >> 5] = sq_part;
        /*
        DPCT1118: SYCL group functions and algorithms must be encountered in
        converged control flow. You may need to adjust the code.
        */
        /*
        DPCT1065: Consider replacing sycl::nd_item::barrier() with
        sycl::nd_item::barrier(sycl::access::fence_space::local_space) for
        better performance if there is no access to global memory.
        */
        item_ct1.barrier();
        float yv = 0.0f;
        if (rg == 0) {
            const float ss = wsum[0] + wsum[1] + wsum[2] + wsum[3];
            const float scale = sycl::rsqrt(ss / (float)S + eps);
            const float zz = z[(size_t) t * value_dim + head * S + col];
            yv = oc * scale * gamma[col] * (1.0f / (1.0f + sycl::native::exp(-zz)));
            y[(size_t)t * value_dim + head * S + col] = yv;
        }
        if constexpr (Q) {
            float amax = sycl::fabs(yv), sum = yv;
            if constexpr (SG == 16) {
                if (rg == 0) {
                    red[0][col] = amax;
                    red[1][col] = sum;
                }
                item_ct1.barrier(sycl::access::fence_space::local_space);
                if (rg == 0) {
                    amax = sycl::fmax(amax, red[0][col ^ 16]);
                    sum += red[1][col ^ 16];
                }
            } else if (rg == 0) {
                amax = sycl::fmax(amax, sycl::permute_group_by_xor(item_ct1.get_sub_group(), amax, 16));
                sum += sycl::permute_group_by_xor(item_ct1.get_sub_group(), sum, 16);
            }
            if (rg == 0) {
#pragma unroll
                for (int offset = 8; offset > 0; offset >>= 1) {
                    amax = sycl::fmax(amax, sycl::permute_group_by_xor(item_ct1.get_sub_group(), amax, offset));
                    sum += sycl::permute_group_by_xor(item_ct1.get_sub_group(), sum, offset);
                }
                const size_t idx = (size_t) (t - t_out_begin) * value_dim + head * S + col;
                const float d = q8_1_finite(amax / 127.0f);
                xq[idx / 32].qs[idx % 32] = q8_1_quant(yv, d, amax);
                if ((idx & 31) == 0) xq[idx / 32].ds = q8_1_ds(d, sum);
            }
        }
    }
    if (n_keep != nullptr && n > 0) {
#pragma unroll
        for (int r = 0; r < RPG; ++r) base[r * row_stride] = s[r];
    }
}

template <int SG, bool Q>
__dpct_inline__ void gdn_sync_1(
    float *__restrict__ state, const float *__restrict__ hbuf, int C,
    const float *__restrict__ gate, const float *__restrict__ beta,
    const float *__restrict__ z, const float *__restrict__ gamma, float eps,
    float *__restrict__ y, int h_k, int h_v, int T,
    const int32_t *__restrict__ n_keep, int t_out_begin,
    GdnQ81 *__restrict__ xq) {
    auto item_ct1 = sycl::ext::oneapi::this_work_item::get_nd_item<3>();
auto &sk = *sycl::ext::oneapi::group_local_memory_for_overwrite<float[S]>(
    sycl::ext::oneapi::this_work_item::get_work_group<3>());
    auto &sq = *sycl::ext::oneapi::group_local_memory_for_overwrite<float[S]>(
        sycl::ext::oneapi::this_work_item::get_work_group<3>());
    auto &red =
        *sycl::ext::oneapi::group_local_memory_for_overwrite<float[RG][S]>(
            sycl::ext::oneapi::this_work_item::get_work_group<3>());
    auto &wsum = *sycl::ext::oneapi::group_local_memory_for_overwrite<
        float[S * RG / 32]>(
        sycl::ext::oneapi::this_work_item::get_work_group<3>());
    const int head = item_ct1.get_group(2);
    const int col = item_ct1.get_local_id(2);
    const int rg = item_ct1.get_local_id(1);
    const int tid = rg * S + col;
    const int qh = head % h_k;
    const int qk = S * h_k;             // q at [0, qk), k at [qk, 2qk), v at [2qk, ...)
    const int value_dim = S * h_v;
    const int n = n_keep ? *n_keep : T;
    float s[RPG];
    float* base = state + ((size_t) (rg * RPG) * h_v + head) * S + col;
    const size_t row_stride = (size_t) h_v * S;
#pragma unroll
    for (int r = 0; r < RPG; ++r) s[r] = base[r * row_stride];
    for (int t = 0; t < n; ++t) {
        const float* ht = hbuf + (size_t) t * C;
        /*
        DPCT1118: SYCL group functions and algorithms must be encountered in
        converged control flow. You may need to adjust the code.
        */
        /*
        DPCT1065: Consider replacing sycl::nd_item::barrier() with
        sycl::nd_item::barrier(sycl::access::fence_space::local_space) for
        better performance if there is no access to global memory.
        */
        if (t > 0) item_ct1.barrier(sycl::access::fence_space::local_space); // retire the previous token before reusing local memory
        if (tid < S) { sk[tid] = ht[qk + qh * S + tid]; sq[tid] = ht[qh * S + tid]; }
        /*
        DPCT1118: SYCL group functions and algorithms must be encountered in
        converged control flow. You may need to adjust the code.
        */
        /*
        DPCT1065: Consider replacing sycl::nd_item::barrier() with
        sycl::nd_item::barrier(sycl::access::fence_space::local_space) for
        better performance if there is no access to global memory.
        */
        item_ct1.barrier(sycl::access::fence_space::local_space);
        const float g = sycl::native::exp(gate[(size_t)t * h_v + head]);
        float kv = 0.0f;
#pragma unroll
        for (int r = 0; r < RPG; ++r)
            kv = sycl::fma(s[r], sk[rg * RPG + r], kv);
        red[rg][col] = kv;
        /*
        DPCT1118: SYCL group functions and algorithms must be encountered in
        converged control flow. You may need to adjust the code.
        */
        /*
        DPCT1065: Consider replacing sycl::nd_item::barrier() with
        sycl::nd_item::barrier(sycl::access::fence_space::local_space) for
        better performance if there is no access to global memory.
        */
        item_ct1.barrier(sycl::access::fence_space::local_space);
        const float kv_col = red[0][col] + red[1][col] + red[2][col] + red[3][col];
        const float delta = (ht[2 * qk + head * S + col] - g * kv_col) * beta[(size_t) t * h_v + head];
        float o = 0.0f;
#pragma unroll
        for (int r = 0; r < RPG; ++r) {
            s[r] = sycl::fma((float)g, s[r], sk[rg * RPG + r] * delta);
            o = sycl::fma(s[r], sq[rg * RPG + r], o);
        }
        /*
        DPCT1118: SYCL group functions and algorithms must be encountered in
        converged control flow. You may need to adjust the code.
        */
        /*
        DPCT1065: Consider replacing sycl::nd_item::barrier() with
        sycl::nd_item::barrier(sycl::access::fence_space::local_space) for
        better performance if there is no access to global memory.
        */
        item_ct1.barrier(sycl::access::fence_space::local_space);
        red[rg][col] = o;
        /*
        DPCT1118: SYCL group functions and algorithms must be encountered in
        converged control flow. You may need to adjust the code.
        */
        /*
        DPCT1065: Consider replacing sycl::nd_item::barrier() with
        sycl::nd_item::barrier(sycl::access::fence_space::local_space) for
        better performance if there is no access to global memory.
        */
        item_ct1.barrier(sycl::access::fence_space::local_space);
        float oc = 0.0f, sq_part = 0.0f;
        if (rg == 0) {
            oc = (red[0][col] + red[1][col] + red[2][col] + red[3][col]) *
                 sycl::rsqrt((float)S);
            sq_part = oc * oc;
        }
        if (t < t_out_begin) continue;   // a replayed token: its state update is needed, its output is not
        /*
DPCT1108: '__shfl_xor_sync' was migrated with the experimental feature
masked sub_group function which may not be supported by all compilers or
runtimes. You may need to adjust the code.
*/
        if constexpr (SG == 16) {
            // Preserve the SIMD32 XOR tree across two SIMD16 subgroups, with fewer live GRFs per subgroup.
            item_ct1.barrier(sycl::access::fence_space::local_space);
            red[rg][col] = sq_part;
            item_ct1.barrier(sycl::access::fence_space::local_space);
            sq_part += red[rg][col ^ 16];
        } else {
            sq_part += dpct::experimental::permute_sub_group_by_xor(
                0xffffffffu, item_ct1.get_sub_group(), sq_part, 16);
        }
#pragma unroll
        for (int o2 = 8; o2 > 0; o2 >>= 1) sq_part +=
            dpct::experimental::permute_sub_group_by_xor(
                0xffffffffu, sycl::ext::oneapi::this_work_item::get_sub_group(),
                sq_part, o2);
        if ((tid & 31) == 0) wsum[tid >> 5] = sq_part;
        /*
        DPCT1118: SYCL group functions and algorithms must be encountered in
        converged control flow. You may need to adjust the code.
        */
        /*
        DPCT1065: Consider replacing sycl::nd_item::barrier() with
        sycl::nd_item::barrier(sycl::access::fence_space::local_space) for
        better performance if there is no access to global memory.
        */
        item_ct1.barrier(sycl::access::fence_space::local_space);
        float yv = 0.0f;
        if (rg == 0) {
            const float ss = wsum[0] + wsum[1] + wsum[2] + wsum[3];
            const float scale = sycl::rsqrt(ss / (float)S + eps);
            const float zz = z[(size_t) t * value_dim + head * S + col];
            yv = oc * scale * gamma[col] * (1.0f / (1.0f + sycl::native::exp(-zz)));
            y[(size_t)t * value_dim + head * S + col] = yv;
        }
        if constexpr (Q) {
            float amax = sycl::fabs(yv), sum = yv;
            if constexpr (SG == 16) {
                if (rg == 0) {
                    red[0][col] = amax;
                    red[1][col] = sum;
                }
                item_ct1.barrier(sycl::access::fence_space::local_space);
                if (rg == 0) {
                    amax = sycl::fmax(amax, red[0][col ^ 16]);
                    sum += red[1][col ^ 16];
                }
            } else if (rg == 0) {
                amax = sycl::fmax(amax, sycl::permute_group_by_xor(item_ct1.get_sub_group(), amax, 16));
                sum += sycl::permute_group_by_xor(item_ct1.get_sub_group(), sum, 16);
            }
            if (rg == 0) {
#pragma unroll
                for (int offset = 8; offset > 0; offset >>= 1) {
                    amax = sycl::fmax(amax, sycl::permute_group_by_xor(item_ct1.get_sub_group(), amax, offset));
                    sum += sycl::permute_group_by_xor(item_ct1.get_sub_group(), sum, offset);
                }
                const size_t idx = (size_t) (t - t_out_begin) * value_dim + head * S + col;
                const float d = q8_1_finite(amax / 127.0f);
                xq[idx / 32].qs[idx % 32] = q8_1_quant(yv, d, amax);
                if ((idx & 31) == 0) xq[idx / 32].ds = q8_1_ds(d, sum);
            }
        }
    }
    if (n_keep != nullptr && n > 0) {
#pragma unroll
        for (int r = 0; r < RPG; ++r) base[r * row_stride] = s[r];
    }
}

template <int SG, bool Q>
__dpct_inline__ void gdn_sync_2(
    float *__restrict__ state, const float *__restrict__ hbuf, int C,
    const float *__restrict__ gate, const float *__restrict__ beta,
    const float *__restrict__ z, const float *__restrict__ gamma, float eps,
    float *__restrict__ y, int h_k, int h_v, int T,
    const int32_t *__restrict__ n_keep, int t_out_begin,
    GdnQ81 *__restrict__ xq) {
    auto item_ct1 = sycl::ext::oneapi::this_work_item::get_nd_item<3>();
auto &sk = *sycl::ext::oneapi::group_local_memory_for_overwrite<float[S]>(
    sycl::ext::oneapi::this_work_item::get_work_group<3>());
    auto &sq = *sycl::ext::oneapi::group_local_memory_for_overwrite<float[S]>(
        sycl::ext::oneapi::this_work_item::get_work_group<3>());
    auto &red =
        *sycl::ext::oneapi::group_local_memory_for_overwrite<float[RG][S]>(
            sycl::ext::oneapi::this_work_item::get_work_group<3>());
    auto &kv_red = *sycl::ext::oneapi::group_local_memory_for_overwrite<float[RG][S]>(
        sycl::ext::oneapi::this_work_item::get_work_group<3>());
    auto &wsum = *sycl::ext::oneapi::group_local_memory_for_overwrite<
        float[S * RG / 32]>(
        sycl::ext::oneapi::this_work_item::get_work_group<3>());
    const int head = item_ct1.get_group(2);
    const int col = item_ct1.get_local_id(2);
    const int rg = item_ct1.get_local_id(1);
    const int tid = rg * S + col;
    const int qh = head % h_k;
    const int qk = S * h_k;             // q at [0, qk), k at [qk, 2qk), v at [2qk, ...)
    const int value_dim = S * h_v;
    const int n = n_keep ? *n_keep : T;
    float s[RPG];
    float* base = state + ((size_t) (rg * RPG) * h_v + head) * S + col;
    const size_t row_stride = (size_t) h_v * S;
#pragma unroll
    for (int r = 0; r < RPG; ++r) s[r] = base[r * row_stride];
    for (int t = 0; t < n; ++t) {
        const float* ht = hbuf + (size_t) t * C;
        /*
        DPCT1118: SYCL group functions and algorithms must be encountered in
        converged control flow. You may need to adjust the code.
        */
        /*
        DPCT1065: Consider replacing sycl::nd_item::barrier() with
        sycl::nd_item::barrier(sycl::access::fence_space::local_space) for
        better performance if there is no access to global memory.
        */
        if (t > 0) item_ct1.barrier(sycl::access::fence_space::local_space); // retire the previous token before reusing local memory
        if (tid < S) { sk[tid] = ht[qk + qh * S + tid]; sq[tid] = ht[qh * S + tid]; }
        /*
        DPCT1118: SYCL group functions and algorithms must be encountered in
        converged control flow. You may need to adjust the code.
        */
        /*
        DPCT1065: Consider replacing sycl::nd_item::barrier() with
        sycl::nd_item::barrier(sycl::access::fence_space::local_space) for
        better performance if there is no access to global memory.
        */
        item_ct1.barrier(sycl::access::fence_space::local_space);
        const float g = sycl::native::exp(gate[(size_t)t * h_v + head]);
        float kv = 0.0f;
#pragma unroll
        for (int r = 0; r < RPG; ++r)
            kv = sycl::fma(s[r], sk[rg * RPG + r], kv);
        kv_red[rg][col] = kv;
        /*
        DPCT1118: SYCL group functions and algorithms must be encountered in
        converged control flow. You may need to adjust the code.
        */
        /*
        DPCT1065: Consider replacing sycl::nd_item::barrier() with
        sycl::nd_item::barrier(sycl::access::fence_space::local_space) for
        better performance if there is no access to global memory.
        */
        item_ct1.barrier(sycl::access::fence_space::local_space);
        const float kv_col = kv_red[0][col] + kv_red[1][col] + kv_red[2][col] + kv_red[3][col];
        const float delta = (ht[2 * qk + head * S + col] - g * kv_col) * beta[(size_t) t * h_v + head];
        float o = 0.0f;
#pragma unroll
        for (int r = 0; r < RPG; ++r) {
            s[r] = sycl::fma((float)g, s[r], sk[rg * RPG + r] * delta);
            o = sycl::fma(s[r], sq[rg * RPG + r], o);
        }
        /*
        DPCT1118: SYCL group functions and algorithms must be encountered in
        converged control flow. You may need to adjust the code.
        */
        /*
        DPCT1065: Consider replacing sycl::nd_item::barrier() with
        sycl::nd_item::barrier(sycl::access::fence_space::local_space) for
        better performance if there is no access to global memory.
        */
        // Separate reductions keep output writes from racing state-dot readers.
        red[rg][col] = o;
        /*
        DPCT1118: SYCL group functions and algorithms must be encountered in
        converged control flow. You may need to adjust the code.
        */
        /*
        DPCT1065: Consider replacing sycl::nd_item::barrier() with
        sycl::nd_item::barrier(sycl::access::fence_space::local_space) for
        better performance if there is no access to global memory.
        */
        item_ct1.barrier(sycl::access::fence_space::local_space);
        float oc = 0.0f, sq_part = 0.0f;
        if (rg == 0) {
            oc = (red[0][col] + red[1][col] + red[2][col] + red[3][col]) *
                 sycl::rsqrt((float)S);
            sq_part = oc * oc;
        }
        if (t < t_out_begin) continue;   // a replayed token: its state update is needed, its output is not
        /*
DPCT1108: '__shfl_xor_sync' was migrated with the experimental feature
masked sub_group function which may not be supported by all compilers or
runtimes. You may need to adjust the code.
*/
        if constexpr (SG == 16) {
            // Preserve the SIMD32 XOR tree across two SIMD16 subgroups, with fewer live GRFs per subgroup.
            item_ct1.barrier(sycl::access::fence_space::local_space);
            red[rg][col] = sq_part;
            item_ct1.barrier(sycl::access::fence_space::local_space);
            sq_part += red[rg][col ^ 16];
        } else {
            sq_part += dpct::experimental::permute_sub_group_by_xor(
                0xffffffffu, item_ct1.get_sub_group(), sq_part, 16);
        }
#pragma unroll
        for (int o2 = 8; o2 > 0; o2 >>= 1) sq_part +=
            dpct::experimental::permute_sub_group_by_xor(
                0xffffffffu, sycl::ext::oneapi::this_work_item::get_sub_group(),
                sq_part, o2);
        if ((tid & 31) == 0) wsum[tid >> 5] = sq_part;
        /*
        DPCT1118: SYCL group functions and algorithms must be encountered in
        converged control flow. You may need to adjust the code.
        */
        /*
        DPCT1065: Consider replacing sycl::nd_item::barrier() with
        sycl::nd_item::barrier(sycl::access::fence_space::local_space) for
        better performance if there is no access to global memory.
        */
        item_ct1.barrier(sycl::access::fence_space::local_space);
        float yv = 0.0f;
        if (rg == 0) {
            const float ss = wsum[0] + wsum[1] + wsum[2] + wsum[3];
            const float scale = sycl::rsqrt(ss / (float)S + eps);
            const float zz = z[(size_t) t * value_dim + head * S + col];
            yv = oc * scale * gamma[col] * (1.0f / (1.0f + sycl::native::exp(-zz)));
            y[(size_t)t * value_dim + head * S + col] = yv;
        }
        if constexpr (Q) {
            float amax = sycl::fabs(yv), sum = yv;
            if constexpr (SG == 16) {
                if (rg == 0) {
                    red[0][col] = amax;
                    red[1][col] = sum;
                }
                item_ct1.barrier(sycl::access::fence_space::local_space);
                if (rg == 0) {
                    amax = sycl::fmax(amax, red[0][col ^ 16]);
                    sum += red[1][col ^ 16];
                }
            } else if (rg == 0) {
                amax = sycl::fmax(amax, sycl::permute_group_by_xor(item_ct1.get_sub_group(), amax, 16));
                sum += sycl::permute_group_by_xor(item_ct1.get_sub_group(), sum, 16);
            }
            if (rg == 0) {
#pragma unroll
                for (int offset = 8; offset > 0; offset >>= 1) {
                    amax = sycl::fmax(amax, sycl::permute_group_by_xor(item_ct1.get_sub_group(), amax, offset));
                    sum += sycl::permute_group_by_xor(item_ct1.get_sub_group(), sum, offset);
                }
                const size_t idx = (size_t) (t - t_out_begin) * value_dim + head * S + col;
                const float d = q8_1_finite(amax / 127.0f);
                xq[idx / 32].qs[idx % 32] = q8_1_quant(yv, d, amax);
                if ((idx & 31) == 0) xq[idx / 32].ds = q8_1_ds(d, sum);
            }
        }
    }
    if (n_keep != nullptr && n > 0) {
#pragma unroll
        for (int r = 0; r < RPG; ++r) base[r * row_stride] = s[r];
    }
}

template <int SG, bool Q>
__dpct_inline__ void gdn_sync_3(
    float *__restrict__ state, const float *__restrict__ hbuf, int C,
    const float *__restrict__ gate, const float *__restrict__ beta,
    const float *__restrict__ z, const float *__restrict__ gamma, float eps,
    float *__restrict__ y, int h_k, int h_v, int T,
    const int32_t *__restrict__ n_keep, int t_out_begin,
    GdnQ81 *__restrict__ xq) {
    auto item_ct1 = sycl::ext::oneapi::this_work_item::get_nd_item<3>();
auto &sk = *sycl::ext::oneapi::group_local_memory_for_overwrite<float[S]>(
    sycl::ext::oneapi::this_work_item::get_work_group<3>());
    auto &sq = *sycl::ext::oneapi::group_local_memory_for_overwrite<float[S]>(
        sycl::ext::oneapi::this_work_item::get_work_group<3>());
    auto &red =
        *sycl::ext::oneapi::group_local_memory_for_overwrite<float[RG][S]>(
            sycl::ext::oneapi::this_work_item::get_work_group<3>());
    auto &wsum = *sycl::ext::oneapi::group_local_memory_for_overwrite<
        float[S * RG / 32]>(
        sycl::ext::oneapi::this_work_item::get_work_group<3>());
    const int head = item_ct1.get_group(2);
    const int col = item_ct1.get_local_id(2);
    const int rg = item_ct1.get_local_id(1);
    const int tid = rg * S + col;
    const int qh = head % h_k;
    const int qk = S * h_k;             // q at [0, qk), k at [qk, 2qk), v at [2qk, ...)
    const int value_dim = S * h_v;
    const int n = n_keep ? *n_keep : T;
    float s[RPG];
    float* base = state + ((size_t) (rg * RPG) * h_v + head) * S + col;
    const size_t row_stride = (size_t) h_v * S;
#pragma unroll
    for (int r = 0; r < RPG; ++r) s[r] = base[r * row_stride];
    for (int t = 0; t < n; ++t) {
        const float* ht = hbuf + (size_t) t * C;
        /*
        DPCT1118: SYCL group functions and algorithms must be encountered in
        converged control flow. You may need to adjust the code.
        */
        /*
        DPCT1065: Consider replacing sycl::nd_item::barrier() with
        sycl::nd_item::barrier(sycl::access::fence_space::local_space) for
        better performance if there is no access to global memory.
        */
        if (t > 0) item_ct1.barrier(sycl::access::fence_space::local_space); // retire the previous token before reusing local memory
        if (tid < S) { sk[tid] = ht[qk + qh * S + tid]; if (t >= t_out_begin) sq[tid] = ht[qh * S + tid]; }
        /*
        DPCT1118: SYCL group functions and algorithms must be encountered in
        converged control flow. You may need to adjust the code.
        */
        /*
        DPCT1065: Consider replacing sycl::nd_item::barrier() with
        sycl::nd_item::barrier(sycl::access::fence_space::local_space) for
        better performance if there is no access to global memory.
        */
        item_ct1.barrier(sycl::access::fence_space::local_space);
        const float g = sycl::native::exp(gate[(size_t)t * h_v + head]);
        float kv = 0.0f;
#pragma unroll
        for (int r = 0; r < RPG; ++r)
            kv = sycl::fma(s[r], sk[rg * RPG + r], kv);
        red[rg][col] = kv;
        /*
        DPCT1118: SYCL group functions and algorithms must be encountered in
        converged control flow. You may need to adjust the code.
        */
        /*
        DPCT1065: Consider replacing sycl::nd_item::barrier() with
        sycl::nd_item::barrier(sycl::access::fence_space::local_space) for
        better performance if there is no access to global memory.
        */
        item_ct1.barrier(sycl::access::fence_space::local_space);
        const float kv_col = red[0][col] + red[1][col] + red[2][col] + red[3][col];
        const float delta = (ht[2 * qk + head * S + col] - g * kv_col) * beta[(size_t) t * h_v + head];
        if (t < t_out_begin) {
            // ponytail: replay needs only the ordered state updates.
#pragma unroll
            for (int r = 0; r < RPG; ++r)
                s[r] = sycl::fma((float)g, s[r], sk[rg * RPG + r] * delta);
            continue;
        }
        float o = 0.0f;
#pragma unroll
        for (int r = 0; r < RPG; ++r) {
            s[r] = sycl::fma((float)g, s[r], sk[rg * RPG + r] * delta);
            o = sycl::fma(s[r], sq[rg * RPG + r], o);
        }
        /*
        DPCT1118: SYCL group functions and algorithms must be encountered in
        converged control flow. You may need to adjust the code.
        */
        /*
        DPCT1065: Consider replacing sycl::nd_item::barrier() with
        sycl::nd_item::barrier(sycl::access::fence_space::local_space) for
        better performance if there is no access to global memory.
        */
        item_ct1.barrier(sycl::access::fence_space::local_space);
        red[rg][col] = o;
        /*
        DPCT1118: SYCL group functions and algorithms must be encountered in
        converged control flow. You may need to adjust the code.
        */
        /*
        DPCT1065: Consider replacing sycl::nd_item::barrier() with
        sycl::nd_item::barrier(sycl::access::fence_space::local_space) for
        better performance if there is no access to global memory.
        */
        item_ct1.barrier(sycl::access::fence_space::local_space);
        float oc = 0.0f, sq_part = 0.0f;
        if (rg == 0) {
            oc = (red[0][col] + red[1][col] + red[2][col] + red[3][col]) *
                 sycl::rsqrt((float)S);
            sq_part = oc * oc;
        }
        /*
DPCT1108: '__shfl_xor_sync' was migrated with the experimental feature
masked sub_group function which may not be supported by all compilers or
runtimes. You may need to adjust the code.
*/
        if constexpr (SG == 16) {
            // Preserve the SIMD32 XOR tree across two SIMD16 subgroups, with fewer live GRFs per subgroup.
            item_ct1.barrier(sycl::access::fence_space::local_space);
            red[rg][col] = sq_part;
            item_ct1.barrier(sycl::access::fence_space::local_space);
            sq_part += red[rg][col ^ 16];
        } else {
            sq_part += dpct::experimental::permute_sub_group_by_xor(
                0xffffffffu, item_ct1.get_sub_group(), sq_part, 16);
        }
#pragma unroll
        for (int o2 = 8; o2 > 0; o2 >>= 1) sq_part +=
            dpct::experimental::permute_sub_group_by_xor(
                0xffffffffu, sycl::ext::oneapi::this_work_item::get_sub_group(),
                sq_part, o2);
        if ((tid & 31) == 0) wsum[tid >> 5] = sq_part;
        /*
        DPCT1118: SYCL group functions and algorithms must be encountered in
        converged control flow. You may need to adjust the code.
        */
        /*
        DPCT1065: Consider replacing sycl::nd_item::barrier() with
        sycl::nd_item::barrier(sycl::access::fence_space::local_space) for
        better performance if there is no access to global memory.
        */
        item_ct1.barrier(sycl::access::fence_space::local_space);
        float yv = 0.0f;
        if (rg == 0) {
            const float ss = wsum[0] + wsum[1] + wsum[2] + wsum[3];
            const float scale = sycl::rsqrt(ss / (float)S + eps);
            const float zz = z[(size_t) t * value_dim + head * S + col];
            yv = oc * scale * gamma[col] * (1.0f / (1.0f + sycl::native::exp(-zz)));
            y[(size_t)t * value_dim + head * S + col] = yv;
        }
        if constexpr (Q) {
            float amax = sycl::fabs(yv), sum = yv;
            if constexpr (SG == 16) {
                if (rg == 0) {
                    red[0][col] = amax;
                    red[1][col] = sum;
                }
                item_ct1.barrier(sycl::access::fence_space::local_space);
                if (rg == 0) {
                    amax = sycl::fmax(amax, red[0][col ^ 16]);
                    sum += red[1][col ^ 16];
                }
            } else if (rg == 0) {
                amax = sycl::fmax(amax, sycl::permute_group_by_xor(item_ct1.get_sub_group(), amax, 16));
                sum += sycl::permute_group_by_xor(item_ct1.get_sub_group(), sum, 16);
            }
            if (rg == 0) {
#pragma unroll
                for (int offset = 8; offset > 0; offset >>= 1) {
                    amax = sycl::fmax(amax, sycl::permute_group_by_xor(item_ct1.get_sub_group(), amax, offset));
                    sum += sycl::permute_group_by_xor(item_ct1.get_sub_group(), sum, offset);
                }
                const size_t idx = (size_t) (t - t_out_begin) * value_dim + head * S + col;
                const float d = q8_1_finite(amax / 127.0f);
                xq[idx / 32].qs[idx % 32] = q8_1_quant(yv, d, amax);
                if ((idx & 31) == 0) xq[idx / 32].ds = q8_1_ds(d, sum);
            }
        }
    }
    if (n_keep != nullptr && n > 0) {
#pragma unroll
        for (int r = 0; r < RPG; ++r) base[r * row_stride] = s[r];
    }
}

template <int SG, bool Q>
__dpct_inline__ void gdn_sync_4(
    float *__restrict__ state, const float *__restrict__ hbuf, int C,
    const float *__restrict__ gate, const float *__restrict__ beta,
    const float *__restrict__ z, const float *__restrict__ gamma, float eps,
    float *__restrict__ y, int h_k, int h_v, int T,
    const int32_t *__restrict__ n_keep, int t_out_begin,
    GdnQ81 *__restrict__ xq) {
    auto item_ct1 = sycl::ext::oneapi::this_work_item::get_nd_item<3>();
auto &sk = *sycl::ext::oneapi::group_local_memory_for_overwrite<float[S]>(
    sycl::ext::oneapi::this_work_item::get_work_group<3>());
    auto &sq = *sycl::ext::oneapi::group_local_memory_for_overwrite<float[S]>(
        sycl::ext::oneapi::this_work_item::get_work_group<3>());
    auto &red =
        *sycl::ext::oneapi::group_local_memory_for_overwrite<float[RG][S]>(
            sycl::ext::oneapi::this_work_item::get_work_group<3>());
    auto &kv_red = *sycl::ext::oneapi::group_local_memory_for_overwrite<float[RG][S]>(
        sycl::ext::oneapi::this_work_item::get_work_group<3>());
    auto &wsum = *sycl::ext::oneapi::group_local_memory_for_overwrite<
        float[S * RG / 32]>(
        sycl::ext::oneapi::this_work_item::get_work_group<3>());
    const int head = item_ct1.get_group(2);
    const int col = item_ct1.get_local_id(2);
    const int rg = item_ct1.get_local_id(1);
    const int tid = rg * S + col;
    const int qh = head % h_k;
    const int qk = S * h_k;             // q at [0, qk), k at [qk, 2qk), v at [2qk, ...)
    const int value_dim = S * h_v;
    const int n = n_keep ? *n_keep : T;
    float s[RPG];
    float* base = state + ((size_t) (rg * RPG) * h_v + head) * S + col;
    const size_t row_stride = (size_t) h_v * S;
#pragma unroll
    for (int r = 0; r < RPG; ++r) s[r] = base[r * row_stride];
    for (int t = 0; t < n; ++t) {
        const float* ht = hbuf + (size_t) t * C;
        /*
        DPCT1118: SYCL group functions and algorithms must be encountered in
        converged control flow. You may need to adjust the code.
        */
        /*
        DPCT1065: Consider replacing sycl::nd_item::barrier() with
        sycl::nd_item::barrier(sycl::access::fence_space::local_space) for
        better performance if there is no access to global memory.
        */
        if (t > 0) item_ct1.barrier(sycl::access::fence_space::local_space); // retire the previous token before reusing local memory
        if (tid < S) { sk[tid] = ht[qk + qh * S + tid]; if (t >= t_out_begin) sq[tid] = ht[qh * S + tid]; }
        /*
        DPCT1118: SYCL group functions and algorithms must be encountered in
        converged control flow. You may need to adjust the code.
        */
        /*
        DPCT1065: Consider replacing sycl::nd_item::barrier() with
        sycl::nd_item::barrier(sycl::access::fence_space::local_space) for
        better performance if there is no access to global memory.
        */
        item_ct1.barrier(sycl::access::fence_space::local_space);
        const float g = sycl::native::exp(gate[(size_t)t * h_v + head]);
        float kv = 0.0f;
#pragma unroll
        for (int r = 0; r < RPG; ++r)
            kv = sycl::fma(s[r], sk[rg * RPG + r], kv);
        kv_red[rg][col] = kv;
        /*
        DPCT1118: SYCL group functions and algorithms must be encountered in
        converged control flow. You may need to adjust the code.
        */
        /*
        DPCT1065: Consider replacing sycl::nd_item::barrier() with
        sycl::nd_item::barrier(sycl::access::fence_space::local_space) for
        better performance if there is no access to global memory.
        */
        item_ct1.barrier(sycl::access::fence_space::local_space);
        const float kv_col = kv_red[0][col] + kv_red[1][col] + kv_red[2][col] + kv_red[3][col];
        const float delta = (ht[2 * qk + head * S + col] - g * kv_col) * beta[(size_t) t * h_v + head];
        if (t < t_out_begin) {
            // ponytail: replay needs only the ordered state updates.
#pragma unroll
            for (int r = 0; r < RPG; ++r)
                s[r] = sycl::fma((float)g, s[r], sk[rg * RPG + r] * delta);
            continue;
        }
        float o = 0.0f;
#pragma unroll
        for (int r = 0; r < RPG; ++r) {
            s[r] = sycl::fma((float)g, s[r], sk[rg * RPG + r] * delta);
            o = sycl::fma(s[r], sq[rg * RPG + r], o);
        }
        /*
        DPCT1118: SYCL group functions and algorithms must be encountered in
        converged control flow. You may need to adjust the code.
        */
        /*
        DPCT1065: Consider replacing sycl::nd_item::barrier() with
        sycl::nd_item::barrier(sycl::access::fence_space::local_space) for
        better performance if there is no access to global memory.
        */
        // Separate reductions keep output writes from racing state-dot readers.
        red[rg][col] = o;
        /*
        DPCT1118: SYCL group functions and algorithms must be encountered in
        converged control flow. You may need to adjust the code.
        */
        /*
        DPCT1065: Consider replacing sycl::nd_item::barrier() with
        sycl::nd_item::barrier(sycl::access::fence_space::local_space) for
        better performance if there is no access to global memory.
        */
        item_ct1.barrier(sycl::access::fence_space::local_space);
        float oc = 0.0f, sq_part = 0.0f;
        if (rg == 0) {
            oc = (red[0][col] + red[1][col] + red[2][col] + red[3][col]) *
                 sycl::rsqrt((float)S);
            sq_part = oc * oc;
        }
        /*
DPCT1108: '__shfl_xor_sync' was migrated with the experimental feature
masked sub_group function which may not be supported by all compilers or
runtimes. You may need to adjust the code.
*/
        if constexpr (SG == 16) {
            // Preserve the SIMD32 XOR tree across two SIMD16 subgroups, with fewer live GRFs per subgroup.
            item_ct1.barrier(sycl::access::fence_space::local_space);
            red[rg][col] = sq_part;
            item_ct1.barrier(sycl::access::fence_space::local_space);
            sq_part += red[rg][col ^ 16];
        } else {
            sq_part += dpct::experimental::permute_sub_group_by_xor(
                0xffffffffu, item_ct1.get_sub_group(), sq_part, 16);
        }
#pragma unroll
        for (int o2 = 8; o2 > 0; o2 >>= 1) sq_part +=
            dpct::experimental::permute_sub_group_by_xor(
                0xffffffffu, sycl::ext::oneapi::this_work_item::get_sub_group(),
                sq_part, o2);
        if ((tid & 31) == 0) wsum[tid >> 5] = sq_part;
        /*
        DPCT1118: SYCL group functions and algorithms must be encountered in
        converged control flow. You may need to adjust the code.
        */
        /*
        DPCT1065: Consider replacing sycl::nd_item::barrier() with
        sycl::nd_item::barrier(sycl::access::fence_space::local_space) for
        better performance if there is no access to global memory.
        */
        item_ct1.barrier(sycl::access::fence_space::local_space);
        float yv = 0.0f;
        if (rg == 0) {
            const float ss = wsum[0] + wsum[1] + wsum[2] + wsum[3];
            const float scale = sycl::rsqrt(ss / (float)S + eps);
            const float zz = z[(size_t) t * value_dim + head * S + col];
            yv = oc * scale * gamma[col] * (1.0f / (1.0f + sycl::native::exp(-zz)));
            y[(size_t)t * value_dim + head * S + col] = yv;
        }
        if constexpr (Q) {
            float amax = sycl::fabs(yv), sum = yv;
            if constexpr (SG == 16) {
                if (rg == 0) {
                    red[0][col] = amax;
                    red[1][col] = sum;
                }
                item_ct1.barrier(sycl::access::fence_space::local_space);
                if (rg == 0) {
                    amax = sycl::fmax(amax, red[0][col ^ 16]);
                    sum += red[1][col ^ 16];
                }
            } else if (rg == 0) {
                amax = sycl::fmax(amax, sycl::permute_group_by_xor(item_ct1.get_sub_group(), amax, 16));
                sum += sycl::permute_group_by_xor(item_ct1.get_sub_group(), sum, 16);
            }
            if (rg == 0) {
#pragma unroll
                for (int offset = 8; offset > 0; offset >>= 1) {
                    amax = sycl::fmax(amax, sycl::permute_group_by_xor(item_ct1.get_sub_group(), amax, offset));
                    sum += sycl::permute_group_by_xor(item_ct1.get_sub_group(), sum, offset);
                }
                const size_t idx = (size_t) (t - t_out_begin) * value_dim + head * S + col;
                const float d = q8_1_finite(amax / 127.0f);
                xq[idx / 32].qs[idx % 32] = q8_1_quant(yv, d, amax);
                if ((idx & 31) == 0) xq[idx / 32].ds = q8_1_ds(d, sum);
            }
        }
    }
    if (n_keep != nullptr && n > 0) {
#pragma unroll
        for (int r = 0; r < RPG; ++r) base[r * row_stride] = s[r];
    }
}

} // namespace
void gdn_sync_variant(int mode, float* state, const float* h, int C, const float* gate,
    const float* beta, const float* z, const float* gamma, float eps, float* y,
    int hk, int hv, int T, const int32_t* keep, void* stream, int begin, void* xq) {
    auto launch = [&]<int M, int SG, bool Q> {
        strata::q_of(stream)->parallel_for<
            dpct_kernel_name<class gdn_sync_experiment, dpct_kernel_scalar<M>, dpct_kernel_scalar<SG>, dpct_kernel_scalar<Q>>>(
            sycl::nd_range<3>(sycl::range(1,1,(unsigned)hv)*sycl::range(1,RG,S),sycl::range(1,RG,S)),
            sycl::ext::oneapi::experimental::properties{sycl::ext::oneapi::experimental::use_root_sync},
            [=](sycl::nd_item<3>) [[sycl::reqd_sub_group_size(SG)]] {
                if constexpr(M==0) gdn_step_norm_multi_kernel<SG,Q>(state,h,C,gate,beta,z,gamma,eps,y,hk,hv,T,keep,begin,static_cast<GdnQ81*>(xq));
                if constexpr(M==1) gdn_sync_1<SG,Q>(state,h,C,gate,beta,z,gamma,eps,y,hk,hv,T,keep,begin,static_cast<GdnQ81*>(xq));
                if constexpr(M==2) gdn_sync_2<SG,Q>(state,h,C,gate,beta,z,gamma,eps,y,hk,hv,T,keep,begin,static_cast<GdnQ81*>(xq));
                if constexpr(M==3) gdn_sync_3<SG,Q>(state,h,C,gate,beta,z,gamma,eps,y,hk,hv,T,keep,begin,static_cast<GdnQ81*>(xq));
                if constexpr(M==4) gdn_sync_4<SG,Q>(state,h,C,gate,beta,z,gamma,eps,y,hk,hv,T,keep,begin,static_cast<GdnQ81*>(xq));
            });
    };
    auto select = [&]<int M> {
        const char* sg = std::getenv("STRATA_GDN_SG16");
        if (sg && sg[0]=='1') {
            if(xq) launch.template operator()<M,16,true>(); else launch.template operator()<M,16,false>();
        } else {
            if(xq) launch.template operator()<M,32,true>(); else launch.template operator()<M,32,false>();
        }
    };
    switch(mode) {
        case 0: select.template operator()<0>(); break;
        case 1: select.template operator()<1>(); break;
        case 2: select.template operator()<2>(); break;
        case 3: select.template operator()<3>(); break;
        case 4: select.template operator()<4>(); break;
        default: throw std::runtime_error("invalid experiment mode");
    }
}
}
