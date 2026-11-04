#include <orca_l0/drm.hpp>
#include "arguments.hpp"
#include <i915_drm.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <array>
#include <cerrno>
#include <limits>
#include <system_error>
#include <time.h>
#include <atomic>
#include <mutex>
#include <new>
#include <immintrin.h>

namespace orca::l0::dg2 {
namespace detail = drm::detail;
namespace {
using detail::require;
uint64_t ptr(const void* p) { return reinterpret_cast<uintptr_t>(p); }
void call(int fd, unsigned long request, void* argument, const char* label) {
    int result;
    do { result = ioctl(fd, request, argument); } while (result < 0 && errno == EINTR);
    if (result < 0) throw std::system_error(errno, std::generic_category(), label);
}
int parameter(int fd, int name) {
    int value = 0;
    drm_i915_getparam get{}; get.param = name; get.value = &value;
    call(fd, DRM_IOCTL_I915_GETPARAM, &get, "I915_GETPARAM");
    return value;
}
i915_engine_class_instance engine(int fd) {
    drm_i915_query_item item{}; item.query_id = DRM_I915_QUERY_ENGINE_INFO;
    drm_i915_query query{}; query.num_items = 1; query.items_ptr = ptr(&item);
    call(fd, DRM_IOCTL_I915_QUERY, &query, "query compute engine size");
    require(item.length >= int(sizeof(drm_i915_query_engine_info)) && item.length <= 1024 * 1024, "invalid engine query size");
    std::vector<uint64_t> storage((item.length + 7) / 8);
    item.data_ptr = ptr(storage.data());
    call(fd, DRM_IOCTL_I915_QUERY, &query, "query compute engines");
    require(item.length >= int(sizeof(drm_i915_query_engine_info)) && size_t(item.length) <= storage.size() * 8, "invalid engine query response");
    const auto* info = reinterpret_cast<const drm_i915_query_engine_info*>(storage.data());
    require(info->num_engines <= (item.length - sizeof(*info)) / sizeof(info->engines[0]), "invalid engine count");
    for (uint32_t i = 0; i < info->num_engines; ++i)
        if (info->engines[i].engine.engine_class == I915_ENGINE_CLASS_COMPUTE) return info->engines[i].engine;
    throw std::runtime_error("DG2/i915 compute engine unavailable");
}
drm_i915_memory_region_info local_region(int fd, std::vector<uint64_t>& storage) {
    drm_i915_query_item item{}; item.query_id = DRM_I915_QUERY_MEMORY_REGIONS;
    drm_i915_query query{}; query.num_items = 1; query.items_ptr = ptr(&item);
    if (storage.empty()) {
        call(fd, DRM_IOCTL_I915_QUERY, &query, "query memory region size");
        require(item.length >= int(sizeof(drm_i915_query_memory_regions)) && item.length <= 1024 * 1024, "invalid memory region query size");
        storage.resize((item.length + 7) / 8);
    }
    item.length = int(storage.size() * 8); item.data_ptr = ptr(storage.data());
    call(fd, DRM_IOCTL_I915_QUERY, &query, "query memory regions");
    require(item.length >= int(sizeof(drm_i915_query_memory_regions)) && size_t(item.length) <= storage.size() * 8, "invalid memory region response");
    const auto* info = reinterpret_cast<const drm_i915_query_memory_regions*>(storage.data());
    require(info->num_regions <= (item.length - sizeof(*info)) / sizeof(info->regions[0]), "invalid memory region count");
    drm_i915_memory_region_info result{}; bool found = false;
    for (uint32_t i = 0; i < info->num_regions; ++i) if (info->regions[i].region.memory_class == I915_MEMORY_CLASS_DEVICE) {
        require(!found && info->regions[i].probed_size, "ambiguous or empty local memory region");
        found = true; result = info->regions[i];
    }
    require(found, "i915 device-local memory unavailable");
    return result;
}
} // namespace

struct NativeCompletion {
    std::atomic<bool> closing{false};
    std::atomic<uint64_t> observed{0};
    int fd = -1;
    uint32_t timeline = 0;
    void cancel() noexcept { closing.store(true, std::memory_order_release); }
    ~NativeCompletion() {
        if (fd >= 0) {
            if (timeline) { drm_syncobj_destroy d{}; d.handle = timeline; ioctl(fd, DRM_IOCTL_SYNCOBJ_DESTROY, &d); }
            ::close(fd);
        }
    }
    void remember(uint64_t value) {
        auto prior = observed.load(std::memory_order_relaxed);
        while (prior < value && !observed.compare_exchange_weak(prior, value, std::memory_order_release, std::memory_order_relaxed)) {}
    }
    bool ready(uint64_t target) {
        require(!closing.load(std::memory_order_acquire), "completion belongs to a closing or destroyed runtime");
        if (observed.load(std::memory_order_acquire) >= target) return true;
        require(!closing.load(std::memory_order_acquire) && fd >= 0, "completion belongs to a closing or destroyed runtime");
        uint64_t value = 0;
        drm_syncobj_timeline_array query{}; query.handles = ptr(&timeline); query.points = ptr(&value); query.count_handles = 1;
        call(fd, DRM_IOCTL_SYNCOBJ_QUERY, &query, "query completion observer");
        remember(value);
        return value >= target;
    }
    bool wait(uint64_t target, uint64_t timeout_ns) {
        require(!closing.load(std::memory_order_acquire), "completion belongs to a closing or destroyed runtime");
        if (observed.load(std::memory_order_acquire) >= target) return true;
        require(!closing.load(std::memory_order_acquire) && fd >= 0, "completion belongs to a closing or destroyed runtime");
        const auto monotonic = [] {
            timespec value{};
            if (clock_gettime(CLOCK_MONOTONIC, &value)) throw std::system_error(errno, std::generic_category(), "completion observer clock");
            return int64_t(value.tv_sec) * 1000000000ll + value.tv_nsec;
        };
        auto now = monotonic();
        const auto deadline = now + int64_t(std::min(timeout_ns, uint64_t(INT64_MAX - now)));
        while (!closing.load(std::memory_order_acquire)) {
            drm_syncobj_timeline_wait wait{}; wait.handles = ptr(&timeline); wait.points = ptr(&target); wait.count_handles = 1;
            wait.timeout_nsec = now + std::min<int64_t>(50000000, deadline - now);
            try { call(fd, DRM_IOCTL_SYNCOBJ_TIMELINE_WAIT, &wait, "wait completion observer"); remember(target); return true; }
            catch (const std::system_error& error) {
                if (error.code().value() != ETIME && error.code().value() != ETIMEDOUT) throw;
            }
            now = monotonic();
            if (now >= deadline) return false;
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
    static_assert(sizeof(std::atomic<uint32_t>) == 4 && std::atomic<uint32_t>::is_always_lock_free);
    std::shared_ptr<const int> owner;
    std::mutex mutex;
    std::atomic<uint32_t>* word = nullptr;
    bool reusable = false;
    Allocation memory;
    uint64_t gpu = 0;
    void signal() {
        std::lock_guard<std::mutex> lock(mutex);
        require(word, "host gate belongs to a destroyed runtime");
        word->store(1, std::memory_order_release);
        // GEM_SET_DOMAIN would wait for the very GPU request this word releases.
        // Flush only this dedicated WB cache line while that request is pending.
        _mm_clflush(word); _mm_mfence();
    }
    void close() noexcept { std::lock_guard<std::mutex> lock(mutex); word = nullptr; }
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
    uint32_t context = 0, timeline = 0;
    uint64_t sequence = 0, done = 0;
    std::array<size_t, 2> current_scratch{SIZE_MAX, SIZE_MAX};
    size_t current_private = SIZE_MAX;
};
struct Runtime::State : NativeQueue {
    int fd = -1;
    uint32_t max_threads = 0, vm = 0;
    i915_engine_class_instance compute_engine{};
    std::deque<NativeQueue> queues;
    // ponytail: one reusable page per live gate; pack cache-line slots if callbacks make this measurable.
    std::vector<std::pair<Allocation, std::weak_ptr<HostGate::State>>> host_gates;
    NativeQueue& queue(size_t index) { return index ? queues.at(index - 1) : *this; }
    const NativeQueue& queue(size_t index) const { return index ? queues.at(index - 1) : *this; }
    bool idle() const {
        // Immutable completion observers remember successful waits. Owner-side
        // command/resource collection may intentionally follow at the next submit.
        const auto complete = [](const NativeQueue& q) { return q.done == q.sequence || q.observer->ready(q.sequence); };
        return complete(*this) && std::all_of(queues.begin(), queues.end(), complete);
    }
    uint32_t mocs = 2;
    bool host_visibility = true;
    bool disable_overdispatch = false;
    bool local_scratch_cache = false;
    uint64_t next_va = 0x1000000;
    std::vector<Allocation> allocations;
    std::array<uint64_t, 2> cpu_bounds{UINT64_MAX, 0};
    std::map<uint32_t, detail::RecordingStorage> storage;
    std::map<uint64_t, detail::RecordingStorage*> storage_by_gpu;
    detail::AllocationPool batch_pool, parameter_pool, transfer_batch_pool, transfer_parameter_pool;
    struct List {
        std::vector<uint32_t> commands;
        detail::DispatchState encoder_state;
        std::vector<drm_i915_gem_exec_object2> objects;
        drm_i915_gem_execbuffer2 execute{};
        std::shared_ptr<const Allocation> batch;
        std::vector<std::shared_ptr<const Allocation>> captures;
        std::map<uint32_t, std::array<size_t, 2>> parameter_uploads;
        std::vector<std::shared_ptr<HostGate::State>> gates;
        std::vector<detail::RecordingStorage*> storage;
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
    std::map<std::pair<std::string, std::string>, detail::RecordedImage> images;
    std::map<std::string, detail::ModuleData> module_data;
    std::string memory_module;
    bool memory_copy_2d = false, memory_copy_coaligned = false;
    size_t storage_queue = SIZE_MAX;
    bool local_thread_memory = false, local_isa = false;
    Allocation thread_readback, isa_staging;
    std::vector<detail::ThreadMemory> thread_memory;
    std::map<std::array<size_t, 2>, Allocation> scratch_descriptors;
    drm_i915_gem_memory_class_instance device_region{};
    std::vector<uint64_t> memory_regions;
    bool device_region_known = false;
    bool failed = false;
    void create_queue(NativeQueue& q) {
        q.observer = std::make_shared<NativeCompletion>();
        drm_i915_gem_context_create create{};
        call(fd, DRM_IOCTL_I915_GEM_CONTEXT_CREATE, &create, "GEM_CONTEXT_CREATE"); q.context = create.ctx_id;
        drm_i915_gem_context_param parameter{};
        parameter.ctx_id = q.context; parameter.param = I915_CONTEXT_PARAM_VM; parameter.value = vm;
        call(fd, DRM_IOCTL_I915_GEM_CONTEXT_SETPARAM, &parameter, "context shared VM");
        struct { uint64_t extensions; i915_engine_class_instance engines[1]; } map{};
        map.engines[0] = compute_engine;
        parameter.param = I915_CONTEXT_PARAM_ENGINES; parameter.size = 8 + sizeof(i915_engine_class_instance); parameter.value = ptr(&map);
        call(fd, DRM_IOCTL_I915_GEM_CONTEXT_SETPARAM, &parameter, "context compute engine map");
        drm_syncobj_create timeline{};
        call(fd, DRM_IOCTL_SYNCOBJ_CREATE, &timeline, "create completion timeline"); q.timeline = timeline.handle;
        // dup retains this DRM open-file description and its syncobj namespace.
        // Closing Runtime's fd cannot invalidate an in-progress observer wait.
        q.observer->timeline = q.timeline;
        do { q.observer->fd = fcntl(fd, F_DUPFD_CLOEXEC, 0); } while (q.observer->fd < 0 && errno == EINTR);
        if (q.observer->fd < 0) throw std::system_error(errno, std::generic_category(), "duplicate completion observer fd");
    }
    ~State() {
        if (fd < 0) return;
        for (size_t i = 0; i <= queues.size(); ++i) if (queue(i).observer) queue(i).observer->cancel();
        for (const auto& slot : host_gates) if (const auto gate = slot.second.lock()) try { gate->signal(); } catch (...) {}
        timespec now{};
        const bool have_clock = clock_gettime(CLOCK_MONOTONIC, &now) == 0;
        const auto deadline = int64_t(now.tv_sec) * 1000000000ll + now.tv_nsec + 10000000000ll;
        for (size_t i = 0; i <= queues.size(); ++i) {
            auto& q = queue(i);
            if (have_clock && q.timeline && q.done != q.sequence) {
                drm_syncobj_timeline_wait wait{}; wait.handles = ptr(&q.timeline); wait.points = ptr(&q.sequence);
                wait.count_handles = 1; wait.timeout_nsec = deadline;
                try { call(fd, DRM_IOCTL_SYNCOBJ_TIMELINE_WAIT, &wait, "drain queue during teardown"); } catch (...) {}
            }
            if (q.context) { drm_i915_gem_context_destroy d{}; d.ctx_id = q.context; ioctl(fd, DRM_IOCTL_I915_GEM_CONTEXT_DESTROY, &d); }
            if (q.timeline && (!q.observer || q.observer->fd < 0)) { drm_syncobj_destroy d{}; d.handle = q.timeline; ioctl(fd, DRM_IOCTL_SYNCOBJ_DESTROY, &d); }
        }
        if (vm) { drm_i915_gem_vm_control d{}; d.vm_id = vm; ioctl(fd, DRM_IOCTL_I915_GEM_VM_DESTROY, &d); }
        for (const auto& slot : host_gates) if (const auto gate = slot.second.lock()) gate->close();
        for (const auto& a : allocations) {
            if (a.data && !a.imported) munmap(a.data, a.bytes);
            drm_gem_close close{}; close.handle = a.handle; ioctl(fd, DRM_IOCTL_GEM_CLOSE, &close);
        }
        ::close(fd);
    }
};
Runtime::Runtime(const char* render_node) : state_(std::make_unique<State>()) {
    auto& s = *state_;
    s.fd = open(render_node ? render_node : "/dev/dri/renderD128", O_RDWR | O_CLOEXEC);
    if (s.fd < 0) throw std::runtime_error(std::string("open render node: ") + std::strerror(errno));
    char name[16]{}; drm_version version{};
    version.name = name; version.name_len = sizeof(name);
    call(s.fd, DRM_IOCTL_VERSION, &version, "DRM driver name");
    require(version.name_len == 4 && !std::memcmp(name, "i915", 4), "only i915 is supported; xe needs a different transport");
    const auto device = parameter(s.fd, I915_PARAM_CHIPSET_ID);
    require(device >= 0x5690 && device <= 0x56bf, "only DG2 devices are supported");
    require(parameter(s.fd, I915_PARAM_HAS_EXEC_SOFTPIN), "i915 softpin is required");
    require(parameter(s.fd, I915_PARAM_HAS_EXEC_TIMELINE_FENCES), "i915 timeline submission fences are required");
    require(parameter(s.fd, I915_PARAM_HAS_EXEC_ASYNC), "i915 explicit object synchronization is required");
    const auto eus = parameter(s.fd, I915_PARAM_EU_TOTAL);
    require(eus > 0 && eus <= 512, "unexpected DG2 EU count");
    s.max_threads = uint32_t(eus) * 8;
    s.compute_engine = engine(s.fd);
    drm_i915_gem_vm_control vm{};
    call(s.fd, DRM_IOCTL_I915_GEM_VM_CREATE, &vm, "GEM_VM_CREATE"); s.vm = vm.vm_id;
    require(s.vm, "shared VM returned no handle");
    s.create_queue(s);
}
Runtime::~Runtime() = default;
Allocation Runtime::allocate(size_t bytes) {
    auto& s = *state_;
    require(!s.recording.closed && !s.failed && bytes && bytes <= (uint64_t{1} << 47) - 4096, "invalid allocation or recording already closed");
    bytes = detail::align(bytes, 4096);
    require(bytes < (uint64_t{1} << 47) - s.next_va - 4096, "GPU VA exhausted");
    auto identity = std::make_shared<const int>(0);
    drm_i915_gem_create create{}; create.size = bytes;
    // ponytail: system-memory BOs only; local-memory placement is a separate measured optimization.
    call(s.fd, DRM_IOCTL_I915_GEM_CREATE, &create, "GEM_CREATE");
    Allocation a{}; a.handle = create.handle; a.bytes = bytes; a.gpu = s.next_va; a.identity = std::move(identity);
    try {
        drm_i915_gem_mmap_offset offset{}; offset.handle = a.handle; offset.flags = I915_MMAP_OFFSET_WB;
        call(s.fd, DRM_IOCTL_I915_GEM_MMAP_OFFSET, &offset, "GEM_MMAP_OFFSET");
        a.data = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, s.fd, offset.offset);
        if (a.data == MAP_FAILED) { a.data = nullptr; throw std::runtime_error(std::string("mmap: ") + std::strerror(errno)); }
        // This fresh BO has never been submitted; initialize its CPU domain without draining other queues.
        drm_i915_gem_set_domain domain{}; domain.handle = a.handle;
        domain.read_domains = domain.write_domain = I915_GEM_DOMAIN_CPU;
        call(s.fd, DRM_IOCTL_I915_GEM_SET_DOMAIN, &domain, "GEM_SET_DOMAIN(new host allocation)");
        s.allocations.push_back(a);
        detail::include_cpu_mapping(s.cpu_bounds, a);
    } catch (...) {
        if (a.data) munmap(a.data, a.bytes);
        drm_gem_close close{}; close.handle = a.handle; ioctl(s.fd, DRM_IOCTL_GEM_CLOSE, &close);
        throw;
    }
    s.next_va += bytes + 4096; // Unmapped guard page between BOs.
    return a;
}
Allocation Runtime::allocate_device(size_t bytes) {
    auto& s = *state_;
    require(!s.recording.closed && !s.failed && bytes && bytes <= (uint64_t{1} << 47) - 2 * 1024 * 1024,
            "invalid device allocation or closed/failed recording");
    bytes = detail::align(bytes, 65536);
    const auto gpu = detail::align(s.next_va, 2 * 1024 * 1024);
    const auto span = detail::align(bytes, 2 * 1024 * 1024);
    require(gpu < (uint64_t{1} << 47) - span - 2 * 1024 * 1024, "GPU VA exhausted");
    if (!s.device_region_known) { s.device_region = local_region(s.fd, s.memory_regions).region; s.device_region_known = true; }
    s.allocations.reserve(s.allocations.size() + 1);
    auto identity = std::make_shared<const int>(0);
    drm_i915_gem_create_ext_memory_regions regions{};
    regions.base.name = I915_GEM_CREATE_EXT_MEMORY_REGIONS;
    regions.num_regions = 1; regions.regions = ptr(&s.device_region);
    drm_i915_gem_create_ext create{}; create.size = bytes; create.extensions = ptr(&regions);
    call(s.fd, DRM_IOCTL_I915_GEM_CREATE_EXT, &create, "GEM_CREATE_EXT(device-local)");
    try {
        require(create.size == bytes && create.handle, "unexpected local allocation size/handle");
        const Allocation a{nullptr, gpu, bytes, create.handle, false, true, std::move(identity)};
        s.allocations.push_back(a);
        // DG2 forbids mixing 4K/64K PTEs in one 2 MiB PDE; retain the entire padded VA span.
        s.next_va = gpu + span + 2 * 1024 * 1024;
        return a;
    } catch (...) {
        drm_gem_close close{}; close.handle = create.handle; ioctl(s.fd, DRM_IOCTL_GEM_CLOSE, &close);
        throw;
    }
}
Allocation Runtime::import_host(void* data, size_t bytes) {
    auto& s = *state_;
    require(!s.recording.closed && !s.failed, "host import requires an open, healthy recording");
    detail::validate_host_import(data, bytes, s.allocations);
    require(bytes < (uint64_t{1} << 47) - s.next_va - 4096, "GPU VA exhausted");
    s.allocations.reserve(s.allocations.size() + 1);
    auto identity = std::make_shared<const int>(0);
    drm_i915_gem_userptr user{}; user.user_ptr = ptr(data); user.user_size = bytes;
    call(s.fd, DRM_IOCTL_I915_GEM_USERPTR, &user, "GEM_USERPTR");
    const Allocation a{data, s.next_va, bytes, user.handle, true, false, std::move(identity)};
    s.allocations.push_back(a);
    detail::include_cpu_mapping(s.cpu_bounds, a);
    s.next_va += bytes + 4096;
    return a;
}
void Runtime::cpu_access(const Allocation& a, bool write) {
    auto& s = *state_;
    require(!s.failed && (s.storage.count(a.handle) ? storage_idle(a) : s.idle()), "CPU access before GPU completion or after failure");
    require(a.data, "device-local allocation has no CPU mapping; use a GPU transfer");
    (void)allocation_index(a);
    drm_i915_gem_set_domain d{}; d.handle = a.handle;
    d.read_domains = I915_GEM_DOMAIN_CPU; d.write_domain = write ? I915_GEM_DOMAIN_CPU : 0;
    call(s.fd, DRM_IOCTL_I915_GEM_SET_DOMAIN, &d, "GEM_SET_DOMAIN(CPU)");
}
void Runtime::release(Allocation& memory) {
    auto& s = *state_;
    const auto index = allocation_index(memory);
    require(!s.failed && s.idle() && s.recording.commands.empty() && !s.recording.closed,
            "allocation release requires idle healthy queues and an empty open recording");
    require(!s.storage.count(memory.handle), "cannot release internal pooled storage");
    auto& own = s.allocations[index];
    try {
        drm_gem_close close{}; close.handle = own.handle;
        call(s.fd, DRM_IOCTL_GEM_CLOSE, &close, "release GEM allocation"); own.handle = 0;
        if (own.data && !own.imported) require(munmap(own.data, own.bytes) == 0, "release CPU mapping");
        own.data = nullptr;
        // Frozen residency is conservative: remove this BO from unrelated lists.
        // A list that actually uses this allocation must have been discarded first.
        for (auto& list : s.recordings) {
            auto& objects = list.objects;
            objects.erase(std::remove_if(objects.begin(), objects.end(), [&](const auto& object) {
                return object.handle == memory.handle;
            }), objects.end());
            list.execute.buffer_count = uint32_t(objects.size());
        }
        s.allocations.erase(s.allocations.begin() + index); memory = {};
    } catch (...) { s.failed = true; throw; }
}
void Runtime::storage_access(const Allocation& a) {
    require(!state_->failed && storage_idle(a), "internal storage is still in GPU use");
    drm_i915_gem_set_domain d{}; d.handle = a.handle;
    d.read_domains = d.write_domain = I915_GEM_DOMAIN_CPU;
    call(state_->fd, DRM_IOCTL_I915_GEM_SET_DOMAIN, &d, "GEM_SET_DOMAIN(recording storage)");
}
MemoryInfo Runtime::memory_info() {
    auto& s = *state_;
    require(!s.failed, "memory query after runtime failure");
    const auto info = local_region(s.fd, s.memory_regions);
    if (!s.device_region_known) { s.device_region = info.region; s.device_region_known = true; }
    uint64_t own = 0;
    for (const auto& a : s.allocations) if (a.device_local) own += a.bytes;
    const auto remaining = own < info.probed_size ? info.probed_size - own : 0;
    return {info.probed_size, std::min(info.unallocated_size, remaining), false, geteuid() == 0};
}
#include "recording.inl"
HostGate Runtime::create_gate(bool reusable) {
    auto& s = *state_;
    require(!s.recording.closed && !s.failed, "host gate creation requires an open healthy recording");
    auto gate = std::make_shared<HostGate::State>(); gate->owner = s.identity;
    gate->reusable = reusable;
    auto slot = std::find_if(s.host_gates.begin(), s.host_gates.end(), [](const auto& slot) { return slot.second.expired(); });
    if (slot == s.host_gates.end()) {
        const auto memory = allocate_storage(4096);
        s.host_gates.emplace_back(memory, std::weak_ptr<HostGate::State>{}); slot = std::prev(s.host_gates.end());
    }
    storage_access(slot->first);
    gate->memory = slot->first;
    gate->word = new (slot->first.data) std::atomic<uint32_t>(0); gate->gpu = slot->first.gpu;
    _mm_clflush(gate->word); _mm_mfence();
    slot->second = gate;
    HostGate result; result.state_ = std::move(gate); return result;
}
void Runtime::depend_on(const HostGate& gate) {
    auto& s = *state_;
    require(gate.state_ && gate.state_->owner.get() == s.identity.get(), "invalid or foreign host gate");
    auto& list = s.recording;
    require(!list.closed && !list.encoder_state.valid && !s.failed, "host gates must precede kernels in an open healthy recording");
    if (std::find(list.gates.begin(), list.gates.end(), gate.state_) != list.gates.end()) return;
    require(list.gates.size() < 16, "too many host gates");
    const auto wait = detail::host_gate_wait(gate.state_->gpu);
    // Reserve first: allocation failure must not leave an unretained GPU wait.
    list.gates.reserve(list.gates.size() + 1);
    use_storage(gate.state_->memory);
    detail::emit(list.commands, wait); list.gates.push_back(gate.state_);
}
void Runtime::close() {
    auto& s = *state_;
    require(!s.recording.closed && !s.failed && !s.recording.commands.empty(), "empty, closed or failed recording");
    try {
        s.recording.commands.push_back(0x05000000); // MI_BATCH_BUFFER_END
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
        storage_access(*s.recording.batch); std::memcpy(s.recording.batch->data, s.recording.commands.data(), bytes);
        use_storage(*s.recording.batch);
        const auto resident = [&](const Allocation& a) {
            drm_i915_gem_exec_object2 object{};
            object.handle = a.handle; object.offset = a.gpu;
            object.flags = EXEC_OBJECT_PINNED | EXEC_OBJECT_SUPPORTS_48B_ADDRESS | EXEC_OBJECT_WRITE | EXEC_OBJECT_ASYNC;
            s.recording.objects.push_back(object);
        };
        // Keep user allocations conservative; unused internal BOs must not acquire unrelated busy fences.
        for (const auto& a : s.allocations) if (!s.storage.count(a.handle)) resident(a);
        for (const auto* storage : s.recording.storage)
            if (storage->allocation.handle != s.recording.batch->handle) resident(storage->allocation);
        // i915 executes the last object. A recycled batch is not necessarily the newest BO.
        resident(*s.recording.batch);
        s.recording.execute.buffers_ptr = ptr(s.recording.objects.data()); s.recording.execute.buffer_count = uint32_t(s.recording.objects.size());
        s.recording.execute.batch_len = uint32_t(s.recording.commands.size() * 4);
        s.recording.execute.flags = I915_EXEC_NO_RELOC | I915_EXEC_USE_EXTENSIONS;
        s.recording.execute.rsvd1 = s.queue(s.recording.queue).context;
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
    std::array<drm_i915_gem_exec_fence, 17> objects{};
    std::array<uint64_t, 17> points{};
    uint32_t count = 1;
    objects[0].handle = q.timeline; objects[0].flags = I915_EXEC_FENCE_SIGNAL; points[0] = q.sequence + 1;
    for (size_t wi = 0; wi < wait_count; ++wi) {
        const auto& wait = waits[wi];
        const auto source = completion_index(wait);
        if (source == queue) continue;
        const auto object = s.queue(source).timeline;
        uint32_t i = 1;
        while (i < count && objects[i].handle != object) ++i;
        if (i == count) { objects[count].handle = object; objects[count++].flags = I915_EXEC_FENCE_WAIT; }
        points[i] = std::max(points[i], wait.sequence_);
    }
    drm_i915_gem_execbuffer_ext_timeline_fences fences{};
    fences.base.name = DRM_I915_GEM_EXECBUFFER_EXT_TIMELINE_FENCES; fences.fence_count = count;
    fences.handles_ptr = ptr(objects.data()); fences.values_ptr = ptr(points.data());
    auto execute = list.execute; execute.cliprects_ptr = ptr(&fences);
    // Fixed arrays and persistent BOs: no allocation, lock or per-kernel ioctl on replay.
    try {
        call(s.fd, DRM_IOCTL_I915_GEM_EXECBUFFER2, &execute, "GEM_EXECBUFFER2(compute)"); list.sequence = q.sequence = points[0];
        for (auto* storage : list.storage) storage->last_use[queue] = q.sequence;
    }
    catch (...) { s.failed = true; throw; }
    return q.sequence;
}
void Runtime::wait_sequence(size_t index, uint64_t target, uint64_t timeout_ns) {
    auto& s = *state_;
    auto& q = s.queue(index);
    require(target && target <= q.sequence && !s.failed && timeout_ns <= uint64_t(INT64_MAX), "wait requires a submitted healthy list and finite timeout");
    if (target <= q.done) return;
    timespec now{};
    if (clock_gettime(CLOCK_MONOTONIC, &now)) throw std::system_error(errno, std::generic_category(), "completion clock");
    const uint64_t ns = uint64_t(now.tv_sec) * 1000000000ull + uint64_t(now.tv_nsec);
    drm_syncobj_timeline_wait wait{}; wait.handles = ptr(&q.timeline); wait.points = ptr(&target); wait.count_handles = 1;
    wait.timeout_nsec = int64_t(ns + std::min(timeout_ns, uint64_t(INT64_MAX) - ns));
    try { call(s.fd, DRM_IOCTL_SYNCOBJ_TIMELINE_WAIT, &wait, "SYNCOBJ_TIMELINE_WAIT(compute)"); q.done = std::max(q.done, target); }
    catch (const std::system_error& error) {
        if (error.code().value() != ETIME && error.code().value() != ETIMEDOUT) s.failed = true;
        throw;
    }
}
uint64_t Runtime::completed_queue(size_t index) {
    auto& s = *state_;
    auto& q = s.queue(index);
    require(!s.failed, "completion query after runtime failure");
    if (q.done == q.sequence) return q.done;
    uint64_t value = 0;
    drm_syncobj_timeline_array query{}; query.handles = ptr(&q.timeline); query.points = ptr(&value); query.count_handles = 1;
    try {
        call(s.fd, DRM_IOCTL_SYNCOBJ_QUERY, &query, "SYNCOBJ_QUERY(compute)");
        require(value <= q.sequence, "completion timeline exceeds submitted sequence");
        q.done = std::max(q.done, value);
    } catch (...) { s.failed = true; throw; }
    return q.done;
}
void Runtime::completed_queues(const size_t* indices, uint64_t* values, size_t count) {
    auto& s = *state_;
    std::array<uint32_t, 16> handles{};
    std::array<uint64_t, 16> points{};
    std::array<size_t, 16> pending{};
    size_t n = 0;
    for (size_t i = 0; i < count; ++i) {
        const auto& q = s.queue(indices[i]);
        values[i] = q.done;
        if (q.done == q.sequence) continue;
        handles[n] = q.timeline; pending[n++] = i;
    }
    if (!n) return;
    drm_syncobj_timeline_array query{};
    query.handles = ptr(handles.data()); query.points = ptr(points.data()); query.count_handles = uint32_t(n);
    try {
        call(s.fd, DRM_IOCTL_SYNCOBJ_QUERY, &query, "SYNCOBJ_QUERY(completion batch)");
        for (size_t i = 0; i < n; ++i)
            require(points[i] <= s.queue(indices[pending[i]]).sequence, "completion timeline exceeds submitted sequence");
        for (size_t i = 0; i < n; ++i) {
            auto& q = s.queue(indices[pending[i]]);
            values[pending[i]] = q.done = std::max(q.done, points[i]);
        }
    } catch (...) { s.failed = true; throw; }
}
void Runtime::reset() {
    auto& s = *state_;
    require(s.recording.closed && s.recording.sequence <= s.queue(s.recording.queue).done && !s.failed, "reset requires a closed completed healthy list");
    s.recording.commands.clear(); s.recording.objects.clear(); s.recording.execute = {};
    s.recording.encoder_state = {}; s.recording.closed = false; s.recording.sequence = 0;
    s.recording.captures.clear();
    s.recording.gates.clear();
    s.recording.storage.clear();
    if (s.indirect_heap.allocation.use_count() == 1) s.indirect_heap = {};
}
} // namespace orca::l0::dg2
