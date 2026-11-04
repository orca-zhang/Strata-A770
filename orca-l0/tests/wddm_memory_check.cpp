// Direct Windows kernel transport, independent of Level Zero, Intel UMD and GMM.
// Opt-in --submit checks Orca-encoded GPU stores, not zebin compute execution.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winternl.h>
#include <d3dkmthk.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <stdexcept>
#include <array>
#include <algorithm>
#include <chrono>
#include <vector>
#include <cmath>
#include <immintrin.h>
#ifdef ORCA_L0_WDDM_BASELINE
#include <orca_l0/runtime.hpp>
#include "runtime_dependencies.hpp"
#endif
#ifdef ORCA_L0_WDDM_DG2
#include <orca_l0/dg2.hpp>
#endif
#include "intel-dg2/packets.hpp"
#include "../src/drm/encoding.hpp"

namespace {
constexpr size_t output_offset = 4096;
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
void check(NTSTATUS status, const char* operation) {
    if (status < 0) {
        char message[160];
        std::snprintf(message, sizeof(message), "%s: NTSTATUS=0x%08lx", operation, static_cast<unsigned long>(status));
        throw std::runtime_error(message);
    }
}
void dependencies() {
#ifdef ORCA_L0_WDDM_BASELINE
    check_runtime_dependencies();
#else
    for (const auto* dll : {L"ze_loader.dll", L"ze_intel_gpu64.dll", L"sycl8.dll", L"sycl7.dll", L"sycl6.dll",
                           L"ur_loader.dll", L"ur_adapter_level_zero.dll", L"igc64.dll", L"igc-default64.dll",
                           L"igdfcl64.dll", L"igdgmm64.dll", L"igdgmm2_64.dll", L"igdgmmstaging_64.dll"})
        require(!GetModuleHandleW(dll), "unexpected compute runtime/compiler/GMM dependency");
#endif
}
uint64_t canonical(uint64_t address) {
    if (address >= (uint64_t{1} << 48)) throw std::invalid_argument("GPU VA outside the calibrated 48-bit range");
    return address & (uint64_t{1} << 47) ? address | 0xffff000000000000ull : address;
}
std::array<uint32_t, 6> packet(uint64_t address, uint32_t value) {
    if (address & 3) throw std::invalid_argument("unaligned GPU store address");
    using P = orca::l0::drm::packet::DG2;
    auto store = P::MI_STORE_DATA_IMM::sInit();
    store.setAddress(canonical(address)); store.setDataDword0(value);
    store.setForceWriteCompletionCheck(true);
    const auto end = P::MI_BATCH_BUFFER_END::sInit();
    std::array<uint32_t, 6> result{};
    // StoreDword consumes four dwords, although the generated union also has DataDword1.
    std::memcpy(result.data(), &store, 16);
    std::memcpy(result.data() + 4, &end, sizeof(end));
    return result;
}
void self_check() {
    require(packet(0x800000001000ull, 0x31415926) ==
            std::array<uint32_t, 6>{0x10000402, 0x1000, 0xffff8000, 0x31415926, 0x05000000, 0},
            "wrong DG2 store/end packet");
    require(canonical(0x12345000) == 0x12345000 && canonical(0x800000000000ull) == 0xffff800000000000ull,
            "wrong GPU VA canonicalization");
    require(orca::l0::drm::detail::canonical_gpu_va(0xffff800000000000ull) == canonical(0x800000000000ull),
            "shared dispatch encoder disagrees on canonical GPU VA");
    for (uint64_t address : {uint64_t{3}, uint64_t{1} << 48}) {
        bool rejected = false;
        try { (void)packet(address, 0); } catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "invalid GPU address accepted");
    }
    std::puts("PASS DG2 packet/canonical address checks; CPU only");
}
struct Adapters {
    D3DKMT_ENUMADAPTERS info{};
    Adapters() { check(D3DKMTEnumAdapters(&info), "enumerate adapters"); }
    ~Adapters() {
        for (UINT i = 0; i < info.NumAdapters; ++i) {
            D3DKMT_CLOSEADAPTER close{}; close.hAdapter = info.Adapters[i].hAdapter;
            D3DKMTCloseAdapter(&close);
        }
    }
    Adapters(const Adapters&) = delete;
    Adapters& operator=(const Adapters&) = delete;
};
template<class T> T query(D3DKMT_HANDLE adapter, KMTQUERYADAPTERINFOTYPE type) {
    T value{};
    D3DKMT_QUERYADAPTERINFO q{};
    q.hAdapter = adapter; q.Type = type; q.pPrivateDriverData = &value; q.PrivateDriverDataSize = sizeof(value);
    check(D3DKMTQueryAdapterInfo(&q), "query adapter");
    return value;
}
struct MemoryCheck {
    size_t bytes = 65536;
    size_t data_offset = output_offset;
    uint32_t warmups = 8, repeats = 64, launches = 1;
    uint32_t spin_us = 0;
    bool polling = false;
    orca::l0::drm::detail::DispatchState encoder_state;
#ifdef ORCA_L0_WDDM_BASELINE
    std::unique_ptr<orca::l0::Runtime> l0;
    orca::l0::Commands* l0_commands = nullptr;
    std::vector<ze_kernel_handle_t> l0_kernels;
#endif
#ifdef ORCA_L0_WDDM_DG2
    std::unique_ptr<orca::l0::dg2::Runtime> dg2;
#endif
    const char* image = nullptr;
    D3DKMT_HANDLE adapter = 0, device = 0, paging = 0, paging_fence = 0, allocation = 0, resource = 0, context = 0;
    unsigned long hw_context = 0;
    D3DKMT_HANDLE monitor = 0;
    UINT64 monitor_address = 0;
    volatile UINT64* monitor_cpu = nullptr;
    D3DGPU_VIRTUAL_ADDRESS address = 0;
    void* memory = nullptr;
    HANDLE event = nullptr;
    explicit MemoryCheck(D3DKMT_HANDLE a) : adapter(a) {}
    MemoryCheck(const MemoryCheck&) = delete;
    MemoryCheck& operator=(const MemoryCheck&) = delete;
    bool release() noexcept {
#ifdef ORCA_L0_WDDM_DG2
        if (dg2) { dg2.reset(); memory = nullptr; return true; }
#endif
#ifdef ORCA_L0_WDDM_BASELINE
        if (l0) { l0.reset(); memory = nullptr; return true; }
#endif
        // Destroy the device before releasing backing pages even on partial failure.
        bool ok = true;
        if (context) {
            D3DKMT_DESTROYCONTEXT destroy{}; destroy.hContext = context;
            if (D3DKMTDestroyContext(&destroy) < 0) return false;
            context = 0;
        }
        if (monitor) {
            D3DKMT_DESTROYSYNCHRONIZATIONOBJECT destroy{}; destroy.hSyncObject = monitor;
            ok = D3DKMTDestroySynchronizationObject(&destroy) >= 0 && ok; monitor = 0;
        }
        if (address) {
            D3DKMT_FREEGPUVIRTUALADDRESS free{};
            free.hAdapter = adapter; free.BaseAddress = address; free.Size = bytes;
            ok = D3DKMTFreeGpuVirtualAddress(&free) >= 0 && ok; address = 0;
        }
        if (allocation) {
            D3DKMT_DESTROYALLOCATION2 destroy{};
            destroy.hDevice = device;
            if (resource) destroy.hResource = resource;
            else { destroy.phAllocationList = &allocation; destroy.AllocationCount = 1; }
            ok = D3DKMTDestroyAllocation2(&destroy) >= 0 && ok; allocation = resource = 0;
        }
        if (paging) {
            D3DDDI_DESTROYPAGINGQUEUE destroy{}; destroy.hPagingQueue = paging;
            ok = D3DKMTDestroyPagingQueue(&destroy) >= 0 && ok; paging = 0;
        }
        bool device_released = true;
        if (device) {
            D3DKMT_DESTROYDEVICE destroy{}; destroy.hDevice = device;
            device_released = D3DKMTDestroyDevice(&destroy) >= 0;
            ok = device_released && ok;
            if (device_released) device = 0;
        }
        if (memory && device_released) { ok = VirtualFree(memory, 0, MEM_RELEASE) != 0 && ok; memory = nullptr; }
        if (event) { ok = CloseHandle(event) != 0 && ok; event = nullptr; }
        return ok;
    }
    ~MemoryCheck() { release(); }
    void wait(UINT64 fence, D3DKMT_HANDLE object = 0) {
        if (!object) object = paging_fence;
        require(ResetEvent(event) != 0, "reset paging event");
        D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMCPU w{};
        w.hDevice = device; w.ObjectCount = 1; w.ObjectHandleArray = &object;
        w.FenceValueArray = &fence; w.hAsyncEvent = event;
        check(D3DKMTWaitForSynchronizationObjectFromCpu(&w), "wait WDDM fence");
        require(WaitForSingleObject(event, 10000) == WAIT_OBJECT_0, "WDDM fence did not complete within 10 seconds");
    }
    void run(bool map_gpu, bool create_context, bool submit, const std::vector<orca::l0::drm::detail::Kernel>& kernels) {
        if (kernels.size() == 4 || data_offset != output_offset)
            bytes = (data_offset + 12288 + kernels.size() * 24576 + 65535) & ~size_t{65535};
#ifdef ORCA_L0_WDDM_DG2
        require(!kernels.empty(), "DG2 runtime benchmark requires compute workloads");
        dg2 = std::make_unique<orca::l0::dg2::Runtime>();
        const auto data = dg2->allocate(bytes);
        dg2->cpu_access(data, true);
        memory = data.data; address = data.gpu & ((uint64_t{1} << 48) - 1);
        auto* words = static_cast<UINT64*>(memory);
        for (size_t i = 0; i < bytes / sizeof(*words); ++i) words[i] = UINT64(i) ^ 0x5a9c17e3b86402dfull;
#elif defined(ORCA_L0_WDDM_BASELINE)
        require(!kernels.empty(), "Level Zero baseline only supports compute/pipeline workloads");
        l0 = std::make_unique<orca::l0::Runtime>();
        memory = l0->allocate(bytes, orca::l0::Memory::host, 65536);
        orca::l0::check(zeContextMakeMemoryResident(l0->native_context(), l0->native_device(), memory, bytes), "baseline residency");
        address = reinterpret_cast<uintptr_t>(memory);
        auto* words = static_cast<UINT64*>(memory);
        for (size_t i = 0; i < bytes / sizeof(*words); ++i) words[i] = UINT64(i) ^ 0x5a9c17e3b86402dfull;
        const auto module = l0->module(image, ZE_MODULE_FORMAT_NATIVE);
        if (kernels.size() == 4) {
            for (const auto* name : {"rms_norm", "gate_up", "swiglu", "advance"}) l0_kernels.push_back(l0->kernel(module, name));
        } else l0_kernels.push_back(l0->kernel(module, "advance"));
        l0_commands = &l0->commands(orca::l0::Ordering::explicit_dependencies);
#else
        D3DKMT_CREATEDEVICE create{}; create.hAdapter = adapter; // Normal priority and GPU timeout stay enabled.
        check(D3DKMTCreateDevice(&create), "create device"); device = create.hDevice;
        if (create_context) {
            D3DKMT_NODEMETADATA metadata{};
            metadata.NodeOrdinalAndAdapterIndex = 5; // Intel gmmlib UmKmEnum.h: GPUNODE_CCS0.
            D3DKMT_QUERYADAPTERINFO q{};
            q.hAdapter = adapter; q.Type = KMTQAITYPE_NODEMETADATA;
            q.pPrivateDriverData = &metadata; q.PrivateDriverDataSize = sizeof(metadata);
            check(D3DKMTQueryAdapterInfo(&q), "query compute node");
            std::printf("INFO node=%u type=%u gpu_mmu=%d\n", metadata.NodeOrdinalAndAdapterIndex,
                        unsigned(metadata.NodeData.EngineType), int(metadata.NodeData.GpuMmuSupported));
            // ponytail: this private ABI is calibrated only for A770 driver 32.0.101.9034.
            // The driver's own context call and its shipped libwsl_compute_helper.so
            // agree on size/offsets. NEO's 32/56-byte public fallback is NOT this ABI.
            // Keep unneeded fields zero; add another verified profile for another driver.
            alignas(8) std::array<uint8_t, 144> data{};
            const auto pointer = reinterpret_cast<uintptr_t>(&hw_context);
            const auto pid = GetCurrentProcessId();
            const uint32_t umd_opencl = 5;
            std::memcpy(data.data(), &pointer, sizeof(pointer));
            std::memcpy(data.data() + 12, &pid, sizeof(pid));
            data[36] = 1; // GpuVAContext
            data[68] = 1; // NoRingFlushes (NEO's default)
            std::memcpy(data.data() + 80, &umd_opencl, sizeof(umd_opencl));
            D3DKMT_CREATECONTEXTVIRTUAL cc{};
            cc.hDevice = device; cc.NodeOrdinal = 5;
            cc.ClientHint = D3DKMT_CLIENTHINT_ONEAPI_LEVEL0;
            cc.pPrivateDriverData = data.data(); cc.PrivateDriverDataSize = sizeof(data);
            check(D3DKMTCreateContextVirtual(&cc), "create virtual compute context"); context = cc.hContext;
            require(context != 0, "context returned no handle");
            std::printf("INFO compute_context=1 hw_id=%lu private_bytes=%zu\n", hw_context, sizeof(data));
        }
        D3DKMT_CREATEPAGINGQUEUE pq{};
        pq.hDevice = device; pq.Priority = D3DDDI_PAGINGQUEUE_PRIORITY_NORMAL;
        check(D3DKMTCreatePagingQueue(&pq), "create paging queue");
        paging = pq.hPagingQueue; paging_fence = pq.hSyncObject;
        event = CreateEventW(nullptr, FALSE, FALSE, nullptr); require(event != nullptr, "create paging event");
        memory = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        require(memory != nullptr, "allocate backing pages");
        auto* words = static_cast<UINT64*>(memory);
        for (size_t i = 0; i < bytes / sizeof(*words); ++i) words[i] = UINT64(i) ^ 0x5a9c17e3b86402dfull;

        // Public WDDM 2.3 existing-heap allocation avoids Intel's private GMM resource blob.
        D3DKMT_CREATESTANDARDALLOCATION standard{};
        standard.Type = D3DKMT_STANDARDALLOCATIONTYPE_EXISTINGHEAP; standard.ExistingHeapData.Size = bytes;
        D3DDDI_ALLOCATIONINFO2 ai{}; ai.pSystemMem = memory;
        D3DKMT_CREATEALLOCATION ca{};
        ca.hDevice = device; ca.NumAllocations = 1; ca.pAllocationInfo2 = &ai;
        ca.Flags.StandardAllocation = 1; ca.pStandardAllocation = &standard;
        // Required by the public StandardAllocation contract. Sharing stays
        // local to this process; no shared NT handle is exported.
        ca.Flags.ExistingSysMem = 1; ca.Flags.CreateResource = 1;
        ca.Flags.CreateShared = 1; ca.Flags.CrossAdapter = 1; ca.Flags.NtSecuritySharing = 1;
        check(D3DKMTCreateAllocation2(&ca), "create standard existing-heap allocation");
        allocation = ai.hAllocation; resource = ca.hResource;
        require(allocation != 0, "allocation returned no handle");
        D3DDDI_MAKERESIDENT resident{};
        resident.hPagingQueue = paging; resident.NumAllocations = 1; resident.AllocationList = &allocation;
        check(D3DKMTMakeResident(&resident), "make allocation resident");
        require(resident.NumAllocations == 1, "allocation was not made resident");
        wait(resident.PagingFenceValue);

        if (map_gpu) {
            // Experimental: Intel's normal initialization also configures the
            // address space through GMM; this path deliberately does not load GMM.
            D3DDDI_MAPGPUVIRTUALADDRESS map{};
            map.hPagingQueue = paging; map.hAllocation = allocation; map.SizeInPages = bytes / 4096;
            map.MinimumAddress = 65536; // MaximumAddress=0 lets VidMm select its valid range.
            map.Protection.Write = 1;
            check(D3DKMTMapGpuVirtualAddress(&map), "map GPU virtual address"); address = map.VirtualAddress;
            require(address >= map.MinimumAddress && address <= UINT64_MAX - bytes + 1 && address % 4096 == 0,
                    "invalid GPU virtual address");
            wait(map.PagingFenceValue);
        }
#endif
        for (size_t i = 0; i < bytes / sizeof(*words); ++i)
            require(words[i] == (UINT64(i) ^ 0x5a9c17e3b86402dfull), "backing pages changed during mapping/paging");
        if (submit) execute(kernels);
        std::printf("PASS standard_heap_bytes=%zu gpu_va=0x%llx mapped=%d paging_complete=1 commands_submitted=%u\n",
                    bytes, static_cast<unsigned long long>(address), int(map_gpu), submit ? warmups + repeats : 0);
    }
    void append(std::vector<uint32_t>& commands, const orca::l0::drm::detail::Kernel& k,
                uint32_t local, uint32_t groups, std::initializer_list<uint64_t> args, size_t slot) {
        namespace d = orca::l0::drm::detail;
        require(k.isa.size() <= 16384 && k.cross_bytes <= 4096 && slot < 4, "demo heap size exceeded");
        require(k.required_local == std::array<uint32_t, 3>{} ||
                k.required_local == std::array<uint32_t, 3>{local, 1, 1}, "required local size mismatch");
        const size_t isa_offset = data_offset + 12288 + slot * 24576, indirect_offset = isa_offset + 20480;
        require(indirect_offset + 4096 <= bytes, "demo heap outside allocation");
        std::memset(static_cast<uint8_t*>(memory) + isa_offset, 0, 24576);
        std::memcpy(static_cast<uint8_t*>(memory) + isa_offset, k.isa.data(), k.isa.size());
        std::vector<uint8_t> cross(k.cross_bytes);
        uint32_t count = 0;
        for (const auto& p : k.payload) {
            auto* target = cross.data() + p.offset;
            if (p.index != UINT32_MAX) {
                // ponytail: this demo has pointer/scalar arguments of at most eight bytes.
                // The general runtime needs the complete argument byte spans.
                require(p.index < args.size() && p.source <= 8 && p.size <= 8 - p.source, "demo argument size/index mismatch");
                count = std::max(count, p.index + 1);
                std::memcpy(target, reinterpret_cast<const uint8_t*>(args.begin() + p.index) + p.source, p.size);
            } else {
                std::array<uint32_t, 3> v{};
                if (p.type == "local_size" || p.type == "enqueued_local_size") v = {local, 1, 1};
                if (p.type == "global_size") v = {local * groups, 1, 1};
                if (p.type == "group_count") v = {groups, 1, 1};
                std::memcpy(target, v.data(), sizeof(v));
            }
        }
        require(count == args.size(), "demo argument count mismatch");
#ifdef ORCA_L0_WDDM_DG2
        std::vector<orca::l0::dg2::Argument> arguments;
        for (uint32_t i = 0; i < count; ++i) {
            const auto p = std::find_if(k.payload.begin(), k.payload.end(), [&](const auto& p) { return p.index == i; });
            require(p != k.payload.end() && !p->source, "benchmark argument is not a simple C ABI value");
            arguments.push_back({args.begin() + i, p->size});
        }
        constexpr const char* names[]{"rms_norm", "gate_up", "swiglu", "advance"};
        dg2->append(image, count == 2 ? "advance" : names[slot], local, groups, arguments);
#endif
#ifdef ORCA_L0_WDDM_BASELINE
        const auto handle = l0_kernels.at(slot);
        for (uint32_t i = 0; i < count; ++i) {
            const auto p = std::find_if(k.payload.begin(), k.payload.end(), [&](const auto& p) { return p.index == i; });
            require(p != k.payload.end() && !p->source, "baseline argument is not a simple C ABI value");
            orca::l0::check(zeKernelSetArgumentValue(handle, i, p->size, args.begin() + i), "baseline kernel argument");
        }
        l0_commands->launch(handle, {groups, 1, 1}, {local, 1, 1});
        l0_commands->barrier();
#endif
        const auto inline_bytes = std::min(k.inline_bytes, k.cross_bytes);
        std::memcpy(static_cast<uint8_t*>(memory) + indirect_offset, cross.data() + inline_bytes, cross.size() - inline_bytes);
        // Calibrated DG2 fixed WB entry from gmmlib GmmGen12dGPUCachePolicy.cpp.
        // Keep GPU dependencies between kernels; CPU visibility is established
        // once at the end of the list before its monitored completion fence.
        d::record_dispatch(commands, k, cross, local, groups, address + isa_offset, 20480, address + indirect_offset, 4096, &encoder_state, 6, false);
    }
    void execute(const std::vector<orca::l0::drm::detail::Kernel>& kernels) {
        const bool compute = !kernels.empty(), pipeline = kernels.size() == 4;
#if !defined(ORCA_L0_WDDM_BASELINE) && !defined(ORCA_L0_WDDM_DG2)
        D3DKMT_CREATESYNCHRONIZATIONOBJECT2 sync{};
        sync.hDevice = device; sync.Info.Type = D3DDDI_MONITORED_FENCE;
        check(D3DKMTCreateSynchronizationObject2(&sync), "create monitored fence"); monitor = sync.hSyncObject;
        monitor_address = sync.Info.MonitoredFence.FenceValueGPUVirtualAddress;
        monitor_cpu = static_cast<volatile UINT64*>(sync.Info.MonitoredFence.FenceValueCPUVirtualAddress);
        require(monitor && monitor_cpu && monitor_address, "missing monitored fence addresses");
#endif
        // Same calibrated driver profile as the 144-byte context; the shipped helper
        // identifies a 184-byte header, NOT NEO's 24-byte public fallback.
        alignas(8) std::array<uint8_t, 184> header{};
        header[2] = 5; // UmdContextType=UMD_OCL (bits 16..19)
        header[8] = 4; // NeedsMidBatchPreEmptionSupport
        const uint32_t perf_tag = 0xc000; // gmmlib UmKmDmaPerfTimer.h: PERFTAG_OCL
        std::memcpy(header.data() + 28, &perf_tag, sizeof(perf_tag));
        std::memcpy(header.data() + 48, &monitor_address, sizeof(monitor_address));
        D3DKMT_SUBMITCOMMAND command{};
        command.Commands = canonical(address); command.CommandLength = sizeof(packet(0, 0));
        command.BroadcastContextCount = 1; command.BroadcastContext[0] = context;
        command.pPrivateDriverData = header.data(); command.PrivateDriverDataSize = header.size();
        std::vector<uint8_t> expected_bytes(bytes);
        const size_t input = data_offset, weight = input + 256, norm = input + 512, middle = input + 768,
                     output = input + 1024, parameter = input + 1280, matrix = input + 4096;
        constexpr uint32_t width = 32, rows = 16;
        struct Parameters { uint32_t width, rows; float scale, epsilon; } params{width, rows, 1, 1e-5f};
        float square = 0;
        auto floats = [&](size_t at) { return reinterpret_cast<float*>(static_cast<uint8_t*>(memory) + at); };
        if (compute) {
            namespace d = orca::l0::drm::detail;
            std::vector<uint32_t> commands;
            if (pipeline) {
                auto va = [&](size_t at) { return canonical(address + at); };
                append(commands, kernels[0], 64, 1, {va(input), va(weight), va(norm), va(parameter)}, 0);
                append(commands, kernels[1], 32, 1, {va(matrix), va(norm), va(middle), va(parameter)}, 1);
                append(commands, kernels[2], 16, 1, {va(middle), va(output), va(parameter)}, 2);
                append(commands, kernels[3], 16, 1, {va(output), 3}, 3);
                for (uint32_t i = 0; i < width; ++i) {
                    floats(input)[i] = float(int(i % 17) - 8) / 16;
                    floats(weight)[i] = 0.5f + float(i % 7) / 8;
                    square += floats(input)[i] * floats(input)[i];
                }
                for (uint32_t i = 0; i < 2 * rows * width; ++i) floats(matrix)[i] = float(int(i % 13) - 6) / 16;
            } else {
                for (uint32_t i = 0; i < launches; ++i)
                    append(commands, kernels[0], 64, 16, {canonical(address + data_offset), 3}, 0);
                for (uint32_t i = 0; i < 1040; ++i) floats(data_offset)[i] = float(i);
            }
            d::emit(commands, d::barrier()); // Flush to host before KMD signals the completion fence.
            d::emit(commands, orca::l0::drm::packet::DG2::MI_BATCH_BUFFER_END::sInit());
            if (commands.size() & 1) commands.push_back(0);
            require(commands.size() * 4 <= data_offset, "demo command buffer size exceeded");
            std::memcpy(memory, commands.data(), commands.size() * 4);
            command.CommandLength = static_cast<UINT>(commands.size() * 4);
#ifdef ORCA_L0_WDDM_BASELINE
            l0_commands->close();
#endif
#ifdef ORCA_L0_WDDM_DG2
            dg2->close();
#endif
            std::memcpy(expected_bytes.data(), memory, bytes);
        }
        std::vector<double> submit_us, complete_us;
        uint32_t native_wait_calls = 0;
        for (UINT64 pass = 1; pass <= warmups + repeats; ++pass) {
            const uint32_t expected = 0x31410000 + static_cast<uint32_t>(pass);
            const auto commands = packet(address + output_offset, expected);
            if (!compute) std::memcpy(memory, commands.data(), sizeof(commands));
            if (pipeline) {
                params.scale = 0.5f + float(pass) / 128;
                std::memcpy(static_cast<uint8_t*>(memory) + parameter, &params, sizeof(params));
                std::memcpy(expected_bytes.data() + parameter, &params, sizeof(params));
            }
            using Clock = std::chrono::steady_clock;
            ULONG64 cycles_before = 0, cycles_after = 0;
            require(QueryThreadCycleTime(GetCurrentThread(), &cycles_before) != 0, "read thread cycles before submit");
            const auto start = Clock::now();
            MemoryBarrier();
#ifdef ORCA_L0_WDDM_DG2
            dg2->submit();
#elif defined(ORCA_L0_WDDM_BASELINE)
            l0_commands->submit();
#else
            std::memcpy(header.data() + 56, &pass, sizeof(pass));
            check(D3DKMTSubmitCommand(&command), "submit Orca-encoded commands");
#endif
            const auto submitted = Clock::now();
            bool completed = false;
            if (polling || spin_us) {
                uint32_t polls = 0;
                for (;;) {
#ifdef ORCA_L0_WDDM_DG2
                    completed = dg2->ready();
#elif defined(ORCA_L0_WDDM_BASELINE)
                    completed = l0_commands->ready();
#else
                    completed = *monitor_cpu >= pass;
#endif
                    if (completed) break;
                    _mm_pause();
                    if (!(++polls & (polling ? 255 : 31))) {
                        const auto elapsed = Clock::now() - submitted;
                        require(elapsed < std::chrono::seconds(10), "polling fence timeout");
                        if (spin_us && elapsed >= std::chrono::microseconds(spin_us)) break;
                    }
                }
            }
            if (!completed) {
                if (pass > warmups) ++native_wait_calls;
#ifdef ORCA_L0_WDDM_DG2
                dg2->wait(10000000000ull);
#elif defined(ORCA_L0_WDDM_BASELINE)
                require(l0_commands->wait(10000000000ull), "baseline fence timeout");
#else
                wait(pass, monitor);
#endif
            }
            const auto done = Clock::now();
            require(QueryThreadCycleTime(GetCurrentThread(), &cycles_after) != 0 && cycles_after > cycles_before,
                    "read thread cycles after completion");
#if !defined(ORCA_L0_WDDM_BASELINE) && !defined(ORCA_L0_WDDM_DG2)
            require(*monitor_cpu >= pass, "monitored fence signaled too early");
#endif
            if (pipeline) {
                auto verify_value = [&](size_t at, uint32_t i, float expected_value) {
                    const auto value = floats(at)[i];
                    require(std::isfinite(value) && std::abs(value - expected_value) <= 2e-4f * (1 + std::abs(expected_value)),
                            "RMS/GEMV/SwiGLU/value-argument pipeline mismatch");
                    std::memcpy(expected_bytes.data() + at + i * 4, &value, 4);
                };
                const auto factor = params.scale / std::sqrt(square / width + params.epsilon);
                for (uint32_t i = 0; i < width; ++i) verify_value(norm, i, floats(input)[i] * factor * floats(weight)[i]);
                for (uint32_t row = 0; row < rows; ++row) {
                    float gate = 0, up = 0;
                    for (uint32_t i = 0; i < width; ++i) {
                        const auto value = floats(input)[i] * factor * floats(weight)[i];
                        gate += floats(matrix)[row * width + i] * value;
                        up += floats(matrix)[(rows + row) * width + i] * value;
                    }
                    verify_value(middle, row, gate); verify_value(middle, rows + row, up);
                    verify_value(output, row, gate / (1 + std::exp(-gate)) * up + 3);
                }
                require(!std::memcmp(memory, expected_bytes.data(), bytes), "pipeline modified input/heap/output guards");
            } else if (compute) {
                for (uint32_t i = 0; i < 1024; ++i) {
                    const auto value = float(i + 3 * pass * launches);
                    std::memcpy(expected_bytes.data() + data_offset + i * 4, &value, 4);
                }
                require(!std::memcmp(memory, expected_bytes.data(), bytes), "zebin multi-group increment/guard mismatch");
            } else {
                require(*reinterpret_cast<volatile uint32_t*>(static_cast<uint8_t*>(memory) + output_offset) == expected,
                        "GPU store/replay output mismatch");
                const auto* words = static_cast<const UINT64*>(memory);
                for (size_t i = commands.size() / 2; i < bytes / sizeof(*words); ++i) {
                    auto expected_word = UINT64(i) ^ 0x5a9c17e3b86402dfull;
                    if (i == output_offset / sizeof(*words)) expected_word = (expected_word & 0xffffffff00000000ull) | expected;
                    require(words[i] == expected_word, "GPU store changed guard pages");
                }
            }
            if (pass > warmups) {
                submit_us.push_back(std::chrono::duration<double, std::micro>(submitted - start).count());
                complete_us.push_back(std::chrono::duration<double, std::micro>(done - start).count());
                std::printf("SAMPLE %llu %.3f %.3f %llu\n", static_cast<unsigned long long>(pass - warmups - 1),
                            submit_us.back(), complete_us.back(), static_cast<unsigned long long>(cycles_after - cycles_before));
            }
        }
        std::sort(submit_us.begin(), submit_us.end()); std::sort(complete_us.begin(), complete_us.end());
        const auto mid = repeats / 2;
        std::printf("PASS %u GPU %s and guards; launches=%u wait=%s median submit_us=%.3f complete_us=%.3f native_wait_calls=%u\n", warmups + repeats,
                    pipeline ? "zebin pipelines (CPU tolerance 2e-4 relative + absolute)" : compute ? "exact zebin increments" : "exact stores",
                    launches, polling ? "poll" : spin_us == 50 ? "hybrid50" : spin_us == 100 ? "hybrid100" : "native",
                    (submit_us[mid - 1] + submit_us[mid]) / 2, (complete_us[mid - 1] + complete_us[mid]) / 2, native_wait_calls);
    }
};
}
int main(int argc, char** argv) try {
    if (argc == 2 && !std::strcmp(argv[1], "--self-check")) { self_check(); return 0; }
    const bool bench = argc == 5 && !std::strcmp(argv[1], "--bench");
    if (bench) {
        require(!std::strcmp(argv[2], "increment") || !std::strcmp(argv[2], "pipeline") || !std::strcmp(argv[2], "chain400"), "unknown benchmark workload");
        require(!std::strcmp(argv[4], "native") || !std::strcmp(argv[4], "poll") ||
                !std::strcmp(argv[4], "hybrid50") || !std::strcmp(argv[4], "hybrid100"), "unknown benchmark wait mode");
    }
    const bool pipeline = (bench && !std::strcmp(argv[2], "pipeline")) || (argc == 3 && !std::strcmp(argv[1], "--pipeline"));
    const bool compute = bench || pipeline || (argc == 3 && !std::strcmp(argv[1], "--compute"));
    if (!compute && (argc > 2 || (argc == 2 && std::strcmp(argv[1], "--map") && std::strcmp(argv[1], "--context") && std::strcmp(argv[1], "--submit"))))
        throw std::invalid_argument("usage: wddm-check [--map|--context|--submit|--self-check|--compute zebin|--pipeline zebin|--bench increment|pipeline|chain400 zebin native|poll|hybrid50|hybrid100]");
    const char* image = compute ? argv[bench ? 3 : 2] : nullptr;
    std::vector<orca::l0::drm::detail::Kernel> kernels;
    if (pipeline) {
        for (const auto* name : {"rms_norm", "gate_up", "swiglu", "advance"}) kernels.push_back(orca::l0::drm::detail::load(image, name));
    } else if (compute) kernels.push_back(orca::l0::drm::detail::load(image, "advance"));
    const bool submit = compute || (argc == 2 && !std::strcmp(argv[1], "--submit"));
    const bool create_context = submit || (argc == 2 && !std::strcmp(argv[1], "--context"));
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    dependencies();
    Adapters adapters;
    D3DKMT_HANDLE selected = 0;
    for (UINT i = 0; i < adapters.info.NumAdapters; ++i) {
        const auto a = adapters.info.Adapters[i].hAdapter;
        const auto type = query<D3DKMT_ADAPTERTYPE>(a, KMTQAITYPE_ADAPTERTYPE);
        if (type.SoftwareDevice || type.IndirectDisplayDevice) continue;
        const auto ids = query<D3DKMT_QUERY_DEVICE_IDS>(a, KMTQAITYPE_PHYSICALADAPTERDEVICEIDS);
        if (ids.DeviceIds.VendorID != 0x8086) continue;
        require(selected == 0, "multiple Intel adapters: select a device explicitly before testing");
        selected = a;
        const auto version = query<D3DKMT_DRIVERVERSION>(a, KMTQAITYPE_DRIVERVERSION);
        const auto driver = query<UINT64>(a, KMTQAITYPE_UMD_DRIVER_VERSION);
        require(version >= KMT_DRIVERVERSION_WDDM_2_3, "standard allocation requires WDDM 2.3");
        std::printf("INFO vendor=0x%x device=0x%x wddm=%u\n", ids.DeviceIds.VendorID, ids.DeviceIds.DeviceID, unsigned(version));
        if (create_context) {
            std::printf("INFO driver_version=0x%llx\n", static_cast<unsigned long long>(driver));
            require(ids.DeviceIds.DeviceID == 0x56a0 && driver == ((UINT64{32} << 48) | (UINT64{101} << 16) | 9034),
                    "context ABI is only calibrated for A770 driver 32.0.101.9034");
        }
    }
    require(selected != 0, "no Intel hardware adapter");
    for (int i = 0; i < (bench ? 1 : 3); ++i) {
        MemoryCheck memory(selected);
        if (bench) {
            memory.data_offset = 262144; memory.warmups = 128; memory.repeats = 512;
            memory.launches = !std::strcmp(argv[2], "chain400") ? 400 : 1;
            memory.polling = !std::strcmp(argv[4], "poll");
            if (!std::strcmp(argv[4], "hybrid50")) memory.spin_us = 50;
            if (!std::strcmp(argv[4], "hybrid100")) memory.spin_us = 100;
        }
        memory.image = image;
        memory.run(argc >= 2, create_context, submit, kernels);
        require(memory.release(), "WDDM cleanup failed");
    }
    dependencies();
#ifdef ORCA_L0_WDDM_DG2
    std::puts("PASS reusable DG2 runtime and cleanup; no Level Zero/Intel UMD/compiler/GMM loaded");
#elif defined(ORCA_L0_WDDM_BASELINE)
    std::puts("PASS Level Zero baseline and cleanup; Intel UMD used, no SYCL/UR/ze_loader");
#else
    std::puts("PASS WDDM allocation/residency/cleanup; no Level Zero/Intel UMD/compiler/GMM loaded");
#endif
    return 0;
} catch (const std::exception& e) { std::fprintf(stderr, "%s\n", e.what()); return 1; }
