#pragma once
// A770 / 32.0.101.9034 allocation ABI, calibrated against the driver translator.
// This encodes one linear, uncompressed, local-only resource; it is not GMM.
#include "../drm/zebin.hpp"

namespace orca::l0::drm::detail {
// A770/WDDM can reject local residency for near-4-GiB parts despite budget headroom.
// Keep the same contiguous GPU VA and total bytes, with validated 1-GiB parts.
constexpr size_t native_device_allocation_limit = size_t{1} << 30;
inline std::array<uint8_t, 2376> native_device_allocation(size_t bytes) {
    require(bytes && !(bytes & 65535) && bytes <= native_device_allocation_limit,
            "native device allocation requires 64 KiB alignment and a size at most 1 GiB");
    std::array<uint8_t, 2376> data{};
    const auto put = [&](size_t offset, uint64_t value) { std::memcpy(data.data() + offset, &value, sizeof(value)); };
    // Process pointers, vtables, allocation IDs and padding remain zero.
    for (const auto [offset, value] : std::array<std::pair<size_t, uint64_t>, 23>{{
            {0x8, 6}, {0x10, 0x14500000006ull}, {0x18, 8},
            {0x20, 0x010c000400000004ull}, // Texture, Cacheable, Linear, LocalOnly, NotLockable.
            {0x28, 0x800000}, {0x30, 0x2000}, {0x40, 0x100000001ull},
            {0x50, 0xdc00000000ull}, {0x60, 1}, {0x68, 0x8000010000ull}, {0x70, 4},
            {0x1d8, 0x10}, {0x7d8, 0x1a}, {0x808, 0x1000000000000ull},
            {0x810, 0x100000008ull}, {0x818, 0x100000001ull}, {0x820, 0x800000001full},
            {0x828, 0x100000004ull}, {0x840, 1}, {0x850, 0x14500000000ull},
            {0x858, 0x400000000ull}, {0x860, 0x1000000}, {0x8c8, 1019034}}}) put(offset, value);
    for (const size_t offset : {0x38, 0x110, 0x120, 0x248, 0x7f8, 0x848}) put(offset, bytes);
    put(0x830, 0x100000000ull | bytes);
    return data;
}
} // namespace orca::l0::drm::detail

// Owned nonlocal system memory, observed through zeMemAllocHost on A770/9034.
// No captured process pointer, vtable or allocation ID is reused.
namespace orca::l0::drm::detail {
inline std::array<uint8_t, 2376> native_host_allocation(size_t bytes) {
    require(bytes && !(bytes & 65535) && bytes <= (size_t{2} << 30), "native Host allocation exceeds calibrated size");
    require(bytes < (size_t{2} << 20) || !(bytes & ((size_t{2} << 20) - 1)), "large native Host size requires 2 MiB alignment");
    auto data = native_device_allocation(std::min(bytes, native_device_allocation_limit));
    const auto put = [&](size_t offset, uint64_t value) { std::memcpy(data.data() + offset, &value, sizeof(value)); };
    put(0x20, 0x0208000400000004ull); // NonLocalOnly; CPU lockable, no ExistingSysMem/LocalOnly.
    put(0x50, 0xe700000000ull); // Calibrated Host cache policy, not a PAT register index.
    if (bytes == (size_t{2} << 30)) put(0x18, 0x0004000000000008ull); // Gpu.NoRestriction for the 2 GiB mirror chunk.
    if (bytes >= (size_t{2} << 20)) { put(0x68, 0x8000200000ull); put(0x808, 0x20000000000000ull); }
    for (const size_t offset : {0x38, 0x110, 0x120, 0x248, 0x7f8, 0x848}) put(offset, bytes);
    put(0x830, 0x100000000ull | bytes);
    return data;
}
} // namespace orca::l0::drm::detail

// Caller-owned WB pages, observed through zexDriverImportExternalPointer on A770/9034.
namespace orca::l0::drm::detail {
constexpr size_t native_import_allocation_limit = size_t{2} << 30;
inline std::array<uint8_t, 2376> native_import_allocation(uintptr_t address, size_t bytes) {
    require(address && !(address & 65535) && bytes && !(bytes & 65535) &&
            bytes <= native_import_allocation_limit && bytes <= UINTPTR_MAX - address,
            "native import requires a non-overflowing 64 KiB aligned span at most 2 GiB");
    auto data = native_host_allocation(65536);
    const auto put = [&](size_t offset, uint64_t value) { std::memcpy(data.data() + offset, &value, sizeof(value)); };
    put(0x20, 0x0328002400000004ull);
    put(0x240, 0x10000000000ull);
    if (bytes == native_import_allocation_limit) put(0x18, 0x0004000000000008ull);
    for (const size_t offset : {0x38, 0x110, 0x120, 0x248, 0x770, 0x7f8, 0x848}) put(offset, bytes);
    for (const size_t offset : {0x750, 0x758, 0x760}) put(offset, address);
    put(0x830, 0x100000000ull | bytes);
    return data;
}
} // namespace orca::l0::drm::detail
