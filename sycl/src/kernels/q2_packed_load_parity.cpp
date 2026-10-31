// Standalone GPU check: compile with the usual SYCL includes and oneMKL link flags.
#include "cuda/iq_kernels.dp.cpp"
#include <stdexcept>
#include <vector>

int main() try {
    constexpr int N = 4096;
    auto& q = dpct::get_in_order_queue();
    std::vector<block_q2_0> blocks(N);
    for (int b = 0; b < N; ++b) {
        blocks[b].d = sycl::half((b % 31) / 32.f);
        for (int j = 0; j < 16; ++j) blocks[b].qs[j] = uint8_t(b + 17 * j);
    }
    auto* weights = sycl::malloc_device<block_q2_0>(N, q);
    auto* errors = sycl::malloc_device<int>(2 * N, q);
    if (!weights || !errors) throw std::bad_alloc();
    q.memcpy(weights, blocks.data(), blocks.size() * sizeof(block_q2_0));
    q.parallel_for(sycl::range<1>(2 * N), [=](sycl::id<1> id) {
        const int b = id[0] / 2, half = id[0] % 2;
        strata::kernels::MultiW decoded;
        strata::kernels::Multi<42>::prep(weights, b, half, decoded);
        int bad = decoded.d != float(weights[b].d);
        for (int j = 0; j < 8; ++j) {
            const uint8_t code = weights[b].qs[half * 8 + j];
            uint32_t expected = 0;
            for (int k = 0; k < 4; ++k)
                expected |= uint32_t(uint8_t(int((code >> (2 * k)) & 3) - 1)) << (8 * k);
            bad += uint32_t(decoded.w[j]) != expected;
        }
        errors[id[0]] = bad;
    });
    std::vector<int> result(2 * N);
    q.memcpy(result.data(), errors, result.size() * sizeof(int)).wait_and_throw();
    for (int bad : result) if (bad) throw std::runtime_error("Q2 packed load differs from byte reference");
    sycl::free(weights, q); sycl::free(errors, q);
    std::printf("PASS Q2 packed loads: %d halves, all 256 codes, both 2-byte alignments\n", 2 * N);
    return 0;
} catch (const std::exception& e) {
    std::fprintf(stderr, "FAIL %s\n", e.what());
    return 1;
}
