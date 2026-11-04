#include <orca_l0/compiler/dg2.hpp>
#include <sycl/ext/intel/esimd.hpp>
#include <sycl/ext/intel/experimental/grf_size_properties.hpp>
#include <cstdio>
#include "runtime_dependencies.hpp"

struct Shape {
    std::array<int,2> dimensions;
    const float* bias;
    uint64_t scalar;
};
template<class F> void reject(F&& function) {
    bool caught = false;
    try { function(); } catch (const std::exception&) { caught = true; }
    if (!caught) throw std::runtime_error("DG2 compiler negative check accepted input");
}

int main(int argc, char** argv) try {
    check_runtime_dependencies(true);
    if (argc == 3 && std::string(argv[1]) == "--index-only") {
        orca::l0::dg2::Program program(argv[2]);
        reject([&] { program.kernel_file(nullptr); });
        reject([&] { program.kernel_file("missing-orca-kernel"); });
        std::puts("PASS DG2 native index parsing and missing/null kernel rejection; CPU only");
        return 0;
    }
    if (argc != 3) throw std::invalid_argument("usage: dg2-compiler-check native-pack memory.zebin | --index-only pack");
    namespace dg2 = orca::l0::dg2;
    namespace compiler = orca::l0::compiler;
    dg2::Runtime runtime;
    dg2::Program program(argv[1]);
    runtime.set_memory_module(argv[2]);
    reject([&] { program.add(argv[1]); });
    constexpr size_t count = 768, prefix = 16;
    const auto device = runtime.allocate_device((count + 2 * prefix) * sizeof(float));
    const auto integers = runtime.allocate_device((count + 2 * prefix) * sizeof(uint64_t));
    const auto input = runtime.allocate((count + 2 * prefix) * sizeof(float));
    const auto bias = runtime.allocate(16 * sizeof(float));
    const auto result = runtime.allocate(device.bytes), tags = runtime.allocate(integers.bytes);
    auto* output = reinterpret_cast<float*>(uintptr_t(device.gpu)) + prefix;
    auto* scalar_output = reinterpret_cast<uint64_t*>(uintptr_t(integers.gpu)) + prefix;
    const auto* source = static_cast<float*>(input.data) + prefix;
    const auto* host_bias = static_cast<float*>(bias.data) + 3;
    if (runtime.gpu_address(host_bias) != bias.gpu + 12 || runtime.gpu_address(output) != device.gpu + prefix * 4 || runtime.gpu_address(nullptr))
        throw std::runtime_error("public CPU/GPU interior-address conversion mismatch");
    reject([&] { runtime.gpu_address(reinterpret_cast<void*>(uintptr_t{1})); });
    const float sentinel = -12345.f;
    const uint64_t integer_guard = 0xdeadbeef12345678ull;
    runtime.fill(device.gpu, &sentinel, sizeof(sentinel), device.bytes);
    runtime.fill(integers.gpu, &integer_guard, sizeof(integer_guard), integers.bytes);
    const uint64_t address_bits = uint64_t(reinterpret_cast<uintptr_t>(host_bias));
    std::vector<dg2::Commands> pipeline;
    for (int part = 0; part < 3; ++part) {
        const auto* slice_input = source + part * 256;
        auto* slice = output + part * 256;
        auto* scalar_slice = scalar_output + part * 256;
        const float scale = 0.25f * (part + 1);
        const Shape shape{{8 + part,32},host_bias,address_bits};
        compiler::parallel_for(program, runtime, sycl::nd_range<1>(256,32),
            sycl::ext::oneapi::experimental::properties{sycl::ext::intel::experimental::grf_size<256>},
            [=](sycl::nd_item<1> item) {
                const auto i = item.get_global_id(0);
                slice[i] = slice_input[i] * scale + shape.dimensions[0] + shape.dimensions[1] + *shape.bias;
                scalar_slice[i] = shape.scalar;
            });
        pipeline.push_back(runtime.finish());
    }
    int handler = 0;
    compiler::local_accessor<float> scratch(sycl::range<1>(32), handler), empty(sycl::range<1>(0), handler);
    compiler::parallel_for<class Dg2Scratch>(program, runtime, sycl::nd_range<3>({1,1,count},{1,1,32}),
        [=](sycl::nd_item<3> item) [[sycl::reqd_sub_group_size(32)]] {
            const auto i = item.get_global_id(2), lane = item.get_local_id(2);
            if (*host_bias < 0.f) empty[0] = 0.f;
            scratch[lane] = output[i]; item.barrier(sycl::access::fence_space::local_space);
            output[i] = scratch[31 - lane] + 0.25f;
        });
    pipeline.push_back(runtime.finish());
    compiler::parallel_for<class Dg2Vector>(program, runtime, sycl::nd_range<3>({2,2,12},{2,2,4}),
        sycl::ext::oneapi::experimental::properties{sycl::ext::intel::experimental::grf_size<256>},
        [=](sycl::nd_item<3> item) SYCL_ESIMD_KERNEL {
            sycl::ext::intel::esimd::simd<float,16> values;
            auto* pointer = output + item.get_global_linear_id() * 16;
            values.copy_from(pointer); values += 1.f; values.copy_to(pointer);
        });
    pipeline.push_back(runtime.finish());
    compiler::parallel_for<class Dg2VectorRange>(program, runtime, sycl::range<1>(48),
        sycl::ext::oneapi::experimental::properties{sycl::ext::intel::experimental::grf_size<256>},
        [=](sycl::id<1> item) SYCL_ESIMD_KERNEL {
            sycl::ext::intel::esimd::simd<float,16> values;
            auto* pointer = output + item[0] * 16;
            values.copy_from(pointer); values += 0.5f; values.copy_to(pointer);
        });
    pipeline.push_back(runtime.finish());
    compiler::parallel_for<class Dg2Finish>(program, runtime, sycl::range<1>(count),
        [=](sycl::id<1> item) { output[item[0]] += 0.25f; });
    pipeline.push_back(runtime.finish());
    runtime.copy(result.gpu, device.gpu, device.bytes);
    runtime.copy(tags.gpu, integers.gpu, integers.bytes);
    pipeline.push_back(runtime.finish());
    for (unsigned replay = 0; replay < 12; ++replay) {
        for (auto a : {input,bias,result,tags}) runtime.cpu_access(a, true);
        auto* data = static_cast<float*>(input.data); auto* b = static_cast<float*>(bias.data);
        for (size_t i = 0; i < input.bytes / 4; ++i) data[i] = float((i + replay * 7) % 113) / 8;
        for (size_t i = 0; i < bias.bytes / 4; ++i) b[i] = float(replay) / 2;
        std::memset(result.data, 0, result.bytes); std::memset(tags.data, 0, tags.bytes);
        for (const auto& list : pipeline) runtime.submit(list);
        runtime.wait(pipeline.back()); runtime.check_scratch_guards();
        for (auto a : {input,bias,result,tags}) runtime.cpu_access(a, false);
        for (size_t i = 0; i < result.bytes / 4; ++i) {
            float expected = sentinel;
            if (i >= prefix && i - prefix < count) {
                const auto j = i - prefix, reversed = j / 32 * 32 + 31 - j % 32, part = j / 256;
                expected = data[prefix + reversed] * (0.25f * float(part + 1)) + float(40 + part) + b[3] + 2.f;
            }
            if (static_cast<float*>(result.data)[i] != expected) throw std::runtime_error("DG2 compiler float output/prefix/tail mismatch");
        }
        for (size_t i = 0; i < tags.bytes / 8; ++i)
            if (static_cast<uint64_t*>(tags.data)[i] != (i >= prefix && i - prefix < count ? address_bits : integer_guard))
                throw std::runtime_error("aggregate scalar was translated or integer guards changed");
        for (size_t i = 0; i < input.bytes / 4; ++i)
            if (data[i] != float((i + replay * 7) % 113) / 8) throw std::runtime_error("compiler pipeline modified input");
        for (size_t i = 0; i < bias.bytes / 4; ++i)
            if (b[i] != float(replay) / 2) throw std::runtime_error("compiler pipeline modified aggregate input");
    }
    check_runtime_dependencies(true);
    std::puts("PASS 12 DG2 compiler-generated pipeline replays across eight frozen lists: CPU/GPU/interior/aggregate pointers, unchanged address-like scalars, dynamic/empty local memory, 3D/SYCL/ESIMD/range kernels, changed inputs and full guards; no L0/Intel UMD/GMM/compiler runtime");
    return 0;
} catch (const std::exception& error) { std::fprintf(stderr,"%s\n",error.what()); return 1; }
