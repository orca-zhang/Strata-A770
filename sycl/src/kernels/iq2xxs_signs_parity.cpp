// Standalone GPU check: compile with the usual SYCL includes and oneMKL link flags.
#include "cuda/iq_kernels.dp.cpp"
#include <cstring>
#include <stdexcept>
#include <vector>

int main() try {
    constexpr int N = 256 * 128;
    auto& q = dpct::get_in_order_queue();
    std::vector<block_iq2_xxs> blocks(N);
    for (int i = 0; i < N; ++i) {
        blocks[i].d = sycl::half(1.f);
        uint32_t codes = 0, aux = 0;
        for (int j = 0; j < 4; ++j) {
            codes |= uint32_t((i / 128 + 67 * j) & 255) << (8 * j);
            aux |= uint32_t((i % 128 + 29 * j) & 127) << (7 * j);
        }
        std::memcpy(blocks[i].qs, &codes, 4);
        std::memcpy(reinterpret_cast<uint8_t*>(blocks[i].qs) + 4, &aux, 4);
    }
    auto* weights = sycl::malloc_device<block_iq2_xxs>(N, q);
    auto* activation = sycl::malloc_device<block_q8_1>(1, q);
    auto* output = sycl::malloc_device<float>(N, q);
    if (!weights || !activation || !output) throw std::bad_alloc();
    q.memcpy(weights, blocks.data(), blocks.size() * sizeof(block_iq2_xxs));
    std::vector<float> result(N);
    for (int pos = 0; pos < 32; ++pos) {
        block_q8_1 x{};
        x.ds[0] = sycl::half(1.f);
        x.qs[pos] = 8;  // Cancels IQ2_XXS's integer /8, exposing one signed grid byte exactly.
        q.memcpy(activation, &x, sizeof(x));
        q.parallel_for(sycl::range<1>(N), [=](sycl::id<1> id) {
            output[id[0]] = strata::kernels::vec_dot_iq2_xxs_q8_1(weights, activation, int(id[0]), 0);
        });
        q.memcpy(result.data(), output, N * sizeof(float)).wait_and_throw();
        for (int i = 0; i < N; ++i) {
            const int index = (i / 128 + 67 * (pos / 8)) & 255;
            const auto* grid = reinterpret_cast<const uint8_t*>(iq2xxs_grid + index);
            const int sign = (ksigns_iq2xs[(i % 128 + 29 * (pos / 8)) & 127] >> (pos % 8)) & 1;
            const float expected = sign ? -int(grid[pos % 8]) : int(grid[pos % 8]);
            if (result[i] != expected) throw std::runtime_error("IQ2_XXS dot differs from byte/table reference");
        }
    }
    sycl::free(weights, q); sycl::free(activation, q); sycl::free(output, q);
    std::printf("PASS IQ2_XXS signs: %d exact dots, all 256 grid codes, 128 signs, 32 positions, both 2-byte alignments\n", 32 * N);
    return 0;
} catch (const std::exception& e) {
    std::fprintf(stderr, "FAIL %s\n", e.what());
    return 1;
}
