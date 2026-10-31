#include <dpct/dpct.hpp>
#include <strata/kernels/elementwise.hpp>
#include "runtime_dependencies.hpp"
#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>

namespace strata::prefill {
void orca_xmx_gemm(const uint16_t*, const uint16_t*, float*, int64_t, int64_t, int64_t,
                   int64_t, const uint16_t*, bool, void*);
}

template<class T> std::vector<T> load(const std::filesystem::path& path, size_t count) {
    std::vector<T> values(count);
    std::ifstream input(path, std::ios::binary);
    input.read(reinterpret_cast<char*>(values.data()), count * sizeof(T));
    if (!input || std::filesystem::file_size(path) != count * sizeof(T))
        throw std::runtime_error("missing or wrong-sized frozen GEMM fixture: " + path.string());
    return values;
}

template<class T> void save(const std::filesystem::path& path, const std::vector<T>& values) {
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(values.data()), values.size() * sizeof(T));
    if (!output) throw std::runtime_error("cannot save GEMM capture: " + path.string());
}

int main(int argc, char** argv) try {
    if (argc < 4 || argc > 5 || (argc == 5 && std::string(argv[4]) != "--consumer"))
        throw std::invalid_argument(
            "usage: strata-orca-gemm-replay-check images frozen-case0 new-capture-directory [--consumer]");
    const bool consumer = argc == 5;
    check_runtime_dependencies();
    constexpr int slots = 8, calls = 2048, guard = 128;
    const std::filesystem::path fixture(argv[2]), captures(argv[3]);
    int t = 703, n = 1280, k = 2560;
    if (std::filesystem::exists(fixture / "shape.txt")) {
        std::ifstream shape(fixture / "shape.txt");
        if (!(shape >> t >> n >> k) || t < 1 || t > 8192 || n < 8 || n > 32768 || n % 8 ||
            k < 16 || k > 32768 || k % 16 || size_t(t) * n > (16u << 20) ||
            size_t(n) * k > (64u << 20))
            throw std::invalid_argument("invalid frozen GEMM shape");
        shape >> std::ws;
        if (!shape.eof()) throw std::invalid_argument("trailing frozen GEMM shape data");
    }
    const size_t cells = size_t(t) * n, stride = cells + 2 * guard;
    const auto x = load<uint16_t>(fixture / "x.f16", size_t((t + 7) / 8 * 8) * k);
    const auto w = load<uint16_t>(fixture / "weights.f16", size_t(n) * k);
    const auto expected = load<float>(fixture / "expected.f32", cells);
    const auto bound = load<float>(fixture / "bound.f32", cells);
    for (size_t i = 0; i < cells; ++i)
        if (!std::isfinite(expected[i]) || !std::isfinite(bound[i]) || bound[i] < 0)
            throw std::runtime_error("invalid CPU64 reference/bound");
    if (!std::filesystem::create_directory(captures))
        throw std::runtime_error("capture directory must be new");

    // Keep host transfer buffers alive until the queue/runtime drain on exceptions.
    std::vector<float> actual(slots * stride, std::numeric_limits<float>::quiet_NaN());
    std::vector<uint16_t> read_x(x.size()), read_w(w.size());
    orca::l0::Runtime runtime;
    orca::l0::compiler::Program program(runtime, std::filesystem::path(argv[1]) / "xmx");
    if (consumer) program.add(std::filesystem::path(argv[1]) / "elementwise");
    strata::l0::Queue queue(runtime, program);
    auto* dx = static_cast<uint16_t*>(runtime.allocate(x.size() * 2));
    auto* dw = static_cast<uint16_t*>(runtime.allocate(w.size() * 2));
    auto* dy = static_cast<float*>(runtime.allocate(slots * stride * sizeof(float)));
    for (int slot = 0; slot < slots; ++slot) for (int j = 0; j < guard; ++j) {
        actual[slot * stride + j] = -1234.f;
        actual[slot * stride + guard + cells + j] = -1234.f;
    }
    queue.memcpy(dx, x.data(), x.size() * 2);
    queue.memcpy(dw, w.data(), w.size() * 2);
    queue.memcpy(dy, actual.data(), actual.size() * sizeof(float));
    size_t nonfinite = 0, outside = 0, guards = 0;
    int submitted = 0, first_bad_slot = -1;
    // ponytail: eight private outputs keep transient failures from being overwritten;
    // one readback per eight calls changes scheduling, so this is not a model/performance test.
    for (; submitted < calls && first_bad_slot < 0;) {
        for (int slot = 0; slot < slots; ++slot) {
            strata::prefill::orca_xmx_gemm(dx, dw, dy + slot * stride + guard,
                t, n, k, k, dx + size_t(t / 8 * 8) * k, false, &queue);
            if (consumer) strata::kernels::scale_inplace(dy + slot * stride + guard, cells, 2.f, &queue);
        }
        queue.memcpy(actual.data(), dy, actual.size() * sizeof(float)).wait_and_throw();
        submitted += slots;
        for (int slot = 0; slot < slots; ++slot) {
            const auto* row = actual.data() + slot * stride;
            const auto before = nonfinite + outside + guards;
            for (int j = 0; j < guard; ++j)
                guards += row[j] != -1234.f || row[guard + cells + j] != -1234.f;
            for (size_t i = 0; i < cells; ++i) {
                const float value = row[guard + i];
                if (!std::isfinite(value)) ++nonfinite;
                else if (std::abs(double(value) - expected[i] * (consumer ? 2. : 1.)) >
                         bound[i] * (consumer ? 2. : 1.)) ++outside;
            }
            if (first_bad_slot < 0 && nonfinite + outside + guards != before) first_bad_slot = slot;
        }
        if (submitted % 256 == 0 || first_bad_slot >= 0) {
            std::printf("CHECK calls=%d nonfinite=%zu outside_bound=%zu guards=%zu first_bad_slot=%d\n",
                submitted, nonfinite, outside, guards, first_bad_slot);
            std::fflush(stdout);
        }
    }
    queue.memcpy(read_x.data(), dx, read_x.size() * 2);
    queue.memcpy(read_w.data(), dw, read_w.size() * 2).wait_and_throw();
    const bool operands_exact = x == read_x && w == read_w;
    if (first_bad_slot >= 0 || !operands_exact) {
        save(captures / "outputs.f32", actual);
        save(captures / "x.f16", read_x);
        save(captures / "weights.f16", read_w);
    }
    check_runtime_dependencies();
    runtime.deallocate(dy); runtime.deallocate(dw); runtime.deallocate(dx);
    std::printf("RESULT calls=%d nonfinite=%zu outside_bound=%zu guards=%zu operands_exact=%d first_bad_slot=%d\n",
        submitted, nonfinite, outside, guards, int(operands_exact), first_bad_slot);
    return first_bad_slot >= 0 || !operands_exact ? 2 : 0;
} catch (const std::exception& e) {
    std::fprintf(stderr, "FAIL %s\n", e.what());
    return 1;
}
