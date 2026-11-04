#include "strata/orca_engine.hpp"
#include "strata/core/device.hpp"
#include "strata/core/graph.hpp"
#include "strata/core/pinned.hpp"
#include "strata/prefill/prefill.hpp"
#include "strata/kernels/elementwise.hpp"
#include "runtime_dependencies.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <new>

namespace {
void dependencies() {
#ifdef STRATA_ORCA_DG2
    check_runtime_dependencies(true);
#else
    check_runtime_dependencies();
#endif
}
}

int main(int argc, char** argv) try {
    if (argc != 2) throw std::invalid_argument("usage: strata-orca-engine-check images");
    dependencies();
    strata::l0::install_engine(argv[1]);
    {
        strata::prefill::Prefill unused;
        unused.reset();
        unused.reset();
    }
    const auto info = strata::core::device_info();
    if (info.ordinal != 0 || info.name.empty() || !info.total_bytes || info.free_bytes > info.total_bytes)
        throw std::runtime_error("invalid native engine device information");
    std::printf("native engine: %s, free %.1f / total %.1f GiB\n", info.name.c_str(),
                double(info.free_bytes) / (1ull << 30), double(info.total_bytes) / (1ull << 30));
    auto& queue = dpct::get_in_order_queue();
#ifndef STRATA_ORCA_DG2
    {
        ze_driver_properties_t actual{};
        actual.stype = ZE_STRUCTURE_TYPE_DRIVER_PROPERTIES;
        orca::l0::check(zeDriverGetProperties(queue.runtime().native_driver(), &actual), "check raw driver properties");
        if (actual.driverVersion != queue.runtime().driver_properties().driverVersion)
            throw std::runtime_error("cached driver version mismatch");
        uint32_t count = 0;
        orca::l0::check(zeDriverGetExtensionProperties(queue.runtime().native_driver(), &count, nullptr), "check extension count");
        std::vector<ze_driver_extension_properties_t> actual_extensions(count);
        if (count) orca::l0::check(zeDriverGetExtensionProperties(queue.runtime().native_driver(), &count, actual_extensions.data()), "check extensions");
        const auto& cached = queue.runtime().extensions();
        if (cached.size() != count) throw std::runtime_error("cached extension count mismatch");
        for (const auto& extension : actual_extensions)
            if (std::none_of(cached.begin(), cached.end(), [&](const auto& value) {
                return std::string(value.name) == extension.name && value.version == extension.version;
            })) throw std::runtime_error("cached extension mismatch");
        std::puts("PASS cached native driver properties and extension capabilities match raw queries");
    }
    const bool discrete = !(queue.runtime().properties().flags & ZE_DEVICE_PROPERTY_FLAG_INTEGRATED);
#else
    if (info.arch != "Intel DG2 (Orca native)" || info.driver_version || info.runtime_version)
        throw std::runtime_error("native device information contains invented Level Zero properties");
    const bool discrete = true;
#endif
    if (info.free_bytes > (1ull << 30) && discrete) {
        constexpr size_t probe_bytes = 256u << 20;
        auto* probe = static_cast<uint32_t*>(queue.runtime().allocate(probe_bytes));
        queue.fill(probe, 0x12345678u, probe_bytes / sizeof(uint32_t)).wait_and_throw();
        size_t after = 0, total = 0;
        strata::l0::memory_info(after, total);
        queue.runtime().deallocate(probe);
        std::printf("native memory accounting: before %llu, after touched allocation %llu, allocation %llu bytes\n",
                    (unsigned long long)info.free_bytes, (unsigned long long)after, (unsigned long long)probe_bytes);
        // Allow driver granularity and desktop fluctuations while rejecting a
        // query which ignores this process's device allocations entirely.
        if (total != info.total_bytes || after > info.free_bytes - probe_bytes / 4)
            throw std::runtime_error("native free-memory query did not account for the touched allocation");
    }
#ifndef STRATA_ORCA_DG2
    const auto limit = queue.runtime().properties().maxMemAllocSize;
    if (limit && limit < SIZE_MAX - (512ull << 20) &&
        info.free_bytes > std::max<uint64_t>(limit, 1ull << 32) + (512ull << 20)) {
        const size_t bytes = (size_t(std::max<uint64_t>(limit, 1ull << 32)) + 4099) & ~size_t(3);
        auto* large = static_cast<uint32_t*>(queue.runtime().allocate(bytes));
        auto* tail = large + bytes / sizeof(uint32_t) - 1;
        queue.fill(large, 0x3f800000u, 1);
        queue.fill(tail, 0x3f800000u, 1);
        strata::kernels::scale_inplace(reinterpret_cast<float*>(tail), 1, 2.f, &queue);
        queue.wait_and_throw();
        std::array<uint32_t, 3> edges{};
        auto* copy = strata::l0::create_copy_queue();
        const uint32_t updated = 0x40400000u;
        copy->memcpy(&edges[0], large, sizeof(uint32_t));
        copy->memcpy(&edges[1], tail, sizeof(uint32_t));
        copy->memcpy(tail, &updated, sizeof(updated));
        copy->memcpy(&edges[2], tail, sizeof(uint32_t)).wait_and_throw();
        dpct::get_current_device().destroy_queue(copy);
        queue.runtime().deallocate(large);
        if (edges != std::array<uint32_t, 3>{0x3f800000u, 0x40000000u, updated})
            throw std::runtime_error("native relaxed allocation boundary mismatch");
        std::printf("native relaxed allocation: %llu bytes > device limit %llu; both edges checked\n",
                    (unsigned long long)bytes, (unsigned long long)limit);
    } else std::puts("SKIP relaxed allocation check: insufficient free memory above the device limit");
#endif
    strata::core::DeviceArena arena(4096, 0, true);
    auto* destination = static_cast<float*>(arena.alloc(256));
    std::array<float, 64> output;
    queue.memcpy(output.data(), destination, sizeof(output)).wait_and_throw();
    for (auto value : output) if (!std::isnan(value)) throw std::runtime_error("native arena poison mismatch");
    bool rejected = false;
    try { arena.alloc(UINT64_MAX); } catch (const strata::core::CudaError&) { rejected = true; }
    if (!rejected || arena.used() != 256) throw std::runtime_error("native arena overflow was accepted");

    std::array<float, 32> source;
    for (size_t i = 0; i < source.size(); ++i) source[i] = float(i + 1);
    auto* recording = dpct::get_current_device().create_queue();
    dpct::experimental::begin_recording(recording);
    const bool relaxed_recording = recording->allows_independent();
    recording->fill(destination, -9.f, 64);
    dpct::async_dpct_memcpy(destination, size_t(8 * 4), source.data(), size_t(4 * 4),
                          size_t(3 * 4), size_t(8), dpct::host_to_device, *recording);
    dpct::experimental::command_graph_ptr captured = nullptr;
    dpct::experimental::end_recording(recording, &captured);
    std::unique_ptr<dpct::experimental::Graph> graph(captured);
    auto executable = graph->finalize();
    size_t count = 0;
    dpct::experimental::get_nodes(graph.get(), nullptr, &count);
    namespace e = sycl::ext::oneapi::experimental;
    std::vector<e::node_type> expected_nodes{e::node_type::memset};
    if (relaxed_recording) expected_nodes.push_back(e::node_type::ext_oneapi_barrier);
    expected_nodes.push_back(e::node_type::memcpy);
    if (relaxed_recording) expected_nodes.push_back(e::node_type::ext_oneapi_barrier);
    if (count != expected_nodes.size()) throw std::runtime_error("native graph node count mismatch: " + std::to_string(count));
    std::vector<dpct::experimental::node_ptr> nodes(count);
    dpct::experimental::get_nodes(graph.get(), nodes.data(), &count);
    for (size_t i = 0; i < count; ++i)
        if (nodes[i]->get_type() != expected_nodes[i]) throw std::runtime_error("native graph node type mismatch");
    dpct::get_current_device().destroy_queue(recording);
    graph.reset(); // Executable owns its recorded list after the recorder is gone.
    for (int replay = 0; replay < 3; ++replay) {
        queue.ext_oneapi_graph(executable);
        auto done = queue.ext_oneapi_submit_barrier();
        queue.memcpy(output.data(), destination, sizeof(output)).wait_and_throw();
        if (!done.ready()) throw std::runtime_error("native graph completion missing");
        for (size_t row = 0; row < 8; ++row)
            for (size_t col = 0; col < 8; ++col)
                if (output[row * 8 + col] != (col < 3 ? source[row * 4 + col] : -9.f))
                    throw std::runtime_error("native graph pitched copy mismatch");
        for (auto& value : source) value += 1.f;
    }
    strata::core::CapturedGraph engine_graph;
    std::string error;
    if (!engine_graph.begin(&queue, error)) throw std::runtime_error(error);
    queue.fill(destination, 12.f, 64);
    if (!engine_graph.end(&queue, error) || !engine_graph.launch(&queue, error) || !engine_graph.wait_ms(10000))
        throw std::runtime_error("Strata CapturedGraph failed: " + error);
    queue.memcpy(output.data(), destination, sizeof(output)).wait_and_throw();
    for (auto value : output) if (value != 12.f) throw std::runtime_error("Strata graph output mismatch");
    for (int attempt = 0; attempt < 2; ++attempt) {
        strata::core::PinnedArena host(4096);
        if (!host.base || host.registered_bytes > host.capacity) throw std::runtime_error("invalid native host arena");
        auto* values = static_cast<float*>(host.base);
        for (size_t i = 0; i < output.size(); ++i) values[i] = float(attempt * 100 + i);
        queue.memcpy(destination, values, sizeof(output));
        queue.memcpy(output.data(), destination, sizeof(output)).wait_and_throw();
        for (size_t i = 0; i < output.size(); ++i)
            if (output[i] != float(attempt * 100 + i)) throw std::runtime_error("cached host import copy mismatch");
        std::printf("native host arena %d: imported %llu bytes; %s\n", attempt,
                    (unsigned long long)host.registered_bytes, host.note.c_str());
    }
#ifdef STRATA_ORCA_DG2
    {
        constexpr size_t bytes = 65536;
        struct Borrowed {
            void* pointer = ::operator new(bytes, std::align_val_t(bytes));
            bool imported = false;
            ~Borrowed() {
                try {
                    if (imported) strata::l0::release_host(pointer);
                    ::operator delete(pointer, std::align_val_t(bytes));
                } catch (...) { /* Keep pages alive if native release cannot drain. */ }
            }
        } borrowed;
        auto* values = static_cast<float*>(borrowed.pointer);
        if (strata::l0::import_host(values, bytes - 1) == 0)
            throw std::runtime_error("native host import accepted a partial owned page range");
        for (int pass = 0; pass < 8; ++pass) {
            if (strata::l0::import_host(values, bytes) != 0)
                throw std::runtime_error("native aligned host import failed");
            borrowed.imported = true;
            if (strata::l0::import_host(values, bytes) == 0)
                throw std::runtime_error("native host import accepted an overlapping mapping");
            for (size_t i = 0; i < output.size(); ++i) values[i] = float(pass * 100 + i);
            queue.memcpy(destination, values, sizeof(output));
            queue.memcpy(output.data(), destination, sizeof(output)).wait_and_throw();
            for (size_t i = 0; i < output.size(); ++i)
                if (output[i] != values[i]) throw std::runtime_error("imported native host copy mismatch");
            strata::l0::release_host(values); borrowed.imported = false;
        }
        std::puts("PASS native host import: eight borrowed-page reimports, overlap/alignment rejection and exact copies");
    }
#endif
    dependencies();
    std::puts("PASS native engine bootstrap, arenas, pitched graph copy and Strata graph wrapper");
    return 0;
} catch (const std::exception& e) { std::fprintf(stderr, "FAIL: %s\n", e.what()); return 1; }
