#pragma once
// Strata numerical-source adapter, not the DPCT/SYCL host runtime.
#include <orca_queue.hpp>
#include <algorithm>
#include <cstdlib>
#include <iostream>

template<class...> class dpct_kernel_name;
template<int> class dpct_kernel_scalar;

namespace strata::l0 {
// ponytail: one submission owner. The engine adapter will own its device and
// queues; the kernel check explicitly binds its native default stream.
inline thread_local Queue* default_stream = nullptr;
inline Queue& (*default_factory)() = nullptr;
inline void bind_default(Queue& stream) { default_stream = &stream; }
inline Queue& stream_of(const void* stream) {
    auto* native = stream ? (Queue*)stream : default_stream;
    if (!native && default_factory) native = &default_factory();
    if (!native) throw std::logic_error("native default stream has not been bound");
    return *native;
}
void memory_info(size_t& free_bytes, size_t& total_bytes);
template<class T = void> T* allocate(size_t count, Memory kind, Queue& queue) {
    constexpr size_t size = [] { if constexpr (std::is_void_v<T>) return size_t(1); else return sizeof(T); }();
    if (count > SIZE_MAX / size) throw std::invalid_argument("native allocation overflow");
    return count ? static_cast<T*>(queue.runtime().allocate(count * size, kind)) : nullptr;
}
template<class T = void> T* malloc_device(size_t count, Queue& queue) { return allocate<T>(count, Memory::device, queue); }
template<class T = void> T* aligned_alloc_device(size_t alignment, size_t count, Queue& queue) {
    constexpr size_t size = [] { if constexpr (std::is_void_v<T>) return size_t(1); else return sizeof(T); }();
    if (count > SIZE_MAX / size) throw std::invalid_argument("native aligned allocation overflow");
    return count ? static_cast<T*>(queue.runtime().allocate(count * size, Memory::device, alignment)) : nullptr;
}
template<class T = void> T* malloc_host(size_t count, Queue& queue) { return allocate<T>(count, Memory::host, queue); }
template<class T = void> T* malloc_shared(size_t count, Queue& queue) { return allocate<T>(count, Memory::shared, queue); }
// Integer pointer tables bypass kernel-argument translation; serialize a GPU VA explicitly.
template<class T> T* device_pointer(T* pointer, Queue& queue) {
#ifdef STRATA_ORCA_DG2
    return queue.runtime().invoke([&] { return reinterpret_cast<T*>(queue.runtime().gpu.gpu_address(pointer)); });
#else
    (void)queue; return pointer;
#endif
}
inline void free(void* pointer, Queue& queue) {
    if (pointer) { queue.wait_and_throw(); queue.runtime().deallocate(pointer); }
}
}

namespace dpct {
using queue_ptr = strata::l0::Queue*;
using event_ptr = strata::l0::Event*;
inline auto& get_in_order_queue() { return strata::l0::stream_of(nullptr); }
inline auto& get_default_queue() { return get_in_order_queue(); }
inline auto& get_out_of_order_queue() { return get_in_order_queue(); }
inline void destroy_event(event_ptr event) { delete event; }
inline int sycl_event_query(event_ptr event) { return event && !event->ready() ? 1 : 0; }
inline void sync_barrier(event_ptr event, queue_ptr queue) {
    if (!event || !queue) throw std::invalid_argument("null native event/queue");
    *event = queue->ext_oneapi_submit_barrier();
}
struct device_info {
    std::string name;
    uint64_t memory = 0;
    uint32_t compute_units = 0;
    const char* get_name() const { return name.c_str(); }
    char* get_name() { return name.data(); }
    uint64_t get_global_mem_size() const { return memory; }
    uint32_t get_max_compute_units() const { return compute_units; }
    int get_major_version() const { return 0; }
    int get_minor_version() const { return 0; }
};
struct KernelDevice {
    // ponytail: this numerical adapter exposes the explicitly bound device;
    // the full engine's device registry is a separate integration step.
    std::vector<queue_ptr> queues;
    void queues_wait_and_throw() {
        if (!get_in_order_queue().wait()) throw std::runtime_error("native kernel stream timeout");
        for (auto* queue : queues) queue->wait_and_throw();
    }
    queue_ptr create_queue(bool = true) {
        auto& owner = get_in_order_queue();
        auto queue = std::make_unique<strata::l0::Queue>(owner.runtime(), owner.program());
        queues.push_back(queue.get());
        return queue.release();
    }
    void destroy_queue(queue_ptr queue) {
        auto position = std::find(queues.begin(), queues.end(), queue);
        if (position == queues.end()) throw std::invalid_argument("native queue is not owned by the bound device");
        queue->wait_and_throw();
        queues.erase(position);
        delete queue;
    }
    uint64_t get_local_mem_size() const { return get_in_order_queue().get_device().get_info<sycl::info::device::local_mem_size>(); }
    // Intel kernels have no CUDA compute capability. Keep NVIDIA-only tuning disabled.
    int get_major_version() const noexcept { return 0; }
    int get_minor_version() const noexcept { return 0; }
    template<class Info> auto get_info() const { return get_in_order_queue().get_device().get_info<Info>(); }
    uint32_t get_max_compute_units() const { return get_info<sycl::info::device::max_compute_units>(); }
    uint32_t get_max_clock_frequency() const { return get_info<sycl::info::device::max_clock_frequency>(); }
    void get_memory_info(size_t& free_bytes, size_t& total_bytes) const { strata::l0::memory_info(free_bytes, total_bytes); }
    void get_device_info(device_info& info) const {
        info = {get_info<sycl::info::device::name>(), get_info<sycl::info::device::global_mem_size>(), get_max_compute_units()};
    }
};
inline int get_major_version(const KernelDevice&) { return 1; }
inline KernelDevice& get_current_device() { static KernelDevice device; return device; }
inline int get_current_device_id() { (void)get_in_order_queue(); return 0; }
inline KernelDevice& get_device(int ordinal) {
    if (ordinal != 0) throw std::out_of_range("numerical adapter has one bound device");
    return get_current_device();
}
inline void select_device(int ordinal) { (void)get_device(ordinal); }
inline int device_count() { (void)get_in_order_queue(); return 1; }
inline void has_capability_or_fail(const strata::l0::DeviceView& device,
                                   std::initializer_list<sycl::aspect> aspects) {
    for (auto aspect : aspects)
        if (!device.has(aspect)) throw std::runtime_error("required native device capability unavailable");
}
inline constexpr int success = 0, default_error = 999;
inline const char* get_error_string_dummy(int status) { return status == success ? "success" : "native submission failed"; }
enum memcpy_direction { automatic, host_to_device, device_to_host, device_to_device };
inline strata::l0::Event async_dpct_memcpy(void* destination, int destination_device,
        const void* source, int source_device, size_t bytes,
        strata::l0::Queue& queue = get_in_order_queue()) {
    if (destination_device != 0 || source_device != 0)
        throw std::invalid_argument("native peer copy requires a single bound device");
    return queue.memcpy(destination, source, bytes);
}
inline strata::l0::Event async_dpct_memcpy(void* destination, size_t dst_pitch,
        const void* source, size_t src_pitch, size_t width, size_t height, memcpy_direction,
        strata::l0::Queue& queue = get_in_order_queue()) {
    return queue.memcpy_2d(destination, dst_pitch, source, src_pitch, width, height);
}
namespace experimental {
struct Node {
    sycl::ext::oneapi::experimental::node_type type;
    auto get_type() const { return type; }
};
using node_ptr = Node*;
class Graph {
public:
    explicit Graph(std::shared_ptr<strata::l0::NativeGraph> graph) : graph_(std::move(graph)) {
        for (auto type : graph_->node_types) nodes_.push_back({type});
    }
    Graph finalize() const { return *this; }
    const auto& native() const { return graph_; }
    auto& nodes() { return nodes_; }
private:
    std::shared_ptr<strata::l0::NativeGraph> graph_;
    std::vector<Node> nodes_;
};
using command_graph_ptr = Graph*;
using command_graph_exec_ptr = Graph*;
inline void begin_recording(queue_ptr queue) {
    static const bool ready_ops = [] {
        const char* value = std::getenv("STRATA_ORCA_READY_OPS");
        return value && std::atoi(value) != 0;
    }();
    queue->begin_recording(ready_ops);
}
inline void end_recording(queue_ptr queue, command_graph_ptr* graph) {
    if (!graph) throw std::invalid_argument("null native graph output");
    *graph = new Graph(queue->end_recording());
}
inline void get_nodes(command_graph_ptr graph, node_ptr* output, size_t* count) {
    if (!graph || !count) throw std::invalid_argument("null native graph/count");
    auto& nodes = graph->nodes();
    if (output) for (size_t i = 0; i < std::min(*count, nodes.size()); ++i) output[i] = &nodes[i];
    *count = nodes.size();
}
}
}

#define __dpct_inline__ inline __attribute__((always_inline))
#define __dpct_noinline__ __attribute__((noinline))
#define __dpct_align__(n) alignas(n)
#define DPCT_CHECK_ERROR(expr) [&]() { try { expr; return dpct::success; } \
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return dpct::default_error; } }()
#define DPCT_PI_F (3.14159274101257f)
#define DPCT_PI (3.141592653589793115998)

#include <dpct/atomic.hpp>
#include <dpct/util.hpp>
#include <dpct/math.hpp>

namespace dpct {
enum memory_region { constant };
template<class T, memory_region Region, int Dimensions> struct accessor {
    T* data;
    size_t columns;
    auto operator[](size_t row) const {
        if constexpr (Dimensions == 2) return data + row * columns;
        else return data[row];
    }
};
// The compiler sees a plain pointer/stride capture. The bound Runtime owns
// the backing allocation, including static tables kept until process exit.
template<class T, int Dimensions> class global_memory {
public:
    explicit global_memory(sycl::range<1> range, std::initializer_list<T> values = {})
        : count_(range[0]), columns_(1), initial_(values) {
        static_assert(Dimensions == 1);
        if (!count_ || initial_.size() > count_) throw std::invalid_argument("invalid native table size");
    }
    global_memory(size_t rows, size_t columns) : count_(rows), columns_(columns) {
        static_assert(Dimensions == 2);
        if (!rows || !columns || rows > SIZE_MAX / columns) throw std::invalid_argument("invalid native table dimensions");
        count_ *= columns;
    }
    void init(strata::l0::Queue& queue = get_in_order_queue()) {
        if (owner_ && owner_ != &queue.runtime()) throw std::logic_error("native table used in another context");
        if (data_) return;
        data_ = strata::l0::malloc_device<T>(count_, queue);
        owner_ = &queue.runtime();
        try {
            // Initialization also runs before the first captured launch. Upload
            // outside that recording, then make the table visible before use.
            if (!initial_.empty()) {
#ifdef STRATA_ORCA_DG2
                strata::l0::Stream upload(queue.runtime(), queue.program(), owner_->initial_upload_queue);
                upload.copy(data_, initial_.data(), initial_.size() * sizeof(T));
                if (!upload.wait()) throw std::runtime_error("native table upload timeout");
#else
                auto& upload = owner_->commands(true, true, true);
                upload.copy(data_, initial_.data(), initial_.size() * sizeof(T));
                if (!upload.wait()) throw std::runtime_error("native table upload timeout");
                owner_->discard(upload);
#endif
            }
        } catch (...) {
            // A timed-out upload can still reference this allocation. Runtime
            // retains both allocation and list for its final drain/cleanup.
            data_ = nullptr;
            throw;
        }
    }
    T* get_ptr() { init(); return data_; }
    auto get_access(strata::l0::Queue& queue) { init(queue); return accessor<T, constant, Dimensions>{data_, columns_}; }
private:
    size_t count_, columns_;
    std::vector<T> initial_;
    strata::l0::Runtime* owner_ = nullptr;
    T* data_ = nullptr;
};
template<class T, int Dimensions> using constant_memory = global_memory<T, Dimensions>;
}
