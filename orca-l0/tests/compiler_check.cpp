#include <orca_l0/compiler/stream.hpp>
#include <sycl/ext/intel/esimd.hpp>
#include <sycl/ext/intel/experimental/grf_size_properties.hpp>
#include <array>
#include <cstdio>
#include "runtime_dependencies.hpp"

class Scratch;
class Vector;
struct Shape {
    std::array<int, 2> dimensions;
    const float* bias;
};


int main(int argc, char** argv) try {
    if (argc != 2) throw std::invalid_argument("usage: compiler-check module-directory");
    check_runtime_dependencies();
    orca::l0::Runtime runtime;
    orca::l0::compiler::Program program(runtime, argv[1]);
    orca::l0::compiler::Stream stream(runtime, program);
    stream.begin_recording();
    auto* output = static_cast<float*>(runtime.allocate(768 * sizeof(float)));
    auto* host_bias = static_cast<float*>(runtime.allocate(sizeof(float), orca::l0::Memory::host));
    *host_bias = 0.0f;
    const float bias = 0.25f;
    const int width = 256;
    for (int pass = 0; pass < 3; ++pass) {
        auto* slice = output + pass * width;
        const float scale = bias * (pass + 1);
        const Shape shape{{8 + pass, 32}, host_bias};
        stream.parallel_for(
            sycl::nd_range<1>(256, 32),
            sycl::ext::oneapi::experimental::properties{sycl::ext::intel::experimental::grf_size<256>},
            [=](sycl::nd_item<1> item) {
                const int i = item.get_global_id(0);
                if (i < width) slice[i] = i * scale + shape.dimensions[0] + shape.dimensions[1] + *shape.bias;
            });
    }
    std::array<float, 768> result{};
    int handler = 0;
    orca::l0::compiler::local_accessor<float, 1> scratch(sycl::range<1>(32), handler);
    // Strata captures an empty placeholder on paths that need no scratch.
    orca::l0::compiler::local_accessor<float, 1> empty(sycl::range<1>(0), handler);
    stream.parallel_for<class Scratch>(
        sycl::nd_range<3>({1, 1, 768}, {1, 1, 32}),
        [=](sycl::nd_item<3> item) [[sycl::reqd_sub_group_size(32)]] {
            const auto i = item.get_global_id(2);
            const auto lane = item.get_local_id(2);
            if (*host_bias < 0.0f) empty[0] = 0.0f; // Never accessed by these inputs.
            auto* local = scratch.get_multi_ptr<>().get();
            local[lane] = output[i];
            item.barrier(sycl::access::fence_space::local_space);
            output[i] = local[31 - lane] + bias;
        });
    stream.parallel_for<Vector>(
        sycl::nd_range<1>(48, 1),
        sycl::ext::oneapi::experimental::properties{sycl::ext::intel::experimental::grf_size<256>},
        [=](sycl::nd_item<1> item) SYCL_ESIMD_KERNEL {
            sycl::ext::intel::esimd::simd<float, 16> values;
            auto* pointer = output + item.get_global_id(0) * 16;
            values.copy_from(pointer);
            values += 1.0f;
            values.copy_to(pointer);
        });
    stream.copy(result.data(), output, sizeof(result));
    auto& commands = stream.end_recording();
    for (int replay = 0; replay < 3; ++replay) {
        *host_bias = replay * 0.5f;
        stream.replay(commands);
        if (!stream.wait()) throw std::runtime_error("native compiler replay timeout");
        for (int i = 0; i < 768; ++i) {
            const int pass = i / width;
            const int index = i % width;
            const float expected = (index / 32 * 32 + 31 - index % 32) * bias * (pass + 1) +
                41.25f + pass + replay * 0.5f;
            if (result[i] != expected)
                throw std::runtime_error("native compiler output mismatch");
        }
    }
    runtime.deallocate(output);
    check_runtime_dependencies();
    std::puts("PASS compiler-generated ABI: pointers/scalars/aggregates, local memory, SYCL/ESIMD images and native replay");
    return 0;
} catch (const std::exception& e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
}
