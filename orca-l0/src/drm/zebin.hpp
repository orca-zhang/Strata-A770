#pragma once
// Deliberately bounded ELF64 / ze_info subset, not a general YAML reader.
// Unsupported kernel ABI features fail before recording/submitting a kernel.
#include <algorithm>
#include <cstring>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <cstdint>
#include <array>

namespace orca::l0::drm::detail {
inline void require(bool ok, std::string_view why) {
    if (!ok) throw std::runtime_error("DG2: " + std::string(why));
}
inline size_t align(size_t n, size_t a) {
    require(n <= SIZE_MAX - (a - 1), "size overflow");
    return (n + a - 1) & ~(a - 1);
}
inline std::string trim(std::string s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    return first == s.npos ? "" : s.substr(first, s.find_last_not_of(" \t\r\n") - first + 1);
}
inline uint32_t number(const std::string& s) {
    require(!s.empty() && s.find_first_not_of("0123456789") == s.npos, "expected unsigned decimal, got " + s);
    const auto value = std::stoull(s);
    require(value <= UINT32_MAX, "integer overflow");
    return uint32_t(value);
}
struct Payload {
    std::string type, mode, space;
    uint32_t offset = UINT32_MAX, size = 0, index = UINT32_MAX, source = 0, alignment = 0;
    bool is_pointer = false;
};
struct Kernel {
    std::vector<uint8_t> isa;
    std::vector<uint8_t> global_data, constant_data;
    std::vector<Payload> payload;
    uint32_t simd = 0, grf = 0, slm = 0, barriers = 0, skip = 0, inline_bytes = 0;
    uint32_t cross_bytes = 0, local_id_bytes = 0;
    std::array<uint32_t, 3> required_local{};
    std::array<uint32_t, 2> scratch_bytes{};
    uint32_t private_bytes = 0;
    bool systolic = false, disable_eu_fusion = false, packed_local_ids = false;
};
inline uint32_t workgroup_threads(const std::array<uint32_t, 3>& local, const std::array<uint32_t, 3>& groups) {
    uint32_t threads = 1, total = 1;
    for (size_t d = 0; d < 3; ++d) {
        require(local[d] && local[d] <= 1024 / threads && groups[d] &&
                uint64_t(local[d]) * groups[d] <= UINT32_MAX / total, "invalid or overflowing 3D dispatch size");
        threads *= local[d]; total *= local[d] * groups[d];
    }
    return threads;
}
inline bool hardware_local_ids(const Kernel& k, const std::array<uint32_t, 3>& local) {
    // Walk order X,Y,Z requires the first two dimensions to be powers of two.
    return k.local_id_bytes && !k.packed_local_ids &&
        !(local[0] & (local[0] - 1)) && !(local[1] & (local[1] - 1));
}
inline size_t local_id_payload_bytes(const Kernel& k, const std::array<uint32_t, 3>& local) {
    const auto threads = workgroup_threads(local, {1,1,1});
    if (k.packed_local_ids) return threads * 32;
    return k.local_id_bytes && !hardware_local_ids(k, local) ?
        (threads + k.simd - 1) / k.simd * k.local_id_bytes : 0;
}
inline std::array<uint32_t, 3> required_local_size(std::string value) {
    value.erase(std::remove(value.begin(), value.end(), ' '), value.end());
    require(value.size() >= 7 && value.front() == '[' && value.back() == ']', "invalid required local size");
    std::array<uint32_t, 3> local{};
    size_t start = 1;
    for (size_t d = 0; d < 3; ++d) {
        const auto end = value.find(d == 2 ? ']' : ',', start);
        require(end != value.npos, "required local size needs three dimensions");
        local[d] = number(value.substr(start, end - start)); start = end + 1;
    }
    require(start == value.size(), "trailing text in required local size");
    workgroup_threads(local, {1, 1, 1});
    return local;
}
using Fields = std::map<std::string, std::string>;
inline uint32_t get(const Fields& f, const char* key, uint32_t fallback = 0) {
    auto i = f.find(key);
    return i == f.end() ? fallback : number(i->second);
}
inline Kernel metadata(const std::string& yaml, const std::string& name) {
    require(yaml.size() <= 4 * 1024 * 1024 && yaml.find('\0') == yaml.npos, "invalid ze_info size/text");
    Kernel k;
    Fields env;
    std::vector<Fields> memory_buffers;
    Fields* memory_buffer = nullptr;
    std::string section, version;
    uint32_t required_simd = 0;
    bool active = false, found = false, kernels = false, local_ids_seen = false;
    Payload* arg = nullptr;
    for (size_t start = 0; start < yaml.size();) {
        auto end = yaml.find('\n', start);
        if (end == yaml.npos) end = yaml.size();
        std::string raw = yaml.substr(start, end - start);
        start = end + 1;
        auto line = trim(raw);
        if (line.empty() || line == "---" || line == "...") continue;
        const auto indent = raw.find_first_not_of(' ');
        require(raw.find('\t') == raw.npos, "tabs in ze_info");
        auto colon = line.find(':');
        require(colon != line.npos, "malformed ze_info line");
        auto key = trim(line.substr(0, colon)), value = trim(line.substr(colon + 1));
        if (indent == 0) {
            if (key == "version") version = value;
            kernels = key == "kernels";
            active = false;
            continue;
        }
        if (!kernels) continue; // kernels_misc_info describes source names, not execution ABI.
        if (indent == 2 && key == "- name") {
            active = value == name;
            if (active) { require(!found, "duplicate kernel name"); found = true; }
            section.clear(); arg = nullptr; memory_buffer = nullptr;
            continue;
        }
        if (!active) continue;
        if (indent == 4) {
            require(value.empty(), "unexpected kernel scalar");
            require(key == "execution_env" || key == "payload_arguments" ||
                    key == "per_thread_payload_arguments" || key == "per_thread_memory_buffers" || key == "user_attributes", "unsupported kernel section " + key);
            section = key; arg = nullptr; memory_buffer = nullptr; continue;
        }
        if (section == "execution_env") {
            require(indent == 6 && env.emplace(key, value).second, "malformed/duplicate execution field");
        } else if (section == "user_attributes") {
            value.erase(std::remove(value.begin(), value.end(), ' '), value.end());
            if (key == "intel_reqd_workgroup_walk_order") require(value == "[0,1,2]", "unsupported local-ID walk order");
            else if (key == "intel_reqd_sub_group_size") {
                required_simd = number(value);
                require(required_simd == 8 || required_simd == 16 || required_simd == 32, "invalid required subgroup size");
            }
            else if (key == "reqd_work_group_size") {
                require(!k.required_local[0], "duplicate required local size");
                k.required_local = required_local_size(value);
            } else require(false, "unsupported user attribute " + key);
        } else if (section == "per_thread_payload_arguments") {
            if (key == "- arg_type") {
                require((value == "local_id" || value == "packed_local_ids") && !local_ids_seen, "unsupported/duplicate per-thread payload");
                local_ids_seen = true; k.packed_local_ids = value == "packed_local_ids";
            }
            else if (key == "offset") require(local_ids_seen && number(value) == 0, "invalid local-ID offset");
            else if (key == "size") { require(local_ids_seen && !k.local_id_bytes, "invalid/duplicate local-ID size"); k.local_id_bytes = number(value); }
            else require(false, "unsupported per-thread field " + key);
        } else if (section == "per_thread_memory_buffers") {
            if (key == "- type") {
                require(indent == 6, "invalid memory-buffer indentation");
                memory_buffers.emplace_back(); memory_buffer = &memory_buffers.back();
                memory_buffer->emplace("type", value);
            } else {
                require(memory_buffer && indent == 8 && (key == "usage" || key == "size" || key == "slot" || key == "is_simt_thread") &&
                        memory_buffer->emplace(key, value).second, "unknown/duplicate memory-buffer field");
            }
        } else if (section == "payload_arguments") {
            if (key == "- arg_type") { k.payload.push_back({}); arg = &k.payload.back(); arg->type = value; }
            else {
                require(arg != nullptr, "payload field before argument");
                if (key == "offset") arg->offset = number(value);
                else if (key == "size") arg->size = number(value);
                else if (key == "arg_index") arg->index = number(value);
                else if (key == "source_offset") arg->source = number(value);
                else if (key == "slm_alignment") arg->alignment = number(value);
                else if (key == "addrmode") arg->mode = value;
                else if (key == "addrspace") arg->space = value;
                else if (key == "access_type") require(value == "readonly" || value == "readwrite" || value == "writeonly", "invalid access type");
                else if (key == "is_ptr") { require(value == "true", "invalid pointer flag"); arg->is_pointer = true; }
                else require(false, "unsupported payload field " + key);
            }
        } else require(false, "field outside an ABI section");
    }
    require(version == "'1.73'" || version == "1.73", "unsupported ze_info version " + version);
    require(found, "kernel not found: " + name);
    k.simd = get(env, "simd_size"); k.grf = get(env, "grf_count");
    k.slm = get(env, "slm_size"); k.barriers = get(env, "barrier_count");
    k.skip = get(env, "offset_to_skip_per_thread_data_load");
    k.inline_bytes = get(env, "inline_data_payload_size");
    require(k.simd == 1 || k.simd == 8 || k.simd == 16 || k.simd == 32, "unsupported SIMD width");
    for (const auto& buffer : memory_buffers) {
        const auto bytes = get(buffer, "size");
        const auto usage = buffer.find("usage"), simt = buffer.find("is_simt_thread");
        require(bytes && bytes <= 262144 && usage != buffer.end() &&
                (simt == buffer.end() || simt->second == "true" || simt->second == "false"), "invalid per-thread memory buffer");
        const bool per_lane = simt != buffer.end() && simt->second == "true";
        if (buffer.at("type") == "scratch") {
            const auto slot = get(buffer, "slot");
            require(slot < 2 && !k.scratch_bytes[slot] && !per_lane &&
                    (usage->second == "single_space" || usage->second == "spill_fill_space" || usage->second == "private_space"),
                    "unsupported/duplicate scratch slot or usage");
            uint32_t rounded = 64; // DG2 minimum, GfxCoreHelperXeHPAndLater; pitch is a power of two.
            while (rounded < bytes) rounded *= 2;
            k.scratch_bytes[slot] = rounded;
        } else {
            require(buffer.at("type") == "global" && usage->second == "private_space" && !k.private_bytes &&
                    !get(buffer, "slot") && uint64_t(bytes) * (per_lane ? k.simd : 1) <= 262144, "unsupported global private buffer");
            k.private_bytes = bytes * (per_lane ? k.simd : 1);
        }
    }
    require(!required_simd || required_simd == k.simd, "required subgroup size differs from SIMD width");
    require(k.grf == 128 || k.grf == 256, "unsupported GRF count");
    require(k.slm <= 65536 && k.barriers <= 1, "SLM/barrier limit exceeded");
    require(k.inline_bytes == 0 || k.inline_bytes == 32, "unsupported inline payload");
    require(!local_ids_seen || k.local_id_bytes, "missing local-ID size");
    if (k.packed_local_ids) {
        // SIMD1/VC loads one packed xyz tuple from a 32-byte GRF per hardware thread.
        require(k.simd == 1 && k.local_id_bytes == 6 && !k.inline_bytes && !(k.skip & 63), "unsupported packed local-ID layout");
    } else if (k.local_id_bytes) {
        require(k.simd != 1 && k.local_id_bytes == 3 * std::max(16u, k.simd) * 2 && k.skip && !(k.skip & 63), "unsupported local-ID layout");
    } else require(k.skip == 0, "skip offset without local-ID payload");
    for (const auto& e : env) {
        const auto& key = e.first; const auto& value = e.second;
        if (key == "simd_size" || key == "grf_count" || key == "slm_size" || key == "barrier_count" ||
            key == "offset_to_skip_per_thread_data_load" || key == "inline_data_payload_size" || key == "indirect_stateless_count") continue;
        if (key == "eu_thread_count") require(number(value) == (k.grf == 256 ? 4u : 8u), "unsupported EU thread mode");
        else if (key == "required_work_group_size") {
            const auto local = required_local_size(value);
            require(!k.required_local[0] || k.required_local == local, "conflicting required local size metadata");
            k.required_local = local;
        }
        else if (key == "work_group_walk_order_dimensions") {
            auto order = value; order.erase(std::remove(order.begin(), order.end(), ' '), order.end());
            require(order == "[0,0,0]" || order == "[0,1,2]", "unsupported execution walk order");
        }
        else if (key == "has_dpas") { require(value == "true" || value == "false", "invalid DPAS flag"); k.systolic = value == "true"; }
        else if (key == "require_disable_eufusion") { require(value == "true" || value == "false", "invalid EU fusion flag"); k.disable_eu_fusion = value == "true"; }
        // The encoder always selects thread-group preemption, satisfying a ban on mid-thread preemption.
        else if (key == "disable_mid_thread_preemption") require(value == "true" || value == "false", "invalid preemption flag");
        // Statistics only; the per-thread buffer descriptors determine actual allocation sizes.
        else if (key == "spill_size" || key == "private_size") require(number(value) <= 262144, "invalid private/spill statistic");
        // Global atomic instructions/fences are already in the ISA; NEO has no extra DG2 state for this flag.
        else if (key == "has_global_atomics" || key == "has_4gb_buffers" || key == "has_no_stateless_write" || key == "subgroup_independent_forward_progress")
            require(value == "true" || value == "false", "invalid execution flag");
        else require(false, "unsupported execution field " + key);
    }
    std::vector<bool> occupied(65536);
    for (const auto& p : k.payload) {
        require(p.size && p.size <= 4096 && p.offset <= occupied.size() - p.size, "invalid payload range");
        require(!p.is_pointer || p.size == 8, "pointer payload must be 64 bits");
        if (p.type == "arg_bypointer") {
            const bool local = p.mode == "slm" && p.space == "local" && p.alignment &&
                               p.alignment <= 65536 && !(p.alignment & (p.alignment - 1));
            const bool global = p.mode == "stateless" && (p.space == "global" || p.space == "constant") && !p.alignment;
            require(p.index < 256 && p.size == 8 && !p.source && (local || global), "unsupported pointer layout/address space");
        }
        else if (p.type == "arg_byvalue") require(p.index < 256 && p.source <= 4096 - p.size, "invalid value argument");
        else if (p.type == "global_base" || p.type == "const_base" || p.type == "private_base_stateless")
            require(p.size == 8 && p.index == UINT32_MAX && !p.source, "invalid global/constant base payload");
        else {
            require(p.type == "global_id_offset" || p.type == "local_size" || p.type == "enqueued_local_size" ||
                    p.type == "global_size" || p.type == "group_count", "unsupported hidden argument " + p.type);
            require(p.size == 12 && p.index == UINT32_MAX && !p.source, "unsupported hidden argument layout");
        }
        for (uint32_t i = p.offset; i < p.offset + p.size; ++i) {
            require(!occupied[i], "overlapping payload fields"); occupied[i] = true;
        }
        k.cross_bytes = std::max(k.cross_bytes, p.offset + p.size);
    }
    k.cross_bytes = uint32_t(align(k.cross_bytes, 32));
    return k;
}
struct Elf {
    std::vector<uint8_t> bytes;
    uint64_t read(size_t at, unsigned width) const {
        require(at <= bytes.size() && width <= bytes.size() - at, "truncated ELF");
        uint64_t result = 0;
        for (unsigned i = 0; i < width; ++i) result |= uint64_t(bytes[at + i]) << (8 * i);
        return result;
    }
    struct Section { uint32_t name, type, link, info; uint64_t offset, size, entry; };
    std::vector<Section> sections;
    std::string str(const Section& s, uint64_t offset) const {
        require(offset < s.size, "ELF string offset outside table");
        auto begin = bytes.begin() + s.offset + offset;
        auto end = std::find(begin, bytes.begin() + s.offset + s.size, uint8_t{0});
        require(end != bytes.begin() + s.offset + s.size, "unterminated ELF string");
        return {begin, end};
    }
    explicit Elf(std::vector<uint8_t> input) : bytes(std::move(input)) {
        require(bytes.size() >= 64 && bytes.size() <= 64 * 1024 * 1024, "invalid ELF size");
        require(read(0, 4) == 0x464c457f && read(4, 3) == 0x010102, "expected little-endian ELF64 v1");
        require((read(16, 2) == 1 || read(16, 2) == 0xff12) && read(18, 2) == 205 &&
                read(20, 4) == 1 && read(52, 2) == 64, "expected IntelGT zebin");
        const auto offset = read(40, 8), count = read(60, 2);
        require(read(58, 2) == 64 && count && count <= 4096 && offset <= bytes.size() && count <= (bytes.size() - offset) / 64, "invalid section table");
        for (size_t i = 0; i < count; ++i) {
            const auto p = size_t(offset) + 64 * i;
            Section s{uint32_t(read(p, 4)), uint32_t(read(p + 4, 4)), uint32_t(read(p + 40, 4)),
                      uint32_t(read(p + 44, 4)), read(p + 24, 8), read(p + 32, 8), read(p + 56, 8)};
            require(s.offset <= bytes.size() && (s.type == 8 || s.size <= bytes.size() - s.offset), "ELF section outside file");
            sections.push_back(s);
        }
        require(read(62, 2) < sections.size(), "invalid section name table");
        require(sections[read(62, 2)].type == 3, "section names require a string table");
    }
    std::string name(size_t index) const { return str(sections[read(62, 2)], sections[index].name); }
    Kernel kernel(const std::string& wanted) const {
        size_t text = 0, info = 0, notes = 0, globals = 0, constants = 0;
        for (size_t i = 1; i < sections.size(); ++i) {
            const auto n = name(i);
            if (n == ".text." + wanted) { require(!text, "duplicate text section"); text = i; }
            if (n == ".ze_info") { require(!info, "duplicate ze_info"); info = i; }
            if (n == ".note.intelgt.compat") { require(!notes, "duplicate compatibility notes"); notes = i; }
            if (sections[i].size && (n.find(".data") == 0 || n.find(".bss") == 0)) {
                const bool constant = n == ".data.const" || n == ".bss.const";
                require(constant || n == ".data.global" || n == ".bss.global", "unsupported data section " + n);
                auto& slot = constant ? constants : globals;
                require(!slot && sections[i].size <= 16 * 1024 * 1024 &&
                        sections[i].type == (n.find(".bss") == 0 ? 8u : 1u), "duplicate/oversized/invalid data segment");
                slot = i;
            }
        }
        require(text && info && notes && sections[text].type == 1 && sections[info].type == 0xff000011 && sections[notes].type == 7, "missing/invalid kernel text, ze_info or target notes");
        const auto& nt = sections[notes];
        bool dg2 = true, hpg = false, product_seen = false, core_seen = false;
        for (uint64_t p = nt.offset; p < nt.offset + nt.size;) {
            require(nt.offset + nt.size - p >= 12, "truncated target note");
            const auto namesz = read(p, 4), size = read(p + 4, 4), type = read(p + 8, 4);
            const auto length = 12 + align(namesz, 4) + align(size, 4);
            require(namesz == 8 && length <= nt.offset + nt.size - p && std::memcmp(bytes.data() + p + 12, "IntelGT\0", 8) == 0, "invalid target note");
            // VC/ESIMD may identify Xe HPG with an unknown/absent product family.
            // As in NEO validateTargetDevice, a specified product must still match.
            if (type == 1) {
                require(size == 4 && !product_seen, "invalid/duplicate product note"); product_seen = true;
                const auto product = read(p + 20, 4); dg2 = product == 0 || product == 1270;
            }
            if (type == 2) {
                require(size == 4 && !core_seen, "invalid/duplicate core note"); core_seen = true;
                hpg = read(p + 20, 4) == 3079;
            }
            p += length;
        }
        require(dg2 && hpg, "zebin targets a different GPU family");
        const auto& zi = sections[info];
        auto k = metadata(std::string(reinterpret_cast<const char*>(bytes.data() + zi.offset), size_t(zi.size)), wanted);
        for (auto pair : {std::pair<size_t, std::vector<uint8_t>*>{globals, &k.global_data}, {constants, &k.constant_data}}) {
            if (!pair.first) continue;
            const auto& s = sections[pair.first];
            pair.second->resize(size_t(s.size));
            if (s.type != 8) std::memcpy(pair.second->data(), bytes.data() + s.offset, size_t(s.size));
        }
        const auto& tx = sections[text];
        require(tx.size && tx.size <= 16 * 1024 * 1024 && k.skip < tx.size, "invalid kernel ISA/entry point");
        k.isa.assign(bytes.begin() + tx.offset, bytes.begin() + tx.offset + tx.size);
        for (size_t ri = 0; ri < sections.size(); ++ri) {
            const auto& r = sections[ri];
            require(!r.size || (r.type != 9 && r.type != 4) || (r.info != globals && r.info != constants),
                    "data-segment relocations are not supported");
            if (name(ri) == ".rel.text." + wanted || name(ri) == ".rela.text." + wanted)
                require(r.info == text, "relocation targets a different section");
            if ((r.type != 9 && r.type != 4) || r.info != text || !r.size) continue;
            require(r.type == 9 && r.entry == 16 && !(r.size % 16) && r.link < sections.size(), "unsupported relocation table");
            const auto& sym = sections[r.link];
            require(sym.type == 2 && sym.entry == 24 && !(sym.size % 24) && sym.link < sections.size() && sections[sym.link].type == 3, "invalid symbol table");
            for (uint64_t p = r.offset; p < r.offset + r.size; p += 16) {
                const auto offset = read(p, 8), relocation = read(p + 8, 8), index = relocation >> 32;
                require(index < sym.size / 24 && uint32_t(relocation) == 2 && offset <= k.isa.size() && 4 <= k.isa.size() - offset, "unsupported/invalid ISA relocation");
                const auto symbol = sym.offset + index * 24;
                require(str(sections[sym.link], read(symbol, 4)) == "__INTEL_PATCH_CROSS_THREAD_OFFSET_OFF_R0" && read(symbol + 6, 2) == 0, "unsupported relocation symbol");
                // No stack calls, debugger or implicit-args buffer is supported in this subset.
                // Intel Linker::resolveImplicitArgs leaves these immediate values zero in that case.
                require(read(tx.offset + offset, 4) == 0, "unexpected implicit-args relocation addend");
            }
        }
        return k;
    }
};
inline Elf read_zebin(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    require(bool(f), "cannot open zebin " + path);
    const auto size = f.tellg();
    require(size > 0 && size <= 64 * 1024 * 1024, "invalid zebin file size");
    std::vector<uint8_t> input(static_cast<size_t>(size));
    f.seekg(0); f.read(reinterpret_cast<char*>(input.data()), size);
    require(bool(f), "incomplete zebin read");
    return Elf(std::move(input));
}
inline Kernel load(const std::string& path, const std::string& name) { return read_zebin(path).kernel(name); }
} // namespace orca::l0::drm::detail
