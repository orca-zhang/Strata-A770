// Calibrated Windows DG2 transport. Gdi32 is the only GPU-facing dependency.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winternl.h>
#include <d3dkmthk.h>
#include <orca_l0/dg2.hpp>
#include "../drm/arguments.hpp"
#include "allocation.hpp"
#include <chrono>
#include <cstdio>
#include <immintrin.h>
#include <map>
#include <mutex>
#include <atomic>
#include <system_error>

namespace orca::l0::dg2 {
namespace detail = drm::detail;
namespace {
using detail::require;
void check(NTSTATUS result, const char* operation) {
    if (result >= 0) return;
    char message[160];
    std::snprintf(message, sizeof(message), "%s: NTSTATUS=0x%08lx", operation, static_cast<unsigned long>(result));
    throw std::runtime_error(message);
}
template<class T> T query(D3DKMT_HANDLE adapter, KMTQUERYADAPTERINFOTYPE type) {
    T result{};
    D3DKMT_QUERYADAPTERINFO q{};
    q.hAdapter = adapter; q.Type = type; q.pPrivateDriverData = &result; q.PrivateDriverDataSize = sizeof(result);
    check(D3DKMTQueryAdapterInfo(&q), "query adapter");
    return result;
}
constexpr uint64_t address_mask = (uint64_t{1} << 48) - 1;
} // namespace

// Only the device and fence mappings may outlive Runtime. Normal backing pages
// are released during owner teardown; failed teardown retains them until device close.
struct CompletionDevice {
    D3DKMT_HANDLE adapter = 0, device = 0;
    bool contexts_destroyed = false;
    std::vector<Allocation> retained;
    ~CompletionDevice() {
        bool released = true;
        if (device) { D3DKMT_DESTROYDEVICE d{}; d.hDevice = device; released = D3DKMTDestroyDevice(&d) >= 0; }
        // ponytail: failed device teardown keeps backing pages until process exit.
        if (released) for (const auto& a : retained) if (a.data && !a.imported) VirtualFree(a.data, 0, MEM_RELEASE);
        if (adapter) { D3DKMT_CLOSEADAPTER d{}; d.hAdapter = adapter; D3DKMTCloseAdapter(&d); }
    }
};
struct NativeCompletion {
    std::shared_ptr<CompletionDevice> transport;
    explicit NativeCompletion(std::shared_ptr<CompletionDevice> owner) : transport(std::move(owner)) {}
    ~NativeCompletion() {
        if (monitor && transport->contexts_destroyed) {
            D3DKMT_DESTROYSYNCHRONIZATIONOBJECT d{}; d.hSyncObject = monitor;
            D3DKMTDestroySynchronizationObject(&d);
        }
    }
    std::atomic<bool> closing{false};
    std::atomic<uint64_t> observed{0};
    D3DKMT_HANDLE device = 0, monitor = 0;
    volatile UINT64* cpu = nullptr;
    void cancel() noexcept { closing.store(true, std::memory_order_release); }
    uint64_t read() {
        const auto value = *cpu; MemoryBarrier();
        auto prior = observed.load(std::memory_order_relaxed);
        while (prior < value && !observed.compare_exchange_weak(prior, value, std::memory_order_release, std::memory_order_relaxed)) {}
        return value;
    }
    bool ready(uint64_t target) {
        require(!closing.load(std::memory_order_acquire), "completion belongs to a closing or destroyed runtime");
        if (observed.load(std::memory_order_acquire) >= target) return true;
        require(!closing.load(std::memory_order_acquire) && cpu, "completion belongs to a closing or destroyed runtime");
        return read() >= target;
    }
    bool wait(uint64_t target, uint64_t timeout_ns) {
        require(!closing.load(std::memory_order_acquire), "completion belongs to a closing or destroyed runtime");
        if (observed.load(std::memory_order_acquire) >= target) return true;
        require(!closing.load(std::memory_order_acquire) && cpu, "completion belongs to a closing or destroyed runtime");
        using Clock = std::chrono::steady_clock;
        const auto start = Clock::now();
        if (read() >= target) return true;
        if (!timeout_ns) return false;
        // One event per observing thread, reused across queues and Runtime instances.
        struct ThreadEvent {
            HANDLE value = CreateEventW(nullptr, FALSE, FALSE, nullptr);
            ~ThreadEvent() { if (value) CloseHandle(value); }
        };
        thread_local ThreadEvent event;
        require(event.value, "create completion observer event");
        require(ResetEvent(event.value), "reset completion observer event");
        D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMCPU w{};
        w.hDevice = device; w.ObjectCount = 1; w.ObjectHandleArray = &monitor;
        w.FenceValueArray = &target; w.hAsyncEvent = event.value;
        check(D3DKMTWaitForSynchronizationObjectFromCpu(&w), "register completion observer wait");
        while (!closing.load(std::memory_order_acquire)) {
            const auto elapsed = uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count());
            if (elapsed >= timeout_ns) return read() >= target;
            // Cancellation checks bound teardown; these are blocking waits, not polling.
            // Keep the original registration through wait slices and spurious wakes.
            const auto ms = DWORD(std::min<uint64_t>(50, (timeout_ns - elapsed + 999999) / 1000000));
            const auto result = WaitForSingleObject(event.value, ms);
            require(result == WAIT_OBJECT_0 || result == WAIT_TIMEOUT, "completion observer wait failed");
            // An older timed-out registration may wake the reused event. Always check this point.
            if (read() >= target) return true;
        }
        throw std::runtime_error("completion belongs to a closing or destroyed runtime");
    }
};
bool Completion::ready() const {
    require(observer_ && sequence_, "invalid completion observer"); return observer_->ready(sequence_);
}
bool Completion::wait(uint64_t timeout_ns) const {
    require(observer_ && sequence_ && timeout_ns <= uint64_t(INT64_MAX), "invalid completion observer or timeout");
    return observer_->wait(sequence_, timeout_ns);
}
struct HostGate::State {
    std::shared_ptr<const int> owner;
    std::mutex mutex;
    D3DKMT_HANDLE device = 0, monitor = 0;
    bool released = false;
    bool reusable = false;
    Allocation memory;
    std::atomic<uint32_t>* word = nullptr;
    void signal() {
        std::lock_guard<std::mutex> lock(mutex);
        if (word) {
            word->store(1, std::memory_order_release); _mm_clflush(word); _mm_mfence();
            return;
        }
        require(monitor && device, "host gate belongs to a destroyed runtime");
        if (released) return;
        const UINT64 one = 1;
        D3DKMT_SIGNALSYNCHRONIZATIONOBJECTFROMCPU signal{};
        signal.hDevice = device; signal.ObjectCount = 1;
        signal.ObjectHandleArray = &monitor; signal.FenceValueArray = &one;
        MemoryBarrier();
        check(D3DKMTSignalSynchronizationObjectFromCpu(&signal), "signal host gate");
        released = true;
    }
    void close(bool destroy = true) noexcept {
        std::lock_guard<std::mutex> lock(mutex);
        if (monitor && destroy) {
            D3DKMT_DESTROYSYNCHRONIZATIONOBJECT d{}; d.hSyncObject = monitor;
            D3DKMTDestroySynchronizationObject(&d);
        }
        monitor = device = 0; word = nullptr;
    }
    void reset() {
        std::lock_guard<std::mutex> lock(mutex);
        require(reusable && word, "reset requires a live reusable host gate");
        word->store(0, std::memory_order_release); _mm_clflush(word); _mm_mfence();
    }
    ~State() { close(); }
};
void HostGate::signal() const {
    require(bool(state_), "invalid host gate"); state_->signal();
}
struct NativeQueue {
    std::shared_ptr<NativeCompletion> observer;
    D3DKMT_HANDLE context = 0, monitor = 0;
    unsigned long hw_context = 0;
    volatile UINT64* monitor_cpu = nullptr;
    UINT64 monitor_gpu = 0, sequence = 0, done = 0;
    std::array<size_t, 2> current_scratch{SIZE_MAX, SIZE_MAX};
    size_t current_private = SIZE_MAX;
};
struct Runtime::State : NativeQueue {
    std::shared_ptr<CompletionDevice> completion_device = std::make_shared<CompletionDevice>();
    D3DKMT_HANDLE& adapter = completion_device->adapter;
    D3DKMT_HANDLE& device = completion_device->device;
    D3DKMT_HANDLE paging = 0, paging_fence = 0;
    uint64_t local_bytes = 0;
    std::deque<NativeQueue> queues;
    std::vector<std::weak_ptr<HostGate::State>> host_gates;
    NativeQueue& queue(size_t index) { return index ? queues.at(index - 1) : *this; }
    const NativeQueue& queue(size_t index) const { return index ? queues.at(index - 1) : *this; }
    bool idle() const {
        // Immutable completion observers remember successful waits. Owner-side
        // command/resource collection may intentionally follow at the next submit.
        const auto complete = [](const NativeQueue& q) { return q.done == q.sequence || q.observer->ready(q.sequence); };
        return complete(*this) && std::all_of(queues.begin(), queues.end(), complete);
    }
    HANDLE event = nullptr;
    std::vector<Allocation> allocations;
    std::array<uint64_t, 2> cpu_bounds{UINT64_MAX, 0};
    std::map<uint32_t, detail::RecordingStorage> storage;
    std::map<uint64_t, detail::RecordingStorage*> storage_by_gpu;
    struct Backing {
        D3DKMT_HANDLE resource = 0;
        std::vector<D3DKMT_HANDLE> parts; // Large device buffers share one contiguous GPU VA.
    };
    std::vector<Backing> resources;
    detail::AllocationPool batch_pool, parameter_pool, transfer_batch_pool, transfer_parameter_pool;
    struct List {
        std::vector<uint32_t> commands;
        detail::DispatchState encoder_state;
        std::shared_ptr<const Allocation> batch;
        std::vector<std::shared_ptr<const Allocation>> captures;
        std::map<uint32_t, std::array<size_t, 2>> parameter_uploads;
        std::vector<std::shared_ptr<HostGate::State>> gates;
        std::vector<detail::RecordingStorage*> storage;
        alignas(8) std::array<uint8_t, 184> header{};
        D3DKMT_SUBMITCOMMAND execute{};
        uint64_t sequence = 0;
        uint64_t generation = 1;
        size_t queue = 0;
        bool closed = false;
    } recording;
    std::deque<List> recordings;
    std::vector<size_t> free_recordings;
    std::shared_ptr<const int> identity = std::make_shared<const int>(0);
    List& list(size_t index) { return index == SIZE_MAX ? recording : recordings.at(index); }
    detail::RecordingHeap isa_heap, memory_isa_heap;
    detail::CaptureHeap indirect_heap;
    std::map<uint32_t, std::array<Allocation, 2>> parameter_mirrors;
    bool local_parameters = false;
    uint32_t max_threads = 4096, mocs = 6;
    bool host_visibility = false;
    bool disable_overdispatch = false;
    bool local_scratch_cache = false;
    std::map<std::pair<std::string, std::string>, detail::RecordedImage> images;
    std::map<std::string, detail::ModuleData> module_data;
    std::string memory_module;
    bool memory_copy_2d = false, memory_copy_coaligned = false;
    size_t storage_queue = SIZE_MAX;
    bool local_thread_memory = false, local_isa = false;
    Allocation thread_readback, isa_staging;
    std::vector<detail::ThreadMemory> thread_memory;
    std::map<std::array<size_t, 2>, Allocation> scratch_descriptors;
    bool failed = false;

    Allocation create_allocation(size_t bytes, void* host = nullptr, bool device_local = false);
    void create_queue(NativeQueue& q) {
        q.observer = std::make_shared<NativeCompletion>(completion_device);
        alignas(8) std::array<uint8_t, 144> data{};
        const auto pointer = reinterpret_cast<uintptr_t>(&q.hw_context);
        const auto pid = GetCurrentProcessId(); const uint32_t kind = 5;
        std::memcpy(data.data(), &pointer, sizeof(pointer)); std::memcpy(data.data() + 12, &pid, sizeof(pid));
        data[36] = 1; data[68] = 1; std::memcpy(data.data() + 80, &kind, sizeof(kind));
        D3DKMT_CREATECONTEXTVIRTUAL context{};
        context.hDevice = device; context.NodeOrdinal = 5; context.ClientHint = D3DKMT_CLIENTHINT_ONEAPI_LEVEL0;
        context.pPrivateDriverData = data.data(); context.PrivateDriverDataSize = sizeof(data);
        check(D3DKMTCreateContextVirtual(&context), "create compute context"); q.context = context.hContext;
        require(q.context != 0, "context returned no handle");
        D3DKMT_CREATESYNCHRONIZATIONOBJECT2 sync{}; sync.hDevice = device; sync.Info.Type = D3DDDI_MONITORED_FENCE;
        check(D3DKMTCreateSynchronizationObject2(&sync), "create monitored fence"); q.monitor = sync.hSyncObject; q.observer->monitor = q.monitor;
        q.monitor_cpu = static_cast<volatile UINT64*>(sync.Info.MonitoredFence.FenceValueCPUVirtualAddress);
        q.monitor_gpu = sync.Info.MonitoredFence.FenceValueGPUVirtualAddress;
        require(q.monitor && q.monitor_cpu && q.monitor_gpu, "missing monitored fence mapping");
        q.observer->device = device; q.observer->monitor = q.monitor; q.observer->cpu = q.monitor_cpu;
    }

    bool wait_fence(D3DKMT_HANDLE object, UINT64 value, DWORD milliseconds) {
        require(ResetEvent(event) != 0, "reset fence event");
        D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMCPU w{};
        w.hDevice = device; w.ObjectCount = 1; w.ObjectHandleArray = &object;
        w.FenceValueArray = &value; w.hAsyncEvent = event;
        check(D3DKMTWaitForSynchronizationObjectFromCpu(&w), "register CPU fence wait");
        const auto result = WaitForSingleObject(event, milliseconds);
        require(result == WAIT_OBJECT_0 || result == WAIT_TIMEOUT, "CPU fence wait failed");
        return result == WAIT_OBJECT_0;
    }
    ~State() {
        for (size_t i = 0; i <= queues.size(); ++i) if (queue(i).observer) queue(i).observer->cancel();
        // Cancellation during teardown must not leave an unserviced host gate blocking a queue.
        for (const auto& weak : host_gates) if (const auto gate = weak.lock()) try { gate->signal(); } catch (...) {}
        // Backing pages must survive all GPU accesses, including a timed-out list.
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        for (size_t i = 0; i <= queues.size(); ++i) {
            const auto& q = queue(i);
            if (q.done != q.sequence && q.monitor_cpu && *q.monitor_cpu < q.sequence) try {
                const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
                wait_fence(q.monitor, q.sequence, DWORD(std::max<int64_t>(0, remaining)));
            } catch (...) {}
        }
        bool context_destroyed = true;
        for (size_t i = 0; i <= queues.size(); ++i) if (queue(i).context) {
            D3DKMT_DESTROYCONTEXT d{}; d.hContext = queue(i).context;
            context_destroyed = (D3DKMTDestroyContext(&d) >= 0) && context_destroyed;
        }
        for (const auto& weak : host_gates) if (const auto gate = weak.lock()) gate->close(context_destroyed);
        completion_device->contexts_destroyed = context_destroyed;
        bool allocations_released = context_destroyed;
        if (context_destroyed) {
            for (size_t i = 0; i < allocations.size(); ++i) {
                const auto& a = allocations[i];
                if (a.gpu) {
                    D3DKMT_FREEGPUVIRTUALADDRESS d{};
                    d.hAdapter = adapter; d.BaseAddress = a.gpu & address_mask; d.Size = a.bytes;
                    D3DKMTFreeGpuVirtualAddress(&d);
                }
                if (a.handle || resources[i].resource) {
                    D3DKMT_DESTROYALLOCATION2 d{}; d.hDevice = device;
                    d.Flags.SynchronousDestroy = 1;
                    if (resources[i].resource) d.hResource = resources[i].resource;
                    else if (resources[i].parts.empty()) { d.phAllocationList = &a.handle; d.AllocationCount = 1; }
                    else { d.phAllocationList = resources[i].parts.data(); d.AllocationCount = UINT(resources[i].parts.size()); }
                    allocations_released = (D3DKMTDestroyAllocation2(&d) >= 0) && allocations_released;
                }
            }
            if (paging) { D3DDDI_DESTROYPAGINGQUEUE d{}; d.hPagingQueue = paging; D3DKMTDestroyPagingQueue(&d); }
        }
        if (allocations_released) {
            for (const auto& a : allocations) if (a.data && !a.imported) VirtualFree(a.data, 0, MEM_RELEASE);
        } else completion_device->retained = std::move(allocations);
        if (event) CloseHandle(event);
    }
};

Runtime::Runtime(const char* render_node) : state_(std::make_unique<State>()) {
    require(!render_node, "Windows selects a hardware adapter, not a Linux render node");
    auto& s = *state_;
    struct Adapters {
        D3DKMT_ENUMADAPTERS value{};
        ~Adapters() {
            for (UINT i = 0; i < value.NumAdapters; ++i) if (value.Adapters[i].hAdapter) {
                D3DKMT_CLOSEADAPTER d{}; d.hAdapter = value.Adapters[i].hAdapter; D3DKMTCloseAdapter(&d);
            }
        }
    } adapters;
    check(D3DKMTEnumAdapters(&adapters.value), "enumerate adapters");
    UINT selected = UINT32_MAX;
    for (UINT i = 0; i < adapters.value.NumAdapters; ++i) {
        const auto a = adapters.value.Adapters[i].hAdapter;
        const auto type = query<D3DKMT_ADAPTERTYPE>(a, KMTQAITYPE_ADAPTERTYPE);
        if (type.SoftwareDevice || type.IndirectDisplayDevice) continue;
        const auto ids = query<D3DKMT_QUERY_DEVICE_IDS>(a, KMTQAITYPE_PHYSICALADAPTERDEVICEIDS);
        if (ids.DeviceIds.VendorID != 0x8086) continue;
        require(selected == UINT32_MAX, "multiple Intel GPUs need explicit selection");
        selected = i;
        // ponytail: calibrated A770 C0 only; validate other steppings/Linux before opting in.
        s.disable_overdispatch = ids.DeviceIds.RevisionID == 8;
        s.local_scratch_cache = ids.DeviceIds.RevisionID == 8;
        s.local_parameters = ids.DeviceIds.RevisionID == 8; // Diagnostic current-V10 policy, Windows C0 only.
        const auto driver = query<UINT64>(a, KMTQAITYPE_UMD_DRIVER_VERSION);
        require(ids.DeviceIds.DeviceID == 0x56a0 && driver == ((UINT64{32} << 48) | (UINT64{101} << 16) | 9034),
                "WDDM private ABI is calibrated only for A770 driver 32.0.101.9034");
        require(query<D3DKMT_DRIVERVERSION>(a, KMTQAITYPE_DRIVERVERSION) >= KMT_DRIVERVERSION_WDDM_2_3,
                "standard allocations require WDDM 2.3");
    }
    require(selected != UINT32_MAX, "no calibrated Intel GPU");
    s.adapter = adapters.value.Adapters[selected].hAdapter;
    adapters.value.Adapters[selected].hAdapter = 0;
    s.local_bytes = query<D3DKMT_SEGMENTSIZEINFO>(s.adapter, KMTQAITYPE_GETSEGMENTSIZE).DedicatedVideoMemorySize;
    require(s.local_bytes, "adapter reported no dedicated video memory");
    D3DKMT_CREATEDEVICE device{}; device.hAdapter = s.adapter;
    check(D3DKMTCreateDevice(&device), "create device"); s.device = device.hDevice;
    D3DKMT_NODEMETADATA node{}; node.NodeOrdinalAndAdapterIndex = 5;
    D3DKMT_QUERYADAPTERINFO nq{}; nq.hAdapter = s.adapter; nq.Type = KMTQAITYPE_NODEMETADATA;
    nq.pPrivateDriverData = &node; nq.PrivateDriverDataSize = sizeof(node);
    check(D3DKMTQueryAdapterInfo(&nq), "query compute node");
    // This driver's CCS0 metadata reports EngineType=0; the verified ordinal is 5.
    require(node.NodeData.GpuMmuSupported, "calibrated compute node lacks GPU MMU support");
    // ponytail: one verified private ABI; reject other drivers until calibrated on their hardware.
    s.create_queue(s);
    D3DKMT_CREATEPAGINGQUEUE pq{}; pq.hDevice = s.device; pq.Priority = D3DDDI_PAGINGQUEUE_PRIORITY_NORMAL;
    check(D3DKMTCreatePagingQueue(&pq), "create paging queue"); s.paging = pq.hPagingQueue; s.paging_fence = pq.hSyncObject;
    s.event = CreateEventW(nullptr, FALSE, FALSE, nullptr); require(s.event != nullptr, "create fence event");
}
Runtime::~Runtime() = default;

Allocation Runtime::allocate(size_t bytes) {
    auto& s = *state_;
    require(!s.recording.closed && !s.failed && bytes && bytes <= address_mask - 65535, "invalid allocation or closed/failed recording");
    return s.create_allocation(detail::align(bytes, 65536));
}
Allocation Runtime::allocate_device(size_t bytes) {
    auto& s = *state_;
    require(!s.recording.closed && !s.failed && bytes && bytes <= s.local_bytes && bytes <= address_mask - 65535,
            "invalid native device allocation or closed/failed recording");
    return s.create_allocation(detail::align(bytes, 65536), nullptr, true);
}
Allocation Runtime::import_host(void* data, size_t bytes) {
    auto& s = *state_;
    require(!s.recording.closed && !s.failed, "host import requires an open, healthy recording");
    detail::validate_host_import(data, bytes, s.allocations);
    require(bytes <= address_mask - 65535, "host import exceeds GPU address space");
    const auto end = reinterpret_cast<uintptr_t>(data) + bytes;
    for (auto address = reinterpret_cast<uintptr_t>(data); address < end;) {
        MEMORY_BASIC_INFORMATION info{};
        require(VirtualQuery(reinterpret_cast<void*>(address), &info, sizeof(info)) == sizeof(info) &&
                info.State == MEM_COMMIT && !(info.Protect & (PAGE_GUARD | PAGE_NOACCESS | PAGE_NOCACHE | PAGE_WRITECOMBINE)) &&
                (info.Protect & (PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)),
                "host import requires committed write-back read/write pages");
        const auto base = reinterpret_cast<uintptr_t>(info.BaseAddress);
        require(info.RegionSize && info.RegionSize <= UINTPTR_MAX - base && base + info.RegionSize > address,
                "invalid host memory region");
        address = std::min(end, base + info.RegionSize);
    }
    return s.create_allocation(bytes, data);
}
Allocation Runtime::State::create_allocation(size_t bytes, void* host, bool device_local) {
    auto& s = *this;
    s.allocations.reserve(s.allocations.size() + 1); s.resources.reserve(s.resources.size() + 1);
    s.allocations.emplace_back(); s.resources.emplace_back();
    auto& a = s.allocations.back(); a.bytes = bytes; a.data = host; a.imported = host != nullptr;
    a.device_local = device_local;
    bool residency_rolled_back = false;
    try {
        a.identity = std::make_shared<const int>(0);
        if (!host && !device_local) a.data = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        require(device_local || a.data != nullptr, "allocate backing pages");
        const auto count = device_local ? UINT(1 + (bytes - 1) / detail::native_device_allocation_limit) : 1u;
        auto& backing = s.resources.back();
        if (count > 1) backing.parts.resize(count);
        std::vector<std::array<uint8_t, 2376>> native;
        std::vector<D3DDDI_ALLOCATIONINFO2> info(count);
        D3DKMT_CREATESTANDARDALLOCATION standard{};
        standard.Type = D3DKMT_STANDARDALLOCATIONTYPE_EXISTINGHEAP; standard.ExistingHeapData.Size = bytes;
        info[0].pSystemMem = a.data;
        D3DKMT_CREATEALLOCATION create{}; create.hDevice = s.device; create.NumAllocations = count;
        create.pAllocationInfo2 = info.data();
        if (device_local) {
            native.resize(count);
            for (UINT i = 0; i < count; ++i) {
                const auto part_bytes = std::min(detail::native_device_allocation_limit, bytes - size_t(i) * detail::native_device_allocation_limit);
                native[i] = detail::native_device_allocation(part_bytes);
                info[i].pPrivateDriverData = native[i].data(); info[i].PrivateDriverDataSize = unsigned(native[i].size());
            }
        } else {
            create.pStandardAllocation = &standard;
            create.Flags.StandardAllocation = 1; create.Flags.ExistingSysMem = 1; create.Flags.CreateResource = 1;
            create.Flags.CreateShared = 1; create.Flags.CrossAdapter = 1; create.Flags.NtSecuritySharing = 1;
        }
        const auto created = D3DKMTCreateAllocation2(&create);
        a.handle = info[0].hAllocation; backing.resource = create.hResource;
        for (UINT i = 0; i < backing.parts.size(); ++i) backing.parts[i] = info[i].hAllocation;
        check(created, "create GPU allocation");
        require(a.handle != 0, "allocation returned no handle");
        D3DDDI_MAKERESIDENT resident{}; resident.hPagingQueue = s.paging;
        resident.NumAllocations = count; resident.AllocationList = count == 1 ? &a.handle : backing.parts.data();
        check(D3DKMTMakeResident(&resident), "make allocation resident");
        require(s.wait_fence(s.paging_fence, resident.PagingFenceValue, 10000), "residency timeout");
        if (device_local) {
            std::vector<D3DKMT_ALLOCATIONRESIDENCYSTATUS> status(count);
            D3DKMT_QUERYALLOCATIONRESIDENCY query{}; query.hDevice = s.device;
            query.phAllocationList = resident.AllocationList; query.AllocationCount = count; query.pResidencyStatus = status.data();
            check(D3DKMTQueryAllocationResidency(&query), "query device allocation residency");
            if (!std::all_of(status.begin(), status.end(), [](auto value) {
                return value == D3DKMT_ALLOCATIONRESIDENCYSTATUS_RESIDENTINGPUMEMORY;
            })) {
                // Paging completed and no GPU VA exists yet; return all backing before a smaller retry.
                // ponytail: uncertain driver/paging/teardown failures remain terminal.
                D3DKMT_DESTROYALLOCATION2 destroy{}; destroy.hDevice = s.device;
                destroy.Flags.SynchronousDestroy = 1;
                if (backing.resource) destroy.hResource = backing.resource;
                else { destroy.phAllocationList = resident.AllocationList; destroy.AllocationCount = count; }
                check(D3DKMTDestroyAllocation2(&destroy), "rollback nonlocal device allocation");
                s.allocations.pop_back(); s.resources.pop_back();
                residency_rolled_back = true;
                throw std::runtime_error("device allocation is not resident in local GPU memory");
            }
        }
        uint64_t reserved = 0;
        if (count > 1) {
            D3DDDI_RESERVEGPUVIRTUALADDRESS reserve{};
            reserve.hPagingQueue = s.paging; reserve.MinimumAddress = 65536; reserve.Size = bytes;
            check(D3DKMTReserveGpuVirtualAddress(&reserve), "reserve contiguous GPU virtual address");
            reserved = reserve.VirtualAddress; a.gpu = detail::canonical_gpu_va(reserved);
            require(reserved >= 65536 && !(reserved & 65535) && reserved <= address_mask - bytes,
                    "invalid contiguous GPU VA reservation");
        }
        for (UINT i = 0; i < count; ++i) {
            const auto offset = size_t(i) * detail::native_device_allocation_limit;
            D3DDDI_MAPGPUVIRTUALADDRESS map{}; map.hPagingQueue = s.paging; map.hAllocation = info[i].hAllocation;
            map.SizeInPages = (count == 1 ? bytes : std::min(detail::native_device_allocation_limit, bytes - offset)) / 4096;
            map.MinimumAddress = 65536; map.Protection.Write = 1;
            if (reserved) map.BaseAddress = reserved + offset;
            check(D3DKMTMapGpuVirtualAddress(&map), "map GPU virtual address");
            if (!reserved) a.gpu = detail::canonical_gpu_va(map.VirtualAddress);
            require(map.VirtualAddress >= 65536 && !(map.VirtualAddress & 4095) &&
                    map.VirtualAddress <= address_mask - map.SizeInPages * 4096 &&
                    (!reserved || map.VirtualAddress == reserved + offset), "invalid GPU VA mapping");
            require(s.wait_fence(s.paging_fence, map.PagingFenceValue, 10000), "GPU VA mapping timeout");
        }
        detail::include_cpu_mapping(s.cpu_bounds, a);
        return a;
    } catch (...) { if (!residency_rolled_back) s.failed = true; throw; }
}
void Runtime::cpu_access(const Allocation& a, bool) {
    auto& s = *state_;
    require(!s.failed && (s.storage.count(a.handle) ? storage_idle(a) : s.idle()), "CPU access before completion or after failure");
    require(a.data, "device-local allocation has no CPU mapping; use a GPU transfer");
    (void)allocation_index(a);
    MemoryBarrier(); // WB host pages; completion follows the final GPU host-visibility flush.
}
void Runtime::release(Allocation& memory) {
    auto& s = *state_;
    const auto index = allocation_index(memory);
    require(!s.failed && s.idle() && s.recording.commands.empty() && !s.recording.closed,
            "allocation release requires idle healthy queues and an empty open recording");
    require(!s.storage.count(memory.handle), "cannot release internal pooled storage");
    auto& own = s.allocations[index];
    try {
        D3DKMT_FREEGPUVIRTUALADDRESS unmap{};
        unmap.hAdapter = s.adapter; unmap.BaseAddress = own.gpu & address_mask; unmap.Size = own.bytes;
        check(D3DKMTFreeGpuVirtualAddress(&unmap), "release GPU virtual address"); own.gpu = 0;
        D3DKMT_DESTROYALLOCATION2 destroy{}; destroy.hDevice = s.device;
        // Existing-heap pages cannot be VirtualFree'd until pin teardown completes.
        destroy.Flags.SynchronousDestroy = 1;
        auto& backing = s.resources[index];
        if (backing.resource) destroy.hResource = backing.resource;
        else if (backing.parts.empty()) { destroy.phAllocationList = &own.handle; destroy.AllocationCount = 1; }
        else { destroy.phAllocationList = backing.parts.data(); destroy.AllocationCount = UINT(backing.parts.size()); }
        check(D3DKMTDestroyAllocation2(&destroy), "release GPU allocation"); own.handle = 0; backing = {};
        if (own.data && !own.imported && !VirtualFree(own.data, 0, MEM_RELEASE))
            throw std::system_error(GetLastError(), std::system_category(), "release backing pages");
        own.data = nullptr;
        s.allocations.erase(s.allocations.begin() + index); s.resources.erase(s.resources.begin() + index);
        memory = {};
    } catch (...) { s.failed = true; throw; }
}
MemoryInfo Runtime::memory_info() {
    auto& s = *state_;
    require(!s.failed, "memory query after runtime failure");
    D3DKMT_QUERYVIDEOMEMORYINFO info{};
    info.hAdapter = s.adapter; info.MemorySegmentGroup = D3DKMT_MEMORY_SEGMENT_GROUP_LOCAL;
    check(D3DKMTQueryVideoMemoryInfo(&info), "query process video memory budget");
    require(info.Budget, "video memory budget is unavailable");
    const auto ceiling = std::min(s.local_bytes, info.Budget);
    uint64_t own = 0;
    for (const auto& a : s.allocations) if (a.device_local) own += a.bytes;
    const auto available = info.CurrentUsage < ceiling ? ceiling - info.CurrentUsage : 0;
    return {s.local_bytes, std::min(available, own < s.local_bytes ? s.local_bytes - own : 0), true, false};
}
void Runtime::storage_access(const Allocation& a) {
    require(!state_->failed && storage_idle(a), "internal storage is still in GPU use");
    MemoryBarrier();
}
#include "../drm/recording.inl"
HostGate Runtime::create_gate(bool reusable) {
    auto& s = *state_;
    require(!s.recording.closed && !s.failed, "host gate creation requires an open healthy recording");
    auto gate = std::make_shared<HostGate::State>(); gate->owner = s.identity; gate->device = s.device;
    gate->reusable = reusable;
    if (reusable) {
        gate->memory = allocate_storage(64);
        gate->word = new (gate->memory.data) std::atomic<uint32_t>(0);
        _mm_clflush(gate->word); _mm_mfence();
    } else {
    D3DKMT_CREATESYNCHRONIZATIONOBJECT2 sync{}; sync.hDevice = s.device; sync.Info.Type = D3DDDI_MONITORED_FENCE;
    check(D3DKMTCreateSynchronizationObject2(&sync), "create host gate"); gate->monitor = sync.hSyncObject;
    require(gate->monitor, "host gate returned no fence");
    }
    s.host_gates.erase(std::remove_if(s.host_gates.begin(), s.host_gates.end(), [](const auto& g) { return g.expired(); }), s.host_gates.end());
    s.host_gates.push_back(gate);
    HostGate result; result.state_ = std::move(gate); return result;
}
void Runtime::depend_on(const HostGate& gate) {
    auto& s = *state_;
    require(gate.state_ && gate.state_->owner.get() == s.identity.get(), "invalid or foreign host gate");
    auto& list = s.recording;
    require(!list.closed && !list.encoder_state.valid && !s.failed, "host gates must precede kernels in an open healthy recording");
    if (std::find(list.gates.begin(), list.gates.end(), gate.state_) != list.gates.end()) return;
    require(list.gates.size() < 16, "too many host gates");
    if (gate.state_->reusable) {
        list.gates.reserve(list.gates.size() + 1);
        use_storage(gate.state_->memory);
        detail::emit(list.commands, detail::host_gate_wait(gate.state_->memory.gpu));
    }
    list.gates.push_back(gate.state_);
}
void Runtime::close() {
    auto& s = *state_;
    require(!s.recording.closed && !s.failed &&
            (!s.recording.commands.empty() || !s.recording.gates.empty()), "empty, closed or failed recording");
    try {
        detail::emit(s.recording.commands, detail::barrier());
        detail::emit(s.recording.commands, detail::P::MI_BATCH_BUFFER_END::sInit());
        if (s.recording.commands.size() & 1) s.recording.commands.push_back(0);
        require(s.recording.commands.size() <= 1024 * 1024, "command list too large");
        // Upload new immutable parameter ranges once per slab at close, never at replay.
        // Moving the map keeps the memory-module bootstrap list free of these uploads.
        auto uploads = std::move(s.recording.parameter_uploads);
        s.recording.parameter_uploads.clear();
        for (const auto& [handle, range] : uploads) {
            const auto& pair = s.parameter_mirrors.at(handle);
            require(range[0] < range[1] && range[1] <= pair[0].bytes, "invalid parameter upload range");
            transfer_storage(pair[1].gpu + range[0], pair[0].gpu + range[0], range[1] - range[0]);
        }
        const auto bytes = s.recording.commands.size() * 4;
        if (!s.recording.batch || s.recording.batch->bytes < bytes) {
            s.recording.batch.reset();
            auto& pool = s.recording.queue == s.storage_queue ? s.transfer_batch_pool : s.batch_pool;
            s.recording.batch = pool.take([&](size_t n) { return allocate_storage(n); }, bytes);
        }
        storage_access(*s.recording.batch);
        std::memcpy(s.recording.batch->data, s.recording.commands.data(), bytes);
        use_storage(*s.recording.batch);
        s.recording.header[2] = 5; s.recording.header[8] = 4;
        const uint32_t perf_tag = 0xc000;
        std::memcpy(s.recording.header.data() + 28, &perf_tag, sizeof(perf_tag));
        const auto& q = s.queue(s.recording.queue);
        std::memcpy(s.recording.header.data() + 48, &q.monitor_gpu, sizeof(q.monitor_gpu));
        s.recording.execute.Commands = s.recording.batch->gpu; s.recording.execute.CommandLength = static_cast<UINT>(bytes);
        s.recording.execute.BroadcastContextCount = 1; s.recording.execute.BroadcastContext[0] = q.context;
        s.recording.execute.pPrivateDriverData = s.recording.header.data(); s.recording.execute.PrivateDriverDataSize = s.recording.header.size();
        s.recording.closed = true;
    } catch (...) { s.failed = true; throw; }
}
uint64_t Runtime::submit_recording(size_t index, size_t queue, const Completion* waits, size_t wait_count) {
    auto& s = *state_;
    auto& list = s.list(index);
    auto& q = s.queue(queue);
    require(list.queue == queue && list.closed && !s.failed && q.sequence != UINT64_MAX,
            "submit requires a closed completed healthy list");
    // A successful immutable observer wait may precede owner collection.
    // Reuse the cached fence observation without polling a completed point again.
    if (list.sequence > q.done && q.observer->observed.load(std::memory_order_acquire) >= list.sequence) q.done = list.sequence;
    require(list.sequence <= q.done, "submit requires a closed completed healthy list");
    require(wait_count <= 16 && (waits || !wait_count) && wait_count + list.gates.size() <= 16, "too many or invalid submission dependencies");
    std::array<D3DKMT_HANDLE, 16> objects{};
    std::array<UINT64, 16> points{};
    uint32_t count = 0;
    for (const auto& gate : list.gates) if (!gate->reusable) { objects[count] = gate->monitor; points[count++] = 1; }
    for (size_t wi = 0; wi < wait_count; ++wi) {
        const auto& wait = waits[wi];
        const auto source = completion_index(wait);
        if (source == queue) continue; // The same queue already preserves this order.
        const auto object = s.queue(source).monitor;
        uint32_t i = 0;
        while (i < count && objects[i] != object) ++i;
        if (i == count) objects[count++] = object;
        points[i] = std::max(points[i], wait.sequence_);
    }
    const auto next = q.sequence + 1;
    std::memcpy(list.header.data() + 56, &next, sizeof(next));
    // finish() may move the private header; rebind before the first frozen replay.
    list.execute.pPrivateDriverData = list.header.data();
    MemoryBarrier();
    try {
        if (count) {
            D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMGPU dependency{};
            dependency.hContext = q.context; dependency.ObjectCount = count;
            dependency.ObjectHandleArray = objects.data(); dependency.MonitoredFenceValueArray = points.data();
            check(D3DKMTWaitForSynchronizationObjectFromGpu(&dependency), "queue GPU fence dependencies");
        }
        check(D3DKMTSubmitCommand(&list.execute), "submit DG2 commands"); list.sequence = q.sequence = next;
        for (auto* storage : list.storage) storage->last_use[queue] = next;
    }
    catch (...) { s.failed = true; throw; }
    return next;
}
uint64_t Runtime::completed_queue(size_t index) {
    auto& s = *state_;
    auto& q = s.queue(index);
    require(!s.failed, "completion query after runtime failure");
    if (q.done == q.sequence) return q.done;
    const auto value = *q.monitor_cpu;
    require(value <= q.sequence, "monitored fence exceeds submitted sequence");
    MemoryBarrier(); q.done = std::max(q.done, value);
    return q.done;
}
void Runtime::completed_queues(const size_t* indices, uint64_t* values, size_t count) {
    for (size_t i = 0; i < count; ++i) values[i] = completed_queue(indices[i]);
}
void Runtime::wait_sequence(size_t index, uint64_t target, uint64_t timeout_ns) {
    auto& s = *state_;
    auto& q = s.queue(index);
    require(target && target <= q.sequence && !s.failed && timeout_ns <= uint64_t(INT64_MAX), "wait requires a submitted healthy list and finite timeout");
    using Clock = std::chrono::steady_clock;
    const auto start = Clock::now();
    if (target <= q.done || completed_queue(index) >= target) return;
    require(timeout_ns != 0, "DG2 fence timeout; list remains in flight");
    // A short spin handles already-complete work without an OS wakeup.
    for (uint32_t i = 0; i < 128; ++i) { if (completed_queue(index) >= target) return; _mm_pause(); }
    while (completed_queue(index) < target) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count();
        require(uint64_t(elapsed) < timeout_ns, "DG2 fence timeout; list remains in flight");
        const auto remaining = timeout_ns - uint64_t(elapsed);
        const auto ms = static_cast<DWORD>(std::min<uint64_t>((remaining + 999999) / 1000000, MAXDWORD - 1));
        s.wait_fence(q.monitor, target, ms);
    }
}
void Runtime::reset() {
    auto& s = *state_;
    require(s.recording.closed && s.recording.sequence <= s.queue(s.recording.queue).done && !s.failed, "reset requires a closed completed healthy list");
    s.recording.commands.clear(); s.recording.encoder_state = {}; s.recording.closed = false; s.recording.sequence = 0;
    s.recording.captures.clear();
    s.recording.gates.clear();
    s.recording.storage.clear();
    if (s.indirect_heap.allocation.use_count() == 1) s.indirect_heap = {};
}
} // namespace orca::l0::dg2
