#pragma once
#include <orca_l0/dg2.hpp>
#include "encoding.hpp"
#include <tuple>
#include <deque>
#include <memory_resource>

namespace orca::l0::drm::detail {
struct RecordedImage {
    Kernel kernel;
    dg2::Allocation isa;
    std::weak_ptr<const dg2::Allocation> indirect;
    uint32_t isa_offset = 0, indirect_offset = 0;
    std::vector<uint8_t> last_indirect;
    bool last_local_parameters = false;
    std::array<uint32_t, 3> last_local{};
    std::vector<size_t> resident_hints;
};
inline RecordedImage& recorded_image(std::map<std::pair<std::string, std::string>, RecordedImage>& images,
                                     const std::string& path, const std::string& name) {
    const auto key = std::make_pair(path, name);
    auto image = images.find(key);
    if (image == images.end()) {
        RecordedImage entry; entry.kernel = load(path, name);
        image = images.emplace(key, std::move(entry)).first;
    }
    return image->second;
}
inline uint32_t suggest_group_size(const Kernel& k, uint32_t global) {
    require(global, "empty range launch");
    if (k.required_local[0]) {
        require(k.required_local[1] == 1 && k.required_local[2] == 1 && !(global % k.required_local[0]),
                "range does not match the required local size");
        return k.required_local[0];
    }
    uint32_t local = 1;
    while (local < 256 && !(global % (local * 2))) local *= 2;
    return local;
}
struct RecordingHeap { dg2::Allocation allocation; size_t used = 0; };
struct CaptureHeap { std::shared_ptr<const dg2::Allocation> allocation; size_t used = 0; };
struct RecordingStorage {
    dg2::Allocation allocation;
    std::vector<uint64_t> last_use; // One submitted sequence per queue; updated without allocation on replay.
    size_t recording_index = SIZE_MAX;
};
inline void retain_storage(std::vector<RecordingStorage*>& used, RecordingStorage* memory) {
    if (memory->recording_index < used.size() && used[memory->recording_index] == memory) return;
    // ponytail: linear fallback handles new/interleaved lists; use per-list indexing only if fallback dominates.
    const auto found = std::find(used.begin(), used.end(), memory);
    memory->recording_index = size_t(found - used.begin());
    if (found == used.end()) used.push_back(memory);
}
struct AllocationPool {
    std::pmr::unsynchronized_pool_resource leases;
    std::vector<dg2::Allocation> available;
    size_t count = 0;
    struct Lease {
        dg2::Allocation allocation;
        AllocationPool* pool = nullptr;
        ~Lease() { if (pool) pool->available.push_back(allocation); }
    };
    template<class Allocate>
    std::shared_ptr<const dg2::Allocation> take(Allocate&& allocate, size_t bytes) {
        // ponytail: one owner recycles fixed-size lease/control blocks; pools outlive all strong and weak leases.
        auto lease = std::allocate_shared<Lease>(std::pmr::polymorphic_allocator<Lease>{&leases});
        auto& node = lease->allocation;
        size_t best = SIZE_MAX;
        for (size_t i = 0; i < available.size(); ++i)
            if (available[i].bytes >= bytes && (best == SIZE_MAX || available[i].bytes < available[best].bytes)) best = i;
        if (best != SIZE_MAX) {
            node = available[best]; available[best] = available.back(); available.pop_back();
        } else {
            // Reserve space for every eventual return; the lease deleter cannot allocate or throw.
            available.reserve(count + 1);
            node = allocate(bytes); ++count;
        }
        // Leases remain internal to one host owner. Declare pools before their leases in State.
        lease->pool = this;
        return std::shared_ptr<const dg2::Allocation>(lease, &node);
    }
};
inline std::vector<uint8_t> indirect_payload(const Kernel& k, const std::vector<uint8_t>& cross,
                                             std::array<uint32_t, 3> local) {
    const auto threads = workgroup_threads(local, {1,1,1});
    require(cross.size() == k.cross_bytes, "invalid cross-thread payload");
    const auto inline_bytes = std::min(k.inline_bytes, k.cross_bytes), cross_bytes = k.cross_bytes - inline_bytes;
    const auto local_bytes = local_id_payload_bytes(k, local);
    std::vector<uint8_t> payload(align(cross_bytes + local_bytes, 64));
    if (cross_bytes) std::memcpy(payload.data(), cross.data() + inline_bytes, cross_bytes);
    if (k.packed_local_ids) for (uint32_t i = 0; i < threads; ++i) {
        const std::array<uint16_t, 3> xyz{uint16_t(i % local[0]), uint16_t(i / local[0] % local[1]), uint16_t(i / (local[0] * local[1]))};
        std::memcpy(payload.data() + cross_bytes + i * 32, xyz.data(), sizeof(xyz));
    }
    else if (local_bytes) for (uint32_t i = 0; i < threads; ++i) {
        const std::array<uint16_t, 3> xyz{uint16_t(i % local[0]), uint16_t(i / local[0] % local[1]), uint16_t(i / (local[0] * local[1]))};
        const auto base = cross_bytes + (i / k.simd) * k.local_id_bytes;
        for (size_t d = 0; d < 3; ++d)
            std::memcpy(payload.data() + base + (d * std::max(16u, k.simd) + i % k.simd) * 2, &xyz[d], 2);
    }
    return payload;
}
inline std::vector<uint8_t> indirect_payload(const RecordedImage& entry, const std::vector<uint8_t>& cross,
                                             std::array<uint32_t, 3> local) {
    if (entry.last_local != local || entry.last_indirect.empty()) return indirect_payload(entry.kernel, cross, local);
    require(cross.size() == entry.kernel.cross_bytes, "invalid cross-thread payload");
    // ponytail: reuse this kernel's last local-ID layout; regenerate when the local shape changes.
    auto payload = entry.last_indirect;
    const auto inline_bytes = std::min(entry.kernel.inline_bytes, entry.kernel.cross_bytes);
    const auto cross_bytes = entry.kernel.cross_bytes - inline_bytes;
    if (cross_bytes) std::memcpy(payload.data(), cross.data() + inline_bytes, cross_bytes);
    return payload;
}
struct ModuleData { dg2::Allocation globals, constants; };
struct ThreadMemory {
    dg2::Allocation allocation;
    size_t payload_bytes = 0;
    uint32_t bytes_per_thread = 0;
};
constexpr size_t thread_memory_guard = 65536;
constexpr uint8_t thread_memory_sentinel = 0x6d;
// Hardware thread IDs include disabled EUs: reserve DG2's 512 EUs × 8 physical slots.
constexpr uint32_t thread_memory_slots = 4096;
inline bool resident_gpu_address(uint64_t address, const std::vector<dg2::Allocation>& allocations, size_t& hint) {
    const auto contains = [&](const dg2::Allocation& a) { return address >= a.gpu && address - a.gpu < a.bytes; };
    if (hint < allocations.size() && contains(allocations[hint])) return true;
    // ponytail: revalidate after removal; new/alternating ranges scan linearly, index ranges if misses dominate.
    const auto found = std::find_if(allocations.begin(), allocations.end(), contains);
    hint = size_t(found - allocations.begin());
    return found != allocations.end();
}
inline void include_cpu_mapping(std::array<uint64_t, 2>& bounds, const dg2::Allocation& a) {
    if (!a.data || !a.bytes) return;
    const auto base = uint64_t(reinterpret_cast<uintptr_t>(a.data));
    bounds[0] = std::min(bounds[0], base);
    bounds[1] = std::max(bounds[1], base + std::min<uint64_t>(a.bytes - 1, UINT64_MAX - base));
}
inline uint64_t gpu_address(uint64_t address, const std::vector<dg2::Allocation>& allocations, size_t* resident_hint = nullptr,
                            const std::array<uint64_t, 2>* cpu_bounds = nullptr) {
    if (!address) return 0;
    // ponytail: keep bounds after frees; gaps still scan, index CPU ranges if those misses dominate.
    if (!cpu_bounds || (address >= (*cpu_bounds)[0] && address <= (*cpu_bounds)[1])) {
        for (const auto& a : allocations) if (a.data) {
            const auto cpu = uint64_t(reinterpret_cast<uintptr_t>(a.data));
            if (address >= cpu && address - cpu < a.bytes) return a.gpu + (address - cpu);
        }
    }
    require(resident_hint ? resident_gpu_address(address, allocations, *resident_hint) :
        std::any_of(allocations.begin(), allocations.end(), [&](const dg2::Allocation& a) {
        return address >= a.gpu && address - a.gpu < a.bytes;
    }), "capture pointer is not a resident CPU mapping or GPU VA");
    return address;
}
inline void validate_gpu_range(uint64_t address, size_t bytes, const std::vector<dg2::Allocation>& allocations) {
    require(address && bytes && bytes <= UINT64_MAX - address &&
            std::any_of(allocations.begin(), allocations.end(), [&](const dg2::Allocation& a) {
                return address >= a.gpu && address - a.gpu < a.bytes && bytes <= a.bytes - (address - a.gpu);
            }), "memory operation range is not within one resident allocation");
}
inline void validate_host_import(void* data, size_t bytes, const std::vector<dg2::Allocation>& allocations) {
    const auto address = reinterpret_cast<uintptr_t>(data);
    require(address && !(address & 65535) && bytes && !(bytes & 65535) && bytes <= UINTPTR_MAX - address,
            "host import requires a non-overflowing 64 KiB aligned address and size");
    for (const auto& a : allocations) {
        if (!a.data) continue;
        const auto base = reinterpret_cast<uintptr_t>(a.data);
        require(address >= base ? address - base >= a.bytes : base - address >= bytes,
                "host import overlaps an existing allocation");
    }
}
inline uint32_t local_memory_bytes(const Kernel& k, const dg2::Argument* args, size_t argument_count) {
    require(args || !argument_count, "null kernel argument list");
    size_t bytes = k.slm;
    for (const auto& p : k.payload) if (p.mode == "slm") {
        require(p.index < argument_count && !args[p.index].data && args[p.index].bytes, "local argument requires Argument::local(nonzero bytes)");
        bytes = align(bytes, p.alignment);
        require(bytes <= 65536 && args[p.index].bytes <= 65536 - bytes, "local memory exceeds DG2 limit");
        bytes += args[p.index].bytes;
    }
    return uint32_t(bytes);
}
inline uint32_t local_memory_bytes(const Kernel& k, const std::vector<dg2::Argument>& args) {
    return local_memory_bytes(k, args.data(), args.size());
}
inline std::vector<uint8_t> pack_arguments(const Kernel& k, std::array<uint32_t, 3> local, std::array<uint32_t, 3> groups,
        const dg2::Argument* args, size_t argument_count, const std::vector<dg2::Allocation>& allocations,
        uint64_t global_base = 0, uint64_t constant_base = 0, uint64_t private_base = 0,
        std::vector<size_t>* resident_hints = nullptr, const std::array<uint64_t, 2>* cpu_bounds = nullptr) {
    workgroup_threads(local, groups);
    require(!k.required_local[0] || k.required_local == local, "required local size mismatch");
    uint32_t count = 0;
    for (const auto& p : k.payload) if (p.index != UINT32_MAX) count = std::max(count, p.index + 1);
    require(argument_count == count, "kernel argument count mismatch");
    local_memory_bytes(k, args, argument_count);
    if (resident_hints) resident_hints->resize(k.payload.size(), SIZE_MAX);
    size_t slm_offset = k.slm;
    std::vector<uint8_t> cross(k.cross_bytes);
    for (const auto& p : k.payload) {
        auto* target = cross.data() + p.offset;
        if (p.index != UINT32_MAX) {
            const auto a = args[p.index];
            if (p.mode == "slm") {
                slm_offset = align(slm_offset, p.alignment);
                const uint64_t offset = slm_offset;
                std::memcpy(target, &offset, sizeof(offset));
                slm_offset += a.bytes;
                continue;
            }
            require(a.data && p.source <= a.bytes && p.size <= a.bytes - p.source, "kernel argument size mismatch");
            if (p.type == "arg_bypointer" || p.is_pointer) {
                uint64_t va = 0; std::memcpy(&va, static_cast<const uint8_t*>(a.data) + p.source, sizeof(va));
                auto* hint = resident_hints ? &(*resident_hints)[size_t(&p - k.payload.data())] : nullptr;
                if (a.captured_pointers) {
                    va = gpu_address(va, allocations, hint, cpu_bounds);
                    std::memcpy(target, &va, sizeof(va));
                    continue;
                }
                require(va == 0 || (hint ? resident_gpu_address(va, allocations, *hint) :
                    std::any_of(allocations.begin(), allocations.end(), [&](const dg2::Allocation& bo) {
                    return va >= bo.gpu && va - bo.gpu < bo.bytes;
                })), "pointer argument is not a resident GPU VA");
            }
            std::memcpy(target, static_cast<const uint8_t*>(a.data) + p.source, p.size);
        } else if (p.type == "global_base" || p.type == "const_base" || p.type == "private_base_stateless") {
            const auto va = p.type == "global_base" ? global_base : p.type == "const_base" ? constant_base : private_base;
            std::memcpy(target, &va, sizeof(va));
        } else {
            std::array<uint32_t, 3> v{};
            if (p.type == "local_size" || p.type == "enqueued_local_size") v = local;
            if (p.type == "global_size") for (size_t d = 0; d < 3; ++d) v[d] = local[d] * groups[d];
            if (p.type == "group_count") v = groups;
            std::memcpy(target, v.data(), sizeof(v));
        }
    }
    return cross;
}
inline std::vector<uint8_t> pack_arguments(const Kernel& k, std::array<uint32_t, 3> local, std::array<uint32_t, 3> groups,
        const std::vector<dg2::Argument>& args, const std::vector<dg2::Allocation>& allocations,
        uint64_t global_base = 0, uint64_t constant_base = 0, uint64_t private_base = 0,
        std::vector<size_t>* resident_hints = nullptr, const std::array<uint64_t, 2>* cpu_bounds = nullptr) {
    return pack_arguments(k, local, groups, args.data(), args.size(), allocations,
                          global_base, constant_base, private_base, resident_hints, cpu_bounds);
}
inline void bind_base_arguments(const Kernel& k, std::vector<uint8_t>& cross,
                                uint64_t global_base, uint64_t constant_base, uint64_t private_base) {
    require(cross.size() == k.cross_bytes, "invalid cross-thread payload");
    // ponytail: arguments are already validated; only these bases become known after resource allocation.
    for (const auto& p : k.payload) if (p.index == UINT32_MAX &&
            (p.type == "global_base" || p.type == "const_base" || p.type == "private_base_stateless")) {
        const auto va = p.type == "global_base" ? global_base : p.type == "const_base" ? constant_base : private_base;
        std::memcpy(cross.data() + p.offset, &va, sizeof(va));
    }
}
inline std::vector<uint8_t> pack_arguments(const Kernel& k, uint32_t local, uint32_t groups,
        const std::vector<dg2::Argument>& args, const std::vector<dg2::Allocation>& allocations,
        uint64_t global_base = 0, uint64_t constant_base = 0, uint64_t private_base = 0,
        std::vector<size_t>* resident_hints = nullptr, const std::array<uint64_t, 2>* cpu_bounds = nullptr) {
    return pack_arguments(k, {local, 1, 1}, {groups, 1, 1}, args, allocations, global_base, constant_base, private_base, resident_hints, cpu_bounds);
}
} // namespace orca::l0::drm::detail
