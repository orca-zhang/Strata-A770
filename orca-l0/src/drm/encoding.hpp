#pragma once
#include "zebin.hpp"
#include "../../third_party/intel-dg2/packets.hpp"
#include <array>
namespace orca::l0::drm::detail {
using P = packet::DG2;
inline uint64_t canonical_gpu_va(uint64_t address) {
    require(address < (uint64_t{1} << 48) || address >= 0xffff800000000000ull,
            "GPU VA outside the 48-bit canonical range");
    return address & (uint64_t{1} << 47) ? address | 0xffff000000000000ull : address;
}
template<class T> inline void emit(std::vector<uint32_t>& words, const T& command) {
    static_assert(sizeof(T) % 4 == 0);
    const auto at = words.size(); words.resize(at + sizeof(T) / 4);
    std::memcpy(words.data() + at, &command, sizeof(T));
}
inline P::MI_SEMAPHORE_WAIT host_gate_wait(uint64_t address) {
    require(address && !(address & 3), "invalid host gate address");
    auto wait = P::MI_SEMAPHORE_WAIT::sInit();
    wait.setSemaphoreGraphicsAddress(canonical_gpu_va(address));
    wait.setSemaphoreDataDword(0);
    wait.setCompareOperation(P::MI_SEMAPHORE_WAIT::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD);
    wait.setWaitMode(P::MI_SEMAPHORE_WAIT::WAIT_MODE_POLLING_MODE);
    return wait;
}
inline P::PIPE_CONTROL barrier(bool invalidate = false, bool host_visibility = true) {
    auto p = P::PIPE_CONTROL::sInit();
    p.setCommandStreamerStallEnable(true);
    p.setHdcPipelineFlush(true); p.setUnTypedDataPortCacheFlush(true);
    p.setDcFlushEnable(host_visibility);
    if (invalidate) {
        p.setStateCacheInvalidationEnable(true); p.setInstructionCacheInvalidateEnable(true);
        p.setTextureCacheInvalidationEnable(true); p.setConstantCacheInvalidationEnable(true);
    }
    return p;
}
inline P::PIPE_CONTROL signal_value(uint64_t address, uint64_t value) {
    require(address && !(address & 7), "invalid completion tag address");
    address = canonical_gpu_va(address);
    auto signal = barrier();
    signal.setPostSyncOperation(P::PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA);
    signal.setAddress(uint32_t(address)); signal.setAddressHigh(uint32_t(address >> 32));
    signal.setImmediateData(value);
    return signal;
}
inline P::PIPE_CONTROL timestamp_value(uint64_t address) {
    auto timestamp = signal_value(address, 0);
    timestamp.setPostSyncOperation(P::PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_TIMESTAMP);
    return timestamp;
}
inline P::MI_SEMAPHORE_WAIT wait_value(uint64_t address, uint32_t value) {
    auto wait = host_gate_wait(address);
    wait.setSemaphoreDataDword(value);
    wait.setCompareOperation(P::MI_SEMAPHORE_WAIT::COMPARE_OPERATION_SAD_EQUAL_SDD);
    return wait;
}
struct DispatchState {
    std::array<uint64_t, 10> programmed{};
    bool valid = false;
    bool in_order = true;
};
inline P::RENDER_SURFACE_STATE scratch_surface(uint64_t address, uint32_t pitch, uint32_t threads, uint32_t mocs, bool local_scratch_cache = false) {
    require(address && !(address & 63) && pitch >= 64 && pitch <= 262144 && !(pitch & (pitch - 1)) &&
            threads && threads <= 4096 && mocs <= 6 && !(mocs & 1), "invalid scratch surface");
    auto surface = P::RENDER_SURFACE_STATE::sInit();
    // NEO SurfaceStateBufferLength: 7 width, 14 height, 11 depth bits, in hardware threads.
    const auto length = uint32_t(align(threads, 4)) - 1;
    surface.setWidth((length & 127) + 1); surface.setHeight(((length >> 7) & 16383) + 1);
    surface.setDepth((length >> 21) + 1); surface.setSurfacePitch(pitch);
    surface.setSurfaceType(P::RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_SCRATCH);
    surface.setSurfaceFormat(P::RENDER_SURFACE_STATE::SURFACE_FORMAT_RAW);
    surface.setSurfaceVerticalAlignment(P::RENDER_SURFACE_STATE::SURFACE_VERTICAL_ALIGNMENT_VALIGN_4);
    surface.setSurfaceHorizontalAlignment(P::RENDER_SURFACE_STATE::SURFACE_HORIZONTAL_ALIGNMENT_HALIGN_DEFAULT);
    surface.setTileMode(P::RENDER_SURFACE_STATE::TILE_MODE_LINEAR);
    surface.setCoherencyType(P::RENDER_SURFACE_STATE::COHERENCY_TYPE_IA_COHERENT);
    surface.setMemoryObjectControlState(mocs);
    if (local_scratch_cache) surface.setL1CacheControlCachePolicy(P::RENDER_SURFACE_STATE::L1_CACHE_CONTROL_WB);
    surface.setSurfaceBaseAddress(canonical_gpu_va(address));
    return surface;
}

inline void record_dispatch(std::vector<uint32_t>& commands, const Kernel& k,
                            const std::vector<uint8_t>& cross, std::array<uint32_t, 3> local, std::array<uint32_t, 3> groups,
                            uint64_t isa_va, size_t isa_bytes, uint64_t indirect_va, uint32_t max_threads,
                            DispatchState* state = nullptr, uint32_t mocs = 2, bool host_visibility = true,
                            uint32_t dynamic_slm = 0, uint64_t scratch_heap_va = 0,
                            uint32_t kernel_offset = 0, uint32_t indirect_offset = 0, bool disable_overdispatch = false) {
    const auto threads = workgroup_threads(local, groups);
    require(k.simd && max_threads && max_threads <= 4096 &&
            isa_bytes && !(isa_bytes & 4095) && cross.size() == k.cross_bytes && !(isa_va & 4095) && !(indirect_va & 4095) &&
            mocs <= 6 && !(mocs & 1) && k.slm <= 65536 && dynamic_slm <= 65536 - k.slm && !(scratch_heap_va & 4095) &&
            ((!k.scratch_bytes[0] && !k.scratch_bytes[1]) || scratch_heap_va) &&
            !(kernel_offset & 63) && !(indirect_offset & 63) && kernel_offset < isa_bytes &&
            k.isa.size() <= isa_bytes - kernel_offset && k.skip < isa_bytes - kernel_offset &&
            uint64_t(kernel_offset) + k.skip <= UINT32_MAX,
            "invalid dispatch heaps");
    isa_va = canonical_gpu_va(isa_va); indirect_va = canonical_gpu_va(indirect_va);
    scratch_heap_va = canonical_gpu_va(scratch_heap_va);
    // A no-scratch kernel can keep this list's already-retained descriptor.
    // Fresh lists start invalid; required scratch is checked before this reuse.
    if (!scratch_heap_va && state && state->valid) scratch_heap_va = state->programmed[7];
    const auto inline_size = std::min(k.inline_bytes, k.cross_bytes);
    const auto indirect_size = align(k.cross_bytes - inline_size + local_id_payload_bytes(k, local), 64);
    require(uint64_t(indirect_offset) + indirect_size <= 0xfffff000ull, "indirect payload exceeds programmed heap range");
    require(isa_va <= UINT64_MAX - isa_bytes && indirect_va <= UINT64_MAX - indirect_size, "dispatch heap overflow");
    if (state) {
        // A recorded list can address separate immutable allocations through one
        // 4-GiB window. Keep the original heap near the last unmapped-size page
        // or when an allocation spans windows; ownership/residency stay unchanged.
        constexpr uint64_t window_mask = 0xffffffffull, window_bytes = 0xfffff000ull;
        const auto code_offset = isa_va & window_mask;
        if (code_offset + isa_bytes <= window_bytes) {
            kernel_offset += uint32_t(code_offset);
            isa_va -= code_offset; isa_bytes = window_bytes;
        }
        const auto parameter_offset = indirect_va & window_mask;
        if (parameter_offset + indirect_offset + indirect_size <= window_bytes) {
            indirect_offset += uint32_t(parameter_offset);
            indirect_va -= parameter_offset;
        }
    }
    const std::array<uint64_t, 10> programmed{isa_va, isa_bytes, indirect_va, max_threads, k.grf, k.systolic, mocs, scratch_heap_va, k.disable_eu_fusion, disable_overdispatch};
    // A recorded list starts with a fresh state. Reuse only identical global
    // programming. Explicit ordering relies on caller-recorded dependency joins;
    // required state invalidations and workarounds remain in both modes.
    if (!state || !state->valid || state->programmed != programmed) {
        const bool grf_only = disable_overdispatch && state && state->valid &&
            std::equal(programmed.begin(), programmed.begin() + 4, state->programmed.begin()) &&
            std::equal(programmed.begin() + 5, programmed.end(), state->programmed.begin() + 5);
        auto sync = barrier(true, host_visibility);
        if (grf_only) {
            sync.setStateCacheInvalidationEnable(false);
            sync.setConstantCacheInvalidationEnable(false);
            sync.setTextureCacheInvalidationEnable(false);
            sync.setInstructionCacheInvalidateEnable(false);
        }
        emit(commands, sync);
        auto preemption = P::MI_LOAD_REGISTER_IMM::sInit();
        preemption.setRegisterOffset(0x2580);
        preemption.setDataDword(0x60002); // NEO PreemptionConfig: thread-group boundaries, no debugger/SIP.
        preemption.setMmioRemapEnable(true);
        emit(commands, preemption);
        auto select = P::PIPELINE_SELECT::sInit();
        select.setPipelineSelection(P::PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU);
        select.setSystolicModeEnable(k.systolic);
        select.setMaskBits(0x83);
        emit(commands, select);
        auto mode = P::STATE_COMPUTE_MODE::sInit();
        mode.setLargeGrfMode(k.grf == 256);
        mode.setForceNonCoherent(P::STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT);
        mode.setMaskBits(0x8018);
        emit(commands, mode);
        if (!grf_only) {
            auto base = P::STATE_BASE_ADDRESS::sInit();
            base.setGeneralStateBaseAddressModifyEnable(true);
            base.setGeneralStateBaseAddress(indirect_va);
            base.setGeneralStateBufferSizeModifyEnable(true); base.setGeneralStateBufferSize(0xfffff);
            base.setInstructionBaseAddressModifyEnable(true); base.setInstructionBaseAddress(isa_va);
            base.setInstructionBufferSizeModifyEnable(true); base.setInstructionBufferSize(uint32_t(isa_bytes / 4096));
            // DG2 fixed table: index 1 (encoded 2) is UC, index 3 (encoded 6) is L3 WB.
            // The caller selects its verified policy; unvalidated backends retain UC.
            base.setGeneralStateMemoryObjectControlState(mocs); base.setInstructionMemoryObjectControlState(mocs);
            base.setStatelessDataPortAccessMemoryObjectControlState(mocs);
            base.setSurfaceStateBaseAddressModifyEnable(true); base.setSurfaceStateBaseAddress(scratch_heap_va);
            base.setSurfaceStateMemoryObjectControlState(mocs);
            base.setBindlessSurfaceStateBaseAddressModifyEnable(true); base.setBindlessSurfaceStateBaseAddress(scratch_heap_va);
            base.setBindlessSurfaceStateMemoryObjectControlState(mocs);
            base.setBindlessSurfaceStateSize(scratch_heap_va ? 63 : 0); // One 4 KiB descriptor heap.
            emit(commands, base);
            // Conservative repeat covers the DG2 early-stepping SBA workaround.
            emit(commands, base);
        }
        auto cfe = P::CFE_STATE::sInit();
        cfe.setMaximumNumberOfThreads(max_threads);
        cfe.setComputeOverdispatchDisable(disable_overdispatch);
        // Xe HPG's bit is inverted: true disables fused dispatch (NEO preamble_xe_hpg_core.cpp).
        cfe.setFusedEuDispatch(k.disable_eu_fusion);
        cfe.setScratchSpaceBuffer(scratch_heap_va ? 128 : 0); // Slot 0 is reserved; two adjacent 64-byte descriptors.
        emit(commands, cfe);
    }
    auto walker = P::COMPUTE_WALKER::sInit();
    const auto simd = k.simd == 8 ? 0u : k.simd == 16 ? 1u : 2u;
    walker.setSimdSize(static_cast<P::COMPUTE_WALKER::SIMD_SIZE>(simd));
    walker.setMessageSimd(simd);
    const auto lanes = threads % k.simd;
    walker.setExecutionMask(lanes ? (uint32_t{1} << lanes) - 1 : (k.simd == 1 || k.simd == 32 ? UINT32_MAX : (1u << k.simd) - 1));
    walker.setLocalXMaximum(local[0] - 1); walker.setLocalYMaximum(local[1] - 1); walker.setLocalZMaximum(local[2] - 1);
    walker.setThreadGroupIdXDimension(groups[0]); walker.setThreadGroupIdYDimension(groups[1]); walker.setThreadGroupIdZDimension(groups[2]);
    const bool hardware_ids = hardware_local_ids(k, local);
    walker.setWalkOrder(0); walker.setGenerateLocalId(hardware_ids);
    walker.setEmitLocalId(hardware_ids ? 7 : 0);
    walker.setEmitInlineParameter(inline_size != 0);
    if (inline_size) std::memcpy(walker.getInlineDataPointer(), cross.data(), inline_size);
    walker.setIndirectDataLength(uint32_t(indirect_size));
    walker.setIndirectDataStartAddress(indirect_offset); // Relative to the general-state base on Xe HPG.
    auto& idd = walker.getInterfaceDescriptor();
    // IGC initializes CR0 with OR instructions and assumes denorm bits start clear.
    // FTZ is the launch state; each kernel can still enable denorm retention in its body.
    idd.setDenormMode(P::INTERFACE_DESCRIPTOR_DATA::DENORM_MODE_FTZ);
    idd.setKernelStartPointer(uint64_t(kernel_offset) + (hardware_ids ? k.skip : 0));
    const auto group_threads = (threads + k.simd - 1) / k.simd;
    idd.setNumberOfThreadsInGpgpuThreadGroup(group_threads);
    // DG2 NEO policy: two groups for one hardware thread; one otherwise.
    if (disable_overdispatch) idd.setThreadGroupDispatchSize(group_threads == 1 ?
        P::INTERFACE_DESCRIPTOR_DATA::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_2 :
        P::INTERFACE_DESCRIPTOR_DATA::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_1);
    idd.setNumberOfBarriers(static_cast<P::INTERFACE_DESCRIPTOR_DATA::NUMBER_OF_BARRIERS>(k.barriers));
    uint32_t slm = 0;
    const auto slm_bytes = k.slm + dynamic_slm;
    if (slm_bytes) { slm = 1; for (uint32_t n = 1024; n < slm_bytes; n *= 2) ++slm; }
    idd.setSharedLocalMemorySize(slm);
    idd.setPreferredSlmAllocationSize(slm_bytes ? P::INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE_MAX
                                              : P::INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE_0KB);
    emit(commands, walker);
    if (!state || state->in_order) emit(commands, barrier(false, host_visibility));
    if (state) { state->programmed = programmed; state->valid = true; }
}
inline void record_dispatch(std::vector<uint32_t>& commands, const Kernel& k,
                            const std::vector<uint8_t>& cross, uint32_t local, uint32_t groups,
                            uint64_t isa_va, size_t isa_bytes, uint64_t indirect_va, uint32_t max_threads,
                            DispatchState* state = nullptr, uint32_t mocs = 2, bool host_visibility = true,
                            uint32_t dynamic_slm = 0, uint64_t scratch_heap_va = 0,
                            uint32_t kernel_offset = 0, uint32_t indirect_offset = 0, bool disable_overdispatch = false) {
    record_dispatch(commands, k, cross, {local, 1, 1}, {groups, 1, 1}, isa_va, isa_bytes, indirect_va,
                    max_threads, state, mocs, host_visibility, dynamic_slm, scratch_heap_va, kernel_offset, indirect_offset, disable_overdispatch);
}
} // namespace orca::l0::drm::detail
