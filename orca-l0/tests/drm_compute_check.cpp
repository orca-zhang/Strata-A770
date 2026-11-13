#include <orca_l0/drm.hpp>
#include "../src/drm/arguments.hpp"
#include "../src/wddm/allocation.hpp"
#include "runtime_dependencies.hpp"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <functional>
#include <filesystem>
#include <immintrin.h>
#include <future>
#include <atomic>
#include <thread>
#ifndef _WIN32
#include <sys/mman.h>
#endif

using namespace orca::l0::drm;
using detail::require;
namespace {
void dependencies() {
    check_runtime_dependencies(true);
}
const std::string yaml = R"(version: '1.73'
kernels:
  - name: advance
    execution_env:
      grf_count: 128
      inline_data_payload_size: 32
      offset_to_skip_per_thread_data_load: 192
      simd_size: 32
      eu_thread_count: 8
    payload_arguments:
      - arg_type: enqueued_local_size
        offset: 0
        size: 12
      - arg_type: arg_bypointer
        offset: 24
        size: 8
        arg_index: 0
        addrmode: stateless
        addrspace: global
      - arg_type: arg_byvalue
        offset: 32
        size: 4
        arg_index: 1
    per_thread_payload_arguments:
      - arg_type: local_id
        offset: 0
        size: 192
)";
std::vector<uint8_t> fixture(const std::string& segment = "", uint32_t product = 1270, uint32_t core = 3079) {
    std::vector<uint8_t> b(64);
    auto put = [&](size_t offset, uint64_t v, unsigned size) {
        for (unsigned j = 0; j < size; ++j) b.at(offset + j) = uint8_t(v >> (8 * j));
    };
    auto section = [&](const std::vector<uint8_t>& data) {
        const auto offset = b.size(); b.insert(b.end(), data.begin(), data.end()); return offset;
    };
    const auto text = section(std::vector<uint8_t>(512));
    const auto info = section({yaml.begin(), yaml.end()});
    std::vector<uint8_t> notes;
    for (auto pair : {std::pair<uint32_t, uint32_t>{1, product}, {2, core}}) {
        const std::array<uint32_t, 6> note{8, 4, pair.first, 0x65746e49, 0x0054476c, pair.second};
        const auto* p = reinterpret_cast<const uint8_t*>(note.data());
        notes.insert(notes.end(), p, p + sizeof(note));
    }
    const auto note = section(notes);
    std::vector<std::string> labels{".text.advance", ".ze_info", ".note.intelgt.compat", ".shstrtab"};
    if (!segment.empty()) labels.push_back(segment);
    std::vector<uint8_t> names{0};
    for (const auto& label : labels) { names.insert(names.end(), label.begin(), label.end()); names.push_back(0); }
    const auto strings = section(names);
    const auto data = segment.empty() ? 0 : section(std::vector<uint8_t>(16, 0x5a));
    const auto table = detail::align(b.size(), 8); b.resize(table + (labels.size() + 1) * 64);
    put(0, 0x464c457f, 4); put(4, 0x010102, 3); put(16, 1, 2); put(18, 205, 2); put(20, 1, 4);
    put(40, table, 8); put(52, 64, 2); put(58, 64, 2); put(60, labels.size() + 1, 2); put(62, 4, 2);
    const std::array<uint64_t, 5> offsets{text, info, note, strings, data};
    const std::array<uint64_t, 5> sizes{512, yaml.size(), notes.size(), names.size(), 16};
    const std::array<uint32_t, 5> types{1, 0xff000011, 7, 3, segment.find(".bss") == 0 ? 8u : 1u};
    uint32_t name = 1;
    for (size_t i = 0; i < labels.size(); ++i) {
        const auto at = table + (i + 1) * 64;
        put(at, name, 4); put(at + 4, types[i], 4); put(at + 24, offsets[i], 8); put(at + 32, sizes[i], 8);
        name += uint32_t(labels[i].size() + 1);
    }
    return b;
}
void reject(const std::function<void()>& f) {
    bool rejected = false;
    try { f(); } catch (const std::exception&) { rejected = true; }
    require(rejected, "negative check accepted invalid input");
}
void self_check() {
    {
        detail::AllocationPool pool;
        unsigned allocated = 0;
        const auto allocate = [&](size_t bytes) { Allocation a; a.bytes = bytes; a.handle = ++allocated; return a; };
        auto small = pool.take(allocate, 64), large = pool.take(allocate, 128);
        auto retained = small;
        std::weak_ptr<const Allocation> weak = small;
        small.reset();
        require(pool.available.empty() && weak.lock(), "pool returned a retained lease");
        retained.reset(); large.reset();
        require(weak.expired() && pool.available.size() == 2, "pool did not return the last strong leases");
        auto reused = pool.take(allocate, 32);
        require(reused->bytes == 64 && allocated == 2 && weak.expired(), "pool lost best-fit reuse or revived a weak lease");
        const auto count = pool.count, free = pool.available.size();
        reject([&] { (void)pool.take([](size_t) -> Allocation { throw std::bad_alloc(); }, 256); });
        require(pool.count == count && pool.available.size() == free, "failed allocation changed pool ownership");
        reused.reset(); weak.reset();
        for (unsigned wave = 0; wave < 64; ++wave) {
            std::vector<std::shared_ptr<const Allocation>> live;
            for (unsigned i = 0; i < 32; ++i) live.push_back(pool.take(allocate, 64 + (i % 3) * 64));
            const auto first = live.front()->handle;
            std::weak_ptr<const Allocation> stale = live.front();
            live.clear();
            require(stale.expired() && pool.available.size() == pool.count, "wave left an allocation live or revived a weak capture");
            auto next = pool.take(allocate, 64);
            require(next->handle && first && stale.expired(), "lease reuse changed stale-capture validity");
        }
    }
    {
        std::array<uint64_t, 2> bounds{UINT64_MAX, 0};
        std::vector<Allocation> allocations;
        size_t hint = SIZE_MAX;
        uint32_t seed = 0x65a1;
        for (unsigned step = 0; step < 4096; ++step) {
            seed = seed * 1664525u + 1013904223u;
            if (step % 97 == 0) allocations.clear(); // Conservative bounds survive removal of every mapping.
            if (step % 7 == 0) {
                Allocation a{step % 3 ? reinterpret_cast<void*>(uintptr_t{0x10000} + (seed % 257) * 0x20000) : nullptr,
                    uint64_t(step + 1) * 0x20000, 65536, step + 1};
                allocations.push_back(a); detail::include_cpu_mapping(bounds, a);
            }
            if (step % 11 == 0 && !allocations.empty()) allocations.erase(allocations.begin());
            if (step % 131 == 0) std::reverse(allocations.begin(), allocations.end());
            const auto a = allocations.empty() ? Allocation{} : allocations[seed % allocations.size()];
            const auto cpu = uint64_t(reinterpret_cast<uintptr_t>(a.data));
            for (const auto address : {uint64_t{0}, UINT64_MAX, a.gpu + 3, cpu, cpu + a.bytes - bool(a.bytes), cpu + a.bytes}) {
                auto resolve = [&](bool bounded) {
                    try { return std::make_pair(true, detail::gpu_address(address, allocations, &hint, bounded ? &bounds : nullptr)); }
                    catch (const std::runtime_error&) { return std::make_pair(false, uint64_t{0}); }
                };
                require(resolve(true) == resolve(false), "CPU bounds changed translation or invalid-address rejection");
            }
        }
        const Allocation high{reinterpret_cast<void*>(UINTPTR_MAX - 31), 0x80000, 64, 1};
        bounds = {UINT64_MAX, 0}; detail::include_cpu_mapping(bounds, high);
        require(bounds[1] == UINT64_MAX && detail::gpu_address(UINT64_MAX, {high}, &hint, &bounds) == 0x8001f,
                "CPU bounds wrapped its inclusive upper limit");
    }
    {
        std::vector<Allocation> allocations;
        std::array<size_t, 4> hints{SIZE_MAX, SIZE_MAX, SIZE_MAX, SIZE_MAX};
        uint32_t seed = 0x65a1;
        for (unsigned step = 0; step < 8192; ++step) {
            seed = seed * 1664525u + 1013904223u;
            if (step % 97 == 0) allocations.clear();
            if (step % 7 == 0) allocations.push_back({nullptr, uint64_t(step + 1) * 0x20000, 65536, step + 1});
            if (step % 11 == 0 && !allocations.empty()) allocations.erase(allocations.begin());
            if (step % 131 == 0) std::reverse(allocations.begin(), allocations.end());
            const auto a = allocations.empty() ? Allocation{} : allocations[seed % allocations.size()];
            for (const auto address : {uint64_t{0}, UINT64_MAX, a.gpu, a.gpu + a.bytes - bool(a.bytes), a.gpu + a.bytes}) {
                const bool expected = std::any_of(allocations.begin(), allocations.end(), [&](const Allocation& b) {
                    return address >= b.gpu && address - b.gpu < b.bytes;
                });
                auto& hint = hints[step % hints.size()];
                require(detail::resident_gpu_address(address, allocations, hint) == expected &&
                        detail::resident_gpu_address(address, allocations, hint) == expected,
                        "resident hint changed validation after growth/removal/clear/reorder");
            }
        }
    }
    {
        std::map<unsigned, detail::RecordingStorage> storage;
        std::array<std::vector<detail::RecordingStorage*>, 3> used, reference;
        uint32_t seed = 0x65a1;
        for (unsigned step = 0; step < 8192; ++step) {
            seed = seed * 1664525u + 1013904223u;
            const auto list = step % used.size();
            if (step % 97 == 0) { used[list].clear(); reference[list].clear(); }
            auto* memory = &storage[seed % 257]; // Map growth keeps earlier resource pointers stable.
            auto& expected = reference[list];
            if (std::find(expected.begin(), expected.end(), memory) == expected.end()) expected.push_back(memory);
            detail::retain_storage(used[list], memory);
            detail::retain_storage(used[list], memory); // Repeated hits must not duplicate storage.
            require(used[list] == expected, "storage index hint changed membership or insertion order");
            if (step % 131 == 0) {
                auto moved = std::move(used[list]);
                detail::retain_storage(moved, memory);
                require(moved == expected, "storage index hint failed after recording move");
                used[list] = std::move(moved);
            }
        }
    }
    const std::vector<Allocation> ranges{{nullptr, 0x10000, 0x10000, 1}, {nullptr, 0x20000, 0x10000, 2}};
    detail::validate_gpu_range(0x10000, 0x10000, ranges);
    detail::validate_gpu_range(0x1ffff, 1, ranges);
    for (auto range : {std::pair<uint64_t, size_t>{0, 1}, {0x10000, 0}, {0x10000, 0x10001},
                       {0x1ffff, 2}, {0x30000, 1}, {UINT64_MAX, 1}, {0x10001, SIZE_MAX}})
        reject([&] { detail::validate_gpu_range(range.first, range.second, ranges); });
    reject([] { detail::native_device_allocation(0); });
    reject([] { detail::native_device_allocation(65535); });
    reject([] { detail::native_device_allocation(uint64_t{1} << 32); });
    reject([] { detail::native_device_allocation((size_t{1} << 30) + 65536); });
    const auto largest_native = detail::native_device_allocation(size_t{1} << 30);
    uint64_t largest_bytes = 0;
    std::memcpy(&largest_bytes, largest_native.data() + 0x38, sizeof(largest_bytes));
    require(largest_bytes == size_t{1} << 30, "one-GiB physical allocation size changed");
    const auto native = detail::native_device_allocation(65536);
    require(native[38] & 4 && native[39] & 1, "local-only/non-lockable device allocation flags missing");
    const auto host = [](uintptr_t address) { return reinterpret_cast<void*>(address); };
    const std::vector<Allocation> imported{{host(0x40000), 0x1000000, 0x20000, 1, true}};
    detail::validate_host_import(host(0x20000), 0x20000, imported);
    detail::validate_host_import(host(0x60000), 0x10000, imported);
    detail::validate_host_import(host(0x10000), 0x10000, {{nullptr, 0x1000000, 0x1000000, 1, false, true}});
    for (auto range : {std::pair<uintptr_t, size_t>{0, 65536}, {0x10001, 65536}, {0x10000, 0},
                       {0x10000, 65535}, {UINTPTR_MAX & ~uintptr_t{65535}, 65536},
                       {0x30000, 0x20000}, {0x40000, 0x10000}, {0x50000, 0x20000}, {0x30000, 0x40000}})
        reject([&] { detail::validate_host_import(host(range.first), range.second, imported); });
    for (bool enabled : {false, true}) for (uint32_t pitch : {64u, 4096u, 8192u, 262144u})
    for (uint64_t address : {uint64_t{0x4000000}, uint64_t{0xffff800004000000ull}}) {
        const auto surface = detail::scratch_surface(address, pitch, 4096, 6, enabled);
        require(uint32_t(surface.getCoherencyType()) == 1u &&
                uint32_t(surface.getL1CacheControlCachePolicy()) == (enabled ? 2u : 0u) &&
                surface.getMemoryObjectControlState() == 6 &&
                surface.getWidth() == 128 && surface.getHeight() == 32 && surface.getDepth() == 1 &&
                surface.getSurfacePitch() == pitch && surface.getSurfaceBaseAddress() == address,
                "scratch cache profile, fallback, geometry or address changed");
    }
    const auto original = fixture();
    const auto k = detail::Elf(original).kernel("advance");
    const std::string vc_yaml = "version: '1.73'\nkernels:\n  - name: advance\n    execution_env:\n"
        "      grf_count: 256\n      simd_size: 1\n      offset_to_skip_per_thread_data_load: 128\n"
        "      work_group_walk_order_dimensions: [ 0, 0, 0 ]\n    per_thread_payload_arguments:\n"
        "      - arg_type: packed_local_ids\n        offset: 0\n        size: 6\n";
    auto vc = detail::metadata(vc_yaml, "advance"); vc.isa.resize(512); vc.cross_bytes = 32;
    const std::vector<uint8_t> vc_cross(32, 0xa7);
    const auto vc_payload = detail::indirect_payload(vc, vc_cross, {4,2,2});
    require(vc_payload.size() == 576 && std::equal(vc_cross.begin(), vc_cross.end(), vc_payload.begin()), "packed payload lost cross-thread bytes");
    for (size_t i = 0; i < 16; ++i) {
        std::array<uint16_t,3> xyz{}; std::memcpy(xyz.data(), vc_payload.data() + 32 + i * 32, sizeof(xyz));
        require(xyz == std::array<uint16_t,3>{uint16_t(i % 4), uint16_t(i / 4 % 2), uint16_t(i / 8)} &&
                std::all_of(vc_payload.begin() + 32 + i * 32 + 6, vc_payload.begin() + 64 + i * 32, [](uint8_t b) { return !b; }),
                "SIMD1 xyz tuple or per-GRF padding mismatch");
    }
    std::vector<uint32_t> vc_commands;
    detail::record_dispatch(vc_commands, vc, vc_cross, {4,2,2}, {3,1,1}, 0x1000000, 4096, 0x2000000, 4096,
                            nullptr, 2, true, 0, 0, 1024, 256);
    const auto vc_word = std::find(vc_commands.begin(), vc_commands.end(), uint32_t{0x72080025});
    require(vc_word != vc_commands.end(), "missing SIMD1 walker");
    packet::DG2::COMPUTE_WALKER vc_walker{}; std::memcpy(&vc_walker, &*vc_word, sizeof(vc_walker));
    require(vc_walker.getSimdSize() == packet::DG2::COMPUTE_WALKER::SIMD_SIZE_SIMD32 &&
            vc_walker.getExecutionMask() == UINT32_MAX && !vc_walker.getGenerateLocalId() && !vc_walker.getEmitLocalId() &&
            vc_walker.getInterfaceDescriptor().getKernelStartPointer() == 1024 &&
            vc_walker.getInterfaceDescriptor().getNumberOfThreadsInGpgpuThreadGroup() == 16 &&
            vc_walker.getIndirectDataLength() == vc_payload.size() && vc_walker.getIndirectDataStartAddress() == 256,
            "SIMD1 entry/thread/payload/walker encoding mismatch");
    for (auto pair : {std::pair<std::string,std::string>{"size: 6", "size: 4"}, {"simd_size: 1", "simd_size: 8"},
                     {"offset: 0", "offset: 2"}, {"[ 0, 0, 0 ]", "[ 2, 1, 0 ]"}}) {
        auto invalid = vc_yaml; invalid.replace(invalid.find(pair.first), pair.first.size(), pair.second);
        reject([&] { detail::metadata(invalid, "advance"); });
    }
    require(detail::Elf(fixture("", 0)).kernel("advance").isa == k.isa, "core-targeted VC/ESIMD zebin rejected");
    reject([] { detail::Elf(fixture("", 1271)).kernel("advance"); });
    for (uint32_t product : {0u, 1270u})
        for (uint32_t core : {0u, 3078u, 3080u})
            reject([&] { detail::Elf(fixture("", product, core)).kernel("advance"); });
    const auto notes = detail::Elf(original).sections[3].offset;
    auto core_only = original; core_only.at(notes + 8) = 9;
    require(detail::Elf(core_only).kernel("advance").isa == k.isa, "core-only zebin rejected");
    auto missing_core = original; missing_core.at(notes + 24 + 8) = 9;
    reject([&] { detail::Elf(missing_core).kernel("advance"); });
    auto duplicate_product = original; duplicate_product.at(notes + 24 + 8) = 1;
    reject([&] { detail::Elf(duplicate_product).kernel("advance"); });
    auto annotated = yaml;
    annotated.insert(annotated.find("    execution_env:"), "    user_attributes:\n      intel_reqd_sub_group_size: 32\n");
    require(detail::metadata(annotated, "advance").simd == 32, "valid subgroup attribute rejected");
    annotated.replace(annotated.find("sub_group_size: 32"), 18, "sub_group_size: 16");
    reject([&] { detail::metadata(annotated, "advance"); });
    auto thread_yaml = yaml + "    per_thread_memory_buffers:\n      - type: scratch\n        usage: single_space\n        size: 288\n      - type: global\n        usage: private_space\n        size: 16\n        is_simt_thread: true\n";
    const auto thread_kernel = detail::metadata(thread_yaml, "advance");
    require(thread_kernel.scratch_bytes[0] == 512 && thread_kernel.private_bytes == 512, "scratch rounding or SIMD private-memory scaling lost");
    auto invalid_slot = thread_yaml + "        slot: 2\n";
    reject([&] { detail::metadata(invalid_slot, "advance"); });
    auto duplicate_scratch = thread_yaml + "      - type: scratch\n        usage: single_space\n        size: 64\n";
    reject([&] { detail::metadata(duplicate_scratch, "advance"); });
    const auto initialized = detail::Elf(fixture(".data.global")).kernel("advance");
    const auto constants = detail::Elf(fixture(".data.const")).kernel("advance");
    const auto zeroed = detail::Elf(fixture(".bss.const")).kernel("advance");
    require(initialized.global_data == std::vector<uint8_t>(16, 0x5a) &&
            constants.constant_data == std::vector<uint8_t>(16, 0x5a) &&
            zeroed.constant_data == std::vector<uint8_t>(16, 0), "initialized/zero-filled data segment mismatch");
    reject([&] { detail::Elf(fixture(".data.unknown")).kernel("advance"); });
    require(k.cross_bytes == 64 && k.isa.size() == 512 && k.simd == 32 && k.skip == 192, "ELF/ABI decode mismatch");
    for (size_t length : {size_t{0}, size_t{1}, size_t{63}, original.size() - 1})
        reject([&] { detail::Elf({original.begin(), original.begin() + length}).kernel("advance"); });
    for (size_t offset : {size_t{0}, size_t{4}, size_t{5}, size_t{18}, size_t{40}, size_t{58}, size_t{62}}) {
        auto bad = original; bad[offset] ^= 0xff;
        reject([&] { detail::Elf(bad).kernel("advance"); });
    }
    reject([&] { detail::Elf(original).kernel("missing"); });
    for (const auto& pair : {std::pair<std::string, std::string>{"simd_size: 32", "simd_size: 64"},
             {"grf_count: 128", "grf_count: 129"}, {"addrmode: stateless", "addrmode: bindful"},
             {"offset: 32", "offset: 24"}, {"size: 192", "size: 64"},
             {"simd_size: 32", "simd_size: 32\n      scratch_size: 4096"},
             {"version: '1.73'", "version: '2.0'"}}) {
        auto bad = yaml; bad.replace(bad.find(pair.first), pair.first.size(), pair.second);
        reject([&] { detail::metadata(bad, "advance"); });
    }
    std::vector<uint32_t> words;
    std::vector<uint8_t> cross(k.cross_bytes, 0x5a);
    detail::record_dispatch(words, k, cross, 64, 3, 0x1000000, 4096, 0x2000000, 4096);
    const auto at = std::find(words.begin(), words.end(), uint32_t{0x72080025});
    require(at != words.end() && words.end() - at >= 39, "missing/incorrect 39-dword compute walker");
    packet::DG2::COMPUTE_WALKER w{}; std::memcpy(&w, &*at, sizeof(w));
    require(w.getThreadGroupIdXDimension() == 3 && w.getLocalXMaximum() == 63 && w.getExecutionMask() == UINT32_MAX &&
            w.getIndirectDataLength() == 64 && w.getGenerateLocalId() && w.getEmitLocalId() == 7 &&
            w.getInterfaceDescriptor().getKernelStartPointer() == 192 &&
            w.getInterfaceDescriptor().getNumberOfThreadsInGpgpuThreadGroup() == 2 &&
            w.getInterfaceDescriptor().getPreferredSlmAllocationSize() ==
                packet::DG2::INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE_0KB &&
            !std::memcmp(w.getInlineDataPointer(), cross.data(), 32), "dispatch/payload encoding mismatch");
    for (bool enabled : {false, true}) for (uint32_t local : {32u, 64u}) {
        std::vector<uint32_t> probe;
        detail::record_dispatch(probe, k, cross, local, 3, 0x1000000, 4096, 0x2000000, 4096,
                                nullptr, 2, true, 0, 0, 0, 0, enabled);
        auto find = [&](uint32_t header) {
            auto at = std::find(probe.begin(), probe.end(), header);
            require(at != probe.end(), "L0-field check missing packet"); return at;
        };
        packet::DG2::CFE_STATE cfe{};
        std::memcpy(&cfe, &*find(packet::DG2::CFE_STATE::sInit().getRawData(0)), sizeof(cfe));
        packet::DG2::STATE_BASE_ADDRESS sba{};
        std::memcpy(&sba, &*find(packet::DG2::STATE_BASE_ADDRESS::sInit().getRawData(0)), sizeof(sba));
        packet::DG2::COMPUTE_WALKER walker{};
        std::memcpy(&walker, &*find(0x72080025), sizeof(walker));
        require(cfe.getComputeOverdispatchDisable() == enabled, "overdispatch policy mismatch");
        require(uint32_t(sba.getL1CacheControlCachePolicy()) == 0, "L1 policy mismatch");
        require(uint32_t(walker.getInterfaceDescriptor().getThreadGroupDispatchSize()) ==
            (enabled ? (local == 32 ? 2u : 3u) : 0u), "thread-group dispatch policy mismatch");
        require(walker.getInterfaceDescriptor().getDenormMode() ==
            packet::DG2::INTERFACE_DESCRIPTOR_DATA::DENORM_MODE_FTZ, "compiler launch state mismatch");
    }
    // Compare all words: a pure C0 GRF transition changes only its selected component.
    for (bool calibrated : {false, true}) {
        std::vector<uint32_t> probe; detail::DispatchState tracked;
        auto record = [&](const detail::Kernel& kernel, detail::DispatchState* state, bool profile,
                          uint64_t code = 0x1000000, uint32_t mocs = 2) {
            const auto start = probe.size();
            detail::record_dispatch(probe, kernel, cross, 32, 3, code, 4096, 0x2000000, 4096,
                                    state, mocs, true, 0, 0, 0, 0, profile);
            return std::vector<uint32_t>(probe.begin() + start, probe.end());
        };
        const auto baseline = record(k, &tracked, calibrated);
        auto large = k; large.grf = 256;
        auto expected = baseline;
        auto at = std::find(expected.begin(), expected.end(), packet::DG2::STATE_COMPUTE_MODE::sInit().getRawData(0));
        require(at != expected.end(), "missing SCM");
        packet::DG2::STATE_COMPUTE_MODE mode{}; std::memcpy(&mode, &*at, sizeof(mode));
        mode.setLargeGrfMode(true); std::memcpy(&*at, &mode, sizeof(mode));
        if (calibrated) {
            auto sync = detail::barrier(true, true);
            sync.setStateCacheInvalidationEnable(false);
            sync.setConstantCacheInvalidationEnable(false);
            sync.setTextureCacheInvalidationEnable(false);
            sync.setInstructionCacheInvalidateEnable(false);
            std::memcpy(expected.data(), &sync, sizeof(sync));
            const auto at = std::find(expected.begin(), expected.end(), packet::DG2::STATE_BASE_ADDRESS::sInit().getRawData(0));
            require(at != expected.end(), "missing baseline SBA");
            expected.erase(at, at + 2 * sizeof(packet::DG2::STATE_BASE_ADDRESS) / 4);
        }
        require(record(large, &tracked, calibrated) == expected, "transition changed another packet or synchronization field");
        require(record(k, &tracked, calibrated).size() == expected.size(), "returning GRF lost selected policy");
        auto full = [&](const auto& words) {
            require(words.size() == baseline.size(), "coupled/fresh/fallback transition lost full preamble");
            const auto sync = detail::barrier(true, true);
            require(!std::memcmp(words.data(), &sync, sizeof(sync)), "coupled/fresh/fallback transition lost invalidation");
        };
        full(record(large, &tracked, calibrated, 0x1000000, 6));
        full(record(k, &tracked, calibrated, 0x100000000));
        tracked.valid = false; full(record(k, &tracked, calibrated));
        full(record(large, nullptr, calibrated));
        full(record(large, &tracked, !calibrated));
    }
    // A policy transition must reprogram CFE, even with identical kernel/heaps.
    for (bool initial : {false, true}) {
        std::vector<uint32_t> probe; detail::DispatchState tracked;
        auto record = [&](bool enabled) {
            detail::record_dispatch(probe, k, cross, 32, 3, 0x1000000, 4096, 0x2000000, 4096,
                                    &tracked, 2, true, 0, 0, 0, 0, enabled);
        };
        record(initial); const auto full = probe.size();
        record(!initial);
        require(probe.size() == 2 * full, "policy transition omitted global programming");
        const auto before_repeat = probe.size(); record(!initial);
        require((probe.size() - before_repeat) * 4 ==
            sizeof(packet::DG2::COMPUTE_WALKER) + sizeof(packet::DG2::PIPE_CONTROL),
            "unchanged dispatch policy lost state reuse");
    }
    reject([&] { detail::record_dispatch(words, k, cross, 64, 3, 3, 4096, 0, 4096); });
    words.clear();
    detail::record_dispatch(words, k, cross, 64, 3, 0x800000000000ull, 4096, 0x800000002000ull, 4096);
    const auto header = packet::DG2::STATE_BASE_ADDRESS::sInit().getRawData(0);
    const auto base_at = std::find(words.begin(), words.end(), header);
    require(base_at != words.end() && size_t(words.end() - base_at) * 4 >= sizeof(packet::DG2::STATE_BASE_ADDRESS),
            "missing state base for high GPU VA");
    packet::DG2::STATE_BASE_ADDRESS base{}; std::memcpy(&base, &*base_at, sizeof(base));
    require(base.getInstructionBaseAddress() == 0xffff800000000000ull &&
            base.getGeneralStateBaseAddress() == 0xffff800000002000ull, "high GPU VA not canonized in the encoder");
    reject([&] { detail::record_dispatch(words, k, cross, 64, 3, uint64_t{1} << 48, 4096, 0, 4096); });
    reject([&] { detail::record_dispatch(words, k, cross, 64, 3, 0xfffffffffffff000ull, 8192, 0, 4096); });
    words.clear();
    detail::DispatchState state;
    detail::record_dispatch(words, k, cross, 64, 3, 0x1000000, 4096, 0x2000000, 4096, &state);
    const auto first_size = words.size();
    detail::record_dispatch(words, k, cross, 32, 7, 0x1000000, 4096, 0x2000000, 4096, &state);
    const auto repeated_size = words.size() - first_size;
    require(repeated_size * 4 == sizeof(packet::DG2::COMPUTE_WALKER) + sizeof(packet::DG2::PIPE_CONTROL) &&
            repeated_size < first_size, "identical state was reprogrammed or dependency flush omitted");
    packet::DG2::COMPUTE_WALKER repeat{}; std::memcpy(&repeat, words.data() + first_size, sizeof(repeat));
    require(repeat.getLocalXMaximum() == 31 && repeat.getThreadGroupIdXDimension() == 7,
            "reused state lost dynamic walker dimensions");
    std::vector<uint32_t> relaxed;
    auto explicit_state = state; explicit_state.in_order = false;
    detail::record_dispatch(relaxed, k, cross, 32, 7, 0x1000000, 4096, 0x2000000, 4096, &explicit_state);
    require(relaxed.size() * 4 == sizeof(packet::DG2::COMPUTE_WALKER) &&
            std::equal(relaxed.begin(), relaxed.end(), words.begin() + first_size),
            "explicit ordering changed the walker or retained the implicit dependency flush");
    relaxed.clear(); explicit_state.valid = false;
    detail::record_dispatch(relaxed, k, cross, 64, 3, 0x1000000, 4096, 0x2000000, 4096, &explicit_state);
    require(relaxed.size() * 4 + sizeof(packet::DG2::PIPE_CONTROL) == first_size * 4 &&
            std::equal(relaxed.begin(), relaxed.end(), words.begin()),
            "explicit ordering changed required state invalidations or workarounds");
    const auto before_offsets = words.size();
    detail::record_dispatch(words, k, cross, 64, 3, 0x1000000, 4096, 0x2000000, 4096,
                            &state, 2, true, 0, 0, 1024, 256);
    require(words.size() - before_offsets == repeated_size, "shared-heap kernel/capture offsets reprogrammed global state");
    std::memcpy(&repeat, words.data() + before_offsets, sizeof(repeat));
    require(repeat.getInterfaceDescriptor().getKernelStartPointer() == 0x1000000 + k.skip + 1024 && repeat.getIndirectDataStartAddress() == 0x2000000 + 256,
            "shared-heap instruction or indirect offset lost");
    reject([&] { detail::record_dispatch(words, k, cross, 64, 3, 0x1000000, 4096, 0x2000000, 4096, nullptr, 2, true, 0, 0, 3); });
    reject([&] { detail::record_dispatch(words, k, cross, 64, 3, 0x1000000, 4096, 0x2000000, 4096, nullptr, 2, true, 0, 0, 4096); });
    reject([&] { detail::record_dispatch(words, k, cross, 64, 3, 0x1000000, 4096, 0x2000000, 4096, nullptr, 2, true, 0, 0, 0, 3); });
    const auto before_change = words.size();
    detail::record_dispatch(words, k, cross, 64, 3, 0x3000000, 4096, 0x2000000, 4096, &state);
    require(words.size() - before_change == repeated_size, "same-window instruction allocation reprogrammed global state");
    std::memcpy(&repeat, words.data() + before_change, sizeof(repeat));
    require(repeat.getInterfaceDescriptor().getKernelStartPointer() == 0x3000000 + k.skip,
            "same-window instruction allocation lost its absolute offset");
    for (uint64_t address : {uint64_t{0x1000000}, uint64_t{0xffff800001000000ull},
                             uint64_t{0xfffff000}, uint64_t{0x1ffffe000}}) {
        std::vector<uint32_t> probe;
        detail::DispatchState tracked;
        detail::record_dispatch(probe, k, cross, 64, 3, address, 8192, address, 4096, &tracked,
                                2, true, 0, 0, 1024, 256);
        const auto command = std::find(probe.begin(), probe.end(), header);
        require(command != probe.end(), "window mapping lost initial heap programming");
        packet::DG2::STATE_BASE_ADDRESS mapped_base{};
        std::memcpy(&mapped_base, &*command, sizeof(mapped_base));
        const auto walker = std::find(probe.begin(), probe.end(), uint32_t{0x72080025});
        require(walker != probe.end(), "window mapping lost walker");
        packet::DG2::COMPUTE_WALKER mapped_walker{};
        std::memcpy(&mapped_walker, &*walker, sizeof(mapped_walker));
        require(mapped_base.getInstructionBaseAddress() + mapped_walker.getInterfaceDescriptor().getKernelStartPointer() ==
                    address + 1024 + k.skip &&
                mapped_base.getGeneralStateBaseAddress() + mapped_walker.getIndirectDataStartAddress() == address + 256,
                "heap window changed code or parameter GPU address");
        if ((address & 0xffffffffull) + 8192 > 0xfffff000ull)
            require(mapped_base.getInstructionBaseAddress() == address,
                    "cross-window instruction allocation lost allocation-relative fallback");
    }
    const auto before_policy = words.size();
    detail::record_dispatch(words, k, cross, 64, 3, 0x3000000, 4096, 0x2000000, 4096, &state, 6);
    require(words.size() - before_policy == first_size, "changed cache policy reused stale state");
    const auto policy_base = std::find(words.begin() + before_policy, words.end(), header);
    require(policy_base != words.end(), "missing cached-policy SBA");
    std::memcpy(&base, &*policy_base, sizeof(base));
    require(base.getStatelessDataPortAccessMemoryObjectControlState() == 6, "wrong DG2 MOCS index");
    const auto gpu_barrier = detail::barrier(false, false), host_barrier = detail::barrier();
    require(!gpu_barrier.getDcFlushEnable() && gpu_barrier.getHdcPipelineFlush() &&
            gpu_barrier.getUnTypedDataPortCacheFlush() && gpu_barrier.getCommandStreamerStallEnable() &&
            host_barrier.getDcFlushEnable(), "GPU dependency or final host visibility flush lost");
    const auto gate_wait = detail::host_gate_wait(0x800000010000ull);
    require(gate_wait.getSemaphoreGraphicsAddress() == 0xffff800000010000ull &&
            gate_wait.getSemaphoreDataDword() == 0 &&
            gate_wait.getCompareOperation() == packet::DG2::MI_SEMAPHORE_WAIT::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD &&
            gate_wait.getWaitMode() == packet::DG2::MI_SEMAPHORE_WAIT::WAIT_MODE_POLLING_MODE &&
            gate_wait.getMemoryType() == packet::DG2::MI_SEMAPHORE_WAIT::MEMORY_TYPE_PER_PROCESS_GRAPHICS_ADDRESS,
            "host semaphore address/compare/wait mode lost");
    reject([&] { detail::host_gate_wait(0); }); reject([&] { detail::host_gate_wait(0x10001); });
    const auto timestamp = detail::timestamp_value(0x800000010000ull);
    auto expected_timestamp = detail::signal_value(0x800000010000ull, 0);
    expected_timestamp.setPostSyncOperation(packet::DG2::PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_TIMESTAMP);
    require(!std::memcmp(&timestamp, &expected_timestamp, sizeof(timestamp)),
            "timestamp changed required synchronization or canonical destination");
    const auto device_timestamp = detail::timestamp_value(0x800000010000ull, false);
    expected_timestamp.setDcFlushEnable(false);
    require(!std::memcmp(&device_timestamp, &expected_timestamp, sizeof(device_timestamp)) &&
            detail::signal_value(0x10000, 1).getDcFlushEnable(),
            "device timestamp lost ordering or completion lost host visibility");
    reject([] { detail::timestamp_value(0); }); reject([] { detail::timestamp_value(0x10004); });
    reject([] { detail::timestamp_value(0x1000000000000ull); });
    const auto signal = detail::signal_value(0x800000010000ull, 0x1234567887654321ull);
    require(signal.getPostSyncOperation() == packet::DG2::PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA &&
            signal.getAddress() == 0x10000 && signal.getAddressHigh() == 0xffff8000 &&
            signal.getImmediateData() == 0x1234567887654321ull && signal.getCommandStreamerStallEnable() &&
            signal.getHdcPipelineFlush() && signal.getDcFlushEnable(), "completion tag lost address/value/visibility");
    const auto value_wait = detail::wait_value(0x800000010000ull, 17);
    require(value_wait.getSemaphoreDataDword() == 17 &&
            value_wait.getCompareOperation() == packet::DG2::MI_SEMAPHORE_WAIT::COMPARE_OPERATION_SAD_EQUAL_SDD,
            "completion tag wait lost equality/value");
    reject([] { detail::signal_value(0, 1); }); reject([] { detail::signal_value(0x10004, 1); });
    reject([] { detail::signal_value(0x1000000000000ull, 1); });
    uint64_t va = 0x1000040;
    const uint32_t delta = 7;
    const std::vector<Allocation> resident{{nullptr, 0x1000000, 4096, 1}};
    const auto packed = detail::pack_arguments(k, 64, 16, {Argument::value(va), Argument::value(delta)}, resident);
    const std::array<Argument, 2> stack_args{Argument::value(va), Argument::value(delta)};
    require(detail::pack_arguments(k, {64,1,1}, {16,1,1}, stack_args.data(), stack_args.size(), resident) == packed,
            "stack argument view changed packed bytes");
    reject([&] { detail::pack_arguments(k, {64,1,1}, {16,1,1}, nullptr, 2, resident); });
    reject([&] { detail::pack_arguments(k, {64,1,1}, {16,1,1}, stack_args.data(), 1, resident); });
    {
        detail::Kernel empty;
        empty.cross_bytes = 32;
        require(detail::pack_arguments(empty, {1,1,1}, {1,1,1}, nullptr, 0, {}) == std::vector<uint8_t>(32),
                "empty argument view changed zero-initialized payload");
        reject([&] { detail::local_memory_bytes(empty, nullptr, 1); });
    }
    std::vector<size_t> resident_hints;
    for (const auto& shifted : {resident, std::vector<Allocation>{{nullptr, 0x80000, 4096, 2}, resident[0]}, resident})
        require(detail::pack_arguments(k, 64, 16, {Argument::value(va), Argument::value(delta)}, shifted, 0, 0, 0, &resident_hints) == packed,
                "resident hint changed packed argument bytes after allocation index shift");
    uint64_t decoded = 0; std::memcpy(&decoded, packed.data() + 24, sizeof(decoded));
    require(decoded == va && !std::memcmp(packed.data() + 32, &delta, 4), "C ABI argument bytes lost");
    const std::vector<Allocation> mapped{{reinterpret_cast<void*>(uintptr_t{0x80000}), 0x1000000, 4096, 1}};
    const uint64_t cpu_pointer = 0x80040;
    require(detail::gpu_address(0, mapped) == 0 && detail::gpu_address(cpu_pointer, mapped) == va &&
            detail::gpu_address(va, mapped) == va, "CPU/interior/GPU/null address translation failed");
    reject([&] { detail::gpu_address(0x81000, mapped); });
    const auto translated = detail::pack_arguments(k, 64, 16,
        {Argument::capture(&cpu_pointer, sizeof(cpu_pointer)), Argument::value(delta)}, mapped);
    const std::array<Argument, 2> captured_args{Argument::capture(&cpu_pointer, sizeof(cpu_pointer)), Argument::value(delta)};
    require(detail::pack_arguments(k, {64,1,1}, {16,1,1}, captured_args.data(), captured_args.size(), mapped) == translated,
            "stack argument view changed captured-pointer translation");
    {
        detail::Kernel aggregate;
        aggregate.cross_bytes = 64;
        aggregate.payload.push_back({"local_size", "", "", 32, 12, UINT32_MAX, 0});
        std::vector<Allocation> allocations;
        std::array<uint64_t, 4> pointers{};
        for (uint32_t field = 0; field < pointers.size(); ++field) {
            aggregate.payload.push_back({"arg_byvalue", "", "", field * 8, 8, 0, field * 8, 0, true});
            allocations.push_back({nullptr, uint64_t(field + 1) * 0x20000, 65536, field + 1});
        }
        std::vector<size_t> hints;
        std::array<uint64_t, 2> bounds{UINT64_MAX, 0};
        for (const auto& a : allocations) detail::include_cpu_mapping(bounds, a);
        for (unsigned step = 0; step < 16; ++step) {
            for (size_t field = 0; field < pointers.size(); ++field) pointers[field] = allocations[field].gpu + step * 4;
            for (bool captured : {false, true}) {
                const auto arg = captured ? Argument::capture(pointers.data(), sizeof(pointers)) : Argument::value(pointers);
                require(detail::pack_arguments(aggregate, 64, 1, {arg}, allocations, 0, 0, 0, &hints) ==
                            detail::pack_arguments(aggregate, 64, 1, {arg}, allocations),
                        "resident hints changed multiple pointer fields sharing one aggregate argument");
                require(detail::pack_arguments(aggregate, 64, 1, {arg}, allocations, 0, 0, 0, &hints, &bounds) ==
                            detail::pack_arguments(aggregate, 64, 1, {arg}, allocations),
                        "CPU bounds changed aggregate pointer bytes");
            }
            if (step % 3 == 0) std::reverse(allocations.begin(), allocations.end());
        }
    }
    require(detail::pack_arguments(k, 64, 16, {Argument::capture(&cpu_pointer, sizeof(cpu_pointer)), Argument::value(delta)},
                mapped, 0, 0, 0, &resident_hints) == translated, "resident hint changed captured CPU pointer translation");
    std::array<uint64_t, 2> mapped_bounds{UINT64_MAX, 0};
    for (const auto& a : mapped) detail::include_cpu_mapping(mapped_bounds, a);
    require(detail::pack_arguments(k, 64, 16, {Argument::capture(&cpu_pointer, sizeof(cpu_pointer)), Argument::value(delta)},
                mapped, 0, 0, 0, &resident_hints, &mapped_bounds) == translated, "CPU bounds changed captured CPU pointer");
    require(detail::pack_arguments(k, 64, 16, {Argument::capture(&va, sizeof(va)), Argument::value(delta)},
                mapped, 0, 0, 0, &resident_hints) == packed, "resident hint changed captured GPU pointer");
    require(detail::pack_arguments(k, 64, 16, {Argument::capture(&va, sizeof(va)), Argument::value(delta)},
                mapped, 0, 0, 0, &resident_hints, &mapped_bounds) == packed, "CPU bounds changed captured GPU pointer");
    {
        size_t hint = 0;
        const std::vector<Allocation> overlapping{{nullptr, 0x10000, 4096, 1},
            {reinterpret_cast<void*>(uintptr_t{0x10000}), 0x80000, 4096, 2}};
        require(detail::gpu_address(0x10040, overlapping, &hint) == 0x80040,
                "resident GPU hint took precedence over CPU pointer translation");
        std::array<uint64_t, 2> bounds{UINT64_MAX, 0};
        for (const auto& a : overlapping) detail::include_cpu_mapping(bounds, a);
        require(detail::gpu_address(0x10040, overlapping, &hint, &bounds) == 0x80040 &&
                detail::gpu_address(0, overlapping, &hint, &bounds) == 0,
                "CPU bounds changed CPU/GPU overlap priority or null translation");
        reject([&] { detail::gpu_address(0x90000, overlapping, &hint); });
    }
    std::memcpy(&decoded, translated.data() + 24, 8);
    require(decoded == va, "compiler pointer capture was not translated");
    struct Aggregate { uint64_t scalar, pointer; } captured_aggregate{cpu_pointer, cpu_pointer};
    auto aggregate_kernel = k;
    aggregate_kernel.payload[1].type = "arg_byvalue";
    aggregate_kernel.payload[1].mode.clear(); aggregate_kernel.payload[1].space.clear();
    aggregate_kernel.payload[1].is_pointer = true; aggregate_kernel.payload[1].source = 8;
    auto scalar_field = aggregate_kernel.payload[1];
    scalar_field.is_pointer = false; scalar_field.source = 0; scalar_field.offset = 40;
    aggregate_kernel.payload.push_back(scalar_field);
    const auto aggregate_cross = detail::pack_arguments(aggregate_kernel, 64, 16,
        {Argument::capture(&captured_aggregate, sizeof(captured_aggregate)), Argument::value(delta)}, mapped);
    uint64_t kept = 0; std::memcpy(&decoded, aggregate_cross.data() + 24, 8); std::memcpy(&kept, aggregate_cross.data() + 40, 8);
    require(decoded == va && kept == cpu_pointer, "aggregate translation changed a scalar or missed its pointer");
    require(detail::suggest_group_size(k, 768) == 256 && detail::suggest_group_size(k, 48) == 16 &&
            detail::suggest_group_size(k, 37) == 1, "range group divisor selection failed");
    reject([&] { detail::suggest_group_size(k, 0); });
    auto required_kernel = k; required_kernel.required_local = {32,1,1};
    require(detail::suggest_group_size(required_kernel, 768) == 32, "required group size was ignored");
    reject([&] { detail::suggest_group_size(required_kernel, 37); });
    required_kernel.required_local = {4,2,2};
    reject([&] { detail::suggest_group_size(required_kernel, 768); });
    reject([&] { detail::pack_arguments(k, 64, 16, {Argument::value(va)}, resident); });
    reject([&] { detail::pack_arguments(k, 64, 16, {{&va, 4}, Argument::value(delta)}, resident); });
    va = 0x1001000;
    reject([&] { detail::pack_arguments(k, 64, 16, {Argument::value(va), Argument::value(delta)}, resident); });
    reject([&] { detail::pack_arguments(k, 64, 16, {Argument::value(va), Argument::value(delta)}, resident, 0, 0, 0, &resident_hints); });
    va = 0;
    detail::pack_arguments(k, 64, 16, {Argument::value(va), Argument::value(delta)}, resident);
    require(detail::pack_arguments(k, 64, 16, {Argument::value(va), Argument::value(delta)}, resident, 0, 0, 0, &resident_hints) ==
                detail::pack_arguments(k, 64, 16, {Argument::value(va), Argument::value(delta)}, resident), "resident hint rejected null");
    auto aggregate = k;
    aggregate.payload[2].source = 8;
    std::array<uint32_t, 4> capture{1, 2, 13, 4};
    const auto split = detail::pack_arguments(aggregate, 64, 16, {Argument::value(va), Argument::value(capture)}, resident);
    require(!std::memcmp(split.data() + 32, &capture[2], 4), "aggregate source offset ignored");
    aggregate.payload[2].size = 8; aggregate.payload[2].is_pointer = true;
    std::array<uint64_t, 2> pointer_capture{0, 0x1000040};
    detail::pack_arguments(aggregate, 64, 16, {Argument::value(va), Argument::value(pointer_capture)}, resident);
    pointer_capture[1] = 0x1001000;
    reject([&] { detail::pack_arguments(aggregate, 64, 16, {Argument::value(va), Argument::value(pointer_capture)}, resident); });
    auto hidden = k;
    hidden.payload.push_back({"const_base", "", "", 40, 8, UINT32_MAX, 0});
    hidden.payload.push_back({"global_base", "", "", 48, 8, UINT32_MAX, 0});
    hidden.payload.push_back({"private_base_stateless", "", "", 56, 8, UINT32_MAX, 0});
    const uint64_t global_va = 0x12345000, const_va = 0x34567000;
    const uint64_t private_va = 0x45678000;
    const auto bases = detail::pack_arguments(hidden, 64, 16, {Argument::value(va), Argument::value(delta)}, resident, global_va, const_va, private_va);
    require(!std::memcmp(bases.data() + 40, &const_va, 8) && !std::memcmp(bases.data() + 48, &global_va, 8) &&
            !std::memcmp(bases.data() + 56, &private_va, 8),
            "global/constant hidden base pointers not bound");
    auto local_kernel = k;
    local_kernel.slm = 24;
    local_kernel.payload[1].mode = "slm"; local_kernel.payload[1].space = "local"; local_kernel.payload[1].alignment = 16;
    const std::vector<Argument> local_args{Argument::local(512), Argument::value(delta)};
    const auto local_cross = detail::pack_arguments(local_kernel, 64, 16, local_args, {});
    require(detail::pack_arguments(local_kernel, {64,1,1}, {16,1,1}, local_args.data(), local_args.size(), {}) == local_cross &&
            detail::local_memory_bytes(local_kernel, local_args.data(), local_args.size()) == 544,
            "stack argument view changed dynamic local memory");
    uint64_t local_offset = 0; std::memcpy(&local_offset, local_cross.data() + 24, 8);
    require(local_offset == 32 && detail::local_memory_bytes(local_kernel, local_args) == 544,
            "dynamic local offset/alignment or static SLM extent lost");
    reject([&] { detail::pack_arguments(local_kernel, 64, 16, {Argument::local(65536), Argument::value(delta)}, {}); });
    reject([&] { detail::pack_arguments(local_kernel, 64, 16, {Argument::value(va), Argument::value(delta)}, {}); });
    for (unsigned kind = 0; kind < 4; ++kind) {
        auto binding_kernel = kind == 3 ? k : hidden;
        if (kind == 2) {
            binding_kernel.slm = 24;
            binding_kernel.payload[1] = local_kernel.payload[1];
        }
        const std::vector<Argument> binding_args{kind == 2 ? Argument::local(512) : kind == 1 ?
            Argument::capture(&cpu_pointer, sizeof(cpu_pointer)) : Argument::value(mapped[0].gpu), Argument::value(delta)};
        for (const std::array<uint32_t, 3> shape : {std::array<uint32_t, 3>{64,1,1}, {4,2,2}}) {
            auto bound = detail::pack_arguments(binding_kernel, shape, {3,5,7}, binding_args, mapped);
            for (const std::array<uint64_t, 3> addresses : {std::array<uint64_t, 3>{0,0,0}, {global_va,const_va,private_va},
                    {0xffff800012340000ull,0xffff800056780000ull,0xffff80009abc0000ull}, {0,0,0}}) {
                detail::bind_base_arguments(binding_kernel, bound, addresses[0], addresses[1], addresses[2]);
                require(bound == detail::pack_arguments(binding_kernel, shape, {3,5,7}, binding_args, mapped,
                            addresses[0], addresses[1], addresses[2]), "binding bases differs from full argument repack");
            }
            bound.pop_back();
            reject([&] { detail::bind_base_arguments(binding_kernel, bound, global_va, const_va, private_va); });
        }
    }
    words.clear();
    detail::record_dispatch(words, k, cross, 64, 1, 0x1000000, 4096, 0x2000000, 4096, nullptr, 2, true, 2048);
    const auto local_walker = std::find(words.begin(), words.end(), uint32_t{0x72080025});
    require(local_walker != words.end(), "missing dynamic SLM walker");
    std::memcpy(&w, &*local_walker, sizeof(w));
    require(w.getInterfaceDescriptor().getSharedLocalMemorySize() == 2 &&
            w.getInterfaceDescriptor().getPreferredSlmAllocationSize() ==
                packet::DG2::INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE_MAX,
            "dynamic SLM size or conservative allocation preference not encoded");
    words.clear();
    local_kernel.slm = 1024;
    detail::record_dispatch(words, local_kernel, cross, 64, 1, 0x1000000, 4096, 0x2000000, 4096);
    const auto static_walker = std::find(words.begin(), words.end(), uint32_t{0x72080025});
    require(static_walker != words.end(), "missing static SLM walker");
    std::memcpy(&w, &*static_walker, sizeof(w));
    require(w.getInterfaceDescriptor().getSharedLocalMemorySize() == 1 &&
            w.getInterfaceDescriptor().getPreferredSlmAllocationSize() ==
                packet::DG2::INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE_MAX,
            "static SLM extent lost or allocation preference disabled");
    const auto surface = detail::scratch_surface(0x800000010000ull, 2048, 4096, 6);
    require(surface.getSurfaceBaseAddress() == 0xffff800000010000ull && surface.getSurfacePitch() == 2048 &&
            surface.getWidth() == 128 && surface.getHeight() == 32 && surface.getDepth() == 1 &&
            surface.getSurfaceType() == packet::DG2::RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_SCRATCH &&
            surface.getMemoryObjectControlState() == 6, "scratch surface pitch/physical thread extent/address mismatch");
    reject([&] { detail::scratch_surface(0x10001, 2048, 4096, 6); });
    reject([&] { detail::record_dispatch(words, thread_kernel, cross, 64, 1, 0x1000000, 4096, 0x2000000, 4096); });
    words.clear();
    detail::record_dispatch(words, thread_kernel, cross, 64, 1, 0x1000000, 4096, 0x2000000, 4096, nullptr, 6, false, 0, 0x4000000);
    const auto scratch_cfe = std::find(words.begin(), words.end(), packet::DG2::CFE_STATE::sInit().getRawData(0));
    require(scratch_cfe != words.end(), "scratch CFE missing");
    packet::DG2::CFE_STATE cfe{}; std::memcpy(&cfe, &*scratch_cfe, sizeof(cfe));
    require(cfe.getScratchSpaceBuffer() == 128, "scratch surface descriptor offset lost");
    for (const uint64_t scratch : {uint64_t{0x4000000}, uint64_t{0xffff800004000000ull}}) {
        words.clear();
        detail::DispatchState retained;
        detail::record_dispatch(words, thread_kernel, cross, 64, 1, 0x1000000, 4096, 0x2000000, 4096,
                                &retained, 6, false, 0, scratch);
        auto before = words.size();
        detail::record_dispatch(words, k, cross, 64, 1, 0x1000000, 4096, 0x2000000, 4096, &retained, 6, false);
        require(retained.programmed[7] == scratch && words.size() - before == repeated_size,
                "no-scratch kernel cleared the retained descriptor or reprogrammed unchanged state");
        before = words.size();
        detail::record_dispatch(words, thread_kernel, cross, 64, 1, 0x1000000, 4096, 0x2000000, 4096,
                                &retained, 6, false, 0, scratch);
        require(words.size() - before == repeated_size, "returning scratch kernel reprogrammed identical state");
        auto large_grf = k; large_grf.grf = 256;
        before = words.size();
        detail::record_dispatch(words, large_grf, cross, 64, 1, 0x1000000, 4096, 0x2000000, 4096, &retained, 6, false);
        auto encoded = std::find(words.begin() + before, words.end(), header);
        require(encoded != words.end(), "GRF transition lost required state programming");
        std::memcpy(&base, &*encoded, sizeof(base));
        require(base.getSurfaceStateBaseAddress() == scratch && base.getBindlessSurfaceStateBaseAddress() == scratch,
                "GRF transition cleared an inherited scratch base");
        reject([&] { detail::record_dispatch(words, thread_kernel, cross, 64, 1, 0x1000000, 4096, 0x2000000, 4096,
                                             &retained, 6, false); });
        detail::DispatchState other;
        other.programmed[7] = scratch; // An invalid/new list must ignore stale cached fields.
        std::vector<uint32_t> fresh;
        detail::record_dispatch(fresh, k, cross, 64, 1, 0x1000000, 4096, 0x2000000, 4096, &other, 6, false);
        auto fresh_cfe = std::find(fresh.begin(), fresh.end(), packet::DG2::CFE_STATE::sInit().getRawData(0));
        require(fresh_cfe != fresh.end() && other.programmed[7] == 0 && retained.programmed[7] == scratch,
                "scratch state leaked between recordings");
        std::memcpy(&cfe, &*fresh_cfe, sizeof(cfe));
        require(cfe.getScratchSpaceBuffer() == 0, "fresh no-scratch list inherited an old CFE descriptor");
        detail::record_dispatch(fresh, thread_kernel, cross, 64, 1, 0x1000000, 4096, 0x2000000, 4096,
                                &other, 6, false, 0, 0x6000000);
        require(other.programmed[7] == 0x6000000 && retained.programmed[7] == scratch,
                "independent recordings shared mutable scratch state");
    }
    auto flags = yaml;
    flags.insert(flags.find("    execution_env:"), "    user_attributes:\n      reqd_work_group_size: [4,2,2]\n");
    flags.insert(flags.find("      grf_count:"), "      has_global_atomics: true\n      require_disable_eufusion: true\n");
    auto three = detail::metadata(flags, "advance");
    require(three.disable_eu_fusion && three.required_local == std::array<uint32_t, 3>{4,2,2}, "EU fusion/3D required size metadata lost");
    auto env_size = flags;
    env_size.insert(env_size.find("      grf_count:"), "      required_work_group_size: [ 4, 2, 2 ]\n");
    require(detail::metadata(env_size, "advance").required_local == three.required_local, "matching required sizes rejected");
    env_size.replace(env_size.find("[ 4, 2, 2 ]"), 11, "[ 8, 2, 2 ]");
    reject([&] { detail::metadata(env_size, "advance"); });
    env_size = yaml;
    env_size.insert(env_size.find("      grf_count:"), "      required_work_group_size: [ 4, 2, 2 ]\n");
    require(detail::metadata(env_size, "advance").required_local == three.required_local, "execution-only required size lost");
    for (const auto* size : {"[4,2]", "[4,2,2][1,1,1]", "[0,2,2]", "[1024,2,1]"}) {
        auto bad = flags; bad.replace(bad.find("[4,2,2]"), 7, size);
        reject([&] { detail::metadata(bad, "advance"); });
    }
    for (const auto* field : {"has_global_atomics: true", "require_disable_eufusion: true"}) {
        auto bad = flags; bad.replace(bad.find(field), std::strlen(field), std::string(field).substr(0, std::string(field).find(':')) + ": 1");
        reject([&] { detail::metadata(bad, "advance"); });
    }
    reject([&] { detail::workgroup_threads({4,0,2}, {1,1,1}); });
    require(detail::workgroup_threads({3,1,1}, {1,1,1}) == 3, "non-power-of-two local size rejected");
    reject([&] { detail::workgroup_threads({1024,2,1}, {1,1,1}); });
    reject([&] { detail::workgroup_threads({1,1,1}, {65536,65536,1}); });
    words.clear(); state = {};
    detail::record_dispatch(words, three, cross, {4,2,2}, {3,5,7}, 0x1000000, 4096, 0x2000000, 4096, &state);
    const auto three_walker = std::find(words.begin(), words.end(), uint32_t{0x72080025});
    require(three_walker != words.end(), "3D walker missing");
    std::memcpy(&w, &*three_walker, sizeof(w));
    require(w.getLocalXMaximum() == 3 && w.getLocalYMaximum() == 1 && w.getLocalZMaximum() == 1 &&
            w.getThreadGroupIdXDimension() == 3 && w.getThreadGroupIdYDimension() == 5 && w.getThreadGroupIdZDimension() == 7 &&
            w.getExecutionMask() == 0xffff && w.getInterfaceDescriptor().getNumberOfThreadsInGpgpuThreadGroup() == 1,
            "3D local/group dimensions, SIMD mask or thread count lost");
    const auto front = std::find(words.begin(), words.end(), packet::DG2::CFE_STATE::sInit().getRawData(0));
    require(front != words.end(), "EU fusion CFE missing");
    std::memcpy(&cfe, &*front, sizeof(cfe)); require(cfe.getFusedEuDispatch(), "EU fusion disable bit is inverted on DG2");
    const auto previous = words.size(); three.disable_eu_fusion = false;
    detail::record_dispatch(words, three, cross, {4,2,2}, {3,5,7}, 0x1000000, 4096, 0x2000000, 4096, &state);
    const auto restored = std::find(words.begin() + previous, words.end(), packet::DG2::CFE_STATE::sInit().getRawData(0));
    require(restored != words.end(), "EU fusion transition reused stale CFE state");
    std::memcpy(&cfe, &*restored, sizeof(cfe)); require(!cfe.getFusedEuDispatch(), "EU fusion was not restored");
    three.payload.push_back({"global_size", "", "", 64, 12, UINT32_MAX, 0});
    three.payload.push_back({"group_count", "", "", 76, 12, UINT32_MAX, 0}); three.cross_bytes = 96;
    const auto three_cross = detail::pack_arguments(three, {4,2,2}, {3,5,7}, {Argument::value(va), Argument::value(delta)}, resident);
    const std::array<uint32_t,3> local3{4,2,2}, global3{12,10,14}, groups3{3,5,7};
    require(!std::memcmp(three_cross.data(), local3.data(), 12) && !std::memcmp(three_cross.data()+64, global3.data(), 12) &&
            !std::memcmp(three_cross.data()+76, groups3.data(), 12), "3D hidden local/global/group payload lost");
    reject([&] { detail::pack_arguments(three, {8,2,1}, {3,5,7}, {Argument::value(va), Argument::value(delta)}, resident); });
    for (const uint32_t simd : {8u, 16u, 32u}) {
        auto software = three; software.simd = simd; software.local_id_bytes = 3 * std::max(16u, simd) * 2;
        const std::array<uint32_t,3> shape{37,3,1};
        const auto data = detail::indirect_payload(software, std::vector<uint8_t>(software.cross_bytes), shape);
        const auto prefix = software.cross_bytes - std::min(software.inline_bytes, software.cross_bytes);
        const auto count = detail::workgroup_threads(shape, {1,1,1});
        for (uint32_t i = 0; i < ((count + simd - 1) / simd) * simd; ++i)
            for (size_t d = 0; d < 3; ++d) {
                uint16_t value = 0xffff;
                std::memcpy(&value, data.data() + prefix + i / simd * software.local_id_bytes +
                    (d * std::max(16u, simd) + i % simd) * 2, 2);
                const uint16_t expected = i >= count ? 0 : d == 0 ? i % 37 : d == 1 ? i / 37 : 0;
                require(value == expected, "software SIMD local-ID layout or inactive-lane padding mismatch");
            }
        words.clear();
        detail::record_dispatch(words, software, std::vector<uint8_t>(software.cross_bytes), shape, {2,1,1},
                                0x1000000, 4096, 0x2000000, 4096);
        const auto at = std::find(words.begin(), words.end(), uint32_t{0x72080025});
        require(at != words.end(), "software local-ID walker missing");
        std::memcpy(&w, &*at, sizeof(w));
        require(!w.getGenerateLocalId() && !w.getEmitLocalId() &&
                w.getInterfaceDescriptor().getKernelStartPointer() == 0 && w.getIndirectDataLength() == data.size(),
                "software local IDs did not select the kernel load prologue/payload");
    }
    for (const uint32_t layout : {0u, 1u, 8u, 16u, 32u}) for (const uint32_t inline_bytes : {0u, 32u, 128u}) {
        detail::RecordedImage entry;
        entry.kernel = three;
        entry.kernel.packed_local_ids = layout == 1;
        entry.kernel.simd = layout ? layout : 32;
        entry.kernel.local_id_bytes = layout == 1 ? 32 : layout ? 3 * std::max(16u, layout) * 2 : 0;
        entry.kernel.inline_bytes = inline_bytes;
        std::vector<uint8_t> changing_cross(entry.kernel.cross_bytes);
        for (const std::array<uint32_t, 3> shape : {std::array<uint32_t, 3>{4,2,2}, {4,2,2}, {8,2,1}, {8,2,1},
                                                  {37,3,1}, {37,3,1}, {256,1,1}, {256,1,1}}) {
            for (auto& byte : changing_cross) ++byte;
            auto payload = detail::indirect_payload(entry, changing_cross, shape);
            require(payload == detail::indirect_payload(entry.kernel, changing_cross, shape),
                    "cached local-ID payload differs from full generation");
            entry.last_indirect = std::move(payload); entry.last_local = shape;
            reject([&] { detail::indirect_payload(entry, std::vector<uint8_t>(changing_cross.size() - 1), shape); });
        }
        reject([&] { detail::indirect_payload(entry, changing_cross, {0,1,1}); });
    }
    std::puts("PASS bounded ELF/ze_info, ABI rejection, actual dispatch encoder and inline/indirect payload; CPU only");
}
void inspect(const std::string& path) {
    for (const auto* name : {"rms_norm", "gate_up", "swiglu", "advance"}) {
        const auto k = detail::load(path, name);
        std::printf("ABI %s SIMD%u GRF%u SLM%u barrier%u cross%u ISA%zu\n", name, k.simd, k.grf, k.slm, k.barriers, k.cross_bytes, k.isa.size());
    }
}
void inspect_pack(const std::filesystem::path& root) {
    require(std::filesystem::is_directory(root), "native pack must be a directory");
    std::vector<std::filesystem::path> indices;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root))
        if (entry.is_regular_file() && entry.path().filename() == "index.txt") indices.push_back(entry.path());
    std::sort(indices.begin(), indices.end());
    size_t supported = 0, unsupported = 0;
    for (const auto& index : indices) {
        std::ifstream stream(index);
        require(bool(stream), "cannot open pack index");
        for (std::string line; std::getline(stream, line);) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;
            const auto tab = line.find('\t');
            require(tab != line.npos && tab && tab + 1 < line.size(), "malformed pack index entry");
            const std::filesystem::path file = line.substr(0, tab);
            require(file == file.filename() && file != "." && file != "..", "pack index path must be a filename");
            if (file.extension() != ".zebin") continue;
            const auto image = index.parent_path() / file;
            const auto name = line.substr(tab + 1);
            try {
                const auto k = detail::load(image.string(), name);
                std::printf("SUPPORTED\t%s\t%s\tSIMD%u GRF%u SLM%u cross%u\n",
                            image.string().c_str(), name.c_str(), k.simd, k.grf, k.slm, k.cross_bytes);
                ++supported;
            } catch (const std::exception& error) {
                std::printf("UNSUPPORTED\t%s\t%s\t%s\n", image.string().c_str(), name.c_str(), error.what());
                ++unsupported;
            }
        }
        require(stream.eof(), "incomplete pack index read");
    }
    require(supported + unsupported > 0, "pack contains no native exports");
    std::printf("INSPECT supported=%zu unsupported=%zu; CPU metadata validation only\n", supported, unsupported);
}
std::pair<std::string, std::string> find_native(const std::filesystem::path& pack, const char* unit, const char* pattern) {
        std::ifstream index(pack / unit / "index.txt");
        require(bool(index), "missing Strata native index");
        std::pair<std::string, std::string> found;
        for (std::string line; std::getline(index, line);) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            const auto tab = line.find('\t');
            require(tab != line.npos, "malformed Strata index");
            if (line.substr(tab + 1).find(pattern) == line.npos) continue;
            const auto file = std::filesystem::path(line.substr(0, tab));
            require(file == file.filename() && file.extension() == ".zebin" && found.first.empty(), "ambiguous or invalid Strata native export");
            found = {(pack / unit / file).string(), line.substr(tab + 1)};
        }
        require(index.eof() && !found.first.empty(), "required Strata kernel not found or incomplete index");
        return found;
}
void strata_gate(const std::filesystem::path& pack) {
    const auto gate_kernel = find_native(pack, "elementwise", "gdn_gate_kernel_");
    const auto copy_kernel = find_native(pack, "elementwise", "copy_from_mapped_kernel_");
    constexpr int64_t heads = 37, n = heads * 19;
    constexpr size_t guard = 16;
    Runtime r;
    const auto alpha = r.allocate((n + guard) * 4), dt = r.allocate((heads + guard) * 4);
    const auto ssm = r.allocate((heads + guard) * 4), gate = r.allocate((n + guard) * 4), copied = r.allocate((n + guard) * 4);
    r.append(gate_kernel.first, gate_kernel.second, 256, uint32_t((n + 255) / 256),
             {Argument::value(alpha.gpu), Argument::value(dt.gpu), Argument::value(ssm.gpu), Argument::value(gate.gpu), Argument::value(n), Argument::value(heads)});
    r.append(copy_kernel.first, copy_kernel.second, 256, uint32_t((n + 255) / 256),
             {Argument::value(copied.gpu), Argument::value(gate.gpu), Argument::value(n)});
    r.close();
    auto floats = [](const Allocation& a) { return static_cast<float*>(a.data); };
    std::vector<float> expected(size_t(n + guard), -12345.0f);
    double max_error = 0;
    for (uint32_t pass = 0; pass < 24; ++pass) {
        for (auto a : {alpha, dt, ssm, gate, copied}) {
            r.cpu_access(a, true); std::fill_n(floats(a), a.bytes / 4, -12345.0f);
        }
        for (int64_t h = 0; h < heads; ++h) {
            floats(dt)[h] = float(h % 7 - 3) / 4;
            floats(ssm)[h] = -0.25f - float(h % 5) / 8;
        }
        for (int64_t i = 0; i < n; ++i) {
            floats(alpha)[i] = float((i + pass) % 127 - 63) / 2;
            const auto x = floats(alpha)[i] + floats(dt)[i % heads];
            expected[size_t(i)] = (x > 20 ? x : std::log1p(std::exp(x))) * floats(ssm)[i % heads];
        }
        r.submit(); r.wait(); r.cpu_access(gate, false); r.cpu_access(copied, false);
        for (int64_t i = 0; i < n; ++i)
            require(floats(alpha)[i] == float((i + pass) % 127 - 63) / 2, "Strata gate modified alpha input");
        for (int64_t h = 0; h < heads; ++h)
            require(floats(dt)[h] == float(h % 7 - 3) / 4 && floats(ssm)[h] == -0.25f - float(h % 5) / 8,
                    "Strata gate modified parameter input");
        for (size_t i = 0; i < size_t(n); ++i) {
            const auto value = floats(gate)[i];
            max_error = std::max(max_error, double(std::abs(value - expected[i])));
            require(std::isfinite(value) && std::abs(value - expected[i]) <= 2e-4f * (1 + std::abs(expected[i])), "Strata native GDN gate mismatch");
            require(floats(copied)[i] == value, "Strata native dependent copy mismatch");
        }
        for (auto a : {alpha, dt, ssm, gate, copied}) {
            const auto length = a.handle == dt.handle || a.handle == ssm.handle ? heads : n;
            for (size_t i = size_t(length); i < a.bytes / 4; ++i)
                require(floats(a)[i] == -12345.0f, "Strata native input/output guard changed");
        }
    }
    std::printf("PASS 24 Strata native GDN-gate/copy pipelines, 703 elements, dynamic input, initialized/zeroed constants and guards; max_abs_error=%.9g\n", max_error);
}
void gpu_transfers(const std::string& module) {
    for (const char* name : {"orca_copy", "orca_fill", "orca_fill_wide"}) {
        const auto k = detail::load(module, name);
        require(!k.private_bytes && !k.scratch_bytes[0] && !k.scratch_bytes[1], "memory kernels must not spill to scratch/private memory");
    }
    Runtime r;
    const auto first = r.allocate_device(2 * 1024 * 1024 + 128), second = r.allocate_device(first.bytes);
    const auto upload = r.allocate(first.bytes), download = r.allocate(first.bytes);
    const auto source_back = r.allocate(first.bytes), host_copy = r.allocate(first.bytes);
    r.copy(0, 0, 0); r.fill(0, nullptr, 0, 0);
    r.copy(first.gpu, first.gpu, first.bytes);
    reject([&] { r.copy(first.gpu, upload.gpu, 1); });
    uint8_t byte = 7;
    reject([&] { r.fill(first.gpu, &byte, 1, 1); });
    reject([&] { r.set_memory_module(module + ".missing"); });
    r.set_memory_module(module);
    reject([&] { r.set_memory_module(module); });
    reject([&] { r.copy(first.gpu + 1, first.gpu, 64); });
    reject([&] { r.copy(first.gpu, first.gpu + 1, 64); });
    reject([&] { r.copy(first.gpu + first.bytes - 1, upload.gpu, 2); });
    reject([&] { r.copy(first.gpu, upload.gpu + upload.bytes - 1, 2); });
    reject([&] { r.copy(first.gpu, 0, 1); });
    reject([&] { r.copy(first.gpu, upload.gpu, SIZE_MAX); });
    reject([&] { r.fill(first.gpu, nullptr, 1, 1); });
    for (size_t bad : {size_t{0}, size_t{3}, size_t{256}, SIZE_MAX})
        reject([&] { r.fill(first.gpu, &byte, bad, 1); });
    reject([&] { r.fill(first.gpu + first.bytes - 1, &byte, 1, 2); });
    size_t cases = 0, replays = 0;
    std::array<unsigned, 8> patterns{};
    constexpr uint8_t guard = 0xcd;
    for (size_t bytes : {size_t{0}, size_t{1}, size_t{2}, size_t{3}, size_t{7}, size_t{15}, size_t{16}, size_t{17},
                         size_t{31}, size_t{63}, size_t{127}, size_t{128}, size_t{129}, size_t{255}, size_t{256}, size_t{257},
                         size_t{4095}, size_t{4096}, size_t{4097}, size_t{65535}, size_t{65536}, size_t{65537}, size_t{2 * 1024 * 1024 + 37}}) {
        for (auto offsets : {std::pair<size_t, size_t>{0, 0}, {16, 32}, {1, 3}, {3, 1}, {15, 15}, {16, 17}, {1, 1}, {2, 2}, {3, 3}, {4, 4}, {5, 5}, {6, 6}, {7, 7}, {8, 8}, {9, 9}, {10, 10}, {11, 11}, {12, 12}, {13, 13}, {14, 14}}) {
            if (!offsets.first && !offsets.second) { std::printf("TRANSFER bytes=%zu\n", bytes); std::fflush(stdout); }
            if (cases) r.reset();
            const auto [src, dst] = offsets;
            const size_t pattern_bytes = size_t{1} << (cases % 8);
            ++patterns[cases % 8];
            std::array<uint8_t, 128> pattern{};
            for (size_t i = 0; i < pattern_bytes; ++i) pattern[i] = uint8_t(i * 19 + cases * 31);
            const auto expected_pattern = pattern;
            const auto fill_offset = dst + bytes / 4, fill_bytes = bytes / 2;
            r.fill(first.gpu, &guard, 1, first.bytes);
            r.fill(second.gpu, &guard, 1, second.bytes);
            r.copy(first.gpu + src, upload.gpu + src, bytes);
            r.copy(second.gpu + dst, first.gpu + src, bytes);
            r.fill(second.gpu + fill_offset, pattern.data(), pattern_bytes, fill_bytes);
            // Stack patterns may be changed/destroyed before execution; recording must have captured them.
            pattern.fill(0);
            r.copy(download.gpu, second.gpu, second.bytes);
            r.copy(source_back.gpu, first.gpu, first.bytes);
            r.copy(host_copy.gpu, download.gpu, download.bytes);
            r.close();
            if (!cases) {
                reject([&] { r.copy(first.gpu, upload.gpu, 1); });
                reject([&] { r.fill(first.gpu, &guard, 1, 1); });
                reject([&] { r.set_memory_module(module); });
            }
            for (unsigned pass = 0; pass < 2; ++pass) {
                const auto input = [&](size_t i) { return uint8_t(i * 73u ^ (i >> 7) ^ (i >> 17) * 19u ^ cases * 37u ^ pass * 101u); };
                for (auto a : {upload, download, source_back, host_copy}) {
                    r.cpu_access(a, true); std::memset(a.data, 0xe7, a.bytes);
                }
                for (size_t i = 0; i < upload.bytes; ++i) static_cast<uint8_t*>(upload.data)[i] = input(i);
                r.submit(); r.wait(); r.check_scratch_guards();
                for (auto a : {upload, download, source_back, host_copy}) r.cpu_access(a, false);
                for (size_t i = 0; i < first.bytes; ++i) {
                    const uint8_t source_value = i >= src && i - src < bytes ? input(i) : guard;
                    uint8_t value = i >= dst && i - dst < bytes ? input(src + i - dst) : guard;
                    if (i >= fill_offset && i - fill_offset < fill_bytes) value = expected_pattern[(i - fill_offset) & (pattern_bytes - 1)];
                    require(static_cast<uint8_t*>(upload.data)[i] == input(i), "GPU copy modified its upload source");
                    require(static_cast<uint8_t*>(source_back.data)[i] == source_value, "H2D/D2H source data or full guards mismatch");
                    require(static_cast<uint8_t*>(download.data)[i] == value && static_cast<uint8_t*>(host_copy.data)[i] == value,
                            "D2D/fill/D2H/H2H data or full guards mismatch");
                }
                ++replays;
            }
            ++cases;
        }
    }
    require(std::all_of(patterns.begin(), patterns.end(), [](unsigned n) { return n > 0; }), "fill pattern coverage incomplete");
    std::printf("PASS %zu native memory replays / %zu recordings: H2D/D2D/D2H/H2H, 0..2097189 bytes, aligned/unaligned offsets, captured 1..128-byte patterns, exact full buffers/guards and invalid ranges/overlaps; no scratch/private memory\n", replays, cases);
}
void strata_device(const std::string& memory_module) {
    for (const size_t requested : {size_t{1}, size_t{16 * 1024 * 1024 + 1}, size_t{320 * 1024 * 1024 + 1}}) {
        Runtime r;
        r.set_memory_module(memory_module);
        reject([&] { r.allocate_device(0); });
        reject([&] { r.allocate_device(SIZE_MAX); });
        const auto first = r.allocate_device(requested), second = r.allocate_device(requested);
        require(first.device_local && second.device_local && !first.data && !second.data && !first.imported &&
                first.bytes >= requested && first.bytes == second.bytes && !(first.bytes & 65535), "device-local allocation properties mismatch");
        reject([&] { r.cpu_access(first, true); });
        const auto upload = r.allocate(first.bytes), download = r.allocate(first.bytes);
        r.copy(first.gpu, upload.gpu, first.bytes);
        r.copy(second.gpu, first.gpu, first.bytes);
        r.copy(download.gpu, second.gpu, first.bytes);
        r.close();
        reject([&] { r.allocate_device(65536); });
        auto* input = static_cast<uint32_t*>(upload.data); auto* result = static_cast<uint32_t*>(download.data);
        for (uint32_t pass = 0; pass < 4; ++pass) {
            r.cpu_access(upload, true); r.cpu_access(download, true);
            const auto pattern = [&](size_t i) { return uint32_t(0x3f000000u | ((uint32_t(i) * 1664525u + pass * 1013904223u) & 0x7fffffu)); };
            for (size_t i = 0; i < first.bytes / 4; ++i) { input[i] = pattern(i); result[i] = 0xdeadbeef; }
            r.submit(); r.wait(); r.cpu_access(upload, false); r.cpu_access(download, false);
            for (size_t i = 0; i < first.bytes / 4; ++i)
                require(result[i] == pattern(i) && input[i] == pattern(i), "device-local upload/D2D/download roundtrip mismatch");
        }
        std::printf("PASS device-local memory bytes=%zu, 4 complete GPU H2D/D2D/D2H replays with exact full-buffer comparison\n", first.bytes);
    }
}
void strata_gemv(const std::filesystem::path& pack, bool device = false, const std::string& memory_module = {}) {
    const auto kernel = find_native(pack, "bf16_gemv", "bf16_gemv_split_kernel_");
    constexpr int64_t width = 1027, rows = 19;
    Runtime r;
    if (device) r.set_memory_module(memory_module);
    const auto alloc = [&](size_t bytes) { return device ? r.allocate_device(bytes) : r.allocate(bytes); };
    const auto gx = alloc(size_t(width + 16) * 2), gm = alloc(size_t(width * rows + 16) * 2), go = alloc(size_t(rows + 16) * 4);
    const auto x = device ? r.allocate(gx.bytes) : gx, matrix = device ? r.allocate(gm.bytes) : gm;
    const auto output = device ? r.allocate(go.bytes) : go;
    auto encode = [](float f) { uint32_t u; std::memcpy(&u, &f, 4); return uint16_t(u >> 16); };
    auto decode = [](uint16_t u) { const uint32_t bits = uint32_t(u) << 16; float f; std::memcpy(&f, &bits, 4); return f; };
    auto* input = static_cast<uint16_t*>(x.data); auto* weights = static_cast<uint16_t*>(matrix.data);
    auto* result = static_cast<float*>(output.data);
    r.cpu_access(matrix, true); std::fill_n(weights, matrix.bytes / 2, uint16_t{0xbeef});
    for (int64_t i = 0; i < rows * width; ++i) weights[i] = encode(float(i * 7 % 23 - 11) / 16);
    double max_error = 0;
    std::vector<float> expected(size_t(rows), 0);
    for (const int32_t threads : {64, 256}) {
        if (threads != 64) r.reset();
        if (device) { r.copy(gx.gpu, x.gpu, gx.bytes); r.copy(gm.gpu, matrix.gpu, gm.bytes); r.copy(go.gpu, output.gpu, go.bytes); }
        r.append(kernel.first, kernel.second, uint32_t(threads), uint32_t(rows),
                 {Argument::value(gx.gpu), Argument::value(gm.gpu), Argument::value(go.gpu),
                  Argument::value(width), Argument::value(rows), Argument::value(threads), Argument::local(size_t(threads) * 4)});
        if (device) { r.copy(x.gpu, gx.gpu, gx.bytes); r.copy(matrix.gpu, gm.gpu, gm.bytes); r.copy(output.gpu, go.gpu, go.bytes); }
        r.close();
        for (uint32_t pass = 0; pass < 24; ++pass) {
            r.cpu_access(x, true); r.cpu_access(output, true);
            std::fill_n(input, x.bytes / 2, uint16_t{0xbeef}); std::fill_n(result, output.bytes / 4, -12345.0f);
            for (int64_t i = 0; i < width; ++i) input[i] = encode(float((i + pass) % 17 - 8) / 8);
            std::fill(expected.begin(), expected.end(), 0.0f);
            for (int64_t row = 0; row < rows; ++row)
                for (int64_t i = 0; i < width; ++i)
                    expected[size_t(row)] += decode(input[i]) * decode(weights[row * width + i]);
            r.submit(); r.wait(); r.cpu_access(output, false);
            if (device) { r.cpu_access(x, false); r.cpu_access(matrix, false); }
            for (int64_t row = 0; row < rows; ++row) {
                max_error = std::max(max_error, double(std::abs(result[row] - expected[size_t(row)])));
                require(std::isfinite(result[row]) && std::abs(result[row] - expected[size_t(row)]) <= 1e-5f, "Strata native dynamic-SLM BF16 GEMV mismatch");
            }
            for (int64_t i = 0; i < width; ++i) require(input[i] == encode(float((i + pass) % 17 - 8) / 8), "GEMV modified input");
            for (int64_t i = 0; i < rows * width; ++i) require(weights[i] == encode(float(i * 7 % 23 - 11) / 16), "GEMV modified matrix");
            for (size_t i = rows; i < output.bytes / 4; ++i) require(result[i] == -12345.0f, "GEMV output guard changed");
            for (size_t i = width; i < x.bytes / 2; ++i) require(input[i] == 0xbeef, "GEMV input guard changed");
            for (size_t i = rows * width; i < matrix.bytes / 2; ++i) require(weights[i] == 0xbeef, "GEMV matrix guard changed");
        }
    }
    std::printf("PASS 48 Strata native BF16 GEMV replays, 19x1027, local64/local256, dynamic SLM, changed inputs and guards; device_local=%u max_abs_error=%.9g\n", unsigned(device), max_error);
}
// Inflate only the declared memory requirement; ISA and argument ABI stay byte-identical.
std::string larger_thread_fixture(const std::pair<std::string, std::string>& original,
                                  const std::filesystem::path& target, bool scratch) {
    auto elf = detail::read_zebin(original.first);
    bool changed = false;
    for (size_t i = 1; i < elf.sections.size(); ++i) if (elf.name(i) == ".ze_info") {
        const auto& section = elf.sections[i];
        std::string info(reinterpret_cast<const char*>(elf.bytes.data() + section.offset), size_t(section.size));
        const auto name = info.find(original.second);
        require(name != info.npos, "fixture kernel metadata missing");
        const auto limit = info.find("\n  - name:", name);
        const auto buffers = info.find("per_thread_memory_buffers:", name);
        require(buffers != info.npos && (limit == info.npos || buffers < limit), "fixture memory descriptor missing");
        const auto size = info.find("size:", buffers);
        require(size != info.npos && (limit == info.npos || size < limit), "fixture memory size missing");
        const auto begin = info.find_first_not_of(" \t", size + 5);
        const auto end = info.find_first_not_of("0123456789", begin);
        const auto value = std::stoul(info.substr(begin, end - begin));
        const auto larger = std::to_string(value * 2);
        require(larger.size() == end - begin && !changed, "fixture size edit would move ELF sections");
        std::copy(larger.begin(), larger.end(), elf.bytes.begin() + section.offset + begin);
        changed = true;
    }
    require(changed, "fixture metadata was not enlarged");
    std::ofstream file(target, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(elf.bytes.data()), std::streamsize(elf.bytes.size())); file.close();
    require(bool(file), "write enlarged thread fixture");
    const auto before = detail::load(original.first, original.second), after = detail::load(target.string(), original.second);
    require(after.isa == before.isa && (scratch ? after.scratch_bytes[0] == 2 * before.scratch_bytes[0] :
            after.private_bytes == 2 * before.private_bytes), "enlarged fixture changed ISA or unexpected thread ABI");
    return target.string();
}
void strata_thread_trim(const std::filesystem::path& pack, const std::string& memory) {
    const auto spill = find_native(pack, "cvec", "cvec_kernel_");
    const auto priv = find_native(pack, "dequant_bf16", "dpct_kernel_scalarILi11EEtEE");
    require(detail::load(spill.first, spill.second).scratch_bytes[0] == 2048 &&
            detail::load(priv.first, priv.second).private_bytes == 512, "thread trim fixtures changed");
    struct Temporary {
        std::filesystem::path path;
        Temporary() {
            const auto candidate = std::filesystem::temp_directory_path() / ("orca-thread-trim-" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            require(std::filesystem::create_directory(candidate), "create exclusive thread fixture directory");
            path = candidate;
        }
        ~Temporary() { std::error_code e; for (auto name : {"scratch.zebin", "private.zebin"}) std::filesystem::remove(path / name, e); std::filesystem::remove(path, e); }
    } temporary;
    const auto grown_spill = larger_thread_fixture(spill, temporary.path / "scratch.zebin", true);
    const auto grown_priv = larger_thread_fixture(priv, temporary.path / "private.zebin", false);
    for (bool local : {false, true}) {
        Runtime r;
        r.set_memory_module(memory, local, local);
        const auto main = r.default_queue(), other = r.create_queue();
        constexpr int32_t n = 4096, hc = 3, mode = 0;
        constexpr int64_t layer = 1, ld = int64_t(n) * hc, bo_ld = n, inj_ld = hc;
        constexpr int64_t rows = 19, row0 = 1, row_bytes = 220, groups = 16;
        constexpr size_t guard = 64;
        const uint8_t write = 0;
        // Reservation sizes are a platform property (Windows native Host rounds large spans to 2MiB).
        const auto reserved = [&](size_t bytes) {
            auto allocation = local ? r.allocate_device(bytes) : r.allocate(bytes);
            const auto size = allocation.bytes; r.release(allocation); return uint64_t(size);
        };
        const auto expected = reserved(size_t(2048) * detail::thread_memory_slots + 2 * detail::thread_memory_guard) +
            reserved(size_t(512) * detail::thread_memory_slots + 2 * detail::thread_memory_guard) + reserved(4096);
        const auto residual = r.allocate(size_t(ld) * 4 + 2 * guard), direction = r.allocate(3 * n * 4 + 2 * guard);
        const auto scales = r.allocate(3 * 4 + 2 * guard), on = r.allocate(4 + 2 * guard);
        const auto blocks = r.allocate(size_t((rows + row0) * row_bytes) + 2 * guard);
        const auto output = r.allocate(size_t(rows * 512) * 2 + 2 * guard);
        const auto va = [guard](const Allocation& a) { return a.gpu + guard; };
        const auto record = [&](const Queue& q, bool grown) {
            r.begin(q);
            const auto x = va(residual), d = va(direction), y = va(output), b = va(blocks);
            struct { uint64_t dir, scales, on; } tables{d, va(scales), va(on)};
            r.append(grown ? grown_spill : spill.first, spill.second, 256, hc,
                {Argument::value(x), Argument::value(tables), Argument::value(mode), Argument::value(layer),
                 Argument::value(n), Argument::value(hc), Argument::value(ld), Argument::value(d),
                 Argument::value(bo_ld), Argument::value(d), Argument::value(inj_ld), Argument::value(write)});
            r.append(grown ? grown_priv : priv.first, priv.second, 256, uint32_t((rows * groups + 255) / 256),
                {Argument::value(b), Argument::value(row_bytes), Argument::value(row0), Argument::value(rows),
                 Argument::value(groups), Argument::value(y)});
            reject([&] { r.trim_thread_memory(); }); // Nonempty active recording.
            return r.finish();
        };
        const auto replay = [&](const Commands& list, const Queue& q) {
            for (auto a : {residual, direction, scales, on, blocks, output}) {
                r.cpu_access(a, true); std::memset(a.data, 0xa7, a.bytes);
            }
            auto* values = reinterpret_cast<float*>(static_cast<uint8_t*>(residual.data) + guard);
            std::fill_n(values, size_t(ld), 2.0f);
            std::fill_n(reinterpret_cast<float*>(static_cast<uint8_t*>(direction.data) + guard), 3 * n, 1.0f / 256);
            std::fill_n(reinterpret_cast<float*>(static_cast<uint8_t*>(scales.data) + guard), 3, 1.0f / 8);
            const int32_t enabled = 1; std::memcpy(static_cast<uint8_t*>(on.data) + guard, &enabled, 4);
            for (int64_t row = 0; row < rows + row0; ++row) for (int block = 0; block < 2; ++block) {
                auto* b = static_cast<uint8_t*>(blocks.data) + guard + row * row_bytes + block * 110;
                std::memset(b, 0xff, 32); std::memset(b + 32, 0x55, 64);
                std::memset(b + 96, 0x77, 8); std::memset(b + 104, 0, 4);
                const uint16_t half = 0x3c00; std::memcpy(b + 108, &half, 2);
            }
            std::vector<std::vector<uint8_t>> before;
            for (auto a : {residual, direction, scales, on, blocks, output})
                before.emplace_back(static_cast<uint8_t*>(a.data), static_cast<uint8_t*>(a.data) + a.bytes);
            for (size_t j = 0; j < size_t(ld); ++j) {
                const float value = 2.0f * 127.0f / 128;
                std::memcpy(before[0].data() + guard + j * 4, &value, 4);
            }
            const uint16_t bf16 = 0xc1c8; // (7 - 32) * 1 = -25.
            for (size_t j = 0; j < size_t(rows * 512); ++j) std::memcpy(before[5].data() + guard + j * 2, &bf16, 2);
            r.wait(r.submit(list, q));
            size_t index = 0;
            for (auto a : {residual, direction, scales, on, blocks, output}) {
                r.cpu_access(a, false); require(!std::memcmp(a.data, before[index++].data(), a.bytes), "trim changed exact result/input/full guards");
            }
            r.check_scratch_guards();
        };
        auto old = record(main, false), preserved = record(other, false), current = record(main, true);
        const auto held = r.allocated_bytes();
        require(r.trim_thread_memory() == 0 && r.allocated_bytes() == held, "trim freed a pool held by an idle recorded list");
        replay(old, main); replay(preserved, other); replay(current, main);
        const auto gate = r.create_gate();
        r.begin(main); r.depend_on(gate); r.barrier();
        r.close(); reject([&] { r.trim_thread_memory(); }); // Closed implicit list.
        auto blocked = r.finish(); const auto pending = r.submit(blocked, main);
        try { reject([&] { r.trim_thread_memory(); }); } catch (...) { gate.signal(); r.wait(pending); throw; }
        gate.signal(); r.wait(pending); r.discard(blocked);
        r.discard(old);
        const auto before = r.allocated_bytes();
        const auto freed = r.trim_thread_memory();
        std::printf("TRIM_BYTES local=%u expected=%llu reclaimed=%llu allocation_delta=%llu\n", unsigned(local),
            static_cast<unsigned long long>(expected), static_cast<unsigned long long>(freed), static_cast<unsigned long long>(before - r.allocated_bytes()));
        require(freed == expected && r.allocated_bytes() == before - freed, "trim did not reclaim exactly the obsolete pools/descriptor");
        require(r.trim_thread_memory() == 0, "repeated trim must be idempotent");
        for (int pass = 0; pass < 4; ++pass) { replay(current, main); replay(preserved, other); }
        r.discard(current); r.discard(preserved);
        require(r.trim_thread_memory() == 0, "trim freed current bindings after their lists were discarded");
        auto again = record(main, true); replay(again, main); r.discard(again);
        std::printf("PASS thread trim local=%u: exact reclaimed_bytes=%llu; retained lists, other queue/current bindings, empty/closed/pending rejection, idempotence, re-recording, 12 exact scratch/private replays and all guards\n", unsigned(local), static_cast<unsigned long long>(freed));
    }
}

void strata_scratch(const std::filesystem::path& pack) {
    const auto kernel = find_native(pack, "cvec", "cvec_kernel_");
    const auto metadata = detail::load(kernel.first, kernel.second);
    require(metadata.scratch_bytes[0] == 2048, "fixture must exercise the actual Cvec spill ABI");
    constexpr int32_t n = 4096, hc = 257;
    constexpr int64_t layer = 1, r_ld = int64_t(n) * hc, bo_ld = n, inj_ld = hc;
    constexpr size_t guard = 16;
    Runtime r;
    const auto residual = r.allocate(size_t(r_ld + 2 * guard) * 4), direction = r.allocate((3 * n + 2 * guard) * 4);
    const auto scales = r.allocate((3 + 2 * guard) * 4), on = r.allocate((1 + 2 * guard) * 4);
    const auto bo = r.allocate((n + 2 * guard) * 4), inj = r.allocate((hc + 2 * guard) * 4);
    auto ptr = [guard](const Allocation& a) { return static_cast<float*>(a.data) + guard; };
    auto va = [guard](const Allocation& a) { return a.gpu + guard * 4; };
    struct Tables { uint64_t dir, scales, on; } tables{va(direction), va(scales), va(on)};
    static_assert(sizeof(tables) == 24);
    std::vector<float> expected(residual.bytes / 4);
    double max_error = 0;
    for (int scenario = 0; scenario < 3; ++scenario) {
        if (scenario) r.reset();
        const int32_t mode = scenario == 1 ? 1 : 0;
        const uint8_t write = scenario == 2 ? 1 : 0;
        const auto residual_va = va(residual), bo_va = va(bo), inj_va = va(inj);
        r.append(kernel.first, kernel.second, 256, hc,
                 {Argument::value(residual_va), Argument::value(tables), Argument::value(mode), Argument::value(layer),
                  Argument::value(n), Argument::value(hc), Argument::value(r_ld), Argument::value(bo_va),
                  Argument::value(bo_ld), Argument::value(inj_va), Argument::value(inj_ld), Argument::value(write)});
        r.close();
        for (int pass = 0; pass < 4; ++pass) {
            for (auto a : {residual, direction, scales, on, bo, inj}) {
                r.cpu_access(a, true); std::fill_n(static_cast<float*>(a.data), a.bytes / 4, -12345.0f);
            }
            for (int j = 0; j < 3 * n; ++j) ptr(direction)[j] = float(j % 17 - 8) / 256;
            for (int j = 0; j < 3; ++j) ptr(scales)[j] = float(j + 1) / 8;
            const int32_t enabled = pass != 3;
            std::memcpy(static_cast<uint8_t*>(on.data) + guard * 4, &enabled, 4);
            for (int j = 0; j < n; ++j) ptr(bo)[j] = float((j + pass) % 13 - 6) / 16;
            for (int c = 0; c < hc; ++c) ptr(inj)[c] = float(hc) * float((c + pass) % 7 - 3) / 4;
            for (int64_t j = 0; j < r_ld; ++j) ptr(residual)[j] = float((j * 7 + pass * 3) % 31 - 15) / 32;
            std::copy_n(static_cast<float*>(residual.data), expected.size(), expected.begin());
            for (int c = 0; c < hc; ++c) {
                const float w = write ? 2 / (1 + std::exp(-ptr(inj)[c] / float(hc))) : 0;
                double dot = 0;
                auto* result = expected.data() + guard + int64_t(c) * n;
                for (int j = 0; j < n; ++j) {
                    if (write) result[j] = std::fma(ptr(bo)[j], w, result[j]);
                    dot += double(result[j]) * ptr(direction)[layer * n + j];
                }
                const float scaled_dot = float(dot) * ptr(scales)[layer];
                for (int j = 0; j < n; ++j) if (enabled) {
                    const float d = ptr(direction)[layer * n + j];
                    result[j] = mode == 0 ? std::fma(-scaled_dot, d, result[j]) : result[j] + d;
                }
            }
            std::vector<std::vector<uint8_t>> inputs;
            for (auto a : {direction, scales, on, bo, inj})
                inputs.emplace_back(static_cast<uint8_t*>(a.data), static_cast<uint8_t*>(a.data) + a.bytes);
            r.submit(); reject([&] { r.check_scratch_guards(); }); r.wait();
            r.cpu_access(residual, false); r.check_scratch_guards();
            for (size_t j = 0; j < expected.size(); ++j) {
                const float value = static_cast<float*>(residual.data)[j];
                const bool output = j >= guard && j < guard + size_t(r_ld);
                if (!output) require(value == expected[j], "Cvec output prefix/padding guard changed");
                else {
                    max_error = std::max(max_error, double(std::abs(value - expected[j])));
                    require(std::isfinite(value) && std::abs(value - expected[j]) <= 2e-4f * (1 + std::abs(expected[j])), "native Cvec spill result mismatch");
                }
            }
            size_t i = 0;
            for (auto a : {direction, scales, on, bo, inj}) {
                r.cpu_access(a, false);
                require(!std::memcmp(a.data, inputs[i++].data(), a.bytes), "Cvec modified input or input guards");
            }
        }
    }
    std::printf("PASS 12 Strata Cvec replays, n4096/hc257, project/add/fused-write, changed enable/input, scratch2048, aggregate pointer ABI and all guards; max_abs_error=%.9g\n", max_error);
}
void strata_private(const std::filesystem::path& pack) {
    const auto kernel = find_native(pack, "dequant_bf16", "dpct_kernel_scalarILi11EEtEE");
    const auto metadata = detail::load(kernel.first, kernel.second);
    require(metadata.private_bytes == 512, "fixture must exercise Q3_K SIMD private-memory ABI");
    constexpr int64_t rows = 19, row0 = 1, row_bytes = 220, groups = 16;
    constexpr size_t guard = 64;
    Runtime r;
    const auto blocks = r.allocate(size_t((rows + row0) * row_bytes) + 2 * guard);
    const auto output = r.allocate(size_t(rows * 512) * 2 + 2 * guard);
    const uint64_t blocks_va = blocks.gpu + guard, output_va = output.gpu + guard;
    r.append(kernel.first, kernel.second, 256, uint32_t((rows * groups + 255) / 256),
             {Argument::value(blocks_va), Argument::value(row_bytes), Argument::value(row0), Argument::value(rows),
              Argument::value(groups), Argument::value(output_va)});
    r.close();
    for (int pass = 0; pass < 16; ++pass) {
        r.cpu_access(blocks, true); r.cpu_access(output, true);
        std::memset(blocks.data, 0xa7, blocks.bytes);
        std::fill_n(static_cast<uint16_t*>(output.data), output.bytes / 2, uint16_t{0xbeef});
        std::vector<uint16_t> expected(output.bytes / 2, 0xbeef);
        for (int64_t row = 0; row < rows + row0; ++row) for (int block = 0; block < 2; ++block) {
            auto* b = static_cast<uint8_t*>(blocks.data) + guard + row * row_bytes + block * 110;
            const int q = 1 + int((row + block + pass) % 3);
            const bool high = ((row + block + pass) & 1) != 0;
            const int scale = int((row * 3 + block + pass) % 3);
            const uint16_t half = scale == 0 ? 0x3800 : scale == 1 ? 0x3c00 : 0x4000;
            std::memset(b, high ? 0xff : 0, 32); std::memset(b + 32, q * 0x55, 64);
            constexpr int codes[]{7, 25, 39, 57};
            const int code = codes[(row + block + pass) % 4];
            std::memset(b + 96, (code & 15) * 0x11, 8);
            std::memset(b + 104, (code >> 4) * 0x55, 4); std::memcpy(b + 108, &half, 2);
            const float d = scale == 0 ? 0.5f : scale == 1 ? 1.0f : 2.0f;
            const float value = (d * float(code - 32)) * float(high ? q : q - 4);
            uint32_t bits = 0; std::memcpy(&bits, &value, 4);
            if (row >= row0) std::fill_n(expected.data() + guard / 2 + (row - row0) * 512 + block * 256, 256, uint16_t(bits >> 16));
        }
        const std::vector<uint8_t> input(static_cast<uint8_t*>(blocks.data), static_cast<uint8_t*>(blocks.data) + blocks.bytes);
        r.submit(); reject([&] { r.check_scratch_guards(); }); r.wait();
        r.cpu_access(output, false); r.cpu_access(blocks, false); r.check_scratch_guards();
        require(!std::memcmp(output.data, expected.data(), output.bytes), "native Q3_K private-memory BF16 output/guard mismatch");
        require(!std::memcmp(blocks.data, input.data(), blocks.bytes), "Q3_K changed packed input/guards");
    }
    std::puts("PASS 16 Strata Q3_K-to-BF16 replays, 19x512/row0=1, private512, varied nonzero signed scales per block/lane, exact analytic reference and all guards");
}
void strata_multiqueue_memory(const std::filesystem::path& pack, const std::string& memory = {},
                             bool local_thread_memory = true, bool local_isa = false) {
    const auto spill = find_native(pack, "cvec", "cvec_kernel_");
    const auto priv = find_native(pack, "dequant_bf16", "dpct_kernel_scalarILi11EEtEE");
    const auto private_info = detail::load(priv.first, priv.second);
    require(detail::load(spill.first, spill.second).scratch_bytes[0] == 2048 &&
            private_info.private_bytes == 512 && !private_info.scratch_bytes[0] && !private_info.scratch_bytes[1],
            "scratch/private/no-scratch transition fixtures changed");
    constexpr int32_t n = 4096, hc = 257, mode = 0;
    constexpr int64_t layer = 1, r_ld = int64_t(n) * hc, bo_ld = n, inj_ld = hc;
    constexpr int64_t rows = 1025, row0 = 1, row_bytes = 220, groups = 16;
    constexpr size_t guard = 64;
    const uint8_t write = 0;
    Runtime r;
    if (!memory.empty()) r.set_memory_module(memory, local_thread_memory, local_isa);
    struct Lane {
        Queue queue;
        Commands list;
        Completion completion;
        Allocation residual, direction, scales, on, blocks, output;
        std::vector<std::vector<uint8_t>> inputs;
    };
    std::array<Lane, 3> lanes;
    for (size_t i = 0; i < lanes.size(); ++i) lanes[i].queue = i ? r.create_queue() : r.default_queue();
    const auto gate = r.create_gate();
    struct Release {
        HostGate gate;
        std::promise<void> stop;
        std::future<void> watchdog;
        std::atomic<bool> expired{false};
        ~Release() {
            try { gate.signal(); stop.set_value(); } catch (...) {}
            if (watchdog.valid()) watchdog.wait();
        }
    } release{gate};
    auto stop = release.stop.get_future();
    release.watchdog = std::async(std::launch::async, [&, stop = std::move(stop)]() mutable {
        if (stop.wait_for(std::chrono::seconds(5)) != std::future_status::ready) {
            release.expired.store(true); gate.signal();
        }
    });
    r.depend_on(gate); r.barrier();
    auto blocked = r.finish();
    const auto pending = r.submit(blocked, r.default_queue());
    require(!pending.ready(), "thread-memory fixture did not retain its leading gate");
    const auto va = [guard](const Allocation& a) { return a.gpu + guard; };
    for (size_t i = 0; i < lanes.size(); ++i) {
        auto& lane = lanes[i];
        lane.residual = r.allocate(size_t(r_ld) * 4 + 2 * guard);
        lane.direction = r.allocate(3 * n * 4 + 2 * guard);
        lane.scales = r.allocate(3 * 4 + 2 * guard); lane.on = r.allocate(4 + 2 * guard);
        lane.blocks = r.allocate(size_t((rows + row0) * row_bytes) + 2 * guard);
        lane.output = r.allocate(size_t(rows * 512) * 2 + 2 * guard);
        r.begin(lane.queue);
        const auto before = r.allocated_bytes();
        const auto residual = va(lane.residual), output = va(lane.output), blocks = va(lane.blocks);
        const auto unused_bo = va(lane.direction), unused_inj = va(lane.scales);
        struct { uint64_t dir, scales, on; } tables{va(lane.direction), va(lane.scales), va(lane.on)};
        r.append(spill.first, spill.second, 256, hc,
                 {Argument::value(residual), Argument::value(tables), Argument::value(mode), Argument::value(layer),
                  Argument::value(n), Argument::value(hc), Argument::value(r_ld), Argument::value(unused_bo),
                  Argument::value(bo_ld), Argument::value(unused_inj), Argument::value(inj_ld), Argument::value(write)});
        r.append(priv.first, priv.second, 256, uint32_t((rows * groups + 255) / 256),
                 {Argument::value(blocks), Argument::value(row_bytes), Argument::value(row0), Argument::value(rows),
                  Argument::value(groups), Argument::value(output)});
        // Return to the same queue's scratch after a real no-scratch/private kernel.
        r.append(spill.first, spill.second, 256, hc,
                 {Argument::value(residual), Argument::value(tables), Argument::value(mode), Argument::value(layer),
                  Argument::value(n), Argument::value(hc), Argument::value(r_ld), Argument::value(unused_bo),
                  Argument::value(bo_ld), Argument::value(unused_inj), Argument::value(inj_ld), Argument::value(write)});
        lane.list = r.finish();
        // A serialized GPU run alone could miss aliasing: each queue must also own new writable pools.
        require(r.allocated_bytes() - before >= uint64_t(2048 + 512) * detail::thread_memory_slots +
                    4 * detail::thread_memory_guard, "independent queues reused writable thread-memory pools");
    }
    require(!release.expired.load() && !pending.ready(), "first-use thread-memory preparation waited for a blocked queue");
    gate.signal(); r.wait(pending); r.discard(blocked);
    release.stop.set_value(); release.watchdog.get();
    // First guard check must also work with a closed caller list, before any
    // diagnostic has allocated readback storage lazily.
    r.barrier(); r.close(); r.submit(); r.wait(); r.check_scratch_guards(); r.reset();
    for (int pass = 0; pass < 8; ++pass) {
        for (size_t i = 0; i < lanes.size(); ++i) {
            auto& lane = lanes[i];
            for (auto a : {lane.residual, lane.direction, lane.scales, lane.on, lane.blocks, lane.output}) {
                r.cpu_access(a, true); std::memset(a.data, 0xa7, a.bytes);
            }
            auto* residual = static_cast<float*>(lane.residual.data);
            std::fill_n(residual, lane.residual.bytes / 4, -12345.f);
            for (int c = 0; c < hc; ++c)
                std::fill_n(residual + guard / 4 + int64_t(c) * n, n, float((pass + 1) * (c % 7 - 3)) / 32);
            auto* dir = reinterpret_cast<float*>(static_cast<uint8_t*>(lane.direction.data) + guard);
            std::fill_n(dir, 3 * n, float(i + 1) / 128);
            auto* scales = reinterpret_cast<float*>(static_cast<uint8_t*>(lane.scales.data) + guard);
            std::fill_n(scales, 3, .25f);
            const int32_t enabled = 1;
            std::memcpy(static_cast<uint8_t*>(lane.on.data) + guard, &enabled, 4);
            const int q = 1 + (pass + int(i)) % 3, code = (pass + int(i)) % 2 ? 25 : 39;
            const bool high = ((pass + int(i)) & 2) != 0;
            for (int64_t row = 0; row < rows + row0; ++row) for (int block = 0; block < 2; ++block) {
                auto* b = static_cast<uint8_t*>(lane.blocks.data) + guard + row * row_bytes + block * 110;
                std::memset(b, high ? 0xff : 0, 32); std::memset(b + 32, q * 0x55, 64);
                std::memset(b + 96, (code & 15) * 0x11, 8); std::memset(b + 104, (code >> 4) * 0x55, 4);
                const uint16_t half = 0x3c00; std::memcpy(b + 108, &half, 2);
            }
            std::fill_n(static_cast<uint16_t*>(lane.output.data), lane.output.bytes / 2, uint16_t{0xbeef});
            lane.inputs.clear();
            for (auto a : {lane.direction, lane.scales, lane.on, lane.blocks})
                lane.inputs.emplace_back(static_cast<uint8_t*>(a.data), static_cast<uint8_t*>(a.data) + a.bytes);
        }
        // All three submissions precede any host wait. Correctness does not prove simultaneous execution.
        for (auto& lane : lanes) lane.completion = r.submit(lane.list, lane.queue);
        for (auto& lane : lanes) r.wait(lane.completion);
        r.begin(lanes[1].queue); r.barrier();
        r.check_scratch_guards();
        auto preserved = r.finish();
        const auto marker = r.submit(preserved, lanes[1].queue);
        r.wait(marker); r.discard(preserved);
        for (size_t i = 0; i < lanes.size(); ++i) {
            auto& lane = lanes[i];
            r.cpu_access(lane.residual, false); r.cpu_access(lane.output, false);
            const auto* residual = static_cast<const float*>(lane.residual.data);
            for (size_t j = 0; j < lane.residual.bytes / 4; ++j) {
                float expected = -12345.f;
                if (j >= guard / 4 && j - guard / 4 < size_t(r_ld)) {
                    const int c = int((j - guard / 4) / n);
                    const float x = float((pass + 1) * (c % 7 - 3)) / 32;
                    const auto factor = 1 - float((i + 1) * (i + 1)) / 16;
                    expected = x * factor * factor;
                }
                require(residual[j] == expected, "multiqueue Cvec scratch output/full guard mismatch");
            }
            const int q = 1 + (pass + int(i)) % 3, code = (pass + int(i)) % 2 ? 25 : 39;
            const float value = float(code - 32) * float(((pass + int(i)) & 2) ? q : q - 4);
            uint32_t bits = 0; std::memcpy(&bits, &value, 4);
            const auto* output = static_cast<const uint16_t*>(lane.output.data);
            for (size_t j = 0; j < lane.output.bytes / 2; ++j)
                require(output[j] == (j >= guard / 2 && j - guard / 2 < size_t(rows * 512) ? uint16_t(bits >> 16) : 0xbeef),
                        "multiqueue Q3_K private output/full guard mismatch");
            size_t k = 0;
            for (auto a : {lane.direction, lane.scales, lane.on, lane.blocks}) {
                r.cpu_access(a, false);
                require(!std::memcmp(a.data, lane.inputs[k++].data(), a.bytes), "multiqueue modified input/guards");
            }
        }
    }
    std::puts("PASS 24 multiqueue submissions: three distinct writable scratch2048/private512 pools, 48 Cvec n4096/hc257 around 24 no-scratch Q3_K 1025x512 results, changing queue-specific inputs, exact analytic outputs and full input/output/pool guards; overlap not claimed");
    require(!release.expired.load(), "thread-memory watchdog expired");
    std::printf("PASS %s scratch/private, %s ISA first-use initialization before gated queue completion; guard checks preserve the caller recording\n",
                 memory.empty() || !local_thread_memory ? "host" : "device-local", local_isa ? "device-local" : "host");
}
void strata_atomics(const std::filesystem::path& pack) {
    const auto f32 = find_native(pack, "prefill_helpers", "18finite_count_async");
    const auto f16 = find_native(pack, "prefill_helpers", "13Finite16Count");
    const auto ring = find_native(pack, "elementwise", "doorbell_ring_kernel_");
    constexpr int64_t count = 8195;
    constexpr uint32_t sentinel = 0x6d6d6d6d;
    Runtime r;
    const std::array<Allocation,3> inputs{r.allocate(count * 4), r.allocate(count * 2), r.allocate(count * 2)};
    const auto counter = r.allocate(64);
    for (int i = 0; i < 2; ++i)
        r.append(f32.first, f32.second, 256, 8, {Argument::value(count), Argument::value(inputs[0].gpu), Argument::value(counter.gpu)});
    for (size_t i = 1; i < 3; ++i) {
        const uint16_t exponent = i == 1 ? 0x7c00 : 0x7f80;
        r.append(f16.first, f16.second, 256, 8, {Argument::value(count), Argument::value(inputs[i].gpu), Argument::value(exponent), Argument::value(counter.gpu)});
    }
    r.append(ring.first, ring.second, 1, 1, {Argument::value(counter.gpu)});
    r.close();
    for (uint32_t pass = 0; pass < 24; ++pass) {
        uint32_t expected = 17 + pass;
        std::array<std::vector<uint8_t>,3> before;
        for (size_t j = 0; j < inputs.size(); ++j) {
            const auto a = inputs[j]; r.cpu_access(a, true); std::memset(a.data, 0x6d, a.bytes);
            for (int64_t i = 0; i < count; ++i) {
                const bool bad = pass % 3 == 1 || (pass % 3 == 2 && (i + int64_t(j)) % 17 == pass % 17);
                if (j == 0) {
                    static_cast<uint32_t*>(a.data)[i] = bad ? (0x7f800000u | (i % 3 ? 0u : 0x00400000u) | (uint32_t(i & 1) << 31)) : 0x3f000000u + uint32_t(i % 997) * 512;
                } else {
                    const uint16_t exponent = j == 1 ? 0x7c00 : 0x7f80;
                    static_cast<uint16_t*>(a.data)[i] = bad ? uint16_t(exponent | (i % 3 ? 0 : 1) | ((i & 1) << 15)) : uint16_t(0x3000 + i % 997);
                }
                if (bad) expected += j == 0 ? 2 : 1;
            }
            const auto* bytes = static_cast<const uint8_t*>(a.data); before[j].assign(bytes, bytes + a.bytes);
        }
        r.cpu_access(counter, true);
        auto* values = static_cast<uint32_t*>(counter.data);
        std::fill_n(values, counter.bytes / 4, sentinel); values[0] = 17 + pass;
        r.submit(); r.wait(); r.cpu_access(counter, false);
        require(values[0] == expected + 1, "contended FP32/FP16/BF16 atomic reductions or dependent doorbell mismatch");
        for (size_t i = 1; i < counter.bytes / 4; ++i) require(values[i] == sentinel, "atomic counter output guard changed");
        for (size_t j = 0; j < inputs.size(); ++j) {
            r.cpu_access(inputs[j], false);
            require(!std::memcmp(inputs[j].data, before[j].data(), inputs[j].bytes), "atomic reduction input or full padding changed");
        }
    }
    std::puts("PASS 24 Strata FP32/FP16/BF16 contended atomic reductions, 8195 elements, finite/all-bad/mixed signed Inf/NaN, repeated atomic accumulation, dependent doorbell and full input/output guards");
}
void strata_xmx(const std::filesystem::path& pack, bool device = false, const std::string& memory_module = {}) {
    const auto ring = find_native(pack, "elementwise", "doorbell_ring_kernel_");
    double max_error = 0;
    for (uint32_t variant : {8u, 1u, 0u}) {
        const bool fp16 = variant == 0;
        const uint32_t subgroups = fp16 ? 8 : variant, tile_rows = fp16 ? 4 : 8;
        // Fixture values are exactly representable normal dyadics or zero in both formats.
        auto encode = [fp16](float f) { uint32_t u; std::memcpy(&u, &f, 4); return uint16_t(!fp16 ? u >> 16 : f == 0 ? 0 : ((u >> 16) & 0x8000) | ((((u >> 23) & 255) - 112) << 10) | ((u >> 13) & 1023)); };
        auto decode = [fp16](uint16_t u) { const uint32_t bits = !fp16 ? uint32_t(u) << 16 : u == 0 ? 0 : ((uint32_t(u) & 0x8000) << 16) | ((((u >> 10) & 31) + 112) << 23) | ((uint32_t(u) & 1023) << 13); float f; std::memcpy(&f, &bits, 4); return f; };
        const auto kernel = find_native(pack, "xmx", fp16 ? "native_xmx_launch_m4" : subgroups == 8 ? "native_xmx_launchIN4sycl3_V13ext6oneapi8bfloat16ELi8EE" : "native_xmx_launchIN4sycl3_V13ext6oneapi8bfloat16ELi1EE");
        for (const auto shape : {std::array<int64_t,4>{8,64,32,40}, {17,72,48,56}, {5,8,16,24}}) {
            const auto t = shape[0], n = shape[1], k = shape[2], stride = shape[3];
            Runtime r;
            if (device) r.set_memory_module(memory_module);
            const auto alloc = [&](size_t bytes) { return device ? r.allocate_device(bytes) : r.allocate(bytes); };
            const std::array<Allocation,3> gpu_inputs{alloc(t * stride * 2), alloc(n * k * 2), alloc(8 * k * 2)};
            const std::array<Allocation,3> inputs = device ? std::array<Allocation,3>{r.allocate(gpu_inputs[0].bytes), r.allocate(gpu_inputs[1].bytes), r.allocate(gpu_inputs[2].bytes)} : gpu_inputs;
            const auto gpu_output = alloc(t * n * 4);
            const auto output = device ? r.allocate(gpu_output.bytes) : gpu_output;
            const auto counter = r.allocate(64);
            const uint64_t tail = t % 8 ? gpu_inputs[2].gpu : 0;
            if (device) {
                for (size_t j = 0; j < inputs.size(); ++j) r.copy(gpu_inputs[j].gpu, inputs[j].gpu, gpu_inputs[j].bytes);
                r.copy(gpu_output.gpu, output.gpu, gpu_output.bytes);
            }
            r.append(ring.first, ring.second, 1, 1, {Argument::value(counter.gpu)});
            // Frozen lambda ABI: BF16 N,last,T,x,ldx,K,w,Y; FP16 N,last,T,K,x,ldx,w,Y.
            // Verified against the preserved SPIR-V; source function parameter order differs.
            const std::vector<Argument> args = fp16 ?
                std::vector<Argument>{Argument::value(n), Argument::value(tail), Argument::value(t), Argument::value(k), Argument::value(gpu_inputs[0].gpu), Argument::value(stride), Argument::value(gpu_inputs[1].gpu), Argument::value(gpu_output.gpu), Argument::local(subgroups * tile_rows * 8 * 4)} :
                std::vector<Argument>{Argument::value(n), Argument::value(tail), Argument::value(t), Argument::value(gpu_inputs[0].gpu), Argument::value(stride), Argument::value(k), Argument::value(gpu_inputs[1].gpu), Argument::value(gpu_output.gpu), Argument::local(subgroups * tile_rows * 8 * 4)};
            r.append(kernel.first, kernel.second, {8 * subgroups,1,1}, {1,uint32_t((n + 8 * subgroups - 1) / (8 * subgroups)),uint32_t((t + tile_rows - 1) / tile_rows)}, args);
            r.append(ring.first, ring.second, 1, 1, {Argument::value(counter.gpu)});
            if (device) {
                for (size_t j = 0; j < inputs.size(); ++j) r.copy(inputs[j].gpu, gpu_inputs[j].gpu, gpu_inputs[j].bytes);
                r.copy(output.gpu, gpu_output.gpu, gpu_output.bytes);
            }
            r.close();
            for (uint32_t pass = 0; pass < 16; ++pass) {
                std::array<std::vector<uint16_t>,3> before;
                for (const auto a : inputs) { r.cpu_access(a, true); std::fill_n(static_cast<uint16_t*>(a.data), a.bytes / 2, uint16_t{0xbeef}); }
                auto* x = static_cast<uint16_t*>(inputs[0].data); auto* w = static_cast<uint16_t*>(inputs[1].data);
                auto* last = static_cast<uint16_t*>(inputs[2].data);
                for (int64_t row = 0; row < t; ++row)
                    for (int64_t col = 0; col < k; ++col) x[row * stride + col] = encode(float((row * 3 + col + pass) % 17 - 8) / 8);
                for (int64_t i = 0; i < n * k; ++i) w[i] = encode(float((i * 5 + pass) % 19 - 9) / 16);
                for (int64_t row = 0; row < 8; ++row)
                    for (int64_t col = 0; col < k; ++col) last[row * k + col] = t / 8 * 8 + row < t ? x[(t / 8 * 8 + row) * stride + col] : 0;
                for (size_t j = 0; j < inputs.size(); ++j) {
                    const auto* data = static_cast<const uint16_t*>(inputs[j].data); before[j].assign(data, data + inputs[j].bytes / 2);
                }
                std::vector<float> expected(size_t(t * n), 0);
                for (int64_t row = 0; row < t; ++row)
                    for (int64_t col = 0; col < n; ++col)
                        for (int64_t i = 0; i < k; ++i) expected[size_t(row * n + col)] += decode(x[row * stride + i]) * decode(w[col * k + i]);
                r.cpu_access(output, true); auto* values = static_cast<float*>(output.data);
                std::fill_n(values, output.bytes / 4, -12345.0f);
                r.cpu_access(counter, true); auto* sequence = static_cast<uint32_t*>(counter.data);
                std::fill_n(sequence, counter.bytes / 4, 0xdeadbeefu); sequence[0] = pass;
                r.submit(); r.wait(); r.cpu_access(output, false); r.check_scratch_guards();
                r.cpu_access(counter, false); require(sequence[0] == pass + 2, "mixed non-DPAS/XMX/non-DPAS command list mismatch");
                for (size_t i = 1; i < counter.bytes / 4; ++i) require(sequence[i] == 0xdeadbeefu, "mixed XMX counter guard changed");
                for (size_t i = 0; i < expected.size(); ++i) {
                    max_error = std::max(max_error, double(std::abs(values[i] - expected[i])));
                    if (values[i] != expected[i]) std::fprintf(stderr, "XMX mismatch: fp16=%u subgroups=%u T=%lld N=%lld K=%lld stride=%lld pass=%u index=%zu got=%.9g expected=%.9g\n",
                        unsigned(fp16), subgroups, static_cast<long long>(t), static_cast<long long>(n), static_cast<long long>(k), static_cast<long long>(stride), pass, i, values[i], expected[i]);
                    require(std::isfinite(values[i]) && values[i] == expected[i], "Strata FP16/BF16 XMX 3D GEMM or tail mismatch");
                }
                for (size_t i = expected.size(); i < output.bytes / 4; ++i) require(values[i] == -12345.0f, "XMX output guard changed");
                for (size_t j = 0; j < inputs.size(); ++j) {
                    r.cpu_access(inputs[j], false);
                    require(!std::memcmp(inputs[j].data, before[j].data(), inputs[j].bytes), "XMX modified input/stride padding/tail guard");
                }
            }
        }
    }
    std::printf("PASS 144 Strata FP16 M4/BF16 M8 XMX GEMM replays, 128/256 GRFs, 1/8 subgroups, 8x64x32/17x72x48/5x8x16, 3D group Y/Z, padded strides/tails, non-DPAS/XMX/non-DPAS EU-fusion/GRF transitions and full guards; device_local=%u max_abs_error=%.9g\n", unsigned(device), max_error);
}
void gpu_3d(const std::string& path) {
    Runtime r;
    const auto output = r.allocate(8192 * 4);
    const uint64_t gpu = output.gpu + 64;
    uint32_t index = 0;
    for (const auto shape : {std::array<uint32_t,6>{4,2,2,3,5,7}, {8,4,2,2,3,4}, {16,2,2,3,2,2}, {1,1,1,3,2,5},
                            {3,5,2,2,3,2}, {37,1,1,3,1,1}, {8,3,1,2,3,1}, {4,2,3,2,2,2}}) {
        if (index++) r.reset();
        const std::array<uint32_t,3> local{shape[0],shape[1],shape[2]}, groups{shape[3],shape[4],shape[5]};
        const std::array<uint32_t,3> global{local[0]*groups[0],local[1]*groups[1],local[2]*groups[2]};
        const auto count = global[0] * global[1] * global[2];
        require(count + 32 <= output.bytes / 4, "3D check allocation too small");
        r.append(path, "grid3d", local, groups, {Argument::value(gpu)}); r.close();
        for (uint32_t pass = 0; pass < 8; ++pass) {
            r.cpu_access(output, true); auto* values = static_cast<uint32_t*>(output.data);
            std::fill_n(values, output.bytes / 4, 0xdeadbeefu);
            r.submit(); r.wait(); r.cpu_access(output, false);
            for (size_t i = 0; i < output.bytes / 4; ++i) {
                uint32_t expected = 0xdeadbeefu;
                if (i >= 16 && i < 16 + count) {
                    const auto offset = uint32_t(i - 16);
                    const auto x = offset % global[0], y = offset / global[0] % global[1], z = offset / (global[0]*global[1]);
                    expected = x%local[0] + local[0] * (y%local[1] + local[1] * (z%local[2])) +
                        1024 * (x/local[0] + groups[0] * (y/local[1] + groups[1] * (z/local[2])));
                }
                require(values[i] == expected, "3D local/group/global coordinates or prefix/output padding mismatch");
            }
        }
    }
    std::puts("PASS 64 exact 3D GPU coordinate replays, power/non-power-of-two local sizes, hardware/software local IDs, partial/full SIMD, hidden local/global/group sizes and full prefix/output guards");
}
void gpu_captures(const std::string& path, const std::string& memory = {}) {
    const auto kernel = detail::load(path, "advance");
    const auto capture_bytes = detail::align(kernel.cross_bytes - std::min(kernel.inline_bytes, kernel.cross_bytes), 64);
    require(capture_bytes >= 64 && capture_bytes <= 128, "capture fixture must cross a 1 MiB slab boundary");
    Runtime r; if (!memory.empty()) r.set_memory_module(memory, true, true);
    const auto data = r.allocate(1040 * 4);
    uint32_t sum = 0;
    for (uint32_t i = 0; i < 17000; ++i) {
        const uint32_t delta = 1 + i % 7; sum += delta;
        r.append(path, "advance", 64, 16, {Argument::value(data.gpu), Argument::value(delta)});
    }
    r.close(); r.cpu_access(data, true);
    auto* values = static_cast<float*>(data.data);
    for (size_t i = 0; i < data.bytes / 4; ++i) values[i] = float(i);
    r.submit(); r.wait(); r.cpu_access(data, false);
    for (size_t i = 0; i < data.bytes / 4; ++i)
        require(values[i] == float(i + (i < 1024 ? sum : 0)), "captured scalar changed across slab rollover or output guards changed");
    std::puts("PASS 17000 immutable scalar captures across 1 MiB slab rollover, exact GPU sum and full padded output guards");
}
void gpu_release(const std::string& path, const std::string& memory) {
    Runtime r; r.set_memory_module(memory);
    const auto budget = r.memory_info();
    require(budget.total && budget.available <= budget.total, "invalid native memory budget");
    const auto reject = [](auto&& operation) {
        bool rejected = false;
        try { operation(); } catch (const std::runtime_error&) { rejected = true; }
        require(rejected, "invalid allocation release accepted");
    };
    uint64_t plateau = 0;
    for (uint32_t pass = 0; pass < 64; ++pass) {
        auto host = r.allocate(4096), device = r.allocate_device(host.bytes);
        auto stale = host;
        r.cpu_access(host, true);
        auto* values = static_cast<float*>(host.data);
        for (size_t i = 0; i < host.bytes / 4; ++i) values[i] = float(pass + i);
        r.copy(device.gpu, host.gpu, host.bytes);
        const uint32_t delta = pass + 1;
        r.append(path, "advance", 64, 1, {Argument::value(device.gpu), Argument::value(delta)});
        r.copy(host.gpu, device.gpu, host.bytes);
        auto list = r.finish(); const auto done = r.submit(list, r.default_queue());
        r.wait(done); r.discard(list); r.cpu_access(host, false);
        for (size_t i = 0; i < host.bytes / 4; ++i)
            require(values[i] == float(pass + i + (i < 64 ? delta : 0)), "release cycle result/full guard mismatch");
        auto modified = host; --modified.bytes; reject([&] { r.release(modified); });
        r.release(device); r.release(host);
        require(!host.identity && !device.identity && !host.handle && !device.handle, "release did not invalidate allocation");
        reject([&] { r.release(stale); }); reject([&] { r.release(host); });
        if (!pass) plateau = r.allocated_bytes();
        else require(r.allocated_bytes() == plateau, "released public allocations keep accumulating backing storage");
    }
    auto temporary = r.allocate(65536);
    r.barrier(); auto retained = r.finish(); // Does not use temporary, despite conservative Linux residency.
    r.release(temporary);
    for (int pass = 0; pass < 3; ++pass) r.wait(r.submit(retained, r.default_queue()));
    r.discard(retained);

    auto busy = r.allocate(65536);
    auto gate = r.create_gate(); r.depend_on(gate); r.barrier(); auto gated = r.finish();
    const auto done = r.submit(gated, r.default_queue());
    try { reject([&] { r.release(busy); }); }
    catch (...) { gate.signal(); throw; }
    gate.signal(); r.wait(done); r.discard(gated); r.release(busy);
    auto internal = r.allocate_staging(4096); reject([&] { r.release(internal); });

    Runtime owner;
    auto pages = owner.allocate(2 * 65536);
    auto* borrowed = reinterpret_cast<void*>((reinterpret_cast<uintptr_t>(pages.data) + 65535) & ~uintptr_t(65535));
    reject([&] { r.release(pages); });
    for (int pass = 0; pass < 8; ++pass) {
        auto imported = r.import_host(borrowed, 65536); auto stale = imported;
        const auto address = imported.gpu; const uint32_t delta = 1;
        r.append(path, "advance", 64, 1, {Argument::value(address), Argument::value(delta)});
        auto list = r.finish(); r.wait(r.submit(list, r.default_queue())); r.discard(list);
        r.cpu_access(imported, false);
        const auto* values = static_cast<const float*>(borrowed);
        for (size_t i = 0; i < 64; ++i) require(values[i] == float(pass + 1), "import release result mismatch");
        r.release(imported); reject([&] { r.release(stale); });
        require(static_cast<const float*>(borrowed)[0] == float(pass + 1), "import release freed borrowed pages");
    }
    owner.release(pages);
    const auto after = r.memory_info();
    require(after.total == budget.total && after.available <= after.total, "native memory budget changed capacity or exceeded it");
    std::printf("native memory budget: total=%llu available_before=%llu available_after=%llu process=%u global=%u\n",
                static_cast<unsigned long long>(budget.total), static_cast<unsigned long long>(budget.available),
                static_cast<unsigned long long>(after.available), unsigned(after.process_budget), unsigned(after.global_availability));
    dependencies();
    std::printf("PASS 76 allocation-release submissions: 64 host/local-VRAM roundtrips, retained unrelated list, gated rejection, eight borrowed-page reimports, stale/foreign/modified/internal rejection and full guards; retained_after_cycles=%llu\n",
                static_cast<unsigned long long>(plateau));
}
void gpu_large_device_memory(const std::string& module) {
    Runtime r;
    r.set_memory_module(module);
    constexpr size_t count = 65536, bytes = (size_t{5} << 30) + 2 * count;
    const auto physical = detail::native_device_allocation_limit;
    const auto info = r.memory_info();
    require(info.available > bytes + (256u << 20), "large-device check requires 5.4 GiB available");
    reject([&] { r.allocate_device(0); });
    reject([&] { r.allocate_device(SIZE_MAX); });
    auto large = r.allocate_device(bytes);
    const auto stale = large;
    require(large.bytes == bytes && large.device_local && !large.data && !large.imported,
            "large device allocation metadata mismatch");
    std::vector<size_t> offsets{0, bytes - count};
    for (size_t boundary = physical; boundary < bytes; boundary += physical)
        offsets.push_back(boundary - count / 2);
    const auto payload_bytes = offsets.size() * count;
    auto upload = r.allocate(payload_bytes), download = r.allocate(payload_bytes + 128);
    // Write distinct patterns everywhere before reading: aliased segments must fail.
    for (size_t i = 0; i < offsets.size(); ++i)
        r.copy(large.gpu + offsets[i], upload.gpu + i * count, count);
    for (size_t i = 0; i < offsets.size(); ++i)
        r.copy(download.gpu + i * count, large.gpu + offsets[i], count);
    reject([&] { r.copy(large.gpu + bytes - 1, upload.gpu, 2); });
    auto commands = r.finish();
    for (unsigned pass = 0; pass < 8; ++pass) {
        r.cpu_access(upload, true); r.cpu_access(download, true);
        const auto expected = [&](size_t i) { return uint8_t((i * 73u) ^ (i >> 8) ^ (i / count) * 37u ^ pass * 101u); };
        for (size_t i = 0; i < upload.bytes; ++i) static_cast<uint8_t*>(upload.data)[i] = expected(i);
        std::memset(download.data, 0xe7, download.bytes);
        r.submit(commands); r.wait(commands); r.cpu_access(download, false);
        for (size_t i = 0; i < download.bytes; ++i)
            require(static_cast<const uint8_t*>(download.data)[i] == (i < payload_bytes ? expected(i) : 0xe7),
                    "large allocation edge/segment/4-GiB crossing or readback guard mismatch");
        r.cpu_access(upload, false);
        for (size_t i = 0; i < upload.bytes; ++i)
            require(static_cast<const uint8_t*>(upload.data)[i] == expected(i), "large allocation transfer modified its source");
    }
    r.discard(commands);
    const auto before = r.allocated_bytes();
    r.release(large);
    require(!large.identity && !large.gpu && r.allocated_bytes() == before - bytes,
            "large allocation release/accounting mismatch");
    auto copied_stale = stale;
    reject([&] { r.release(copied_stale); });
    std::printf("PASS 5-GiB contiguous device allocation: 8 replays, %zu distinct regions, both ends, every physical segment and 4-GiB crossings, release/accounting/stale identity\n", offsets.size());
}

void gpu_memory(const std::string& path) {
    struct HostPages {
        enum : size_t { bytes = 2 * 65536 };
        void* raw = nullptr;
        void* data = nullptr;
        HostPages() {
#ifdef _WIN32
            data = raw = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            require(data != nullptr, "allocate external host mapping");
#else
            raw = mmap(nullptr, bytes + 65536, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            require(raw != MAP_FAILED, "allocate external host mapping");
            data = reinterpret_cast<void*>(detail::align(reinterpret_cast<uintptr_t>(raw), 65536));
#endif
        }
        ~HostPages() {
#ifdef _WIN32
            if (raw) VirtualFree(raw, 0, MEM_RELEASE);
#else
            if (raw != MAP_FAILED) munmap(raw, bytes + 65536);
#endif
        }
    } host;
    auto* external = static_cast<float*>(host.data);
    {
        Runtime r;
        reject([&] { r.allocate(SIZE_MAX); });
        reject([&] { r.import_host(nullptr, 65536); });
        reject([&] { r.import_host(external + 1, 65536); });
        reject([&] { r.import_host(external, 65535); });
#ifdef _WIN32
        DWORD protect = 0;
        require(VirtualProtect(host.data, HostPages::bytes, PAGE_READONLY, &protect) != 0, "protect external mapping");
        reject([&] { r.import_host(host.data, HostPages::bytes); });
        require(VirtualProtect(host.data, HostPages::bytes, protect, &protect) != 0, "restore external mapping");
#endif
        const auto large = r.allocate(320 * 1024 * 1024 + 4096);
        const auto borrowed = r.import_host(host.data, HostPages::bytes);
        require(!large.imported && borrowed.imported && borrowed.data == host.data && borrowed.bytes == HostPages::bytes,
                "host import ownership or address mismatch");
        reject([&] { r.import_host(host.data, HostPages::bytes); });
        reject([&] { r.import_host(static_cast<uint8_t*>(host.data) + 65536, 65536); });
        reject([&] { r.import_host(large.data, 65536); });
        constexpr size_t count = 1024, prefix = 64;
        const std::array<size_t, 4> offsets{prefix, 128 * 1024 * 1024 / 4 + prefix,
                256 * 1024 * 1024 / 4 + prefix, large.bytes / 4 - count - prefix};
        const uint32_t delta = 3;
        for (auto offset : offsets) {
            const auto gpu = large.gpu + offset * 4;
            r.append(path, "advance", 64, 16, {Argument::value(gpu), Argument::value(delta)});
        }
        const auto gpu = borrowed.gpu + prefix * 4;
        r.append(path, "advance", 64, 16, {Argument::value(gpu), Argument::value(delta)});
        r.close();
        reject([&] { r.import_host(host.data, HostPages::bytes); });
        const auto check = [&](const float* values, size_t elements, const auto& regions, uint32_t pass) {
            size_t begin = 0;
            for (const auto offset : regions) {
                require(std::all_of(values + begin, values + offset, [](float v) { return v == -12345.0f; }), "large/imported prefix or gap guard mismatch");
                for (size_t i = 0; i < count; ++i) require(values[offset + i] == float(i + pass + delta), "large/imported GPU result mismatch");
                begin = offset + count;
            }
            require(std::all_of(values + begin, values + elements, [](float v) { return v == -12345.0f; }), "large/imported tail guard mismatch");
        };
        auto* values = static_cast<float*>(large.data);
        for (uint32_t pass = 0; pass < 8; ++pass) {
            r.cpu_access(large, true); r.cpu_access(borrowed, true);
            std::fill_n(values, large.bytes / 4, -12345.0f);
            std::fill_n(external, borrowed.bytes / 4, -12345.0f);
            for (const auto offset : offsets) for (size_t i = 0; i < count; ++i) values[offset + i] = float(i + pass);
            for (size_t i = 0; i < count; ++i) external[prefix + i] = float(i + pass);
            r.submit();
            reject([&] { r.cpu_access(borrowed, false); });
            r.wait(); r.cpu_access(large, false); r.cpu_access(borrowed, false);
            check(values, large.bytes / 4, offsets, pass);
            check(external, borrowed.bytes / 4, std::array<size_t, 1>{prefix}, pass);
        }
    }
    // The runtime must release its GPU handle without freeing the caller's mapping.
#ifdef _WIN32
    MEMORY_BASIC_INFORMATION info{};
    require(VirtualQuery(host.data, &info, sizeof(info)) == sizeof(info) && info.State == MEM_COMMIT,
            "runtime freed borrowed host pages");
#endif
    require(external[64] == 10.0f && external[63] == -12345.0f && external[1088] == -12345.0f,
            "borrowed mapping did not survive runtime teardown");
    external[0] = 123;
    require(external[0] == 123, "borrowed mapping is not writable after runtime teardown");
    std::puts("PASS 8 large/imported memory GPU replays, >320 MiB allocation, four separated GPU VA subranges across 256 MiB, borrowed 128 KiB mapping, full allocation guards, invalid imports and caller mapping survives runtime teardown");
}
void gpu_semisync(const std::string& path) {
    Runtime r;
    constexpr uint32_t launches = 400, delta = 3, host_count = 262144;
    const auto data = r.allocate(1040 * sizeof(float));
    for (uint32_t i = 0; i < launches; ++i)
        r.append(path, "advance", 64, 16, {Argument::value(data.gpu), Argument::value(delta)});
    r.close();
    std::vector<int16_t> source(host_count);
    std::vector<float> next_input(host_count);
    for (uint32_t i = 0; i < host_count; ++i) source[i] = int16_t(int(i % 1024) - 512);
    using Clock = std::chrono::steady_clock;
    const auto us = [](auto a, auto b) { return std::chrono::duration<double, std::micro>(b - a).count(); };
    std::array<double, 3> medians{};
    // A0/B/A1 retains drift evidence; this demo establishes overlap/parity, not model throughput.
    for (uint32_t phase = 0; phase < 3; ++phase) {
        const bool overlap = phase == 1;
        const char* mode = overlap ? "overlap" : "serial";
        std::vector<double> totals;
        for (uint32_t pass = 0; pass < 80; ++pass) {
            const float scale = float(1 + pass % 7) / 128;
            r.cpu_access(data, true);
            auto* values = static_cast<float*>(data.data);
            for (size_t i = 0; i < data.bytes / sizeof(float); ++i) values[i] = float(i + pass);
            std::fill(next_input.begin(), next_input.end(), -99999.0f);
            const auto start = Clock::now();
            r.submit();
            const auto sent = Clock::now();
            bool fallback = false;
            const auto finish = [&] { if (!r.ready()) { fallback = true; r.wait(); } };
            if (!overlap) finish();
            const auto prepare = Clock::now();
            // Separate host storage: prepare the next request without touching live GPU buffers.
            for (uint32_t i = 0; i < host_count; ++i) next_input[i] = float(source[i]) * scale;
            const auto prepared = Clock::now();
            if (overlap) finish();
            const auto done = Clock::now();
            r.cpu_access(data, false);
            for (size_t i = 0; i < data.bytes / sizeof(float); ++i)
                require(values[i] == float(i + pass + (i < 1024 ? launches * delta : 0)), "semisync GPU result/output guard mismatch");
            for (uint32_t i = 0; i < host_count; ++i)
                require(next_input[i] == float(int(i % 1024) - 512) * scale, "independent next-request host preparation mismatch");
            if (pass >= 16) {
                totals.push_back(us(start, done));
                std::printf("SEMISYNC_SAMPLE %u %s %u %.3f %.3f %.3f %.3f %u\n", phase, mode, pass - 16,
                            us(start, sent), us(prepare, prepared), us(sent, done) - us(prepare, prepared),
                            totals.back(), unsigned(fallback));
            }
        }
        std::sort(totals.begin(), totals.end());
        medians[phase] = (totals[31] + totals[32]) / 2;
        std::printf("SEMISYNC_PHASE %u %s median_total_us=%.3f\n", phase, mode, medians[phase]);
    }
    const auto control = std::sqrt(medians[0] * medians[2]);
    std::printf("PASS 240 semisync/serial GPU replays, 400 kernels per tail fence, one explicit completion query, independent host dequantization, exact outputs and full guards; paired_change_pct=%.3f control_drift_pct=%.3f\n",
                100 * (medians[1] / control - 1), 100 * (medians[2] / medians[0] - 1));
}
void gpu_multiqueue(const std::string& path, const std::string& memory) {
    Runtime r;
    r.set_memory_module(memory);
    const auto main = r.default_queue(), first = r.create_queue(), second = r.create_queue();
    constexpr uint32_t prefix = 16;
    const auto input = r.allocate(1040 * sizeof(float)), a = r.allocate_device(input.bytes), b = r.allocate_device(input.bytes);
    const auto saved_a = r.allocate_device(input.bytes), saved_b = r.allocate_device(input.bytes);
    const auto output_a = r.allocate(input.bytes), output_b = r.allocate(input.bytes);
    r.copy(a.gpu, input.gpu, input.bytes); r.copy(b.gpu, input.gpu, input.bytes);
    auto root = r.finish();
    const auto branch = [&](const Queue& queue, const Allocation& data, const Allocation& saved, uint32_t count, uint32_t delta) {
        r.begin(queue);
        const uint64_t address = data.gpu + prefix * sizeof(float);
        for (uint32_t i = 0; i < count; ++i) r.append(path, "advance", 64, 16, {Argument::value(address),Argument::value(delta)});
        r.copy(saved.gpu, data.gpu, data.bytes);
        return r.finish();
    };
    auto left = branch(first,a,saved_a,17,3), right = branch(second,b,saved_b,29,5);
    r.begin(second);
    const uint64_t b_address = b.gpu + prefix * sizeof(float);
    const uint32_t one = 1;
    for (uint32_t i = 0; i < 256; ++i) r.append(path,"advance",64,16,{Argument::value(b_address),Argument::value(one)});
    auto later = r.finish();
    r.copy(output_a.gpu,saved_a.gpu,output_a.bytes); r.copy(output_b.gpu,saved_b.gpu,output_b.bytes);
    auto join = r.finish();
    reject([&] { r.begin(Queue{}); }); reject([&] { r.completed(Queue{}); });
    reject([&] { r.ready(Completion{}); }); reject([&] { r.wait(Completion{}); });
    reject([&] { r.submit(left); }); reject([&] { r.submit(left,second); });
    reject([&] { r.submit(root,main,{Completion{}}); });
    Completion old_left;
    {
        Runtime other;
        reject([&] { other.begin(first); }); reject([&] { other.completed(second); });
    }
    for (uint32_t pass = 0; pass < 12; ++pass) {
        r.cpu_access(input,true);
        auto* values = static_cast<float*>(input.data);
        std::fill_n(values,input.bytes/4,-12345.f);
        for (uint32_t i=0;i<1024;++i) values[prefix+i]=float(i+3*pass);
        const auto uploaded=r.submit(root,main);
        const auto l=r.submit(left,first,{uploaded});
        const auto rr=r.submit(right,second,{uploaded});
        const auto tail=r.submit(later,second,{rr});
        const auto joined=r.submit(join,main,{l,rr,l,uploaded});
        require(uploaded.sequence()==2*pass+1 && joined.sequence()==2*pass+2 && l.sequence()==pass+1 &&
                rr.sequence()==2*pass+1 && tail.sequence()==2*pass+2,"independent queue sequences were conflated");
        reject([&] { r.submit(left,first); }); reject([&] { r.discard(left); });
        reject([&] { r.cpu_access(input,false); }); reject([&] { r.create_queue(); });
        r.begin(main); // Empty recording selection is independent of pending frozen lists.
        r.wait(joined); r.wait(tail);
        require(r.completed(first)==l.sequence() && r.completed(second)==tail.sequence() && r.completed()==joined.sequence(),
                "per-queue progress did not retire the expected submissions");
        require(r.ready(l) && r.ready(rr) && r.ready(left) && r.ready(right),"cross-queue completion was lost");
        if (!pass) old_left=l;
        r.wait(old_left,0); // Immutable points do not become the latest replay of their original list.
        r.check_scratch_guards();
        for (const auto& output : {output_a,output_b}) {
            r.cpu_access(output,false);
            const auto* result=static_cast<const float*>(output.data);
            const uint32_t delta=output.handle==output_a.handle ? 51 : 145;
            for (size_t i=0;i<output.bytes/4;++i) {
                const float expected=i>=prefix && i-prefix<1024 ? float(i-prefix+3*pass+delta) : -12345.f;
                require(result[i]==expected,"GPU fork/join snapshot or full guard mismatch");
            }
        }
        r.cpu_access(input,false);
        for (size_t i=0;i<input.bytes/4;++i)
            require(values[i]==(i>=prefix && i-prefix<1024 ? float(i-prefix+3*pass) : -12345.f),"fork/join modified host input");
    }
    {
        Runtime other;
        reject([&] { other.ready(old_left); }); reject([&] { other.wait(old_left); });
        // A dependency is validated before changing the target queue.
        const auto data=other.allocate(4096);
        other.append(path,"advance",64,16,{Argument::value(data.gpu),Argument::value(one)});
        auto list=other.finish();
        reject([&] { other.submit(list,other.default_queue(),{old_left}); });
    }
    auto stale=left;
    for (auto* list : {&root,&left,&right,&later,&join}) r.discard(*list);
    r.begin(first);
    const uint64_t address=a.gpu+prefix*sizeof(float);
    const uint32_t nine=9;
    r.append(path,"advance",64,16,{Argument::value(address),Argument::value(nine)});
    r.copy(output_a.gpu,a.gpu,a.bytes);
    auto replacement=r.finish();
    const auto replaced=r.submit(replacement,first);
    require(replaced.sequence()==13,"queue sequence reset on list recycling");
    reject([&] { r.submit(stale,first); });
    r.wait(old_left,0); r.wait(replacement); r.discard(replacement);
    r.cpu_access(output_a,false);
    const auto* result=static_cast<const float*>(output_a.data);
    for (size_t i=0;i<output_a.bytes/4;++i)
        require(result[i]==(i>=prefix && i-prefix<1024 ? float(i-prefix+33+51+9) : -12345.f),"recycled cross-queue list result/guard mismatch");
    require(r.ready(replaced) && r.ready(old_left),"discard changed immutable completion identity");
    std::puts("PASS 61 submissions on three independent native queues: 12 GPU fork/join waves, local VRAM snapshots, duplicate/same-queue dependencies, per-queue completion sequences, immutable old points, recycled lists, invalid/foreign handles and full guards; no host waits between wave submissions; overlap/backlog depth not claimed");
}
void gpu_inline_events(const std::string& path, const std::string& memory) {
    Runtime r; r.set_memory_module(memory);
    const auto compute = r.default_queue(), consumer = r.create_queue();
    const auto tags = r.allocate_staging(256), data = r.allocate_device(4096), snapshot = r.allocate_device(4096);
    const auto output = r.allocate_staging(4096);
    auto* bytes = static_cast<uint8_t*>(tags.data);
    auto* prefix = new (bytes + 64) std::atomic<uint64_t>(0);
    auto* gate = new (bytes + 128) std::atomic<uint64_t>(0);
    auto* tail = new (bytes + 192) std::atomic<uint64_t>(0);
    struct Release {
        std::atomic<uint64_t>* gate;
        void open() { gate->store(1, std::memory_order_release); _mm_clflush(gate); _mm_mfence(); }
        ~Release() { open(); }
    } release{gate};
    for (const auto address : {uint64_t{0}, UINT64_MAX - 7, tags.gpu + 4})
        reject([&] { r.signal_value(address, 1); });
    reject([&] { r.wait_value(tags.gpu + 1, 1); });
    reject([&] { r.wait_value(0, 1); });
    const uint32_t three = 3, seven = 7;
    const uint64_t address = data.gpu + 16 * sizeof(float);
    const uint32_t sentinel = 0xc640e400; // -12345.0f
    r.fill(data.gpu, &sentinel, sizeof(sentinel), data.bytes);
    r.append(path, "advance", 64, 15, {Argument::value(address), Argument::value(three)});
    r.copy(snapshot.gpu, data.gpu, data.bytes);
    r.signal_value(tags.gpu + 64, 1);
    r.wait_value(tags.gpu + 128, 1);
    r.append(path, "advance", 64, 15, {Argument::value(address), Argument::value(seven)});
    r.signal_value(tags.gpu + 192, 1);
    auto producer = r.finish();
    r.begin(consumer); r.wait_value(tags.gpu + 64, 1); r.copy(output.gpu, snapshot.gpu, output.bytes);
    auto copy = r.finish();
    for (int pass = 0; pass < 16; ++pass) {
        r.cpu_access(tags, true); std::memset(tags.data, 0xa5, tags.bytes);
        for (auto* word : {prefix, gate, tail}) { word->store(0); _mm_clflush(word); }
        _mm_mfence();
        const auto before = r.completed(compute);
        const auto done = r.submit(producer, compute);
        const auto copied = r.submit(copy, consumer);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(300);
        while (!prefix->load(std::memory_order_acquire) && std::chrono::steady_clock::now() < deadline) {
            _mm_clflush(prefix); _mm_mfence(); _mm_pause();
        }
        const bool reached = prefix->load(std::memory_order_acquire) == 1;
        const bool copied_early = copied.wait(100000000ull);
        const bool held = !done.ready() && !tail->load() && r.completed(compute) == before;
        release.open(); r.wait(done); r.wait(copied);
        require(reached && copied_early && held, "in-list prefix/consumer waited for or retired the blocked tail");
        require(r.completed(compute) == before + 1 && tail->load() == 1, "in-list events created extra submissions or lost tail");
        r.cpu_access(output, false);
        const auto* values = static_cast<const float*>(output.data);
        for (size_t i = 0; i < output.bytes / sizeof(float); ++i)
            require(values[i] == (i >= 16 && i < 976 ? -12342.f : -12345.f), "in-list prefix snapshot/guard mismatch");
        for (size_t i = 0; i < tags.bytes; ++i)
            if (!((i >= 64 && i < 72) || (i >= 128 && i < 136) || (i >= 192 && i < 200)))
                require(bytes[i] == 0xa5, "in-list event changed tag guards");
    }
    r.discard(producer); r.discard(copy);
    std::puts("PASS 16 in-list event replays: one producer submission, GPU prefix signal and cross-queue equality wait before blocked tail, queue fence not retired early, exact snapshots and full guards");
}
void gpu_host_gate(const std::string& path, const std::string& memory) {
    Runtime r;
    r.set_memory_module(memory);
    const auto compute = r.default_queue(), copy = r.create_queue(), independent = r.create_queue();
    std::array<Queue, 16> query_queues;
    const std::array<Queue, 3> distinct{compute, copy, independent};
    for (size_t i = 0; i < query_queues.size(); ++i) query_queues[i] = distinct[i % distinct.size()];
    std::array<uint64_t, 18> progress;
    progress.fill(UINT64_MAX);
    const auto query_batch = [&] {
        r.completed(query_queues.data(), progress.data() + 1, query_queues.size());
        require(progress.front() == UINT64_MAX && progress.back() == UINT64_MAX, "completion batch overwrote output guards");
        for (size_t i = 3; i < query_queues.size(); ++i)
            require(progress[i + 1] == progress[i % 3 + 1], "duplicate queues did not share a completion snapshot");
    };
    r.completed(nullptr, nullptr, 0);
    query_batch();
    require(std::all_of(progress.begin() + 1, progress.end() - 1, [](uint64_t value) { return value == 0; }),
            "idle unsubmitted completion batch was not zero");
    const auto initial_progress = progress;
    reject([&] { r.completed(nullptr, progress.data() + 1, 1); });
    reject([&] { r.completed(query_queues.data(), nullptr, 1); });
    reject([&] { r.completed(query_queues.data(), progress.data() + 1, 17); });
    {
        Runtime foreign;
        auto invalid = query_queues;
        invalid.back() = foreign.default_queue();
        reject([&] { r.completed(invalid.data(), progress.data() + 1, invalid.size()); });
        invalid.back() = Queue{};
        reject([&] { r.completed(invalid.data(), progress.data() + 1, invalid.size()); });
    }
    require(progress == initial_progress, "rejected completion batch modified caller output");
    constexpr size_t prefix = 16;
    const auto input = r.allocate(1040 * sizeof(float)), a = r.allocate_device(input.bytes), b = r.allocate_device(input.bytes);
    const auto snapshot = r.allocate(input.bytes), final = r.allocate(input.bytes), separate = r.allocate(input.bytes);
    const uint64_t address_a = a.gpu + prefix * 4, address_b = b.gpu + prefix * 4;
    const uint32_t three = 3, seven = 7, eleven = 11;
    r.copy(a.gpu, input.gpu, input.bytes);
    r.append(path, "advance", 64, 16, {Argument::value(address_a), Argument::value(three)});
    auto first = r.finish();
    r.begin(copy); r.copy(snapshot.gpu, a.gpu, snapshot.bytes); auto copied = r.finish();
    r.begin(independent);
    r.copy(b.gpu, input.gpu, input.bytes);
    r.append(path, "advance", 64, 16, {Argument::value(address_b), Argument::value(eleven)});
    r.copy(separate.gpu, b.gpu, separate.bytes); auto other = r.finish();
    reject([&] { HostGate{}.signal(); }); reject([&] { r.depend_on(HostGate{}); });
    {
        Runtime foreign;
        const auto gate = foreign.create_gate();
        reject([&] { r.depend_on(gate); });
    }
    for (int pass = 0; pass < 16; ++pass) {
        r.cpu_access(input, true);
        auto* values = static_cast<float*>(input.data);
        std::fill_n(values, input.bytes / 4, -12345.f);
        for (size_t j = 0; j < 1024; ++j) values[prefix + j] = float(j + 5 * pass);
        const auto gate = r.create_gate(), gate2 = r.create_gate();
        struct ReleaseOnError {
            HostGate first, second;
            ~ReleaseOnError() { try { first.signal(); second.signal(); } catch (...) {} }
        } release{gate, gate2};
        r.begin(compute);
        r.depend_on(gate); r.depend_on(gate); r.depend_on(gate2);
        reject([&] { r.begin(copy); });
        r.append(path, "advance", 64, 16, {Argument::value(address_a), Argument::value(seven)});
        reject([&] { r.depend_on(gate); });
        r.copy(final.gpu, a.gpu, final.bytes); auto tail = r.finish();
        const auto earlier = r.submit(first, compute);
        reject([&] { r.submit(tail, compute, {earlier, earlier, earlier, earlier, earlier, earlier, earlier, earlier,
                    earlier, earlier, earlier, earlier, earlier, earlier, earlier}); }); // Two gates + fifteen dependencies.
        const auto blocked = r.submit(tail, compute, {earlier});
        const auto saved = r.submit(copied, copy, {earlier});
        const auto free = r.submit(other, independent);
        require(!r.ready(blocked), "unsignaled host gate allowed its queue tail to execute");
        query_batch();
        require(progress[1] <= earlier.sequence() && progress[2] <= saved.sequence() && progress[3] <= free.sequence(),
                "completion batch reported an uncompleted or unsubmitted point");
        reject([&] { r.wait(blocked, 0); });
        // The consumer must finish while the producer's later tail is still host-gated.
        r.wait(saved); r.wait(free);
        query_batch();
        require(progress[1] == earlier.sequence() && progress[2] == saved.sequence() && progress[3] == free.sequence() && !r.ready(blocked),
                "independent prefix consumer waited for, or released, a later gated tail");
        if (pass & 1) gate2.signal(); else gate.signal();
        require(!r.ready(blocked), "one host gate released a two-gate conjunction");
        reject([&] { r.wait(blocked, 0); });
        // A callback may signal while the submitting owner is in an OS fence wait.
        auto callback = std::async(std::launch::async, [gate, gate2] {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            gate.signal(); gate.signal(); gate2.signal();
        });
        r.wait(blocked); callback.get();
        query_batch();
        require(progress[1] == blocked.sequence() && progress[2] == saved.sequence() && progress[3] == free.sequence(),
                "completion batch lost the released tail or other queue progress");
        require(r.ready(blocked), "callback release did not complete the native queue tail");
        for (const auto& output : {snapshot, final, separate}) {
            r.cpu_access(output, false);
            const auto* result = static_cast<const float*>(output.data);
            const int delta = output.handle == snapshot.handle ? 3 : output.handle == final.handle ? 10 : 11;
            for (size_t j = 0; j < output.bytes / 4; ++j)
                require(result[j] == (j >= prefix && j - prefix < 1024 ? float(j - prefix + 5 * pass + delta) : -12345.f),
                        "host-gated prefix/tail/independent output or full guard mismatch");
        }
        r.cpu_access(input, false);
        for (size_t j = 0; j < input.bytes / 4; ++j)
            require(values[j] == (j >= prefix && j - prefix < 1024 ? float(j - prefix + 5 * pass) : -12345.f),
                    "host-gate execution changed input/guards");
        r.discard(tail);
    }
    std::puts("PASS 48 three-queue completion batches: 16 entries, duplicate snapshots, pending/gated/complete prefixes, cached/empty queries, invalid/foreign/oversize/null rejection and untouched output guards");
    // A frozen list retains pre-signaled gates after all external copies disappear.
    std::vector<HostGate> held;
    for (int i = 0; i < 16; ++i) { held.push_back(r.create_gate()); r.depend_on(held.back()); }
    const auto excess = r.create_gate(); reject([&] { r.depend_on(excess); });
    for (const auto& gate : held) { gate.signal(); gate.signal(); }
    auto marker = r.finish(); held.clear();
    for (int replay = 0; replay < 3; ++replay) { r.submit(marker, compute); r.wait(marker); }
    r.discard(marker);
    {
        const auto unused = r.create_gate();
        r.depend_on(unused); r.close(); r.reset(); // Dropping an unsubmitted wait must not retain its gate.
        r.copy(final.gpu, input.gpu, input.bytes); r.close(); r.submit(); r.wait();
        r.cpu_access(final, false); r.cpu_access(input, false);
        require(!std::memcmp(final.data, input.data, input.bytes), "implicit reset retained an abandoned host wait");
        r.reset();
    }
    for (auto* list : {&first, &copied, &other}) r.discard(*list);
    HostGate orphan;
    {
        Runtime dying;
        orphan = dying.create_gate(); dying.depend_on(orphan);
        auto blocked = dying.finish(); dying.submit(blocked, dying.default_queue());
        // Teardown releases an abandoned gate before draining/destroying the context.
    }
    reject([&] { orphan.signal(); });
    std::puts("PASS 69 host-gate submissions: 16 blocked-tail waves on three queues, earlier prefix copies and independent work complete before release, callback-thread signals, idempotence, two-gate conjunction, 16-gate retained marker replays, implicit reset drops unsubmitted waits, cancellation teardown, invalid/foreign/late/overflow gates, exact outputs and full guards");
}
void gpu_observers(const std::string& path, const std::string& memory) {
    reject([&] { Completion{}.ready(); }); reject([&] { Completion{}.wait(0); });
    Runtime r;
    r.set_memory_module(memory);
    const auto compute = r.default_queue(), copy = r.create_queue();
    constexpr size_t prefix = 16;
    const auto input = r.allocate(1040 * sizeof(float)), a = r.allocate_device(input.bytes);
    const auto snapshot = r.allocate(input.bytes), final = r.allocate(input.bytes);
    const uint64_t address = a.gpu + prefix * 4;
    const uint32_t three = 3, seven = 7;
    r.begin(copy); r.copy(snapshot.gpu, a.gpu, snapshot.bytes); auto copied = r.finish();
    Completion old;
    for (int pass = 0; pass < 12; ++pass) {
        r.cpu_access(input, true);
        auto* values = static_cast<float*>(input.data);
        std::fill_n(values, input.bytes / 4, -12345.f);
        for (size_t j = 0; j < 1024; ++j) values[prefix + j] = float(j + 5 * pass);
        const auto start = r.create_gate(), callback_gate = r.create_gate(), late_gate = r.create_gate();
        std::promise<void> armed, observed, allowed, checked;
        auto permission = allowed.get_future();
        struct Cleanup {
            HostGate start, callback, late;
            std::promise<void>* allowed;
            std::future<void> worker;
            ~Cleanup() {
                for (const auto* gate : {&start, &callback, &late}) try { gate->signal(); } catch (...) {}
                try { allowed->set_value(); } catch (...) {}
                if (worker.valid()) worker.wait();
            }
        } cleanup{start, callback_gate, late_gate, &allowed, {}};
        r.begin(compute); r.depend_on(start);
        r.copy(a.gpu, input.gpu, input.bytes);
        r.append(path, "advance", 64, 16, {Argument::value(address), Argument::value(three)});
        auto first = r.finish();
        r.depend_on(callback_gate);
        r.append(path, "advance", 64, 16, {Argument::value(address), Argument::value(seven)});
        r.copy(final.gpu, a.gpu, final.bytes); auto tail = r.finish();
        r.begin(copy); r.depend_on(late_gate); auto marker = r.finish();
        const auto earlier = r.submit(first, compute);
        const auto blocked = r.submit(tail, compute);
        const auto saved = r.submit(copied, copy, {earlier});
        const auto late = r.submit(marker, copy);
        reject([&] { earlier.wait(UINT64_MAX); });
        if (pass) require(old.ready() && old.wait(0), "old observer changed to a later recycled list submission");
        cleanup.worker = std::async(std::launch::async,
            [earlier, blocked, late, callback_gate, &armed, &observed, &checked, permission = std::move(permission)]() mutable {
                try {
                    require(!earlier.ready() && !earlier.wait(0) && !earlier.wait(100000), "pending prefix observer reported completion");
                    require(!blocked.wait(100000), "pending tail observer reported completion");
                    armed.set_value();
                    require(earlier.wait(), "worker prefix observer timeout");
                    observed.set_value();
                    permission.get();
                    callback_gate.signal();
                    // Reuse this thread's event for another queue while older timed-out
                    // registrations may wake it. The new point must remain incomplete.
                    require(!late.wait(200000), "reused observer event accepted another point's wakeup");
                    checked.set_value();
                    require(late.wait(), "worker late observer timeout");
                } catch (...) {
                    const auto failure = std::current_exception();
                    for (auto* promise : {&armed, &observed, &checked}) try { promise->set_exception(failure); } catch (...) {}
                    callback_gate.signal();
                    throw;
                }
            });
        armed.get_future().get();
        reject([&] { r.discard(first); }); reject([&] { r.discard(marker); });
        // Concurrent watchdog observations must not wait behind the worker's blocking wait.
        auto watchdog = std::async(std::launch::async, [earlier, blocked] {
            require(!earlier.ready() && !earlier.wait(0) && !blocked.ready(), "watchdog missed unsignaled gates");
        });
        watchdog.get();
        if (!pass) std::this_thread::sleep_for(std::chrono::milliseconds(60)); // Exercise multiple blocking wait slices.
        start.signal();
        observed.get_future().get();
        r.wait(saved);
        require(!blocked.ready() && !late.ready(), "observer released a host gate");
        // The owner may retire the observed prefix while its later tail stays gated.
        // The observer itself never mutates owner progress or list generations.
        auto first_alias = first;
        r.discard(first);
        reject([&] { r.discard(first_alias); });
        require(r.completed(compute) == earlier.sequence(), "owner could not publish the completed prefix");
        if (!pass) old = earlier;
        require(earlier.ready() && earlier.wait(0), "discard invalidated an immutable observer point");
        allowed.set_value();
        checked.get_future().get();
        r.wait(blocked);
        require(!late.ready(), "tail completion released the other queue's gated point");
        late_gate.signal();
        cleanup.worker.get();
        r.discard(marker); // Owner collection reuses the worker's immutable observation.
        require(late.ready() && late.wait(0), "discard invalidated the late observer");
        r.wait(late);
        for (const auto& output : {snapshot, final}) {
            r.cpu_access(output, false);
            const auto* result = static_cast<const float*>(output.data);
            const int delta = output.handle == snapshot.handle ? 3 : 10;
            for (size_t j = 0; j < output.bytes / 4; ++j)
                require(result[j] == (j >= prefix && j - prefix < 1024 ? float(j - prefix + 5 * pass + delta) : -12345.f),
                        "observer callback prefix/tail output or full guard mismatch");
        }
        r.cpu_access(input, false);
        for (size_t j = 0; j < input.bytes / 4; ++j)
            require(values[j] == (j >= prefix && j - prefix < 1024 ? float(j - prefix + 5 * pass) : -12345.f),
                    "observer callback changed input/guards");
        r.discard(tail);
    }
    r.discard(copied);
    Completion orphan;
    {
        Runtime dying;
        const auto gate = dying.create_gate(); gate.signal(); dying.depend_on(gate);
        auto marker = dying.finish(); orphan = dying.submit(marker, dying.default_queue());
        require(orphan.wait(), "orphan observer did not complete before teardown");
    }
    reject([&] { orphan.ready(); }); reject([&] { orphan.wait(0); });
    std::array<Completion, 2> retained;
    for (unsigned pass = 0; pass < 8; ++pass) {
        auto dying = std::make_unique<Runtime>();
        const auto other = dying->create_queue();
        const auto gate = dying->create_gate(); dying->depend_on(gate);
        auto marker = dying->finish();
        std::array<Completion, 2> points;
        points[0] = dying->submit(marker, dying->default_queue());
        dying->begin(other); dying->depend_on(gate);
        auto second = dying->finish(); points[1] = dying->submit(second, other);
        std::array<std::promise<void>, 2> started;
        std::array<std::future<void>, 2> observers;
        for (size_t i = 0; i < points.size(); ++i) observers[i] = std::async(std::launch::async,
            [point = points[i], &ready = started[i]] {
                ready.set_value();
                try { require(point.wait(uint64_t(INT64_MAX)), "unbounded observer unexpectedly timed out"); }
                catch (const std::runtime_error& error) {
                    require(std::string(error.what()).find("closing or destroyed runtime") != std::string::npos,
                            "teardown observer failed with an unexpected native error");
                }
            });
        for (auto& ready : started) ready.get_future().get();
        dying.reset();
        for (auto& observer : observers) observer.get();
        for (const auto& point : points) { reject([&] { point.ready(); }); reject([&] { point.wait(0); }); }
        // Releasing a previous Runtime's observers must not close this one's device.
        retained = points;
        reject([&] { orphan.ready(); });
    }
    std::puts("PASS 65 observer submissions: 12 two-queue callback pipelines, fixed prefix before gated tail, concurrent worker/watchdog reads, zero/finite timeouts, cross-queue event reuse and long wait slices, owner-only progress publication and retirement, discarded/recycled old points, destroyed-runtime rejection and eight two-queue teardown races with retained older-runtime handles, exact outputs and full guards; actual OS sleep/wakeup overlap not instrumented");
}
void gpu_recording(const std::string& path, const std::string& memory) {
    Runtime r;
    r.set_memory_module(memory);
    const auto compute = r.default_queue();
    auto other = r.create_queue();
    constexpr size_t prefix = 16;
    const auto input = r.allocate(1040 * sizeof(float)), a = r.allocate_device(input.bytes);
    const auto snapshot = r.allocate(input.bytes), final = r.allocate(input.bytes), separate = r.allocate(input.bytes);
    const auto wide = r.allocate(8192);
    Allocation b;
    uint64_t retained = 0;
    for (uint32_t pass = 0; pass < 16; ++pass) {
        if (pass == 8) other = r.create_queue(); // Grow existing storage ledgers before reusing their pools.
        r.cpu_access(input, true);
        auto* values = static_cast<float*>(input.data);
        std::fill_n(values, input.bytes / 4, -12345.f);
        for (size_t j = 0; j < 1024; ++j) values[prefix + j] = float(j + 5 * pass);
        const auto start = r.create_gate(), tail_gate = r.create_gate();
        struct Release {
            HostGate start, tail;
            std::promise<void> stop;
            std::future<void> watchdog;
            std::atomic<bool> expired{false};
            ~Release() {
                try { start.signal(); tail.signal(); stop.set_value(); } catch (...) {}
                if (watchdog.valid()) watchdog.wait();
            }
        } release{start, tail_gate};
        auto stop = release.stop.get_future();
        release.watchdog = std::async(std::launch::async, [&, stop = std::move(stop), start, tail_gate]() mutable {
            if (stop.wait_for(std::chrono::seconds(5)) != std::future_status::ready) {
                release.expired.store(true);
                start.signal(); tail_gate.signal(); // Prevent a bad busy-BO ioctl from leaving a permanent gate.
            }
        });
        r.begin(compute); r.depend_on(start);
        r.copy(a.gpu, input.gpu, input.bytes);
        const uint64_t address_a = a.gpu + prefix * 4;
        const uint32_t three = 3, seven = 7, eleven = 11;
        r.append(path, "advance", 64, 16, {Argument::value(address_a), Argument::value(three)});
        auto first = r.finish();
        const auto earlier = r.submit(first, compute);
        require(!earlier.ready(), "recording fixture did not retain its leading gate");
        if (!pass) {
            b = r.allocate_device(input.bytes);
            const auto unused_host = r.allocate(4096);
            require(unused_host.data && b.gpu, "fresh allocations failed while another queue was pending");
        }
        const auto created_pending = r.create_gate(); created_pending.signal();
        reject([&] { r.cpu_access(input, false); });
        r.begin(other);
        r.copy(b.gpu, input.gpu, input.bytes);
        const uint64_t address_b = b.gpu + prefix * 4;
        r.append(path, "advance", 64, 16, {Argument::value(address_b), Argument::value(seven)});
        r.copy(separate.gpu, b.gpu, separate.bytes);
        const uint8_t guard = 0xcd;
        std::array<uint8_t, 128> pattern;
        for (size_t j = 0; j < pattern.size(); ++j) pattern[j] = uint8_t(j + 7 + pass);
        r.fill(wide.gpu, &guard, 1, wide.bytes);
        r.fill(wide.gpu + 17, pattern.data(), pattern.size(), 4099); // First-use ISA and captured pattern while earlier ISA is live.
        pattern.fill(0);
        auto independent = r.finish();
        const auto free = r.submit(independent, other);
        r.wait(free);
        require(!release.expired.load() && !earlier.ready(), "dynamic recording/independent work waited for the blocked producer");
        r.discard(independent);
        r.begin(compute); r.depend_on(tail_gate);
        r.append(path, "advance", 64, 16, {Argument::value(address_a), Argument::value(eleven)});
        r.copy(final.gpu, a.gpu, final.bytes);
        auto tail = r.finish();
        const auto blocked = r.submit(tail, compute);
        r.begin(other); r.copy(snapshot.gpu, a.gpu, snapshot.bytes);
        auto copied = r.finish();
        const auto saved = r.submit(copied, other, {earlier});
        require(!earlier.ready() && !blocked.ready(), "recording changed an in-flight producer or its host gates");
        start.signal(); r.wait(saved); r.wait(earlier);
        require(!blocked.ready(), "prefix consumer released or waited for the dynamically recorded tail");
        r.discard(first); r.discard(copied);
        tail_gate.signal(); r.wait(blocked); r.discard(tail);
        require(!release.expired.load(), "recording only progressed after its watchdog released a gate");
        for (const auto& output : {snapshot, final, separate}) {
            r.cpu_access(output, false);
            const auto* result = static_cast<const float*>(output.data);
            const uint32_t delta = output.handle == snapshot.handle ? 3 : output.handle == final.handle ? 14 : 7;
            for (size_t j = 0; j < output.bytes / 4; ++j)
                require(result[j] == (j >= prefix && j - prefix < 1024 ? float(j - prefix + 5 * pass + delta) : -12345.f),
                        "dynamic recording changed immutable captures, output or full guards");
        }
        r.cpu_access(wide, false);
        const auto* filled = static_cast<const uint8_t*>(wide.data);
        for (size_t j = 0; j < wide.bytes; ++j)
            require(filled[j] == (j >= 17 && j - 17 < 4099 ? uint8_t((j - 17) % 128 + 7 + pass) : guard),
                    "dynamic wide-fill capture or full guards changed");
        r.cpu_access(input, false);
        for (size_t j = 0; j < input.bytes / 4; ++j)
            require(values[j] == (j >= prefix && j - prefix < 1024 ? float(j - prefix + 5 * pass) : -12345.f),
                    "dynamic recording changed its input or guards");
        if (pass == 3) retained = r.allocated_bytes();
        if (pass > 3) require(r.allocated_bytes() == retained, "dynamic recording backing allocations failed to plateau");
    }
    std::printf("PASS 64 dynamic-recording submissions: 16 blocked-producer waves, record/allocate/create-gate while pending, independent prefix before gated tail, first-use ISA, changed scalar and wide-pattern captures, queue-ledger growth, pool reuse with stable retained_bytes=%llu, exact outputs/inputs and full guards; no full-model or concurrent-kernel performance claim\n",
                static_cast<unsigned long long>(retained));
}
void gpu_module_data(const std::string& path, const std::string& memory) {
    const auto update = detail::load(path, "module_update"), read = detail::load(path, "module_read");
    require(!update.global_data.empty() && !update.constant_data.empty() &&
            update.global_data == read.global_data && update.constant_data == read.constant_data,
            "module fixture needs shared initialized globals and constant data");
    for (bool local : {false, true}) {
        Runtime r; r.set_memory_module(memory, local, local);
        const auto writer = r.default_queue(), reader = r.create_queue();
        constexpr size_t guard = 64;
        const auto output = r.allocate(2 * guard + 8 * sizeof(uint32_t));
        const uint64_t address = output.gpu + guard;
        r.begin(reader);
        r.append(path, "module_read", 8, 1, {Argument::value(address)});
        auto reading = r.finish();
        uint32_t sum = 0; uint64_t plateau = 0;
        for (uint32_t pass = 0; pass <= 16; ++pass) {
            r.cpu_access(output, true);
            auto* values = static_cast<uint32_t*>(output.data);
            std::fill_n(values, output.bytes / 4, 0xdeadbeefu);
            Commands writing;
            if (pass) {
                r.begin(writer);
                r.append(path, "module_update", 4, 1, {Argument::value(pass)});
                writing = r.finish();
                const auto changed = r.submit(writing, writer);
                const auto copied = r.submit(reading, reader, {changed});
                r.wait(copied); r.wait(changed); r.discard(writing);
                sum += pass;
            } else r.wait(r.submit(reading, reader));
            r.cpu_access(output, false);
            constexpr uint32_t initial[4]{3, 5, 7, 11}, coefficient[4]{2, 13, 29, 61};
            for (size_t i = 0; i < output.bytes / 4; ++i) {
                const auto lane = i - guard / 4;
                const auto expected = i < guard / 4 || lane >= 8 ? 0xdeadbeefu :
                    lane < 4 ? initial[lane] + sum * coefficient[lane] : pass;
                require(values[i] == expected, "module constants/global state or output guards changed");
            }
            if (pass == 1) plateau = r.allocated_bytes();
            if (pass > 1) require(r.allocated_bytes() == plateau, "module replays keep allocating storage");
        }
        r.discard(reading);
        std::printf("PASS %s module data: shared initialized/zero-valued globals and volatile constants, two kernels/queues, 16 changing updates, 17 readbacks, persistent state and full guards; stable retained bytes=%llu\n",
                    local ? "device-local" : "host", static_cast<unsigned long long>(plateau));
    }
}
void gpu_reclaim(const std::string& path, const std::string& memory, bool local_isa = false) {
    Runtime r;
    r.set_memory_module(memory, true, local_isa);
    constexpr uint32_t prefix = 16, count = 96, launches = 192;
    const auto data = r.allocate(1040 * sizeof(float)), saved = r.allocate(data.bytes);
    const uint64_t address = data.gpu + prefix * sizeof(float), saved_address = saved.gpu + prefix * sizeof(float);
    const uint32_t seven = 7;
    r.append(path, "advance", 64, 16, {Argument::value(saved_address), Argument::value(seven)});
    auto anchor = r.finish();
    Commands invalid, stale;
    reject([&] { r.discard(invalid); });
    { Runtime other; auto foreign = anchor; reject([&] { other.discard(foreign); }); }
    const auto initialize = [&](const Allocation& a, uint32_t pass) {
        r.cpu_access(a, true);
        auto* values = static_cast<float*>(a.data);
        std::fill_n(values, a.bytes / 4, -12345.f);
        for (uint32_t i = 0; i < 1024; ++i) values[prefix + i] = float(i + pass);
    };
    const auto check = [&](const Allocation& a, const auto& reference) {
        r.cpu_access(a, false);
        const auto* values = static_cast<const float*>(a.data);
        for (size_t i = 0; i < a.bytes / 4; ++i)
            require(values[i] == (i >= prefix && i - prefix < 1024 ? reference(i - prefix) : -12345.f),
                    "retired/reused allocation changed GPU result or full padding");
    };
    uint64_t plateau = 0;
    // Keep one frozen capture alive while other lists roll over and recycle argument slabs.
    for (uint32_t wave = 0; wave < 4; ++wave) {
        initialize(data, wave);
        std::array<Commands, count> lists;
        uint32_t sum = 0;
        for (uint32_t i = 0; i < count; ++i) {
            for (uint32_t j = 0; j < launches; ++j) {
                const uint32_t delta = 1 + (wave + i + j) % 7; sum += delta;
                r.append(path, "advance", 64, 16, {Argument::value(address), Argument::value(delta)});
            }
            lists[i] = r.finish();
        }
        reject([&] { r.submit(stale); }); reject([&] { r.ready(stale); }); reject([&] { r.wait(stale); });
        reject([&] { r.discard(stale); });
        for (const auto& list : lists) r.submit(list);
        reject([&] { r.discard(lists.front()); });
        r.wait(lists.front());
        auto first_alias = lists.front();
        r.discard(lists.front()); // Safe even while later sequences have not been observed complete.
        reject([&] { r.ready(first_alias); });
        r.wait(lists.back());
        check(data, [&](size_t i) { return float(i + wave + sum); });
        stale = lists.back();
        for (uint32_t i = 1; i < count; ++i) r.discard(lists[i]);
        initialize(saved, wave); r.submit(anchor); r.wait(anchor);
        check(saved, [&](size_t i) { return float(i + wave + seven); });
        if (wave == 1) plateau = r.allocated_bytes();
        if (wave >= 2) require(r.allocated_bytes() == plateau, "repeated frozen-list/slab waves keep allocating GPU storage");
        std::printf("RECLAIM_WAVE %u retained_bytes=%llu\n", wave, static_cast<unsigned long long>(r.allocated_bytes()));
    }
    r.discard(anchor);
    // Expire all strong capture references, reuse slots and overwrite wide patterns.
    // Unchanged scalar arguments must be re-captured after a recycled weak lease expires.
    for (uint32_t pass = 0; pass < 512; ++pass) {
        initialize(data, pass);
        std::array<float, 32> pattern;
        for (uint32_t i = 0; i < pattern.size(); ++i) pattern[i] = float(pass * 32 + i);
        const size_t pattern_bytes = pass & 2 ? 128 : 64;
        r.fill(address, pattern.data(), pattern_bytes, 1024 * sizeof(float));
        r.append(path, "advance", 64, 16, {Argument::value(address), Argument::value(seven)});
        if (pass & 1) {
            r.close(); r.submit(); reject([&] { r.reset(); }); r.wait();
        } else {
            auto list = r.finish();
            reject([&] { r.submit(stale); }); reject([&] { r.ready(stale); }); reject([&] { r.wait(stale); });
            reject([&] { r.discard(stale); });
            stale = list; r.submit(list); reject([&] { r.discard(list); }); r.wait(list); r.discard(list);
            reject([&] { r.discard(list); });
        }
        check(data, [&](size_t i) { return pattern[i % (pattern_bytes / 4)] + seven; });
        if (pass & 1) r.reset();
        require(r.allocated_bytes() == plateau, "repeated finish/discard/reset keeps allocating GPU storage");
    }
    r.append(path, "advance", 64, 16, {Argument::value(address), Argument::value(seven)});
    auto unused = r.finish(); auto unused_alias = unused;
    r.discard(unused); reject([&] { r.submit(unused_alias); });
    std::printf("PASS 900 reclamation submissions, four 96-list/18432-capture waves, surviving frozen captures, completed-prefix discard, 512 wide-fill/scalar re-recordings, implicit reset, never-submitted discard, recycled/stale/foreign/default handles, full guards; retained_bytes=%llu stable_after_wave=1\n",
                static_cast<unsigned long long>(plateau));
}
void gpu_completion_modes(const std::string& path) {
    Runtime r;
    constexpr uint32_t count = 16, launches = 25, prefix = 16, host_count = 262144;
    constexpr uint32_t warmups = 128, samples = 512;
    const auto data = r.allocate((1024 + 2 * prefix) * sizeof(float));
    std::array<Commands, count> lists;
    uint32_t sum = 0;
    for (uint32_t i = 0; i < count; ++i) {
        const uint32_t delta = 1 + i % 5;
        const auto address = data.gpu + prefix * sizeof(float);
        for (uint32_t j = 0; j < launches; ++j)
            r.append(path, "advance", 64, 16, {Argument::value(address), Argument::value(delta)});
        sum += launches * delta;
        lists[i] = r.finish();
    }
    std::vector<int16_t> source(host_count);
    std::vector<float> prepared(host_count);
    for (uint32_t i = 0; i < host_count; ++i) source[i] = int16_t(int(i % 1024) - 512);
    using Clock = std::chrono::steady_clock;
    // Each alternative is bracketed by polling controls; retain every sample.
    const std::array<const char*, 9> modes{"poll", "serial", "poll", "tail", "poll", "batch", "poll", "overlap", "poll"};
    for (uint32_t phase = 0; phase < modes.size(); ++phase) {
        const std::string mode = modes[phase];
        for (uint32_t pass = 0; pass < warmups + samples; ++pass) {
            r.cpu_access(data, true);
            auto* values = static_cast<float*>(data.data);
            std::fill_n(values, data.bytes / sizeof(float), -12345.f);
            for (uint32_t i = 0; i < 1024; ++i) values[prefix + i] = float(i + pass + phase);
            std::fill(prepared.begin(), prepared.end(), -99999.f);
            const float scale = float(1 + pass % 7) / 128;
            const auto prepare = [&] {
                // Independent request storage; never touch in-flight GPU data.
                for (uint32_t i = 0; i < host_count; ++i) prepared[i] = float(source[i]) * scale;
            };
            uint64_t cycles_before = 0, cycles_after = 0;
#ifdef _WIN32
            require(QueryThreadCycleTime(GetCurrentThread(), &cycles_before) != 0, "read completion benchmark thread cycles");
#endif
            uint32_t queries = 0, waits = 0;
            std::array<uint64_t, count> tickets{};
            const auto start = Clock::now();
            for (uint32_t i = 0; i < count; ++i) {
                tickets[i] = r.submit(lists[i]);
                if (mode == "serial") { ++waits; r.wait(lists[i]); }
            }
            if (mode == "overlap") prepare();
            if (mode == "tail") { ++waits; r.wait(lists.back()); }
            if (mode == "poll") {
                for (;;) {
                    ++queries;
                    if (r.completed() >= tickets.back()) break;
                    _mm_pause();
                    if (!(queries & 31)) require(Clock::now() - start < std::chrono::seconds(10), "completion polling timeout");
                }
            }
            if (mode == "batch" || mode == "overlap") {
                uint32_t next = 0;
                while (next < count) {
                    ++queries;
                    const auto through = r.completed();
                    // Release the complete prefix immediately, without waiting for a batch to fill.
                    while (next < count && tickets[next] <= through) ++next;
                    if (next < count) { ++waits; r.wait(lists[next]); }
                }
            }
            const auto gpu_done = Clock::now();
            if (mode != "overlap") prepare();
            const auto done = Clock::now();
#ifdef _WIN32
            require(QueryThreadCycleTime(GetCurrentThread(), &cycles_after) != 0 && cycles_after > cycles_before,
                    "read completed benchmark thread cycles");
#endif
            require(r.completed() == tickets.back(), "completion collector missed tail sequence");
            r.cpu_access(data, false);
            for (size_t i = 0; i < data.bytes / sizeof(float); ++i) {
                const auto expected = i >= prefix && i - prefix < 1024 ? float(i - prefix + pass + phase + sum) : -12345.f;
                require(values[i] == expected, "completion mode GPU result/full guard mismatch");
            }
            for (uint32_t i = 0; i < host_count; ++i)
                require(prepared[i] == float(int(i % 1024) - 512) * scale, "completion mode independent host result mismatch");
            if (pass >= warmups) std::printf("COMPLETION_SAMPLE %u %s %u %.3f %.3f %llu %u %u\n", phase, mode.c_str(), pass - warmups,
                std::chrono::duration<double, std::micro>(done - start).count(),
                std::chrono::duration<double, std::micro>(gpu_done - start).count(),
                static_cast<unsigned long long>(cycles_after - cycles_before), queries, waits);
        }
    }
    std::printf("PASS %u completion-mode waves / %u submissions: 16 lists x 25 dependent kernels, serial/poll/tail/batch/overlap, exact GPU/CPU outputs and full guards; initialization/recording excluded, independent CPU work included in total, thread cycles Windows only\n",
                unsigned(modes.size()) * (warmups + samples), unsigned(modes.size()) * (warmups + samples) * count);
}
void gpu_multilist(const std::string& path, const std::string& memory) {
    Runtime r;
    r.set_memory_module(memory);
    constexpr uint32_t count = 40, rounds = 12, prefix = 4;
    const auto device = r.allocate_device(1040 * sizeof(float)), input = r.allocate(device.bytes);
    std::vector<Allocation> outputs;
    std::vector<Commands> lists;
    for (uint32_t i = 0; i < count; ++i) {
        outputs.push_back(r.allocate(device.bytes + 2 * prefix * sizeof(float)));
        if (!i) r.copy(device.gpu, input.gpu, device.bytes);
        const uint32_t delta = 1 + i % 5;
        r.append(path, "advance", 64, 16, {Argument::value(device.gpu), Argument::value(delta)});
        r.copy(outputs.back().gpu + prefix * sizeof(float), device.gpu, device.bytes);
        if (i & 1) r.close(); // finish accepts an already closed list too.
        lists.push_back(r.finish());
    }
    require(r.completed() == 0, "unsubmitted queue reported completion progress");
    for (const auto& list : lists) require(r.ready(list), "unused frozen list is not idle");
    reject([&] { r.submit(); }); reject([&] { r.finish(); });
    reject([&] { r.submit(Commands{}); }); reject([&] { r.ready(Commands{}); }); reject([&] { r.wait(Commands{}); });
    reject([&] { r.wait(lists.front()); });
    Commands stale;
    {
        Runtime other;
        reject([&] { other.submit(lists.front()); }); reject([&] { other.ready(lists.front()); }); reject([&] { other.wait(lists.front()); });
        const auto data = other.allocate(4096);
        const uint32_t delta = 1;
        other.append(path, "advance", 64, 16, {Argument::value(data.gpu), Argument::value(delta)});
        stale = other.finish();
    }
    reject([&] { r.submit(stale); }); reject([&] { r.ready(stale); }); reject([&] { r.wait(stale); });
    const auto alias = lists.front();
    uint64_t last_ticket = 0, seen = 0;
    constexpr float guard = -12345.f;
    for (uint32_t pass = 0; pass < rounds; ++pass) {
        r.cpu_access(input, true);
        auto* source = static_cast<float*>(input.data);
        for (size_t i = 0; i < input.bytes / sizeof(float); ++i) source[i] = float(i + pass * 3);
        for (const auto& output : outputs) {
            r.cpu_access(output, true);
            std::fill_n(static_cast<float*>(output.data), output.bytes / sizeof(float), guard);
        }
        for (const auto& list : lists) {
            const auto ticket = r.submit(list);
            require(ticket == ++last_ticket, "queue sequence is not contiguous");
            reject([&] { r.submit(list); }); // Cached completion must gate list reuse.
        }
        reject([&] { r.cpu_access(input, false); });
        reject([&] { r.finish(); }); reject([&] { r.reset(); });
        if (!pass) {
            bool timed_out = false;
            try { r.wait(lists.back(), 0); } catch (const std::exception&) { timed_out = true; }
            if (timed_out) reject([&] { r.submit(lists.back()); });
        }
        const auto observed = r.completed();
        require(observed >= seen && observed <= last_ticket, "batched completion sequence regressed or skipped unsubmitted work");
        r.wait(lists.back()); // One tail join retires all forty earlier submissions.
        require(r.completed() == last_ticket, "tail join did not retire earlier submissions");
        seen = last_ticket;
        for (const auto& list : lists) require(r.ready(list), "earlier frozen list remained in flight after tail completion");
        require(r.ready(alias), "copied handle lost completion state");
        r.check_scratch_guards(); r.cpu_access(input, false);
        uint32_t delta = 0;
        for (uint32_t list = 0; list < count; ++list) {
            delta += 1 + list % 5;
            r.cpu_access(outputs[list], false);
            const auto* values = static_cast<const float*>(outputs[list].data);
            for (size_t i = 0; i < outputs[list].bytes / sizeof(float); ++i) {
                float expected = guard;
                if (i >= prefix && i - prefix < device.bytes / sizeof(float))
                    expected = float(i - prefix + pass * 3 + (i - prefix < 1024 ? delta : 0));
                require(values[i] == expected, "ordered multilist output or full guard mismatch");
            }
        }
        for (size_t i = 0; i < input.bytes / sizeof(float); ++i)
            require(source[i] == float(i + pass * 3), "multilist pipeline changed host input");
    }
    require(r.submit(alias) == ++last_ticket, "completed alias could not replay");
    reject([&] { r.submit(lists.front()); }); r.wait(lists.front()); r.wait(alias);
    require(r.completed() == last_ticket && r.ready(alias), "replayed alias lost its latest sequence");
    // The original implicit-list API remains usable after many frozen recordings.
    const uint32_t delta = 7;
    r.append(path, "advance", 64, 16, {Argument::value(device.gpu), Argument::value(delta)});
    r.copy(outputs.front().gpu + prefix * sizeof(float), device.gpu, device.bytes);
    r.close(); require(r.submit() == ++last_ticket, "implicit replay lost the shared queue sequence"); r.wait();
    r.cpu_access(outputs.front(), false);
    const auto* result = static_cast<const float*>(outputs.front().data);
    for (size_t i = 0; i < outputs.front().bytes / sizeof(float); ++i) {
        const auto expected = i >= prefix && i - prefix < device.bytes / sizeof(float)
            ? float(i - prefix + (rounds - 1) * 3 + (i - prefix < 1024 ? 8 : 0)) : guard;
        require(result[i] == expected, "frozen-to-implicit replay transition or guards mismatch");
    }
    r.reset();
    std::printf("PASS %llu ordered multilist submissions: 40 frozen lists x 12 rounds, shared local VRAM, immutable captures, one tail join, batch completion sequence, copied/foreign/invalid handle checks, timeout ownership, full guards and implicit-list interoperability\n",
                static_cast<unsigned long long>(last_ticket));
}
void gpu(const char* node, const std::string& path) {
    inspect(path); // Validate the complete image before opening a device.
    {
        Runtime increment(node);
        reject([&] { increment.submit(); });
        reject([&] { increment.ready(); });
        reject([&] { increment.reset(); });
        reject([&] { increment.allocate(0); });
        auto data = increment.allocate(1024 * 4 + 64);
        const uint32_t delta = 3;
        const uint64_t invalid_pointer = data.gpu + data.bytes;
        reject([&] { increment.append(path, "advance", 64, 16, {Argument::value(invalid_pointer), Argument::value(delta)}); });
        const auto first_gate = increment.create_gate(); increment.depend_on(first_gate);
        increment.append(path, "advance", 64, 16, {Argument::value(data.gpu), Argument::value(delta)});
        increment.close(); increment.cpu_access(data, true);
        reject([&] { increment.allocate(4096); });
        reject([&] { increment.close(); });
        auto* values = static_cast<float*>(data.data);
        for (uint32_t i = 0; i < 1040; ++i) values[i] = float(i);
        for (uint32_t pass = 0; pass < 72; ++pass) {
            increment.submit();
            if (!pass) {
                try {
                    reject([&] { increment.submit(); });
                    reject([&] { increment.reset(); });
                    reject([&] { increment.cpu_access(data, false); });
                } catch (...) { first_gate.signal(); throw; }
                first_gate.signal();
            }
            if (pass & 1) {
                const auto start = std::chrono::steady_clock::now();
                while (!increment.ready()) require(std::chrono::steady_clock::now() - start < std::chrono::seconds(10), "poll timeout");
            } else increment.wait();
            increment.cpu_access(data, false);
            for (uint32_t i = 0; i < 1040; ++i)
                require(values[i] == float(i + (i < 1024 ? 3 * (pass + 1) : 0)), "exact multi-group increment/guard mismatch");
        }
        std::puts("PASS 72 exact multi-group increments, hidden IDs, scalar argument and output guards");
        increment.reset();
        for (uint32_t i = 0; i < 400; ++i)
            increment.append(path, "advance", 64, 16, {Argument::value(data.gpu), Argument::value(delta)});
        increment.close(); increment.submit(); increment.wait(); increment.cpu_access(data, false);
        for (uint32_t i = 0; i < 1040; ++i)
            require(values[i] == float(i + (i < 1024 ? 3 * (72 + 400) : 0)), "400-kernel reset/replay or guard mismatch");
        increment.reset();
        const auto timeout_gate = increment.create_gate(); increment.depend_on(timeout_gate);
        const uint32_t different_delta = 5;
        increment.append(path, "advance", 64, 16, {Argument::value(data.gpu), Argument::value(different_delta)});
        increment.close(); increment.submit();
        bool timed_out = false;
        try { increment.wait(0); } catch (const std::exception&) { timed_out = true; }
        try {
            require(timed_out, "closed host gate failed to hold the timeout fixture");
            reject([&] { increment.submit(); }); reject([&] { increment.reset(); });
        } catch (...) { timeout_gate.signal(); throw; }
        timeout_gate.signal(); increment.wait();
        increment.cpu_access(data, false);
        for (uint32_t i = 0; i < 1040; ++i)
            require(values[i] == float(i + (i < 1024 ? 3 * (72 + 400) + 5 : 0)), "reset retained stale scalar capture");
        std::puts("PASS 400-kernel list, changed captures, reset, in-flight rejection and timeout ownership");
    }
    Runtime r(node);
    constexpr uint32_t width = 128, rows = 32;
    struct Parameters { uint32_t width, rows; float scale, epsilon; } params{width, rows, 1, 1e-5f};
    auto input = r.allocate(width * 4), weight = r.allocate(width * 4), norm = r.allocate(width * 4);
    auto matrix = r.allocate(2 * rows * width * 4), middle = r.allocate(2 * rows * 4);
    auto output = r.allocate(rows * 4), parameter = r.allocate(sizeof(params));
    const uint32_t delta = 3;
    r.append(path, "rms_norm", 64, 1, {Argument::value(input.gpu), Argument::value(weight.gpu), Argument::value(norm.gpu), Argument::value(parameter.gpu)});
    r.append(path, "gate_up", 64, 1, {Argument::value(matrix.gpu), Argument::value(norm.gpu), Argument::value(middle.gpu), Argument::value(parameter.gpu)});
    r.append(path, "swiglu", 32, 1, {Argument::value(middle.gpu), Argument::value(output.gpu), Argument::value(parameter.gpu)});
    r.append(path, "advance", 32, 1, {Argument::value(output.gpu), Argument::value(delta)});
    r.close();
    r.cpu_access(input, true); r.cpu_access(weight, true); r.cpu_access(matrix, true);
    auto* x = static_cast<float*>(input.data); auto* wt = static_cast<float*>(weight.data);
    auto* m = static_cast<float*>(matrix.data);
    float square = 0;
    for (uint32_t i = 0; i < width; ++i) { x[i] = float(int(i % 17) - 8) / 16; wt[i] = 0.5f + float(i % 7) / 8; square += x[i] * x[i]; }
    for (uint32_t i = 0; i < 2 * rows * width; ++i) m[i] = float(int(i % 13) - 6) / 16;
    using Clock = std::chrono::steady_clock;
    std::vector<double> submit, complete;
    for (uint32_t pass = 0; pass < 72; ++pass) {
        params.scale = 0.5f + float(pass) / 128;
        r.cpu_access(parameter, true); std::memcpy(parameter.data, &params, sizeof(params));
        const auto start = Clock::now(); r.submit(); const auto sent = Clock::now(); r.wait();
        r.cpu_access(output, false); const auto done = Clock::now();
        const auto* values = static_cast<const float*>(output.data);
        const auto factor = params.scale / std::sqrt(square / width + params.epsilon);
        for (uint32_t row = 0; row < rows; ++row) {
            float gate = 0, up = 0;
            for (uint32_t i = 0; i < width; ++i) {
                const auto v = x[i] * factor * wt[i];
                gate += m[row * width + i] * v; up += m[(rows + row) * width + i] * v;
            }
            const auto expected = gate / (1 + std::exp(-gate)) * up + delta;
            require(std::isfinite(values[row]) && std::abs(values[row] - expected) <= 2e-4f * (1 + std::abs(expected)), "RMS/GEMV/SwiGLU/value-arg pipeline mismatch");
        }
        if (pass >= 8) {
            submit.push_back(std::chrono::duration<double, std::micro>(sent - start).count());
            complete.push_back(std::chrono::duration<double, std::micro>(done - start).count());
            std::printf("SAMPLE %u %.3f %.3f\n", pass - 8, submit.back(), complete.back());
        }
    }
    std::sort(submit.begin(), submit.end()); std::sort(complete.begin(), complete.end());
    std::printf("PASS 72 DG2 zebin pipeline replays; median submit %.3f us, complete %.3f us; preparation excluded\n",
                (submit[31] + submit[32]) / 2, (complete[31] + complete[32]) / 2);
}
// Use tests/denorm.cl baked both with and without -cl-denorms-are-zero.
void gpu_denorm(const std::string& module, const std::string& mode) {
    require(mode == "retain" || mode == "flush", "denorm mode must be retain or flush");
    Runtime r;
    constexpr size_t guard = 16, count = 32;
    const auto input = r.allocate((2 * count + 2 * guard) * 4), output = r.allocate(input.bytes);
    const uint32_t cases[4][2] = {{1, 0x4b000000}, {0x00800000, 0x3f000000},
                                  {0x3fc00000, 0x40000000}, {0x80000000, 0x3f800000}};
    const uint32_t retained[4] = {0x00800000, 0x00400000, 0x40400000, 0x80000000};
    const uint64_t source = input.gpu + guard * 4, destination = output.gpu + guard * 4;
    for (auto queue : {r.default_queue(), r.create_queue()}) {
        r.begin(queue);
        r.append(module, "denorm_mul", 32, 1, {Argument::value(source), Argument::value(destination)});
        auto list = r.finish();
        for (unsigned pass = 0; pass < 3; ++pass) {
            r.cpu_access(input, true); r.cpu_access(output, true);
            std::vector<uint32_t> original(input.bytes / 4, 0x69696969), expected(output.bytes / 4, 0xa7a7a7a7);
            for (size_t i = 0; i < count; ++i) {
                const auto index = (i + pass) % 4;
                const uint32_t sign = i & 8 ? 0x80000000u : 0;
                original[guard + 2 * i] = cases[index][0] ^ sign;
                original[guard + 2 * i + 1] = cases[index][1];
                expected[guard + i] = ((mode == "flush" && index < 2) ? 0 : retained[index]) ^ sign;
            }
            std::memcpy(input.data, original.data(), input.bytes);
            std::memset(output.data, 0xa7, output.bytes);
            const auto completion = r.submit(list, queue); r.wait(completion);
            r.cpu_access(input, false); r.cpu_access(output, false);
            require(!std::memcmp(output.data, expected.data(), output.bytes), "FP32 denorm compiler contract or output guards mismatch");
            require(!std::memcmp(input.data, original.data(), input.bytes), "denorm input or guards changed");
        }
        r.discard(list);
    }
    std::puts("PASS FP32 compiler denorm contract: 192 signed normal/subnormal/underflow/zero products, six overwritten replays, two queues and full guards");
}
void gpu_timestamps(uint32_t frequency, uint32_t bits) {
    require(frequency && frequency <= 1000000000 && bits && bits <= 64, "invalid timestamp calibration");
    Runtime r;
    auto tags = r.allocate(4096);
    auto gate = r.create_gate(true);
    reject([&] { r.write_global_timestamp(0); });
    reject([&] { r.write_global_timestamp(tags.gpu + 4); });
    reject([&] { r.write_global_timestamp(0x1000); });
    const uint64_t mask = bits == 64 ? UINT64_MAX : (uint64_t{1} << bits) - 1;
    require(((uint64_t{3} - uint64_t{254}) & uint64_t{255}) == 5, "timestamp wrap arithmetic mismatch");
    using Clock = std::chrono::steady_clock;
    for (const auto queue : {r.default_queue(), r.create_queue()}) {
        r.begin(queue); r.write_global_timestamp(tags.gpu + 64); const auto first = r.finish();
        r.begin(queue); r.depend_on(gate); r.write_global_timestamp(tags.gpu + 128); const auto last = r.finish();
        for (uint32_t hold : {15u, 50u, 15u, 50u}) {
            r.cpu_access(tags, true);
            std::fill_n(static_cast<uint64_t*>(tags.data), tags.bytes / 8, uint64_t{0xdeadbeefcafef00dull});
            const auto start = Clock::now();
            const auto a = r.submit(first, queue); r.wait(a);
            const auto b = r.submit(last, queue);
            require(!b.ready(), "timestamp tail passed its unsignaled gate");
            const auto hold_start = Clock::now();
            std::this_thread::sleep_for(std::chrono::milliseconds(hold));
            const auto held_ms = std::chrono::duration<double, std::milli>(Clock::now() - hold_start).count();
            gate.signal(); r.wait(b);
            const auto wall_ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
            r.cpu_access(tags, false);
            const auto* values = static_cast<const uint64_t*>(tags.data);
            const auto ticks = (values[16] - values[8]) & mask;
            const auto device_ms = double(ticks) * 1000 / frequency;
            require(values[8] != 0xdeadbeefcafef00dull && values[16] != 0xdeadbeefcafef00dull && ticks &&
                    device_ms >= held_ms * .9 && device_ms <= wall_ms + 2,
                    "timestamp domain, scale, ordering or replay overwrite mismatch");
            for (size_t i = 0; i < tags.bytes / 8; ++i)
                if (i != 8 && i != 16) require(values[i] == 0xdeadbeefcafef00dull, "timestamp overwrote a guard");
            std::printf("TIMESTAMP hold_ms=%.6f device_ms=%.6f wall_ms=%.6f start=%llu end=%llu frequency=%u bits=%u\n",
                held_ms, device_ms, wall_ms, static_cast<unsigned long long>(values[8]),
                static_cast<unsigned long long>(values[16]), frequency, bits);
            r.reset_gate(gate);
        }
        auto a = first, b = last; r.discard(a); r.discard(b);
    }
    r.write_global_timestamp(tags.gpu + 64); r.close();
    reject([&] { r.write_global_timestamp(tags.gpu + 128); });
    std::puts("PASS native raw timestamps: two queues, eight gated replays, overwritten captures, full guards, range/alignment/closed-list rejection and calibrated clock scale");
}
} // namespace
int main(int argc, char** argv) try {
    dependencies();
    if (argc == 2 && std::string(argv[1]) == "--self-check") self_check();
    else if (argc == 4 && std::string(argv[1]) == "--gpu-timestamps") gpu_timestamps(detail::number(argv[2]), detail::number(argv[3]));
    else if (argc == 4 && std::string(argv[1]) == "--gpu-denorm") gpu_denorm(argv[2], argv[3]);
    else if (argc == 3 && std::string(argv[1]) == "--inspect") inspect(argv[2]);
    else if (argc == 3 && std::string(argv[1]) == "--inspect-pack") inspect_pack(argv[2]);
    else if (argc == 3 && std::string(argv[1]) == "--strata-gate") strata_gate(argv[2]);
    else if (argc == 3 && std::string(argv[1]) == "--strata-gemv") strata_gemv(argv[2]);
    else if (argc == 4 && std::string(argv[1]) == "--strata-device") { strata_device(argv[3]); strata_gemv(argv[2], true, argv[3]); strata_xmx(argv[2], true, argv[3]); }
    else if (argc == 4 && std::string(argv[1]) == "--strata-thread-trim") strata_thread_trim(argv[2], argv[3]);
    else if (argc == 3 && std::string(argv[1]) == "--strata-scratch") strata_scratch(argv[2]);
    else if (argc == 3 && std::string(argv[1]) == "--strata-private") strata_private(argv[2]);
    else if (argc == 3 && std::string(argv[1]) == "--strata-multiqueue-memory") strata_multiqueue_memory(argv[2]);
    else if (argc == 4 && std::string(argv[1]) == "--strata-multiqueue-memory") strata_multiqueue_memory(argv[2], argv[3]);
    else if (argc == 4 && std::string(argv[1]) == "--strata-multiqueue-isa") {
        strata_multiqueue_memory(argv[2], argv[3], false, true);
        strata_multiqueue_memory(argv[2], argv[3], true, true);
    }
    else if (argc == 3 && std::string(argv[1]) == "--strata-atomics") strata_atomics(argv[2]);
    else if (argc == 3 && std::string(argv[1]) == "--strata-xmx") strata_xmx(argv[2]);
    else if (argc == 3 && std::string(argv[1]) == "--gpu") gpu(nullptr, argv[2]);
    else if (argc == 3 && std::string(argv[1]) == "--gpu-captures") gpu_captures(argv[2]);
    else if (argc == 4 && std::string(argv[1]) == "--gpu-captures") gpu_captures(argv[2], argv[3]);
    else if (argc == 3 && std::string(argv[1]) == "--gpu-3d") gpu_3d(argv[2]);
    else if (argc == 3 && std::string(argv[1]) == "--gpu-semisync") gpu_semisync(argv[2]);
    else if (argc == 3 && std::string(argv[1]) == "--gpu-completion-modes") gpu_completion_modes(argv[2]);
    else if (argc == 3 && std::string(argv[1]) == "--gpu-memory") gpu_memory(argv[2]);
    else if (argc == 3 && std::string(argv[1]) == "--gpu-large-device-memory") gpu_large_device_memory(argv[2]);
    else if (argc == 4 && std::string(argv[1]) == "--gpu-release") gpu_release(argv[2], argv[3]);
    else if (argc == 3 && std::string(argv[1]) == "--gpu-transfers") gpu_transfers(argv[2]);
    else if (argc == 4 && std::string(argv[1]) == "--gpu-multilist") gpu_multilist(argv[2], argv[3]);
    else if (argc == 4 && std::string(argv[1]) == "--gpu-reclaim") gpu_reclaim(argv[2], argv[3]);
    else if (argc == 4 && std::string(argv[1]) == "--gpu-reclaim-isa") gpu_reclaim(argv[2], argv[3], true);
    else if (argc == 4 && std::string(argv[1]) == "--gpu-module-data") gpu_module_data(argv[2], argv[3]);
    else if (argc == 4 && std::string(argv[1]) == "--gpu-multiqueue") gpu_multiqueue(argv[2], argv[3]);
    else if (argc == 4 && std::string(argv[1]) == "--gpu-host-gate") gpu_host_gate(argv[2], argv[3]);
    else if (argc == 4 && std::string(argv[1]) == "--gpu-inline-events") gpu_inline_events(argv[2], argv[3]);
    else if (argc == 4 && std::string(argv[1]) == "--gpu-observers") gpu_observers(argv[2], argv[3]);
    else if (argc == 4 && std::string(argv[1]) == "--gpu-recording") gpu_recording(argv[2], argv[3]);
    else if (argc == 3) gpu(argv[1], argv[2]);
    else throw std::invalid_argument("usage: dg2-check <--self-check | --inspect zebin | --inspect-pack directory | --strata-gate pack | --strata-gemv pack | --strata-device pack memory.zebin | --strata-scratch pack | --strata-private pack | --strata-multiqueue-memory pack | --strata-atomics pack | --strata-xmx pack | --gpu-3d zebin | --gpu-captures zebin | --gpu-semisync zebin | --gpu-completion-modes zebin | --gpu-memory zebin | --gpu-transfers memory.zebin | --gpu-multilist zebin memory.zebin | --gpu-multiqueue zebin memory.zebin | --gpu-host-gate zebin memory.zebin | --gpu-observers zebin memory.zebin | --gpu-recording zebin memory.zebin | --gpu-reclaim zebin memory.zebin | --gpu zebin | render-node zebin>");
    dependencies();
    return 0;
} catch (const std::exception& e) { std::fprintf(stderr, "%s\n", e.what()); return 1; }
