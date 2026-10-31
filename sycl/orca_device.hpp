#pragma once
#include <orca_l0/runtime.hpp>
#include <sycl/sycl.hpp>
#include <sycl/ext/oneapi/experimental/device_architecture.hpp>
#include <sycl/ext/oneapi/matrix/matrix.hpp>

namespace strata::l0 {
// A cached view of the real Level Zero device. No SYCL device/context is created.
class DeviceView {
public:
    explicit DeviceView(orca::l0::Runtime& runtime) : runtime_(&runtime) {
        compute_.stype = ZE_STRUCTURE_TYPE_DEVICE_COMPUTE_PROPERTIES;
        module_.stype = ZE_STRUCTURE_TYPE_DEVICE_MODULE_PROPERTIES;
        memory_access_.stype = ZE_STRUCTURE_TYPE_DEVICE_MEMORY_ACCESS_PROPERTIES;
        orca::l0::check(zeDeviceGetComputeProperties(native_handle(), &compute_), "native compute properties");
        orca::l0::check(zeDeviceGetModuleProperties(native_handle(), &module_), "native module properties");
        orca::l0::check(zeDeviceGetMemoryAccessProperties(native_handle(), &memory_access_), "native memory access");
        uint32_t count = 0;
        orca::l0::check(zeDeviceGetMemoryProperties(native_handle(), &count, nullptr), "native memory count");
        memory_.resize(count);
        for (auto& memory : memory_) memory.stype = ZE_STRUCTURE_TYPE_DEVICE_MEMORY_PROPERTIES;
        if (count) orca::l0::check(zeDeviceGetMemoryProperties(native_handle(), &count, memory_.data()), "native memory properties");
        for (const auto& extension : runtime.extensions()) {
#ifdef ZE_DEVICE_IP_VERSION_EXT_NAME
            if (std::string(extension.name) == ZE_DEVICE_IP_VERSION_EXT_NAME) {
                ze_device_ip_version_ext_t ip{};
                ip.stype = ZE_STRUCTURE_TYPE_DEVICE_IP_VERSION_EXT;
                auto properties = runtime.properties();
                properties.pNext = &ip;
                if (zeDeviceGetProperties(native_handle(), &properties) == ZE_RESULT_SUCCESS)
                    architecture_ = static_cast<sycl::ext::oneapi::experimental::architecture>(uint64_t(ip.ipVersion) << 8);
            }
#endif
#ifdef ZE_DEVICE_COMPUTE_DOTPRODUCT_PROPERTIES_EXT_NAME
            if (std::string(extension.name) == ZE_DEVICE_COMPUTE_DOTPRODUCT_PROPERTIES_EXT_NAME) {
                ze_device_compute_dotproduct_ext_properties_t dot{};
                dot.stype = ZE_STRUCTURE_TYPE_DEVICE_COMPUTE_DOTPRODUCT_EXT_PROPERTIES;
                auto compute = compute_;
                compute.pNext = &dot;
                if (zeDeviceGetComputeProperties(native_handle(), &compute) == ZE_RESULT_SUCCESS &&
                    (dot.dp_caps & ZE_DEVICE_DP_CAPABILITY_FLAG_DEVICE_SYSTOLIC_DPAS_SIMD8) &&
                    (dot.dp_caps & ZE_DEVICE_DP_CAPABILITY_FLAG_DEVICE_SYSTOLIC_DPAS_DEPTH8) &&
                    (dot.dpas_output_types & ZE_DEVICE_OUTPUT_DATA_TYPE_FLAG_DEVICE_OUTPUT_DATA_FP32)) {
                    namespace m = sycl::ext::oneapi::experimental::matrix;
                    // Advertise the native 8x8 DPAS shape, not unvalidated
                    // larger compiler-composed tiles.
                    if (dot.dpas_input_types & ZE_DEVICE_INPUT_DATA_TYPE_FLAG_DEVICE_INPUT_DATA_FP16_PACK2_PER_SIMD_LANE)
                        matrices_.push_back({8, 8, 16, 8, 8, 16, m::matrix_type::fp16, m::matrix_type::fp16,
                                             m::matrix_type::fp32, m::matrix_type::fp32});
                    if (dot.dpas_input_types & ZE_DEVICE_INPUT_DATA_TYPE_FLAG_DEVICE_INPUT_DATA_BF16_PACK2_PER_SIMD_LANE)
                        matrices_.push_back({8, 8, 16, 8, 8, 16, m::matrix_type::bf16, m::matrix_type::bf16,
                                             m::matrix_type::fp32, m::matrix_type::fp32});
                }
            }
#endif
        }
    }
    ze_device_handle_t native_handle() const noexcept { return runtime_->native_device(); }
    auto get_backend() const noexcept { return sycl::backend::ext_oneapi_level_zero; }
    bool is_gpu() const noexcept { return true; }
    bool has(sycl::aspect aspect) const noexcept {
        switch (aspect) {
        case sycl::aspect::fp16: return module_.flags & ZE_DEVICE_MODULE_FLAG_FP16;
        case sycl::aspect::fp64: return module_.flags & ZE_DEVICE_MODULE_FLAG_FP64;
        case sycl::aspect::atomic64: return module_.flags & ZE_DEVICE_MODULE_FLAG_INT64_ATOMICS;
        case sycl::aspect::usm_host_allocations: return memory_access_.hostAllocCapabilities & ZE_MEMORY_ACCESS_CAP_FLAG_RW;
        case sycl::aspect::usm_device_allocations: return memory_access_.deviceAllocCapabilities & ZE_MEMORY_ACCESS_CAP_FLAG_RW;
        case sycl::aspect::usm_shared_allocations: return memory_access_.sharedSingleDeviceAllocCapabilities & ZE_MEMORY_ACCESS_CAP_FLAG_RW;
        default: return false;
        }
    }
    template<class Info> auto get_info() const {
        namespace d = sycl::info::device;
        const auto& properties = runtime_->properties();
        if constexpr (std::is_same_v<Info, d::name>) return std::string(properties.name);
        else if constexpr (std::is_same_v<Info, d::vendor_id>) return properties.vendorId;
        else if constexpr (std::is_same_v<Info, d::max_compute_units>)
            return properties.numSlices * properties.numSubslicesPerSlice * properties.numEUsPerSubslice;
        else if constexpr (std::is_same_v<Info, d::max_clock_frequency>) return properties.coreClockRate;
        else if constexpr (std::is_same_v<Info, d::max_work_group_size>) return size_t(compute_.maxTotalGroupSize);
        else if constexpr (std::is_same_v<Info, d::max_work_item_sizes<3>>)
            return sycl::range<3>(compute_.maxGroupSizeZ, compute_.maxGroupSizeY, compute_.maxGroupSizeX);
        else if constexpr (std::is_same_v<Info, d::local_mem_size>) return uint64_t(compute_.maxSharedLocalMemory);
        else if constexpr (std::is_same_v<Info, d::global_mem_size>) {
            uint64_t total = 0;
            for (const auto& memory : memory_) total += memory.totalSize;
            return total;
        } else if constexpr (std::is_same_v<Info, d::sub_group_sizes>)
            return std::vector<size_t>(compute_.subGroupSizes, compute_.subGroupSizes + compute_.numSubGroupSizes);
        else if constexpr (std::is_same_v<Info, sycl::ext::oneapi::experimental::info::device::architecture>)
            return architecture_;
        else if constexpr (std::is_same_v<Info, sycl::ext::oneapi::experimental::info::device::matrix_combinations>)
            return matrices_;
        else static_assert(!std::is_same_v<Info, Info>, "unsupported native device query");
    }
private:
    orca::l0::Runtime* runtime_;
    ze_device_compute_properties_t compute_{};
    ze_device_module_properties_t module_{};
    ze_device_memory_access_properties_t memory_access_{};
    std::vector<ze_device_memory_properties_t> memory_;
    sycl::ext::oneapi::experimental::architecture architecture_ = sycl::ext::oneapi::experimental::architecture::unknown;
    std::vector<sycl::ext::oneapi::experimental::matrix::combination> matrices_;
};
}
