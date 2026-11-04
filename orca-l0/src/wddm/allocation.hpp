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
