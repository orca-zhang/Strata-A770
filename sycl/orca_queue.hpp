#pragma once
// Native engine queue: eager kernels, graph replay and copy completion share
// one Level Zero stream. SYCL headers are used only by the device compiler.
#ifdef STRATA_ORCA_DG2
#include "orca_dg2_backend.hpp"
#else
#include <orca_l0/compiler/stream.hpp>
#include "orca_device.hpp"
#endif
#include <orca_l0/ring.hpp>
#include <atomic>
#include <functional>
#include <thread>
#include <deque>
#include <bit>

namespace strata::l0 {
#ifdef STRATA_ORCA_DG2
using Runtime = dg2_backend::Runtime;
using Program = dg2_backend::Program;
using Stream = dg2_backend::Stream;
using Commands = dg2_backend::Commands;
using DeviceView = dg2_backend::DeviceView;
using EventHandle = dg2_backend::EventHandle;
using Ordering = dg2_backend::Ordering;
using Memory = dg2_backend::Memory;
using dg2_backend::signal_event;
using dg2_backend::event_ready;
using dg2_backend::event_wait;
#else
using Runtime = orca::l0::Runtime;
using Program = orca::l0::compiler::Program;
using Stream = orca::l0::compiler::Stream;
using Commands = orca::l0::Commands;
using EventHandle = ze_event_handle_t;
using Ordering = orca::l0::Ordering;
using Memory = orca::l0::Memory;
inline void signal_event(EventHandle event) { orca::l0::check(zeEventHostSignal(event), "native callback completion"); }
inline bool event_ready(EventHandle event) {
    const auto result = zeEventQueryStatus(event);
    if (result == ZE_RESULT_NOT_READY) return false;
    orca::l0::check(result, "native event query"); return true;
}
inline bool event_wait(EventHandle event, uint64_t ns) {
    const auto result = zeEventHostSynchronize(event, ns);
    if (result == ZE_RESULT_NOT_READY) return false;
    orca::l0::check(result, "native event wait"); return true;
}
#endif
class Queue;
template<class Name> class RangeKernel;

#ifdef STRATA_ORCA_DG2
class EventPool {
public:
    explicit EventPool(Runtime& runtime) {
        runtime.invoke([&] {
        const auto memory = runtime.staging(events_.size() * 64);
        runtime.gpu.cpu_access(*memory, true);
        for (size_t i = 0; i < events_.size(); ++i) {
            auto& event = events_[i]; event.runtime = &runtime;
            event.tag_memory = memory; event.tag_address = memory->gpu + i * 64;
            event.tag = new (static_cast<std::byte*>(memory->data) + i * 64) std::atomic<uint64_t>(0);
            _mm_clflush(event.tag);
        }
        _mm_mfence();
        });
    }
    uint32_t take(bool reset = true) {
        auto live = live_.load(std::memory_order_relaxed);
        while (live != UINT64_MAX) {
            const auto slot = uint32_t(std::countr_zero(~live));
            if (!live_.compare_exchange_weak(live, live | (uint64_t{1} << slot),
                    std::memory_order_acquire, std::memory_order_relaxed)) continue;
            try { if (reset) dg2_backend::reset_event(&events_[slot]); }
            catch (...) { give(slot); throw; }
            return slot;
        }
        return UINT32_MAX;
    }
    void give(uint32_t slot) noexcept { live_.fetch_and(~(uint64_t{1} << slot), std::memory_order_release); }
    EventHandle handle(uint32_t slot) noexcept { return &events_[slot]; }
private:
    std::array<dg2_backend::Event, 64> events_;
    std::atomic<uint64_t> live_{0};
};
#else
class EventPool {
public:
    static_assert(std::atomic<uint64_t>::is_always_lock_free);
    explicit EventPool(orca::l0::Runtime& runtime) {
        ze_event_pool_desc_t p{};
        p.stype = ZE_STRUCTURE_TYPE_EVENT_POOL_DESC;
        p.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
        p.count = 64;
        auto device = runtime.native_device();
        orca::l0::check(zeEventPoolCreate(runtime.native_context(), &p, 1, &device, &pool_), "native event pool");
        try {
            for (uint32_t i = 0; i < handles_.size(); ++i) {
                ze_event_desc_t e{};
                e.stype = ZE_STRUCTURE_TYPE_EVENT_DESC;
                e.index = i;
                e.signal = ZE_EVENT_SCOPE_FLAG_HOST | ZE_EVENT_SCOPE_FLAG_DEVICE;
                e.wait = ZE_EVENT_SCOPE_FLAG_HOST | ZE_EVENT_SCOPE_FLAG_DEVICE;
                orca::l0::check(zeEventCreate(pool_, &e, &handles_[i]), "native event");
            }
        } catch (...) { release(); throw; }
    }
    ~EventPool() { release(); }
    EventPool(const EventPool&) = delete;
    uint32_t take() {
        auto live = live_.load(std::memory_order_relaxed);
        while (live != UINT64_MAX) {
            const auto slot = uint32_t(std::countr_zero(~live));
            if (!live_.compare_exchange_weak(live, live | (uint64_t{1} << slot),
                    std::memory_order_acquire, std::memory_order_relaxed)) continue;
            const auto result = zeEventHostReset(handles_[slot]);
            if (result != ZE_RESULT_SUCCESS) give(slot);
            orca::l0::check(result, "native event reuse");
            return slot;
        }
        return UINT32_MAX;
    }
    void give(uint32_t slot) noexcept { live_.fetch_and(~(uint64_t{1} << slot), std::memory_order_release); }
    ze_event_handle_t handle(uint32_t slot) const noexcept { return handles_[slot]; }
private:
    void release() noexcept {
        for (auto event : handles_) if (event) zeEventDestroy(event);
        if (pool_) zeEventPoolDestroy(pool_);
    }
    ze_event_pool_handle_t pool_ = nullptr;
    std::array<ze_event_handle_t, 64> handles_{};
    std::atomic<uint64_t> live_{0};
};
#endif

struct NativeEvent {
    // The last reference returns a slot only after all submitting/waiting queues
    // retire their references. Slots can be returned by the callback consumer.
    std::shared_ptr<EventPool> storage;
    uint32_t slot = UINT32_MAX;
    EventHandle handle = nullptr;
    explicit NativeEvent(Runtime& runtime) {
        storage = std::make_shared<EventPool>(runtime);
        slot = storage->take();
        handle = storage->handle(slot);
    }
    NativeEvent(std::shared_ptr<EventPool> pool, uint32_t index) noexcept
        : storage(std::move(pool)), slot(index), handle(storage->handle(index)) {}
    explicit NativeEvent(EventHandle borrowed) : handle(borrowed) {
        if (!handle) throw std::invalid_argument("null borrowed event");
    }
    ~NativeEvent() { if (storage) storage->give(slot); }
    NativeEvent(const NativeEvent&) = delete;
};

class Event {
    struct State {
        const std::thread::id issuer = std::this_thread::get_id();
        std::atomic<Queue*> pending{nullptr};
        Queue* owner = nullptr;
        std::weak_ptr<int> lifetime;
        uint64_t completed_through = 0;
        std::shared_ptr<NativeEvent> native;
#ifdef STRATA_ORCA_DG2
        std::shared_ptr<dg2_backend::Point> completion;
        bool coarse_completion = false;
#endif
    };
    friend class Queue;
    friend struct NativeGraph;
    explicit Event(std::shared_ptr<State> state) : state_(std::move(state)) {}
    std::shared_ptr<State> state_;
    void retire() const;
public:
    Event() = default;
    static Event borrow(EventHandle handle) {
        auto state = std::make_shared<State>();
        state->native = std::make_shared<NativeEvent>(handle);
        return Event(std::move(state));
    }
    EventHandle native() const;
    bool ready() const {
        auto handle = native();
        if (!handle) return true;
#ifdef STRATA_ORCA_DG2
        if (state_->coarse_completion) {
            if (state_->completion && !state_->completion->ready()) return false;
            retire(); return true;
        }
#endif
        if (!event_ready(handle)) return false;
        retire();
        return true;
    }
    bool wait(uint64_t timeout_ns = 30000000000ULL) const {
#ifdef STRATA_ORCA_DG2
        if (timeout_ns > uint64_t(INT64_MAX)) throw std::invalid_argument("DG2 event timeout exceeds INT64_MAX");
#endif
        auto handle = native();
        if (!handle) return true;
#ifdef STRATA_ORCA_DG2
        if (state_->coarse_completion) {
            if (state_->completion && !state_->completion->wait(timeout_ns)) return false;
            retire(); return true;
        }
#endif
        if (!event_wait(handle, timeout_ns)) return false;
        retire();
        return true;
    }
    void wait_and_throw() const { if (!wait()) throw std::runtime_error("native event timeout"); }
    static void wait(const std::vector<Event>& events) { for (const auto& e : events) e.wait_and_throw(); }
    template<class Info> auto get_info() const {
        static_assert(std::is_same_v<Info, sycl::info::event::command_execution_status>);
        return ready() ? sycl::info::event_command_status::complete : sycl::info::event_command_status::running;
    }
    template<class Info> uint64_t get_profiling_info() const {
        // Completion markers do not measure kernel duration. Never substitute
        // host wall time for a GPU profiling timestamp.
        throw std::runtime_error("native completion events do not expose GPU profiling timestamps");
    }
};

struct NativeGraph {
    NativeGraph(Runtime& owner, Commands& list,
                std::vector<std::shared_ptr<Event::State>> markers,
                std::vector<sycl::ext::oneapi::experimental::node_type> types)
        : commands(&list), events(std::move(markers)), nodes(types.size()),
          node_types(std::move(types)), runtime(owner) {}
    ~NativeGraph() { try { runtime.discard(*commands); } catch (...) { /* Runtime retains failed cleanup until teardown. */ } }
    NativeGraph(const NativeGraph&) = delete;
    NativeGraph& operator=(const NativeGraph&) = delete;
    Commands* commands;
    std::vector<std::shared_ptr<Event::State>> events;
    size_t nodes;
    std::vector<sycl::ext::oneapi::experimental::node_type> node_types;
private:
    Runtime& runtime;
};

class Queue {
    friend class Event;
    struct HostTask {
        Event ready;
        std::shared_ptr<NativeEvent> done;
        std::function<void()> body;
    };
public:
    Queue(Runtime& runtime, Program& program, bool copy_engine = false)
        : runtime_(runtime), program_(program), device_(runtime), stream_(runtime, program, copy_engine) {}
    ~Queue() {
        try { wait_and_throw(); } catch (...) {}
        stopping_.store(true, std::memory_order_release);
        wake_.fetch_add(1, std::memory_order_release);
        wake_.notify_one();
        if (worker_.joinable()) worker_.join();
    }
    Queue(const Queue&) = delete;
    Queue& operator=(const Queue&) = delete;

    template<class Name = void, int Dimensions, class... Args>
    Event parallel_for(sycl::nd_range<Dimensions> range, Args&&... args) {
        before();
        stream_.parallel_for<Name>(range, std::forward<Args>(args)...);
        record_node(sycl::ext::oneapi::experimental::node_type::kernel);
        return marker();
    }
    template<class Name = void, class F>
    Event parallel_for(sycl::range<1> range, F function) {
        const size_t count = range[0];
        if (!count) return marker();
        if (count > SIZE_MAX - 255) throw std::invalid_argument("native range overflow");
        using KernelName = std::conditional_t<std::is_void_v<Name>, void, RangeKernel<Name>>;
        return parallel_for<KernelName>(sycl::nd_range<1>((count + 255) / 256 * 256, 256),
            [=](sycl::nd_item<1> item) {
                const size_t index = item.get_global_id(0);
                if (index < count) function(sycl::id<1>(index));
            });
    }
    template<class F> Event submit(F&& body) {
        std::forward<F>(body)(*this);
        return marker();
    }
    template<class Name = void, class F, class... Properties>
    Event parallel_for(sycl::range<1> range,
                       sycl::ext::oneapi::experimental::detail::properties_t<Properties...> properties, F function) {
        if (!range[0]) return marker();
        before();
        stream_.parallel_for<Name>(range, properties, function);
        record_node(sycl::ext::oneapi::experimental::node_type::kernel);
        return marker();
    }
    Event memcpy(void* destination, const void* source, size_t bytes) {
        before();
        if (bytes) { stream_.copy(destination, source, bytes); record_node(sycl::ext::oneapi::experimental::node_type::memcpy); }
        return marker();
    }
    void write_global_timestamp(uint64_t* destination) {
        before();
        // DG2's ordered PIPE_CONTROL timestamp already stalls and flushes prior work.
        // Level Zero's raw timestamp still needs the explicit preceding barrier.
#ifndef STRATA_ORCA_DG2
        stream_.barrier();
#endif
        stream_.write_global_timestamp(destination);
        record_node(sycl::ext::oneapi::experimental::node_type::ext_oneapi_barrier);
        needs_barrier_ = true; // Timestamp writes preserve ordering on relaxed recorded lists too.
    }
    template<class T> Event fill(T* destination, const T& value, size_t count) {
        if (count && !destination) throw std::invalid_argument("null native fill destination");
        if (!count) return marker();
        if (count > SIZE_MAX / sizeof(T)) throw std::invalid_argument("native fill size overflow");
        if constexpr (std::is_trivially_copyable_v<T> && !(sizeof(T) & (sizeof(T) - 1))) {
            if (sizeof(T) <= stream_.max_fill_pattern()) {
                before();
                stream_.fill(destination, &value, sizeof(T), count * sizeof(T));
                record_node(sycl::ext::oneapi::experimental::node_type::memset);
                return marker();
            }
        }
        return parallel_for(sycl::range<1>(count), [=](sycl::id<1> i) { destination[i] = value; });
    }
    Event memcpy_2d(void* destination, size_t dst_pitch, const void* source, size_t src_pitch,
                    size_t width, size_t height) {
        before();
        if (width && height) {
            stream_.copy_2d(destination, dst_pitch, source, src_pitch, width, height);
            record_node(sycl::ext::oneapi::experimental::node_type::memcpy);
        }
        return marker();
    }
    template<class = void> Event memset(void* destination, int value, size_t bytes) {
        return fill(static_cast<unsigned char*>(destination), static_cast<unsigned char>(value), bytes);
    }
    Event ext_oneapi_submit_barrier(std::initializer_list<Event> waits = {}) {
        before();
        for (const auto& event : waits) if (auto handle = event.native()) {
            retain(event.state_);
            stream_.barrier(nullptr, {handle});
        }
        // Explicit dependency markers are handed to other submission threads.
        // Publish their native signal here; a consumer must never materialize
        // a lazy marker by mutating the producer's queue concurrently.
        auto state = std::make_shared<Event::State>();
        state->owner = this;
        state->lifetime = lifetime_;
#ifdef STRATA_ORCA_DG2
        if (!recording_) {
            state->native = completion_event();
            state->completion = state->native->handle->point;
            state->coarse_completion = true;
            retain(state); state->completed_through = sequence_;
            return Event(std::move(state));
        }
#endif
        state->native = make_event();
        retain(state);
        if (recording_) stream_.reset_event(state->native->handle);
        stream_.barrier(state->native->handle);
        record_node(sycl::ext::oneapi::experimental::node_type::ext_oneapi_barrier);
        state->completed_through = sequence_;
        return Event(std::move(state));
    }
    void depends_on(const Event& event) { depends_on({event}); }
    void depends_on(std::initializer_list<Event> events) {
#ifdef STRATA_ORCA_DG2
        if (runtime_.dependency_only && !recording_ && events.size()) {
            // ponytail: eager callers export no completion here. Keep the wait
            // with subsequent work; queue switches and explicit markers flush it.
            before();
            bool waited = false;
            for (const auto& event : events) if (auto handle = event.native()) {
                retain(event.state_);
                stream_.barrier(nullptr, {handle});
                waited = true;
            }
            if (!waited) stream_.barrier();
            return;
        }
#endif
        ext_oneapi_submit_barrier(events);
    }
    void depends_on(const std::vector<Event>& events) { for (const auto& e : events) depends_on(e); }
    bool wait(uint64_t timeout_ns = 30000000000ULL) {
        // The watchdog may observe completion from another thread. Only the
        // submitter materializes markers or mutates the resource ledgers.
        if (std::this_thread::get_id() != owner_thread_) {
            if (deferred_.load(std::memory_order_acquire)) return false;
            if (!stream_.observe_wait(timeout_ns)) return false;
            check_worker();
            return !deferred_.load(std::memory_order_acquire);
        }
        before();
        if (!stream_.wait(timeout_ns)) return false;
        retained_.clear();
        graphs_.clear();
        check_worker();
        return true;
    }
    void wait_and_throw() { if (!wait()) throw std::runtime_error("native queue timeout"); }
    bool ext_oneapi_empty() const {
        return !deferred_.load(std::memory_order_acquire) &&
            stream_.observe_wait(0) && !deferred_.load(std::memory_order_acquire);
    }
    // Typed operators may combine consecutive ready requests until the next
    // queue operation. No worker, timer or wait-to-fill policy is involved.
    void defer(void* owner, void (*flush)(void*)) {
        if (!owner || !flush) throw std::invalid_argument("invalid native deferred operation");
        if (deferred_owner_ != owner) before();
        deferred_owner_ = owner;
        deferred_flush_ = flush;
        deferred_.store(true, std::memory_order_release);
    }
    void flush() { before(); }
    void cancel_deferred(void* owner) noexcept {
        if (deferred_owner_ != owner) return;
        deferred_owner_ = nullptr;
        deferred_flush_ = nullptr;
        deferred_.store(false, std::memory_order_release);
    }
    bool is_in_order() const noexcept { return !independent_; }
    auto ext_oneapi_get_state() const noexcept {
        using State = sycl::ext::oneapi::experimental::queue_state;
        return recording_ ? State::recording : State::executing;
    }
    auto get_backend() const noexcept { return sycl::backend::ext_oneapi_level_zero; }
    auto native_handle() const noexcept { return stream_.native_handle(); }
    auto& runtime() const noexcept { return runtime_; }
    auto& program() const noexcept { return program_; }
    const DeviceView& get_device() const noexcept { return device_; }

    void begin_recording() {
        begin_recording(false);
    }
    // Outside independent(), preserve ordinary queue dependencies even on a
    // relaxed list. Opt in only when the caller can identify independent work.
    void begin_recording(bool allow_independent) {
        before();
        stream_.begin_recording(allow_independent ? Ordering::relaxed : Ordering::in_order);
        recording_ = true;
        relaxed_ = allow_independent;
        needs_barrier_ = false;
        node_types_.clear();
    }
    bool allows_independent() const noexcept { return relaxed_; }
    // Each operation in body must be independent, including any kernels hidden
    // inside typed operators. Inputs are ready at entry; outputs join at exit.
    // Eager/ordinary recording falls back to the existing in-order execution.
    template<class F> void independent(F&& body) {
        if (!relaxed_) { std::forward<F>(body)(); return; }
        if (independent_) throw std::logic_error("nested native independent phase");
        before();
        independent_ = true;
        try { std::forward<F>(body)(); before(); }
        catch (...) { independent_ = false; throw; }
        independent_ = false;
        if (needs_barrier_) {
            stream_.barrier();
            record_node(sycl::ext::oneapi::experimental::node_type::ext_oneapi_barrier);
        }
    }
    std::shared_ptr<NativeGraph> end_recording() {
        if (!recording_ || independent_) throw std::logic_error("invalid native recording end");
        before();
        auto* commands = &stream_.end_recording();
        recording_ = false;
        relaxed_ = false;
        return std::make_shared<NativeGraph>(runtime_, *commands, std::move(recorded_events_), std::move(node_types_));
    }
    Event replay(std::shared_ptr<NativeGraph> graph) {
        if (!graph || recording_) throw std::logic_error("invalid native graph replay");
        before();
        // Keep every recorded event alive even when the graph owner is reset.
        graphs_.emplace_back(++sequence_, graph);
        stream_.replay(*graph->commands);
        return marker();
    }
    template<class Graph> Event ext_oneapi_graph(const Graph& graph) { return replay(graph.native()); }

    template<class F> void host_task(F&& body) {
        if (recording_) throw std::logic_error("native host tasks cannot be captured");
        before();
        check_worker();
        if (!worker_.joinable()) worker_ = std::thread([this] { consume(); });
        auto task = std::make_unique<HostTask>();
        task->body = std::forward<F>(body);
        task->done = make_event();
        task->ready = marker();
        (void)task->ready.native();
        auto state = std::make_shared<Event::State>();
        state->native = task->done;
        retain(state); // Retain before append: even allocation failure must not destroy an in-use event.
        stream_.barrier(nullptr, {task->done->handle});
        // Disruptor-style SPSC handoff. A full ring applies backpressure; it
        // never drops a callback whose completion already gates the GPU.
        while (!tasks_.try_push(task.get())) std::this_thread::yield();
        task.release();
        wake_.fetch_add(1, std::memory_order_release);
        wake_.notify_one();
    }
private:
    void record_node(sycl::ext::oneapi::experimental::node_type type) {
        if (recording_) {
            node_types_.push_back(type);
            needs_barrier_ = type != sycl::ext::oneapi::experimental::node_type::ext_oneapi_barrier;
        }
    }
    std::shared_ptr<NativeEvent> make_event(bool reset = true) {
        for (const auto& pool : event_pools_) {
#ifdef STRATA_ORCA_DG2
            const auto slot = pool->take(reset);
#else
            const auto slot = pool->take();
#endif
            if (slot == UINT32_MAX) continue;
            try { return std::make_shared<NativeEvent>(pool, slot); }
            catch (...) { pool->give(slot); throw; }
        }
        event_pools_.push_back(std::make_shared<EventPool>(runtime_));
        const auto& pool = event_pools_.back();
#ifdef STRATA_ORCA_DG2
        const auto slot = pool->take(reset);
#else
        const auto slot = pool->take();
#endif
        try { return std::make_shared<NativeEvent>(pool, slot); }
        catch (...) { pool->give(slot); throw; }
    }
#ifdef STRATA_ORCA_DG2
    std::shared_ptr<NativeEvent> completion_event() {
        auto point = stream_.checkpoint();
        auto event = make_event(false);
        // The slot is exclusively owned until its State is published. Reuse the
        // immutable queue fence; no tag reset, extra GPU signal or empty submit.
        auto* value = event->handle;
        value->gate = {}; value->gate_created = false;
        value->point = std::move(point); value->inline_value = false;
        value->kind = value->point ? dg2_backend::Event::Kind::gpu : dg2_backend::Event::Kind::host;
        value->signaled = !value->point;
        return event;
    }
#endif
    Event marker() {
        // A completion returned after a typed request must include that work.
        // Earlier markers were materialized when defer() first accepted it.
        if (deferred_owner_) before();
        if (auto state = pending_.lock()) return Event(std::move(state));
        auto state = std::make_shared<Event::State>();
        state->pending = this;
        state->owner = this;
        state->lifetime = lifetime_;
        pending_ = state;
        return Event(std::move(state));
    }
    void retain(const std::shared_ptr<Event::State>& state) {
        if (recording_) recorded_events_.push_back(state);
        else retained_.emplace_back(++sequence_, state);
    }
    void retire_through(uint64_t sequence) {
        // Only the submission owner mutates these ledgers. The callback worker
        // also waits on events, but never retires the producer's resources.
        if (std::this_thread::get_id() != owner_thread_) return;
        while (!retained_.empty() && retained_.front().first <= sequence) retained_.pop_front();
        while (!graphs_.empty() && graphs_.front().first <= sequence) graphs_.pop_front();
    }
    void before() {
        if (auto state = pending_.lock()) {
#ifdef STRATA_ORCA_DG2
            if (!recording_) {
                auto native = completion_event();
                state->completion = native->handle->point;
                state->coarse_completion = true;
                retain(state);
                state->native = std::move(native);
                state->completed_through = sequence_;
                state->pending.store(nullptr, std::memory_order_release);
            } else {
#endif
            auto native = make_event();
            retain(state);
            // A graph reuses its internal markers. Reset on the GPU in list
            // order, never on the host while an earlier replay may be in flight.
            if (recording_) {
                stream_.reset_event(native->handle);
            }
            stream_.barrier(native->handle);
            record_node(sycl::ext::oneapi::experimental::node_type::ext_oneapi_barrier);
            state->native = std::move(native);
            state->completed_through = sequence_;
            state->pending.store(nullptr, std::memory_order_release);
#ifdef STRATA_ORCA_DG2
            }
#endif
        }
        pending_.reset();
        if (deferred_owner_) {
            auto* owner = std::exchange(deferred_owner_, nullptr);
            auto callback = std::exchange(deferred_flush_, nullptr);
            // Clear the callback before it submits kernels through this queue.
            // Keep watchdogs aware of CPU-side work throughout the flush.
            try { callback(owner); }
            catch (...) { deferred_.store(false, std::memory_order_release); throw; }
            deferred_.store(false, std::memory_order_release);
        }
        // ponytail: explicit barriers preserve legacy ordering between phases;
        // narrow dependency events can replace them after model measurements.
        if (relaxed_ && !independent_ && needs_barrier_) {
            stream_.barrier();
            record_node(sycl::ext::oneapi::experimental::node_type::ext_oneapi_barrier);
        }
    }
    void check_worker() const {
        if (failed_.load(std::memory_order_acquire)) std::rethrow_exception(failure_);
    }
    void consume() noexcept {
        for (;;) {
            const auto observed = wake_.load(std::memory_order_acquire);
            HostTask* raw = nullptr;
            if (!tasks_.try_pop(raw)) {
                if (stopping_.load(std::memory_order_acquire)) return;
                wake_.wait(observed, std::memory_order_acquire);
                continue;
            }
            std::unique_ptr<HostTask> task(raw);
            try {
                task->ready.wait_and_throw();
                task->body();
            } catch (...) {
                if (!failed_.load(std::memory_order_relaxed)) {
                    failure_ = std::current_exception();
                    failed_.store(true, std::memory_order_release);
                }
            }
            // Release the GPU gate even when the callback fails; wait() reports
            // that exception after completion instead of leaving a deadlocked GPU.
            try { signal_event(task->done->handle); }
            catch (...) { if (!failed_.load(std::memory_order_relaxed)) {
                failure_ = std::current_exception(); failed_.store(true, std::memory_order_release);
            } }
        }
    }
    Runtime& runtime_;
    Program& program_;
    DeviceView device_;
    Stream stream_;
    std::vector<std::shared_ptr<EventPool>> event_pools_;
    std::weak_ptr<Event::State> pending_;
    std::vector<std::shared_ptr<Event::State>> recorded_events_;
    std::deque<std::pair<uint64_t, std::shared_ptr<Event::State>>> retained_;
    std::deque<std::pair<uint64_t, std::shared_ptr<NativeGraph>>> graphs_;
    std::shared_ptr<int> lifetime_ = std::make_shared<int>(0);
    const std::thread::id owner_thread_ = std::this_thread::get_id();
    uint64_t sequence_ = 0;
    bool recording_ = false;
    bool relaxed_ = false, independent_ = false, needs_barrier_ = false;
    void* deferred_owner_ = nullptr;
    void (*deferred_flush_)(void*) = nullptr;
    std::atomic<bool> deferred_{false};
    std::vector<sycl::ext::oneapi::experimental::node_type> node_types_;
    orca::l0::Ring<HostTask*, 1024> tasks_;
    std::thread worker_;
    std::atomic<size_t> wake_{0};
    std::atomic<bool> stopping_{false}, failed_{false};
    std::exception_ptr failure_;
};

inline EventHandle Event::native() const {
    if (!state_) return nullptr;
    if (auto* pending = state_->pending.load(std::memory_order_acquire)) {
        if (state_->lifetime.expired())
            throw std::logic_error("native completion outlived its publishing queue");
        if (state_->issuer != std::this_thread::get_id())
            throw std::logic_error("publish a native barrier event before handing completion to another thread");
        pending->before();
    }
    return state_->native ? state_->native->handle : nullptr;
}
inline void Event::retire() const {
    if (state_ && state_->owner && !state_->lifetime.expired()) {
        state_->owner->retire_through(state_->completed_through);
        state_->owner->check_worker();
    }
}
}
