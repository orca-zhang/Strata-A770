#pragma once

namespace strata::kernels {

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

