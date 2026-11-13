#include <orca_l0/runtime.hpp>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <mutex>
#include <sstream>
#ifdef ORCA_L0_DIRECT_DRIVER
#include <orca_l0/direct_driver.hpp>
#endif

namespace orca::l0 {
namespace {
// ponytail: one bookkeeping lock; use per-runtime locks if concurrent setup makes it contended.
std::mutex allocation_mutex;
std::string message(ze_result_t result, const std::string& operation) {
    std::ostringstream out;
    out << operation << " failed: Level Zero 0x" << std::hex << static_cast<uint32_t>(result);
    return out.str();
}
bool completed(ze_result_t result, const char* operation) {
    if (result == ZE_RESULT_NOT_READY) return false;
    check(result, operation);
    return true;
}
ze_event_handle_t* wait_data(std::initializer_list<ze_event_handle_t> waits) {
    // The C API does not modify the input handles despite its non-const signature.
    return waits.size() ? const_cast<ze_event_handle_t*>(waits.begin()) : nullptr;
}
bool power_of_two(std::size_t value) { return value && !(value & (value - 1)); }
ze_command_list_handle_t* replay_data(std::size_t count, const ze_command_list_handle_t* lists) {
    if (!count || count > UINT32_MAX || !lists) throw std::invalid_argument("invalid replay batch");
    for (std::size_t i = 0; i < count; ++i)
        if (!lists[i]) throw std::invalid_argument("null replay list");
    return const_cast<ze_command_list_handle_t*>(lists);
}
} // namespace

Error::Error(ze_result_t result, const std::string& operation)
    : std::runtime_error(message(result, operation)), result(result) {}
void check(ze_result_t result, const char* operation) {
    if (result != ZE_RESULT_SUCCESS) throw Error(result, operation);
}

void replay(ze_command_queue_handle_t queue, ze_command_list_handle_t recorded) {
    replay(queue, 1, &recorded);
}
void replay(ze_command_list_handle_t immediate, ze_command_list_handle_t recorded) {
    replay(immediate, 1, &recorded);
}
void replay(ze_command_queue_handle_t queue, std::size_t count, const ze_command_list_handle_t* recorded) {
    if (!queue) throw std::invalid_argument("null replay queue");
    auto* lists = replay_data(count, recorded);
    check(zeCommandQueueExecuteCommandLists(queue, static_cast<uint32_t>(count), lists, nullptr),
          "zeCommandQueueExecuteCommandLists(borrowed batch)");
}
void replay(ze_command_list_handle_t immediate, std::size_t count, const ze_command_list_handle_t* recorded) {
    if (!immediate) throw std::invalid_argument("null replay immediate list");
    auto* lists = replay_data(count, recorded);
    // ponytail: this older entry point also supports the Level Zero versions used by oneAPI 2025.
    check(zeCommandListImmediateAppendCommandListsExp(immediate, static_cast<uint32_t>(count), lists, nullptr, 0, nullptr),
          "zeCommandListImmediateAppendCommandListsExp(borrowed batch)");
}
bool wait(ze_command_queue_handle_t queue, uint64_t timeout_ns) {
    if (!queue) throw std::invalid_argument("null queue");
    return completed(zeCommandQueueSynchronize(queue, timeout_ns), "zeCommandQueueSynchronize(borrowed)");
}
bool wait(ze_command_list_handle_t immediate, uint64_t timeout_ns) {
    if (!immediate) throw std::invalid_argument("null immediate list");
    return completed(zeCommandListHostSynchronize(immediate, timeout_ns), "zeCommandListHostSynchronize(borrowed)");
}

Runtime::Runtime(uint32_t device_index) {
    try {
        const auto initialized = zeInit(ZE_INIT_FLAG_GPU_ONLY);
#ifdef ORCA_L0_DIRECT_DRIVER
        if (initialized != ZE_RESULT_SUCCESS)
            throw Error(initialized, std::string("zeInit(direct driver): ") + direct_driver_error());
#else
        check(initialized, "zeInit");
#endif
        uint32_t count = 0;
        check(zeDriverGet(&count, nullptr), "zeDriverGet(count)");
        std::vector<ze_driver_handle_t> drivers(count);
        if (count) check(zeDriverGet(&count, drivers.data()), "zeDriverGet");
        ze_driver_handle_t selected = nullptr;
        uint32_t index = 0;
        for (auto driver : drivers) {
            uint32_t n = 0;
            check(zeDeviceGet(driver, &n, nullptr), "zeDeviceGet(count)");
            std::vector<ze_device_handle_t> devices(n);
            if (n) check(zeDeviceGet(driver, &n, devices.data()), "zeDeviceGet");
            for (auto device : devices) {
                ze_device_properties_t props{};
                props.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
                check(zeDeviceGetProperties(device, &props), "zeDeviceGetProperties");
                if (props.type != ZE_DEVICE_TYPE_GPU || props.vendorId != 0x8086) continue;
                if (index++ == device_index) {
                    selected = driver;
                    device_ = device;
                    properties_ = props;
                }
            }
        }
        if (!selected) throw std::out_of_range("Intel Level Zero GPU index not found");
        driver_ = selected;
        driver_properties_.stype = ZE_STRUCTURE_TYPE_DRIVER_PROPERTIES;
        check(zeDriverGetProperties(driver_, &driver_properties_), "driver properties");
        // Driver capabilities stay fixed for this Runtime; allocations and
        // adapter views share the snapshot rather than enumerate them again.
        count = 0;
        check(zeDriverGetExtensionProperties(driver_, &count, nullptr), "driver extension count");
        extensions_.resize(count);
        if (count) check(zeDriverGetExtensionProperties(driver_, &count, extensions_.data()), "driver extensions");
        extensions_.resize(count);
        uint32_t n = 0;
        check(zeDeviceGetCommandQueueGroupProperties(device_, &n, nullptr), "queue groups(count)");
        std::vector<ze_command_queue_group_properties_t> groups(n);
        for (auto& group : groups) group.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_GROUP_PROPERTIES;
        if (n) check(zeDeviceGetCommandQueueGroupProperties(device_, &n, groups.data()), "queue groups");
        bool found = false;
        for (uint32_t i = 0; i < n; ++i) {
            if (groups[i].numQueues && (groups[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE)) {
                compute_ordinal_ = i;
                found = true;
                break;
            }
        }
        if (!found) throw std::runtime_error("GPU has no compute queue");
        copy_ordinal_ = compute_ordinal_;
        for (uint32_t i = 0; i < n; ++i) {
            if (groups[i].numQueues && (groups[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY) &&
                !(groups[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE)) {
                copy_ordinal_ = i;
                break;
            }
        }
        compute_fill_limit_ = groups[compute_ordinal_].maxMemoryFillPatternSize;
        copy_fill_limit_ = groups[copy_ordinal_].maxMemoryFillPatternSize;
        ze_context_desc_t context{};
        context.stype = ZE_STRUCTURE_TYPE_CONTEXT_DESC;
        check(zeContextCreate(selected, &context, &context_), "zeContextCreate");
        ze_command_queue_desc_t queue{};
        queue.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
        queue.ordinal = compute_ordinal_;
        queue.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
        queue.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;
        check(zeCommandQueueCreate(context_, device_, &queue, &queue_), "zeCommandQueueCreate");
    } catch (...) {
        release();
        throw;
    }
}
Runtime::~Runtime() { release(); }

void Runtime::discard(Commands& commands) {
    const auto it = std::find_if(lists_.begin(), lists_.end(),
        [&](const auto& owned) { return owned.get() == &commands; });
    if (it == lists_.end()) throw std::invalid_argument("command list is not owned by this runtime");
    if (commands.in_flight_ || (commands.immediate_ && !commands.ready()))
        throw std::logic_error("cannot discard an in-flight command list");
    // ponytail: setup/graph-cache operation, linear in live lists; index handles
    // only if measurements show graph churn makes this scan significant.
    lists_.erase(it);
}

void Runtime::release() noexcept {
    // Drain every engine before releasing anything referenced by submitted commands.
    // The owner must first signal any host-gated events and stop its submission thread.
    for (auto& list : lists_) {
        if (list->immediate_) zeCommandListHostSynchronize(list->list_, UINT64_MAX);
    }
    if (queue_) zeCommandQueueSynchronize(queue_, UINT64_MAX);
    lists_.clear();
    for (auto kernel : kernels_) zeKernelDestroy(kernel);
    for (auto module : modules_) zeModuleDestroy(module);
    for (auto event : events_) zeEventDestroy(event);
    for (auto pool : pools_) zeEventPoolDestroy(pool);
    for (auto ptr : allocations_) zeMemFree(context_, ptr);
    if (queue_) zeCommandQueueDestroy(queue_);
    if (context_) zeContextDestroy(context_);
}

void* Runtime::allocate(std::size_t bytes, Memory kind, std::size_t alignment) {
    if (!bytes || !power_of_two(alignment)) throw std::invalid_argument("invalid allocation size/alignment");
    ze_device_mem_alloc_desc_t device{};
    device.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    ze_host_mem_alloc_desc_t host{};
    host.stype = ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC;
    ze_relaxed_allocation_limits_exp_desc_t relaxed{};
    relaxed.stype = ZE_STRUCTURE_TYPE_RELAXED_ALLOCATION_LIMITS_EXP_DESC;
    relaxed.flags = ZE_RELAXED_ALLOCATION_LIMITS_EXP_FLAG_MAX_SIZE;
#ifdef ZE_RELAXED_ALLOCATION_LIMITS_EXT_NAME
    ze_relaxed_allocation_limits_ext_desc_t relaxed_ext{};
    relaxed_ext.stype = ZE_STRUCTURE_TYPE_RELAXED_ALLOCATION_LIMITS_EXT_DESC;
    relaxed_ext.flags = ZE_RELAXED_ALLOCATION_LIMITS_EXT_FLAG_MAX_SIZE;
#endif
    if (bytes > properties_.maxMemAllocSize) {
        // Model arenas can exceed the conservative per-allocation limit. Ask
        // for the advertised extension; the driver still enforces real capacity.
        const void* descriptor = nullptr;
        for (const auto& extension : extensions_) {
            if (std::string(extension.name) == ZE_RELAXED_ALLOCATION_LIMITS_EXP_NAME)
                descriptor = &relaxed;
        }
#ifdef ZE_RELAXED_ALLOCATION_LIMITS_EXT_NAME
        for (const auto& extension : extensions_)
            if (std::string(extension.name) == ZE_RELAXED_ALLOCATION_LIMITS_EXT_NAME)
                descriptor = &relaxed_ext;
#endif
        if (!descriptor) throw std::runtime_error("allocation exceeds device limit and relaxed allocation extension is unavailable");
        if (kind == Memory::host) host.pNext = descriptor;
        else device.pNext = descriptor;
    }
    void* ptr = nullptr;
    switch (kind) {
    case Memory::device:
        check(zeMemAllocDevice(context_, &device, bytes, alignment, device_, &ptr), "zeMemAllocDevice");
        break;
    case Memory::host:
        check(zeMemAllocHost(context_, &host, bytes, alignment, &ptr), "zeMemAllocHost");
        break;
    case Memory::shared:
        check(zeMemAllocShared(context_, &device, &host, bytes, alignment, device_, &ptr), "zeMemAllocShared");
        break;
    default: throw std::invalid_argument("invalid memory kind");
    }
    try {
        // Keep device arenas resident before submitting kernels or DMA. Under
        // memory pressure allocation alone need not establish GPU residency.
        if (kind == Memory::device)
            check(zeContextMakeMemoryResident(context_, device_, ptr, bytes),
                  "zeContextMakeMemoryResident(device)");
        std::lock_guard guard(allocation_mutex);
        allocations_.push_back(ptr);
    }
    catch (...) { zeMemFree(context_, ptr); throw; }
    static const bool poison = [] {
        const char* value = std::getenv("ORCA_L0_POISON_ALLOCATIONS");
        const bool enabled = value && std::strcmp(value, "1") == 0;
        if (enabled) std::fputs("orca-l0: allocation poisoning enabled (diagnostic, timings invalid)\n", stderr);
        return enabled;
    }();
    if (poison) {
        // All-one bytes are NaNs in FP16/BF16/FP32/FP64, including odd-sized
        // allocation tails. Keep ownership if a fill fails: Runtime drains it.
        if (kind == Memory::device) {
            auto& initialize = commands(true, false, true);
            const uint8_t pattern = 0xff;
            initialize.fill(ptr, &pattern, sizeof(pattern), bytes);
            if (!initialize.wait()) throw std::runtime_error("allocation poison fill timeout");
            discard(initialize);
        } else std::memset(ptr, 0xff, bytes);
    }
    return ptr;
}

void Runtime::deallocate(void* pointer) {
    if (!pointer) return;
    std::lock_guard guard(allocation_mutex);
    // ponytail: allocation bookkeeping is linear outside submission; use a map if model setup makes it costly.
    const auto it = std::find(allocations_.begin(), allocations_.end(), pointer);
    if (it == allocations_.end()) throw std::invalid_argument("allocation is not owned by this runtime");
    check(zeMemFree(context_, pointer), "zeMemFree");
    allocations_.erase(it);
}

std::string Runtime::binary_identity() const {
    std::ostringstream out;
    out << "orca-native-v2 " << std::hex << std::setfill('0');
    for (auto byte : properties_.uuid.id) out << std::setw(2) << unsigned(byte);
    // Intel's Windows driver UUID includes process initialization time. It is
    // unsuitable for persistent binaries; use the reported driver build instead.
    out << ' ' << properties_.vendorId << ' ' << properties_.deviceId << ' '
        << driver_properties_.driverVersion << ' ' << sizeof(void*);
#ifdef _WIN32
    out << " windows";
#else
    out << " linux";
#endif
    return out.str();
}

std::vector<uint8_t> Runtime::compile_native(const std::filesystem::path& file, const char* flags) {
    auto compiled = module(file, ZE_MODULE_FORMAT_IL_SPIRV, flags);
    const auto release_module = [&] {
        auto result = zeModuleDestroy(compiled);
        if (result == ZE_RESULT_SUCCESS) modules_.pop_back();
        return result;
    };
    std::vector<uint8_t> binary;
    try {
        std::size_t size = 0;
        check(zeModuleGetNativeBinary(compiled, &size, nullptr), "native binary size");
        if (!size) throw std::runtime_error("driver returned an empty native binary");
        binary.resize(size);
        check(zeModuleGetNativeBinary(compiled, &size, binary.data()), "native binary bytes");
        if (!size || size > binary.size()) throw std::runtime_error("invalid native binary size");
        binary.resize(size);
    } catch (...) {
        (void)release_module();
        throw;
    }
    check(release_module(), "destroy temporary native compiler module");
    return binary;
}

ze_module_handle_t Runtime::module(const std::filesystem::path& file, ze_module_format_t format,
                                  const char* build_flags) {
    if (!build_flags) throw std::invalid_argument("null module build flags");
    std::ifstream input(file, std::ios::binary | std::ios::ate);
    if (!input) throw std::runtime_error("cannot open module: " + file.string());
    const auto length = input.tellg();
    if (length <= 0 || static_cast<uintmax_t>(length) > std::numeric_limits<std::size_t>::max() ||
        static_cast<uintmax_t>(length) > static_cast<uintmax_t>(std::numeric_limits<std::streamsize>::max()))
        throw std::runtime_error("invalid module size: " + file.string());
    std::vector<uint8_t> bytes(static_cast<std::size_t>(length));
    input.seekg(0);
    if (!input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
        throw std::runtime_error("cannot read module: " + file.string());
    ze_module_desc_t desc{};
    desc.stype = ZE_STRUCTURE_TYPE_MODULE_DESC;
    desc.format = format;
    desc.inputSize = bytes.size();
    desc.pInputModule = bytes.data();
    // allocate() permits arenas above 4 GiB. Match UR's relaxed-allocation
    // mode: stateful buffer addressing cannot cover those arenas correctly.
    const auto options = std::string(build_flags) + (format == ZE_MODULE_FORMAT_IL_SPIRV
        ? " -ze-opt-greater-than-4GB-buffer-required" : "");
    desc.pBuildFlags = options.c_str();
    ze_module_handle_t module = nullptr;
    ze_module_build_log_handle_t log = nullptr;
    auto result = zeModuleCreate(context_, device_, &desc, &module, &log);
    std::string details;
    if (log) {
        std::size_t size = 0;
        // Retain the compiler's diagnostic, but never replace the original build failure with a log error.
        if (result != ZE_RESULT_SUCCESS && zeModuleBuildLogGetString(log, &size, nullptr) == ZE_RESULT_SUCCESS && size) {
            try {
                std::vector<char> text(size, '\0');
                if (zeModuleBuildLogGetString(log, &size, text.data()) == ZE_RESULT_SUCCESS)
                    details.assign(text.data(), text.size());
            } catch (...) {
                zeModuleBuildLogDestroy(log);
                if (module) zeModuleDestroy(module);
                throw;
            }
        }
        zeModuleBuildLogDestroy(log);
    }
    if (result != ZE_RESULT_SUCCESS) {
        if (module) zeModuleDestroy(module);
        throw Error(result, "zeModuleCreate " + file.string() + " " + details);
    }
    try { modules_.push_back(module); }
    catch (...) { zeModuleDestroy(module); throw; }
    // Optional setup diagnostic, never part of replay or per-dispatch work.
    static const char* trace = std::getenv("ORCA_L0_MODULE_TRACE");
    if (trace && *trace) {
        std::ofstream output(trace, std::ios::app);
        if (!output || !(output << file.generic_string() << '\t' << build_flags << '\n').flush())
            throw std::runtime_error("cannot write native module trace");
    }
    return module;
}

ze_kernel_handle_t Runtime::kernel(ze_module_handle_t module, const char* name) {
    if (!module || !name || !*name) throw std::invalid_argument("invalid module/kernel name");
    ze_kernel_desc_t desc{};
    desc.stype = ZE_STRUCTURE_TYPE_KERNEL_DESC;
    desc.pKernelName = name;
    ze_kernel_handle_t kernel = nullptr;
    check(zeKernelCreate(module, &desc, &kernel), "zeKernelCreate");
    try { kernels_.push_back(kernel); }
    catch (...) { zeKernelDestroy(kernel); throw; }
    static const bool report_resources = [] {
        const char* value = std::getenv("ORCA_L0_KERNEL_RESOURCES");
        return value && std::string(value) == "1";
    }();
    if (report_resources) {
        ze_kernel_properties_t resources{};
        resources.stype = ZE_STRUCTURE_TYPE_KERNEL_PROPERTIES;
        check(zeKernelGetProperties(kernel, &resources), "kernel resource properties");
        std::fprintf(stderr, "ORCA_KERNEL_RESOURCES subgroup=%u slm_bytes=%u private_bytes=%u spill_bytes=%u name=%s\n",
                     resources.requiredSubgroupSize, resources.localMemSize,
                     resources.privateMemSize, resources.spillMemSize, name);
    }
    return kernel;
}

ze_event_handle_t Runtime::event() {
    // ponytail: one pool per event at setup; pool in batches if initialization cost becomes measurable.
    ze_event_pool_desc_t pool_desc{};
    pool_desc.stype = ZE_STRUCTURE_TYPE_EVENT_POOL_DESC;
    pool_desc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    pool_desc.count = 1;
    ze_event_pool_handle_t pool = nullptr;
    check(zeEventPoolCreate(context_, &pool_desc, 1, &device_, &pool), "zeEventPoolCreate");
    try { pools_.push_back(pool); }
    catch (...) { zeEventPoolDestroy(pool); throw; }
    ze_event_desc_t desc{};
    desc.stype = ZE_STRUCTURE_TYPE_EVENT_DESC;
    desc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    desc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
    ze_event_handle_t event = nullptr;
    check(zeEventCreate(pool, &desc, &event), "zeEventCreate");
    try { events_.push_back(event); }
    catch (...) { zeEventDestroy(event); throw; }
    return event;
}

Commands& Runtime::commands(bool immediate, bool copy_engine, bool in_order) {
    auto list = std::unique_ptr<Commands>(new Commands(*this, immediate, copy_engine,
        in_order ? Ordering::in_order : Ordering::explicit_dependencies));
    lists_.push_back(std::move(list));
    return *lists_.back();
}
Commands& Runtime::commands(Ordering order) {
    auto list = std::unique_ptr<Commands>(new Commands(*this, false, false, order));
    lists_.push_back(std::move(list));
    return *lists_.back();
}

Commands::Commands(Runtime& runtime, bool immediate, bool copy_engine, Ordering order)
    : runtime_(runtime), immediate_(immediate) {
    fill_limit_ = immediate && copy_engine ? runtime.copy_fill_limit_ : runtime.compute_fill_limit_;
    if (copy_engine && !immediate) throw std::invalid_argument("copy engine requires an immediate list");
    if (immediate) {
        ze_command_queue_desc_t desc{};
        desc.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
        desc.ordinal = copy_engine ? runtime.copy_ordinal_ : runtime.compute_ordinal_;
        if (order == Ordering::in_order) desc.flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER;
        desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
        desc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;
        check(zeCommandListCreateImmediate(runtime.context_, runtime.device_, &desc, &list_),
              "zeCommandListCreateImmediate");
    } else {
        ze_command_list_desc_t desc{};
        desc.stype = ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC;
        desc.commandQueueGroupOrdinal = runtime.compute_ordinal_;
        if (order == Ordering::in_order) desc.flags = ZE_COMMAND_LIST_FLAG_IN_ORDER;
        else if (order == Ordering::relaxed) desc.flags = ZE_COMMAND_LIST_FLAG_RELAXED_ORDERING;
        check(zeCommandListCreate(runtime.context_, runtime.device_, &desc, &list_), "zeCommandListCreate");
        ze_fence_desc_t fence{};
        fence.stype = ZE_STRUCTURE_TYPE_FENCE_DESC;
        auto result = zeFenceCreate(runtime.queue_, &fence, &fence_);
        if (result != ZE_RESULT_SUCCESS) { zeCommandListDestroy(list_); check(result, "zeFenceCreate"); }
    }
}
Commands::~Commands() {
    // Runtime drains all lists before destroying them.
    if (fence_) zeFenceDestroy(fence_);
    if (list_) zeCommandListDestroy(list_);
}
void Commands::recording() const {
    if (closed_ || in_flight_) throw std::logic_error("command list is not recording");
}
void Commands::copy(void* dst, const void* src, std::size_t bytes, ze_event_handle_t signal,
                    std::initializer_list<ze_event_handle_t> waits) {
    recording();
    if (!dst || !src || !bytes) throw std::invalid_argument("invalid copy pointers/size");
    check(zeCommandListAppendMemoryCopy(list_, dst, src, bytes, signal,
          static_cast<uint32_t>(waits.size()), wait_data(waits)), "zeCommandListAppendMemoryCopy");
}
void Commands::copy_2d(void* dst, std::size_t dst_pitch, const void* src, std::size_t src_pitch,
                       std::size_t width, std::size_t height) {
    recording();
    if (!dst || !src || !width || !height || width > dst_pitch || width > src_pitch ||
        dst_pitch > UINT32_MAX || src_pitch > UINT32_MAX || height > UINT32_MAX ||
        height - 1 > (SIZE_MAX - width) / dst_pitch || height - 1 > (SIZE_MAX - width) / src_pitch)
        throw std::invalid_argument("invalid native 2D copy pointers/shape/pitch");
    const ze_copy_region_t region{0, 0, 0, static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};
    check(zeCommandListAppendMemoryCopyRegion(list_, dst, &region, static_cast<uint32_t>(dst_pitch), 0,
          src, &region, static_cast<uint32_t>(src_pitch), 0, nullptr, 0, nullptr),
          "zeCommandListAppendMemoryCopyRegion");
}
void Commands::fill(void* dst, const void* pattern, std::size_t pattern_bytes, std::size_t bytes) {
    recording();
    if (!dst || !pattern || !bytes || !power_of_two(pattern_bytes) ||
        pattern_bytes > fill_limit_ || bytes % pattern_bytes)
        throw std::invalid_argument("invalid native memory fill");
    check(zeCommandListAppendMemoryFill(list_, dst, pattern, pattern_bytes, bytes, nullptr, 0, nullptr),
          "zeCommandListAppendMemoryFill");
}
void Commands::barrier(ze_event_handle_t signal, std::initializer_list<ze_event_handle_t> waits) {
    recording();
    check(zeCommandListAppendBarrier(list_, signal, static_cast<uint32_t>(waits.size()), wait_data(waits)),
          "zeCommandListAppendBarrier");
}
void Commands::reset_event(ze_event_handle_t event) {
    recording();
    if (!event) throw std::invalid_argument("null event reset");
    check(zeCommandListAppendEventReset(list_, event), "zeCommandListAppendEventReset");
}
void Commands::launch(ze_kernel_handle_t kernel, ze_group_count_t groups, ze_group_count_t local,
                      ze_event_handle_t signal, std::initializer_list<ze_event_handle_t> waits) {
    recording();
    if (!kernel || !groups.groupCountX || !groups.groupCountY || !groups.groupCountZ ||
        !local.groupCountX || !local.groupCountY || !local.groupCountZ)
        throw std::invalid_argument("invalid kernel/launch dimensions");
    check(zeKernelSetGroupSize(kernel, local.groupCountX, local.groupCountY, local.groupCountZ),
          "zeKernelSetGroupSize");
    check(zeCommandListAppendLaunchKernel(list_, kernel, &groups, signal,
          static_cast<uint32_t>(waits.size()), wait_data(waits)), "zeCommandListAppendLaunchKernel");
}
void Commands::close() {
    if (immediate_) throw std::logic_error("cannot close an immediate list");
    recording();
    check(zeCommandListClose(list_), "zeCommandListClose");
    closed_ = true;
}
void Commands::submit() {
    if (immediate_ || !closed_ || in_flight_) throw std::logic_error("command list is not ready for submission");
    check(zeFenceReset(fence_), "zeFenceReset");
    check(zeCommandQueueExecuteCommandLists(runtime_.queue_, 1, &list_, fence_),
          "zeCommandQueueExecuteCommandLists");
    in_flight_ = true;
}
bool Commands::wait(uint64_t timeout_ns) {
    if (immediate_) return completed(zeCommandListHostSynchronize(list_, timeout_ns), "immediate wait");
    if (!in_flight_) return true;
    if (!completed(zeFenceHostSynchronize(fence_, timeout_ns), "zeFenceHostSynchronize")) return false;
    in_flight_ = false;
    return true;
}
bool Commands::ready() {
    if (immediate_) return wait(0);
    if (!in_flight_) return true;
    if (!completed(zeFenceQueryStatus(fence_), "zeFenceQueryStatus")) return false;
    in_flight_ = false;
    return true;
}
void Commands::reset() {
    if (immediate_ || in_flight_) throw std::logic_error("cannot reset an immediate or in-flight list");
    check(zeCommandListReset(list_), "zeCommandListReset");
    closed_ = false;
}

Arena::Arena(Runtime& runtime, std::size_t bytes)
    : base_(static_cast<std::byte*>(runtime.allocate(bytes))), size_(bytes) {}
void* Arena::take(std::size_t bytes, std::size_t alignment) {
    if (!bytes || !power_of_two(alignment)) throw std::invalid_argument("invalid arena size/alignment");
    const auto address = reinterpret_cast<std::uintptr_t>(base_) + used_;
    const auto padding = (std::uintptr_t{0} - address) & (alignment - 1);
    if (padding > size_ - used_ || bytes > size_ - used_ - padding) throw std::bad_alloc();
    auto* ptr = base_ + used_ + padding;
    used_ += padding + bytes;
    return ptr;
}

} // namespace orca::l0
