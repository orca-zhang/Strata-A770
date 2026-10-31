#pragma once

#include "strata/kernels/qsa.hpp"
#include "strata/kernels/rope_scaling.hpp"

namespace strata::kernels {

// Batch of distinct device positions; same stores and host mirrors as kv_append_step.
void kv_append_steps(uint16_t* k_pool, uint16_t* v_pool, const int32_t* table, const int32_t* steps,
                     int step_stride, int n_tok, const float* kcur, const float* vcur,
                     const QsaShapes& s, void* stream, const KvHostPools* host);

// HD=256/n_rot=64, unrotated Q8 KV: same FP32 norm/RoPE and stored-scale quantization as the separate calls.
// Positions are [token * position_stride + head]; steps are [token * step_stride]. Distinct output cells required.
void native_qsa_norm_rope_store_q8_steps(const float* kcur, const float* vcur, const float* gamma,
    const int32_t* positions, int position_stride, const int32_t* steps, int step_stride, int n_tok,
    int8_t* k_q, int8_t* v_q, uint16_t* k_scale, uint16_t* v_scale, const int32_t* table,
    const QsaShapes& s, float epsilon, const RopeScaling& scaling, void* stream, const KvHostPools* host);

// Set before session construction/capture; existing CUDA graphs retain their
// selected kernels. Disabled by default. Integration chooses the call sites.
void native_qsa_set_enabled(bool enabled);
bool native_qsa_enabled();

// Contiguous input/output [n_cols,n_rows], gamma[n_cols] broadcast over rows.
// Each row has independent RMS statistics. Exact output==input is supported;
// otherwise all three spans must be disjoint. Epsilon must be finite and >= 0;
// epsilon==0 additionally requires a nonzero sum of squares in every row.
// Pinned 256-thread (width<1024) or 1024-thread XOR reduction, (scale*x)*gamma.
void native_qsa_rms_norm_weighted(const float* input, const float* gamma, float* output,
                                  int n_cols, int n_rows, float epsilon, void* stream);

// The same per row with gamma[n_cols * groups]: row r is normalized on its own and scaled by gamma's slice
// r % groups (the MTP's hidden input, one row per hyper-connection stream: --mtp-hnorm stream).
void native_qsa_rms_norm_grouped(const float* input, const float* gamma, float* output,
                                 int n_cols, int groups, int n_rows, float epsilon, void* stream);

// attn/output [head_dim,n_head]; q_full [2*head_dim,n_head], each row is
// [query channels, gate channels]. Output is attn*sigmoid(second-half gate).
// Exact output==attn is supported; all other spans must be disjoint.
// If xq_out != nullptr, Q8_1 quantization is performed in-kernel directly into xq_out.
// If output == nullptr and xq_out != nullptr, writing FP32 output is skipped.
void native_qsa_gate_apply(const float* attn, const float* q_full, float* output,
                           int n_head, int head_dim, void* stream, void* xq_out);

void native_qsa_gate_apply(const float* attn, const float* q_full, float* output,
                           int n_head, int head_dim, void* stream = nullptr);

} // namespace strata::kernels

