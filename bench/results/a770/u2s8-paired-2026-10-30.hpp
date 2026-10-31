// Isolated Q2_0 Down: load a whole group64, reuse its scale for both Q8 blocks.
// ponytail: fixed K640/N2560 and M1/2/4/8; expand only after full-chain gains.
template<int M>
void paired_down_tile(const uint8_t* w, const block_q8_1* x, float* y,
                      const int32_t* dst, int e, int row, NativeExpertLayout L) SYCL_ESIMD_FUNCTION {
#pragma clang fp contract(off)
    es::simd<float, M * 8> acc[4];
    for (int i = 0; i < 4; ++i) acc[i] = 0.f;
    es::simd<uint32_t, 64> offsets(0, 1);
    offsets = ((offsets % 16) / 2) * uint32_t(L.d_row) +
              ((offsets % 32) / 16) * 4 + (offsets % 2) * 2 + (offsets / 32) * 8;
    es::simd<uint32_t, 8> scale_offsets(0, uint32_t(L.d_row));
    for (int b = 0; b < 10; ++b) {
        const auto* base = w + L.down_off + size_t(row) * L.d_row + b * 18;
        // Q2 guarantees halfword alignment, including every 18-byte block.
        auto halves = es::gather<uint16_t, 64>(
            reinterpret_cast<const uint16_t*>(base + 2), offsets);
        es::simd<uint8_t, 128> packed = halves.template bit_cast_view<uint8_t>();
        es::simd<float, 8> wd = es::gather<sycl::half, 8>(
            reinterpret_cast<const sycl::half*>(base), scale_offsets);
#pragma unroll
        for (int half = 0; half < 2; ++half) {
            es::simd<int8_t, M * 32> a;
            es::simd<float, M * 8> scale;
#pragma unroll
            for (int t = 0; t < M; ++t) {
                const auto* q = x + size_t(e + t) * 20 + b * 2 + half;
                a.template select<32, 1>(t * 32) = es::block_load<int8_t, 32>(q->qs);
                es::simd<float, 2> ds = es::block_load<sycl::half, 2>(
                    reinterpret_cast<const sycl::half*>(q));
                scale.template select<8, 1>(t * 8) = wd * float(ds[0]);
            }
            es::simd<uint8_t, 64> codes = packed.template select<64, 1>(half * 64);
            using P = es::xmx::dpas_argument_type;
            es::simd<int, M * 8> product =
                es::xmx::dpas<8, M, int, uint8_t, int8_t, P::u2, P::s8>(codes, a);
#pragma unroll
            for (int t = 0; t < M; ++t) {
                es::simd<int, 32> av = a.template select<32, 1>(t * 32);
                product.template select<8, 1>(t * 8) -= es::reduce<int>(av, std::plus<>{});
            }
            es::simd<float, M * 8> fp = product;
            const int slot = (b * 2 + half) % 4;
            acc[slot] = es::simd<float, M * 8>(
                __builtin_elementwise_fma(scale.data(), fp.data(), acc[slot].data()));
        }
    }
    auto total = (acc[0] + acc[2]) + (acc[1] + acc[3]);
#pragma unroll
    for (int t = 0; t < M; ++t)
        es::block_store<float, 8>(y + size_t(dst[e + t]) * L.n_embd + row,
                                  total.template select<8, 1>(t * 8));
}
