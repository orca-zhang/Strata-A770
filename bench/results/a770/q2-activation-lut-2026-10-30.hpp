// Isolated experiment, included inside the frozen IQ helper namespace.
// Q2_0 bytes/scales and Q8_1 activations are unchanged. Requires K=640, N=2560.
// ponytail: fixed shapes and the existing down4 reduction; generalize only after a speed win.
#ifndef STRATA_Q2_LUT_PAIR
#define STRATA_Q2_LUT_PAIR 0
#endif
constexpr int Q2LutStride = STRATA_Q2_LUT_PAIR ? 256 : 128;

__dpct_inline__ unsigned q2_lut_nibble(unsigned v) {
    return (v & 1) | ((v >> 1) & 2) | ((v >> 2) & 4) | ((v >> 3) & 8);
}

void q2_lut_build(const block_q8_1* hq, const int32_t* starts, const int32_t* ng,
                  int cap, int16_t* table, sycl::queue* q) {
    // Each four-activation table holds 16 exact integer subset sums, including zero.
    const size_t count = size_t(cap) * 20 * Q2LutStride;
    q->parallel_for<dpct_kernel_name<class q2_activation_table>>(
        sycl::nd_range<1>((count + 255) / 256 * 256, 256), [=](sycl::nd_item<1> it) {
            const size_t i = it.get_global_id(0);
            const int e = int(i / (20 * Q2LutStride));
            const int groups = *ng;
            if (i >= count || !groups || e < starts[0] || e >= starts[groups]) return;
            const int k = int(i / Q2LutStride) % 20, j = int(i / 16) % (Q2LutStride / 16), mask = int(i % 16);
            constexpr int group = STRATA_Q2_LUT_PAIR ? 2 : 4;
            const auto* a = hq[size_t(e) * 20 + k].qs + j * group;
            int sum = 0;
#pragma unroll
            for (int b = 0; b < group; ++b) {
                if constexpr (STRATA_Q2_LUT_PAIR) sum += (((mask >> (2 * b)) & 3) - 1) * int(a[b]);
                else if (mask & (1 << b)) sum += int(a[b]);
            }
            table[i] = int16_t(sum);
        });
}

template <int E>
__dpct_inline__ void q2_lut_chunk(const uint8_t* row, const block_q8_1* hq,
                                 const int16_t* table, int e, int lane, float* result) {
#pragma clang fp contract(off)
    float sum[E] = {};
    for (int k = lane; k < 20; k += 4) {
        const auto* w = reinterpret_cast<const block_q2_0*>(row) + k / 2;
        const sycl::int2 packed = load8_a2(w->qs + (k % 2) * 8);
        int dots[E] = {};
#pragma unroll
        for (int j = 0; j < 8; ++j) {
            const unsigned word = j < 4 ? unsigned(packed.x()) : unsigned(packed.y());
            const unsigned code = (word >> (8 * (j % 4))) & 255;
            const unsigned lo = q2_lut_nibble(code), hi = q2_lut_nibble(code >> 1);
#pragma unroll
            for (int c = 0; c < E; ++c) {
                const auto* t = table + (size_t(e + c) * 20 + k) * Q2LutStride;
                // c-1 = bit0 + 2*bit1 - 1; all arithmetic here is exact integer.
                if constexpr (STRATA_Q2_LUT_PAIR) dots[c] += int(t[j * 32 + (code & 15)]) + int(t[j * 32 + 16 + (code >> 4)]);
                else dots[c] += int(t[j * 16 + lo]) + 2 * int(t[j * 16 + hi]) - int(t[j * 16 + 15]);
            }
        }
#pragma unroll
        for (int c = 0; c < E; ++c)
            sum[c] += float(w->d) * float(hq[size_t(e + c) * 20 + k].ds[0]) * float(dots[c]);
    }
#pragma unroll
    for (int c = 0; c < E; ++c) result[c] = lanes_sum<4>(sum[c]);
}

void q2_lut_down(const unsigned long long* ptr, const int32_t* starts, const int32_t* ng,
                 const int32_t* dst, const block_q8_1* hq, const int16_t* table,
                 NativeExpertLayout L, float* output, int gy, sycl::queue* q) {
    q->parallel_for<dpct_kernel_name<class q2_activation_down>>(
        sycl::nd_range<3>({1, size_t(gy), size_t(40 * 256)}, {1, 1, 256}),
        [=](sycl::nd_item<3> it) [[sycl::reqd_sub_group_size(16)]] {
            const int row = int(it.get_group(2)) * 64 + int(it.get_local_id(2)) / 4;
            const int lane = int(it.get_local_id(2)) % 4, groups = *ng;
            for (int g = int(it.get_group(1)); g < groups; g += int(it.get_group_range(1))) {
                const auto* w = reinterpret_cast<const uint8_t*>(ptr[g]) + L.down_off + size_t(row) * 180;
                int e = starts[g];
                const int end = starts[g + 1];
                for (; e + 4 <= end; e += 4) {
                    float v[4]; q2_lut_chunk<4>(w, hq, table, e, lane, v);
                    if (!lane) for (int c = 0; c < 4; ++c) output[size_t(dst[e + c]) * 2560 + row] = v[c];
                }
                if (e + 2 <= end) {
                    float v[2]; q2_lut_chunk<2>(w, hq, table, e, lane, v);
                    if (!lane) for (int c = 0; c < 2; ++c) output[size_t(dst[e + c]) * 2560 + row] = v[c];
                    e += 2;
                }
                if (e < end) {
                    float v[1]; q2_lut_chunk<1>(w, hq, table, e, lane, v);
                    if (!lane) output[size_t(dst[e]) * 2560 + row] = v[0];
                }
            }
        });
}
