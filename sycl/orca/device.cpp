#include "strata/orca_engine.hpp"
#include "strata/core/device.hpp"
#include <bit>
#include <cstdio>

namespace strata::core {
const char* compiled_gpu_archs() {
#ifdef STRATA_ORCA_DG2
    return "Intel DG2 (Orca native)";
#else
    return "Intel Level Zero";
#endif
}
int device_count() { try { return dpct::device_count(); } catch (...) { return 0; } }
std::string gpu_arch_problem(int ordinal) {
    return ordinal == 0 ? "" : "Orca currently exposes one Intel GPU";
}
std::string device_code_error() {
    try { (void)dpct::get_in_order_queue(); return {}; }
    catch (const std::exception& e) { return e.what(); }
}
bool device_summary(int ordinal, std::string& name, std::string& detail) {
    try {
        if (ordinal != 0) return false;
        auto& device = dpct::get_in_order_queue().get_device();
        name = device.get_info<sycl::info::device::name>();
        char buffer[128];
        std::snprintf(buffer, sizeof(buffer), "%s, %.1f GiB", compiled_gpu_archs(),
                      double(device.get_info<sycl::info::device::global_mem_size>()) / (1ull << 30));
        detail = buffer;
        return true;
    } catch (...) { return false; }
}
DeviceInfo device_info(int ordinal) {
    dpct::select_device(ordinal);
    auto& queue = dpct::get_in_order_queue();
    DeviceInfo info;
    info.ordinal = ordinal;
    info.name = queue.get_device().get_info<sycl::info::device::name>();
    info.arch = compiled_gpu_archs();
    info.multi_processor_count = queue.get_device().get_info<sycl::info::device::max_compute_units>();
    size_t available = 0, total = 0;
    l0::memory_info(available, total);
    info.free_bytes = available;
    info.total_bytes = total;
#ifndef STRATA_ORCA_DG2
    info.driver_version = int(queue.runtime().driver_properties().driverVersion);
    info.runtime_version = int(ZE_API_VERSION_CURRENT);
#endif
    return info;
}
DeviceArena::DeviceArena(uint64_t bytes, int ordinal, bool poison)
    : capacity_(bytes), ordinal_(ordinal), poison_(poison) {
    if (!bytes || bytes > SIZE_MAX) throw CudaError("invalid native DeviceArena size", -1);
    dpct::select_device(ordinal);
    auto& queue = dpct::get_in_order_queue();
    base_ = queue.runtime().allocate(size_t(bytes), l0::Memory::device, 256);
    if (poison_) {
        try { queue.fill(static_cast<float*>(base_), std::bit_cast<float>(0x7fc00000u), size_t(bytes / 4)).wait_and_throw(); }
        catch (...) {
            // A timed-out fill can still own this allocation; Runtime drains it.
            base_ = nullptr;
            throw;
        }
    }
}
DeviceArena::~DeviceArena() {
    if (!base_) return;
    try {
        dpct::get_current_device().queues_wait_and_throw();
        dpct::get_in_order_queue().runtime().deallocate(base_);
    } catch (...) { /* Runtime retains allocations whose work could not drain. */ }
}
void* DeviceArena::alloc(uint64_t bytes, uint64_t alignment) {
    if (!bytes) return nullptr;
    if (!alignment || (alignment & (alignment - 1)) || used_ > UINT64_MAX - (alignment - 1))
        throw CudaError("invalid native DeviceArena alignment", -1);
    const auto start = (used_ + alignment - 1) & ~(alignment - 1);
    if (start > capacity_ || bytes > capacity_ - start) throw CudaError("native DeviceArena out of memory", -1);
    used_ = start + bytes;
    return static_cast<std::byte*>(base_) + start;
}
}
