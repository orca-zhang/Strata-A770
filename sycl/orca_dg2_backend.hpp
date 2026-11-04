#pragma once
// Strata's queue adapter over Orca's native DG2 transport. No Level Zero handles.
#include <orca_l0/compiler/dg2.hpp>
#include <sycl/ext/oneapi/experimental/device_architecture.hpp>
#include <sycl/ext/oneapi/matrix/matrix.hpp>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <list>
#include <mutex>
#include <optional>
#include <cstdlib>
#include <charconv>
#include <chrono>
#include <thread>
#include <immintrin.h>

namespace strata::l0::dg2_backend {
namespace native = orca::l0::dg2;
enum class Memory { device, host, shared };
enum class Ordering { in_order, relaxed };
class Stream;
struct Commands;

// One consumer owns every native mutation. Producers publish stack-resident
// calls with a lock-free MPSC exchange and block on completion, never spin.
// ponytail: synchronous handoff keeps caller buffers/closures alive; combine
// asynchronous submissions only if measured handoff latency warrants it.
class OwnerExecutor {
    struct alignas(64) Signal { std::atomic<uint32_t> done{0}; };
    struct Call {
        Call* next;
        void* body;
        void (*run)(void*) noexcept;
        std::shared_ptr<Signal> signal;
    };
    alignas(64) std::atomic<Call*> head_{nullptr};
    alignas(64) std::atomic<uint32_t> wake_{0};
    std::atomic<bool> stopping_{false};
    inline static thread_local const OwnerExecutor* current_ = nullptr;
    std::thread worker_{[this] {
        current_ = this;
        for (;;) {
            const auto observed = wake_.load(std::memory_order_acquire);
            auto* calls = head_.exchange(nullptr, std::memory_order_acquire);
            if (!calls) {
                if (stopping_.load(std::memory_order_acquire)) break;
                wake_.wait(observed, std::memory_order_acquire);
                continue;
            }
            Call* ordered = nullptr;
            while (calls) {
                auto* next = calls->next; calls->next = ordered;
                ordered = calls; calls = next;
            }
            while (ordered) {
                auto* next = ordered->next;
                auto signal = ordered->signal; // Keep notification storage alive after publication.
                ordered->run(ordered->body);
                signal->done.store(1, std::memory_order_release);
                signal->done.notify_one();
                ordered = next;
            }
        }
        current_ = nullptr;
    }};
public:
    static_assert(std::atomic<Call*>::is_always_lock_free);
    static_assert(std::atomic<uint32_t>::is_always_lock_free);
    bool on_owner() const noexcept { return current_ == this; }
    ~OwnerExecutor() {
        stopping_.store(true, std::memory_order_release);
        wake_.fetch_add(1, std::memory_order_release); wake_.notify_one();
        worker_.join();
    }
    template<class F> auto invoke(F&& body) -> std::invoke_result_t<F> {
        if (on_owner()) return body();
        using R = std::invoke_result_t<F>;
        using Value = std::conditional_t<std::is_reference_v<R>, std::add_pointer_t<std::remove_reference_t<R>>,
            std::conditional_t<std::is_void_v<R>, int, R>>;
        std::optional<Value> result;
        std::exception_ptr error;
        auto run = [&]() noexcept {
            try {
                if constexpr (std::is_void_v<R>) body();
                else if constexpr (std::is_reference_v<R>) result.emplace(&body());
                else result.emplace(body());
            } catch (...) { error = std::current_exception(); }
        };
        static thread_local auto signal = std::make_shared<Signal>();
        signal->done.store(0, std::memory_order_relaxed);
        Call call{nullptr, &run, [](void* p) noexcept { (*static_cast<decltype(run)*>(p))(); }, signal};
        auto* previous = head_.load(std::memory_order_relaxed);
        do { call.next = previous; }
        while (!head_.compare_exchange_weak(previous, &call, std::memory_order_release, std::memory_order_relaxed));
        wake_.fetch_add(1, std::memory_order_release); wake_.notify_one();
        while (!signal->done.load(std::memory_order_acquire)) signal->done.wait(0, std::memory_order_acquire);
        if (error) std::rethrow_exception(error);
        if constexpr (std::is_reference_v<R>) return **result;
        else if constexpr (!std::is_void_v<R>) return std::move(*result);
    }
};

class Runtime {
public:
    explicit Runtime(const char* memory = nullptr) {
        if (!memory) memory = std::getenv("STRATA_ORCA_DG2_MEMORY");
        if (!memory || !*memory) throw std::invalid_argument("set STRATA_ORCA_DG2_MEMORY to the native memory module");
        const char* thread_memory = std::getenv("STRATA_ORCA_DG2_THREAD_MEMORY");
        if (thread_memory && std::strcmp(thread_memory, "host") && std::strcmp(thread_memory, "device"))
            throw std::invalid_argument("STRATA_ORCA_DG2_THREAD_MEMORY must be host or device");
        const char* isa_memory = std::getenv("STRATA_ORCA_DG2_ISA_MEMORY");
        if (isa_memory && std::strcmp(isa_memory, "host") && std::strcmp(isa_memory, "device"))
            throw std::invalid_argument("STRATA_ORCA_DG2_ISA_MEMORY must be host or device");
        gpu.set_memory_module(memory, !thread_memory || !std::strcmp(thread_memory, "device"),
                              !isa_memory || !std::strcmp(isa_memory, "device"));
        capacity = gpu.memory_info().total;
        // Constant-table initialization can occur during graph recording or
        // while another queue is pending. Reserve its queue before first use.
        initial_upload_queue = gpu.create_queue();
    }
    Runtime(const Runtime&) = delete;
    struct Properties {
        const char* name = "Intel(R) Arc(TM) A770 Graphics";
        // Nanoseconds/tick, including fractional periods. No platform default:
        // supply independently calibrated CS frequency/width for diagnostics.
        double timerResolution = 0;
        uint32_t timestampValidBits = 0;
    };
    Properties properties() const noexcept { return properties_; }
    void* allocate(size_t bytes, Memory kind = Memory::device, size_t alignment = 64) {
        if (!this->on_owner()) return this->invoke([&]() -> decltype(auto) { return allocate(bytes, kind, alignment); });
        if (!bytes || !alignment || (alignment & (alignment - 1)) || alignment > 65536)
            throw std::invalid_argument("invalid DG2 engine allocation size/alignment");
        
        registry_epoch.fetch_add(1, std::memory_order_seq_cst);
        blocks.reserve(blocks.size() + 1);
        auto memory = kind == Memory::device ? gpu.allocate_device(bytes) : gpu.allocate(bytes);
        void* pointer = kind == Memory::device ? reinterpret_cast<void*>(memory.gpu) : memory.data;
        blocks.push_back({memory, pointer, bytes});
        return pointer;
    }
    void deallocate(void* pointer) {
        if (!this->on_owner()) return this->invoke([&]() -> decltype(auto) { return deallocate(pointer); });
        if (!pointer) return;
        
        const auto block = std::find_if(blocks.begin(), blocks.end(), [&](const auto& block) { return block.pointer == pointer; });
        if (block == blocks.end()) throw std::invalid_argument("DG2 free requires an allocation base pointer");
        // ponytail: a pending batch pins the registry until its next command boundary.
        // Per-allocation pins are only needed if unrelated frees must overlap batching.
        registry_epoch.fetch_add(1, std::memory_order_seq_cst);
        if (pending_copy_batches.load(std::memory_order_seq_cst)) throw std::logic_error("DG2 free requires pending copy batches to be flushed");
        gpu.release(block->memory); blocks.erase(block);
    }
    native::MemoryInfo memory_info() {
        if (!this->on_owner()) return this->invoke([&]() -> decltype(auto) { return memory_info(); });  return gpu.memory_info(); }
    int import_host(void* pointer, size_t bytes) {
        if (!this->on_owner()) return this->invoke([&]() -> decltype(auto) { return import_host(pointer, bytes); });
        const auto base = reinterpret_cast<uintptr_t>(pointer);
        if (!base || !bytes || ((base | bytes) & 65535) || bytes > UINTPTR_MAX - base) return 1;
        
        for (const auto& block : blocks) {
            const auto other = reinterpret_cast<uintptr_t>(block.pointer);
            if (base < other ? bytes > other - base : base - other < block.bytes) return 1;
        }
        registry_epoch.fetch_add(1, std::memory_order_seq_cst);
        blocks.reserve(blocks.size() + 1);
        auto memory = gpu.import_host(pointer, bytes);
        blocks.push_back({memory, pointer, bytes});
        return 0;
    }
    void release_host(void* pointer) {
        if (!this->on_owner()) return this->invoke([&]() -> decltype(auto) { return release_host(pointer); });
        
        if (std::none_of(blocks.begin(), blocks.end(), [&](const auto& block) {
                return block.pointer == pointer && block.memory.imported;
            })) throw std::invalid_argument("DG2 host release requires an imported base pointer");
        deallocate(pointer);
    }
    std::optional<uint64_t> address(const void* pointer, size_t bytes) const {
        const auto value = reinterpret_cast<uintptr_t>(pointer);
        for (const auto& block : blocks) {
            const auto base = reinterpret_cast<uintptr_t>(block.pointer);
            if (value >= base && value - base < block.memory.bytes) {
                if (value - base >= block.bytes || bytes > block.bytes - (value - base))
                    throw std::invalid_argument("DG2 transfer exceeds its allocation");
                return block.memory.gpu + value - base;
            }
        }
        return {};
    }
    bool host_address(const void* pointer) const {
        const auto value = reinterpret_cast<uintptr_t>(pointer);
        for (const auto& block : blocks) {
            const auto base = reinterpret_cast<uintptr_t>(block.pointer);
            if (block.memory.data && value >= base && value - base < block.bytes) return true;
        }
        return false;
    }
    void discard(Commands&);
    // The consumer owns the shared native recorder and allocation registry.
    const uint32_t batch_ops = [] {
        const char* value = std::getenv("STRATA_ORCA_DG2_BATCH_OPS");
        if (!value) return uint32_t{64};
        uint32_t count = 0;
        const auto end = value + std::strlen(value);
        const auto parsed = std::from_chars(value, end, count);
        if (parsed.ec != std::errc{} || parsed.ptr != end || count < 1 || count > 4096)
            throw std::invalid_argument("STRATA_ORCA_DG2_BATCH_OPS must be an integer from 1 to 4096");
        return count;
    }();
    const bool inline_events = [] {
        const char* value = std::getenv("STRATA_ORCA_DG2_INLINE_EVENTS");
        if (!value || !std::strcmp(value, "1")) return true;
        if (!std::strcmp(value, "0")) return false;
        throw std::invalid_argument("STRATA_ORCA_DG2_INLINE_EVENTS must be 0 or 1");
    }();
    const bool relaxed_recording = [] {
        const char* value = std::getenv("STRATA_ORCA_DG2_RELAXED");
        if (!value || !std::strcmp(value, "1")) return true;
        if (!std::strcmp(value, "0")) return false;
        throw std::invalid_argument("STRATA_ORCA_DG2_RELAXED must be 0 or 1");
    }();
    OwnerExecutor owner;
    bool on_owner() const noexcept { return owner.on_owner(); }
    template<class F> auto invoke(F&& body) -> std::invoke_result_t<F> { return owner.invoke(std::forward<F>(body)); }
    native::Runtime gpu;
    native::Queue initial_upload_queue;
    uint64_t capacity = 0;
    Stream* active = nullptr;
    size_t stream_count = 0;
    std::atomic<size_t> pending_copy_batches{0};
    std::atomic<uint64_t> registry_epoch{0};
    const Properties properties_ = [] {
        Properties clock;
        const auto* hz_text = std::getenv("STRATA_ORCA_DG2_TIMESTAMP_HZ");
        const auto* bits_text = std::getenv("STRATA_ORCA_DG2_TIMESTAMP_BITS");
        if (!hz_text && !bits_text) return clock;
        auto parse = [](const char* text, uint64_t maximum) {
            uint64_t value = 0;
            if (!text) throw std::invalid_argument("DG2 timestamp calibration requires both HZ and BITS");
            const auto end = text + std::strlen(text);
            const auto result = std::from_chars(text, end, value);
            if (result.ec != std::errc{} || result.ptr != end || !value || value > maximum)
                throw std::invalid_argument("invalid DG2 timestamp calibration HZ or BITS");
            return value;
        };
        clock.timerResolution = 1e9 / double(parse(hz_text, 1000000000000ull));
        clock.timestampValidBits = uint32_t(parse(bits_text, 64));
        return clock;
    }();
    struct Block { native::Allocation memory; void* pointer; size_t bytes; };
    std::vector<Block> blocks;
    struct Pool {
        std::mutex mutex;
        std::vector<native::Allocation> available;
        size_t count = 0;
    };
    std::shared_ptr<Pool> pool = std::make_shared<Pool>();
    std::shared_ptr<const native::Allocation> staging(size_t bytes) {
        if (!this->on_owner()) return this->invoke([&]() -> decltype(auto) { return staging(bytes); });
        std::lock_guard pool_lock(pool->mutex);
        auto memory = std::make_unique<native::Allocation>();
        auto found = std::min_element(pool->available.begin(), pool->available.end(), [bytes](const auto& a, const auto& b) {
            return (a.bytes >= bytes ? a.bytes : SIZE_MAX) < (b.bytes >= bytes ? b.bytes : SIZE_MAX);
        });
        if (found != pool->available.end() && found->bytes >= bytes) {
            *memory = *found; *found = pool->available.back(); pool->available.pop_back();
        } else {
            pool->available.reserve(pool->count + 1);
            *memory = gpu.allocate_staging(bytes); ++pool->count;
        }
        return {memory.release(), [pool = pool](const native::Allocation* allocation) noexcept {
            std::lock_guard lock(pool->mutex); pool->available.push_back(*allocation); delete allocation;
        }};
    }
    std::list<std::unique_ptr<Commands>> graphs;
};
class Program {
public:
    Program(Runtime&, const std::filesystem::path& directory) : gpu(directory) {}
    void add(const std::filesystem::path& directory) { gpu.add(directory); }
    native::Program gpu;
};
using Lease = std::shared_ptr<const native::Allocation>;
struct HostCopy { void* destination; Lease memory; size_t bytes; };
struct Upload { const void* source; Lease memory; size_t bytes; };
struct Readbacks {
    struct Pending { uint64_t sequence; native::Completion completion; std::vector<HostCopy> copies; };
    std::deque<Pending> pending;
};
struct Point {
    Runtime& runtime;
    native::Completion gpu;
    native::Queue queue;
    uint64_t sequence = 0; // Logical stream order, including graphs on retained native queues.
    std::shared_ptr<Readbacks> readbacks;
    std::vector<Lease> leases;
    Lease tag_memory;
    std::atomic<uint64_t>* tag = nullptr;
    std::atomic<bool> finished{false};
    bool needs_readbacks = true;
    Point(Runtime& runtime, std::shared_ptr<Readbacks> readbacks)
        : runtime(runtime), readbacks(std::move(readbacks)) {}
    void finish() {
        if (finished.load(std::memory_order_acquire)) return;
        if (!needs_readbacks) {
            if (tag ? !tag_ready() : !gpu.ready()) throw std::runtime_error("DG2 completion is still in flight");
            // Only a status publication: no shared ledger or leases are modified.
            // The next collector releases resources under its existing context.
            finished.store(true, std::memory_order_release); return;
        }
        finish_locked();
    }
    // The native owner publishes the readback ledger and native cpu_access state.
    void finish_locked() {
        if (!runtime.on_owner()) return runtime.invoke([&]() -> decltype(auto) { return finish_locked(); });
        if (finished.load(std::memory_order_acquire)) { leases.clear(); return; }
        if (tag ? !tag_ready() : !runtime.gpu.ready(gpu))
            throw std::runtime_error("DG2 point completion changed during collection");
        // Waiting on a later event also publishes earlier staged readbacks.
        // The ledger outlives its Stream when a completion is retained elsewhere.
        while (!readbacks->pending.empty() && readbacks->pending.front().sequence <= sequence) {
            if (!runtime.gpu.ready(readbacks->pending.front().completion))
                throw std::runtime_error("DG2 readback dependency has not completed");
            for (const auto& copy : readbacks->pending.front().copies) {
                runtime.gpu.cpu_access(*copy.memory, false);
                std::memcpy(copy.destination, copy.memory->data, copy.bytes);
            }
            readbacks->pending.pop_front();
        }
        leases.clear(); finished.store(true, std::memory_order_release);
    }
    bool tag_ready() const {
        // Tags occupy separate cache lines; refresh only this event's line.
        _mm_clflush(tag); _mm_mfence();
        return tag->load(std::memory_order_acquire) == 1;
    }
    bool ready() {
        if (finished.load(std::memory_order_acquire)) return true;
        if (tag ? !tag_ready() : !gpu.ready()) return finished.load(std::memory_order_acquire);
        finish(); return true;
    }
    bool ready_locked() {
        if (finished.load(std::memory_order_acquire)) return true;
        if (tag ? !tag_ready() : !runtime.gpu.ready(gpu)) return false;
        finish_locked(); return true;
    }
    bool wait(uint64_t ns) {
        if (finished.load(std::memory_order_acquire)) return true;
        if (!tag) { if (!gpu.wait(ns)) return false; finish(); return true; }
        using Clock = std::chrono::steady_clock;
        const auto start = Clock::now();
        uint32_t spins = 0;
        while (!ready()) {
            if (uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count()) >= ns) return false;
            // ponytail: memory tags have no OS wakeup; queue tails retain native
            // fence waits. Bound active spinning before a short sleep.
            if (++spins < 128) _mm_pause();
            else std::this_thread::sleep_for(std::chrono::microseconds(25));
        }
        return true;
    }
};
struct Event {
    enum class Kind { empty, host, gpu } kind = Kind::empty;
    Runtime* runtime = nullptr;
    // The native owner serializes event publication and reset. Only unresolved
    // host waiters use the condition-variable mutex; GPU fence observers do not.
    std::condition_variable wake;
    std::mutex wake_mutex;
    uint64_t wake_epoch = 0;
    void notify() {
        if (waiters) {
            std::lock_guard lock(wake_mutex); ++wake_epoch; wake.notify_all();
        }
    }
    size_t waiters = 0; // Native owner only; notify actual unresolved host waiters.
    native::HostGate gate;
    bool gate_created = false;
    std::shared_ptr<Point> point;
    bool signaled = false;
    bool reusable = false;
    Lease tag_memory;
    std::atomic<uint64_t>* tag = nullptr;
    uint64_t tag_address = 0;
    bool inline_value = false;
};
using EventHandle = Event*;
inline void reset_event(EventHandle event) {
        if (!(*event->runtime).on_owner()) return (*event->runtime).invoke([&]() -> decltype(auto) { return reset_event(event); });
    if (event->point) event->point->finish_locked();
    event->kind = Event::Kind::empty; event->point.reset(); event->gate = {}; event->gate_created = false; event->signaled = false;
    event->inline_value = false;
    if (event->tag) { event->tag->store(0, std::memory_order_release); _mm_clflush(event->tag); _mm_mfence(); }
}
inline void signal_event(EventHandle event) {
        if (!(*event->runtime).on_owner()) return (*event->runtime).invoke([&]() -> decltype(auto) { return signal_event(event); });
    {
        if (event->kind == Event::Kind::gpu) throw std::logic_error("CPU signal of a GPU completion");
        // Publish success only after the native gate accepts the signal.
        // A release before the first wait needs no native gate yet.
        if (event->gate_created) event->gate.signal();
        event->kind = Event::Kind::host; event->signaled = true;
        event->notify();
    }
}
inline bool event_wait(EventHandle event, uint64_t ns) {
    using Clock = std::chrono::steady_clock;
    if (ns > uint64_t(INT64_MAX)) throw std::invalid_argument("DG2 event timeout exceeds INT64_MAX");
    const auto start = Clock::now();
    std::shared_ptr<Point> point;
    bool signaled = false, subscribed = false;
    uint64_t epoch = 0;
    event->runtime->invoke([&] {
        signaled = event->signaled; point = event->point;
        if (!signaled && !point) { ++event->waiters; subscribed = true; epoch = event->wake_epoch; }
    });
    if (subscribed) {
        try {
            std::unique_lock lock(event->wake_mutex);
            event->wake.wait_until(lock, start + std::chrono::nanoseconds(ns), [&] { return event->wake_epoch != epoch; });
        } catch (...) { event->runtime->invoke([&] { --event->waiters; }); throw; }
        event->runtime->invoke([&] { --event->waiters; signaled = event->signaled; point = event->point; });
    }
    if (signaled) return true;
    if (!point) return false;
    const auto elapsed = uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count());
    return point->wait(elapsed >= ns ? 0 : ns - elapsed);
}
inline bool event_ready(EventHandle event) { return event_wait(event, 0); }
class HostEvent {
public:
    explicit HostEvent(Runtime& runtime) : runtime_(runtime) {
        runtime_.invoke([&] {
            value_.runtime = &runtime_; value_.kind = Event::Kind::host; value_.reusable = true;
            value_.gate = runtime_.gpu.create_gate(true); value_.gate_created = true;
        });
    }
    EventHandle handle() noexcept { return &value_; }
    void reset() {
        runtime_.invoke([&] { runtime_.gpu.reset_gate(value_.gate); value_.signaled = false; });
    }
    void signal() { signal_event(&value_); }
private:
    Runtime& runtime_;
    Event value_;
};
struct Segment {
    native::Commands commands;
    struct Signal { EventHandle event; bool inline_value; std::shared_ptr<Point> last; };
    std::vector<Signal> signals;
    std::vector<EventHandle> waits;
    std::vector<Lease> leases;
    std::vector<HostCopy> copies;
    std::vector<Upload> uploads;
    std::shared_ptr<Point> last;
};
struct Commands {
    std::vector<Segment> segments;
    native::Queue queue;
    Runtime* runtime = nullptr;
    bool in_order = true;
};
class Stream {
public:
    Stream(Runtime& runtime, Program& program, bool = false) : runtime_(runtime), program_(program) {
        runtime_.invoke([&] { queue_ = runtime_.stream_count++ ? runtime_.gpu.create_queue() : runtime_.gpu.default_queue(); });
    }
    Stream(Runtime& runtime, Program& program, native::Queue queue)
        : runtime_(runtime), program_(program), queue_(std::move(queue)) {}
    ~Stream() {
        try {
            if (recording_) runtime_.invoke([&] {
                freeze(); runtime_.graphs.push_back(std::move(recording_)); runtime_.discard(*runtime_.graphs.back());
            });
            else wait(30000000000ull);
        } catch (...) {}
        runtime_.invoke([&] {
            try { collect(); } catch (...) {}
            if (runtime_.active == this) runtime_.active = nullptr;
        });
    }
    template<class Name = void, class Range, class... Args>
    void parallel_for(Range range, Args&&... args) {
        if (!runtime_.on_owner()) return runtime_.invoke([&]() -> decltype(auto) { return parallel_for<Name>(range, std::forward<Args>(args)...); }); flush_copies_locked(); activate();
        orca::l0::compiler::parallel_for<Name>(program_.gpu, runtime_.gpu, range, std::forward<Args>(args)...);
        dirty_.store(true, std::memory_order_release);
        record_operation();
    }
    void copy(void* destination, const void* source, size_t bytes) {
        if (!bytes) return;
        if (!destination || !source) throw std::invalid_argument("null DG2 copy pointer");
        if (copy_error_) std::rethrow_exception(copy_error_);
        if (!copy_pin_) {
            // This is a lifetime pin, not a recorder lock. The SC order forbids
            // free seeing zero pins while this producer accepts the old epoch.
            runtime_.pending_copy_batches.fetch_add(1, std::memory_order_seq_cst);
            copy_pin_ = true;
            if (copy_epoch_ != runtime_.registry_epoch.load(std::memory_order_seq_cst)) {
                runtime_.invoke([&] {
                    try { copy_blocks_ = runtime_.blocks; }
                    catch (...) { release_copy_pin(); throw; }
                    copy_epoch_ = runtime_.registry_epoch.load(std::memory_order_seq_cst);
                });
            }
        }
        // The pinned registry is immutable for this batch. Ordinary Host pointers
        // still stage immediately: their contents need not outlive this call.
        const auto find = [&](const void* pointer, size_t& hint) -> const Runtime::Block* {
            const auto value = reinterpret_cast<uintptr_t>(pointer);
            const auto contains = [&](const auto& block) {
                const auto start = reinterpret_cast<uintptr_t>(block.pointer);
                return value >= start && value - start < block.memory.bytes;
            };
            if (hint < copy_blocks_.size() && contains(copy_blocks_[hint])) return &copy_blocks_[hint];
            for (size_t i = 0; i < copy_blocks_.size(); ++i)
                if (contains(copy_blocks_[i])) { hint = i; return &copy_blocks_[i]; }
            return nullptr;
        };
        const auto* to = find(destination, destination_hint_);
        const auto* from = find(source, source_hint_);
        // A newly registered device source may be absent from this pinned
        // snapshot. Only treat an unknown pointer as Host while its epoch is current.
        if (to && !to->memory.data && (from || (!recording_ && bytes <= 131072 &&
                copy_epoch_ == runtime_.registry_epoch.load(std::memory_order_seq_cst)))) {
            const auto address = [&](const Runtime::Block& block, const void* pointer) {
                const auto offset = reinterpret_cast<uintptr_t>(pointer) - reinterpret_cast<uintptr_t>(block.pointer);
                if (offset >= block.bytes || bytes > block.bytes - offset)
                    throw std::invalid_argument("DG2 transfer exceeds its allocation");
                return block.memory.gpu + offset;
            };
            uint64_t dst, src;
            try { dst = address(*to, destination); src = from ? address(*from, source) : 0; }
            catch (...) {
                if (!copy_count_) { release_copy_pin(); }
                throw;
            }
            auto& copy = copies_[copy_count_];
            // Snapshot ordinary Host inputs before returning to their producer.
            // ponytail: cache inputs up to 128 KiB per slot (64 slots per stream);
            // larger inputs keep immediate staging rather than retaining unbounded RAM.
            try {
                if (!from) {
                    if (bytes > copy.host.size()) copy.large_host.resize(bytes);
                    std::memcpy(bytes <= copy.host.size() ? copy.host.data() : copy.large_host.data(), source, bytes);
                }
            } catch (...) { if (!copy_count_) release_copy_pin(); throw; }
            copy.destination = dst; copy.source = src; copy.bytes = bytes;
            ++copy_count_;
            copies_pending_.store(true, std::memory_order_release);
            if (copy_count_ == std::min<size_t>(copies_.size(), runtime_.batch_ops))
                runtime_.invoke([&] { flush_copies_locked(); });
            return;
        }
        runtime_.invoke([&] {
            flush_copies_locked();
            copy_impl(destination, source, bytes, false);
        });
    }
    void copy_2d(void* destination, size_t dp, const void* source, size_t sp, size_t width, size_t height) {
        if (!runtime_.on_owner()) return runtime_.invoke([&]() -> decltype(auto) { return copy_2d(destination, dp, source, sp, width, height); });
        if (!width || !height) return;
        if (!destination || !source || width > dp || width > sp || (height - 1) > (SIZE_MAX - width) / std::max(dp, sp))
            throw std::invalid_argument("invalid DG2 2D copy");
        const auto dst = reinterpret_cast<uintptr_t>(destination), src = reinterpret_cast<uintptr_t>(source);
        if (width > UINTPTR_MAX - dst || (height - 1) * dp > UINTPTR_MAX - dst - width ||
            width > UINTPTR_MAX - src || (height - 1) * sp > UINTPTR_MAX - src - width)
            throw std::invalid_argument("DG2 2D copy address overflow");
        if (width == dp && width == sp) { copy(destination, source, width * height); return; } flush_copies_locked();
        // Validate registered spans before recording any of the rows.
        const auto destination_span = (height - 1) * dp + width;
        const auto source_span = (height - 1) * sp + width;
        const auto to = runtime_.address(destination, destination_span);
        const auto from = runtime_.address(source, source_span);
        // Registered disjoint regions need one kernel; overlapping or unregistered
        // spans retain the staged-row behavior and its existing completion rules.
        if (to && from && (*to > *from ? *to - *from >= source_span : *from - *to >= destination_span)) {
            activate();
            runtime_.gpu.copy_2d(*to, dp, *from, sp, width, height);
            dirty_.store(true, std::memory_order_release);
            record_operation();
            if (!recording_ && runtime_.host_address(destination)) freeze();
            return;
        }
        for (size_t row = 0; row < height; ++row)
            copy_impl(static_cast<std::byte*>(destination) + row * dp,
                      static_cast<const std::byte*>(source) + row * sp, width, true);
        if (!recording_ && (runtime_.host_address(destination) || !runtime_.address(destination, 1))) freeze();
    }
    void fill(void* destination, const void* pattern, size_t pattern_bytes, size_t bytes) {
        if (!runtime_.on_owner()) return runtime_.invoke([&]() -> decltype(auto) { return fill(destination, pattern, pattern_bytes, bytes); }); flush_copies_locked(); activate();
        const auto to = runtime_.address(destination, bytes);
        if (!to) throw std::invalid_argument("DG2 fill requires a registered buffer");
        runtime_.gpu.fill(*to, pattern, pattern_bytes, bytes); dirty_.store(true, std::memory_order_release);
        record_operation();
        if (!recording_ && runtime_.host_address(destination)) freeze();
    }
    size_t max_fill_pattern() const noexcept { return 128; }
    void reset_event(EventHandle event) { dg2_backend::reset_event(event); }
    void barrier(EventHandle signal = nullptr, std::initializer_list<EventHandle> waits = {}) {
        if (!runtime_.on_owner()) return runtime_.invoke([&]() -> decltype(auto) { return barrier(signal, waits); }); flush_copies_locked();
        bool boundary = false;
        for (auto* event : waits) {
            boundary |= event->kind != Event::Kind::gpu || !event->inline_value;
        }
        if (boundary && runtime_.active == this) freeze();
        activate();
        for (auto* event : waits) {
            if (event->runtime != &runtime_) throw std::invalid_argument("foreign DG2 event");
            if (event->kind == Event::Kind::empty) {
                event->kind = Event::Kind::host;
                event->gate = runtime_.gpu.create_gate();
                event->gate_created = true;
            }
            if (event->kind == Event::Kind::host) {
                if (!event->signaled || event->reusable) runtime_.gpu.depend_on(event->gate);
            } else if (event->inline_value) {
                if (!recording_ && !event->point) throw std::logic_error("DG2 dependency has not been submitted");
                segment_.leases.push_back(event->tag_memory);
                runtime_.gpu.wait_value(event->tag_address, 1);
            } else segment_.waits.push_back(event);
        }
        dirty_.store(true, std::memory_order_release);
        if (signal) {
            if (signal->runtime != &runtime_) throw std::invalid_argument("foreign DG2 signal event");
            // Staged CPU destinations need a queue-fence boundary before the
            // owner can publish their copies. Ordinary graph events stay inline.
            const bool inline_value = runtime_.inline_events && recording_ && signal->tag && segment_.copies.empty();
            segment_.signals.push_back({signal, inline_value, {}});
            signal->inline_value = inline_value; signal->kind = Event::Kind::gpu;
            if (inline_value) {
                segment_.leases.push_back(signal->tag_memory);
                runtime_.gpu.signal_value(signal->tag_address, 1);
            } else { runtime_.gpu.barrier(); freeze(); }
        } else runtime_.gpu.barrier();
    }
    void write_global_timestamp(uint64_t* destination) {
        if (!runtime_.on_owner()) return runtime_.invoke([&]() -> decltype(auto) { return write_global_timestamp(destination); }); flush_copies_locked();
        if (!destination || (reinterpret_cast<uintptr_t>(destination) & 7))
            throw std::invalid_argument("DG2 timestamp requires an aligned registered 64-bit destination");
        const auto to = runtime_.address(destination, sizeof(*destination));
        if (!to) throw std::invalid_argument("DG2 timestamp requires a registered destination");
        activate();
        runtime_.gpu.write_global_timestamp(*to);
        dirty_.store(true, std::memory_order_release);
        record_operation();
        if (!recording_ && runtime_.host_address(destination)) freeze();
    }
    void begin_recording(Ordering order) {
        if (!runtime_.on_owner()) return runtime_.invoke([&]() -> decltype(auto) { return begin_recording(order); });
        if (recording_) throw std::logic_error("DG2 stream is already recording");
        freeze(); recording_ = std::make_unique<Commands>(); recording_->queue = queue_; recording_->runtime = &runtime_;
        recording_->in_order = order != Ordering::relaxed || !runtime_.relaxed_recording;
    }
    Commands& end_recording() {
        if (!runtime_.on_owner()) return runtime_.invoke([&]() -> decltype(auto) { return end_recording(); });
        if (!recording_) throw std::logic_error("DG2 stream is not recording");
        freeze(); runtime_.graphs.push_back(std::move(recording_)); return *runtime_.graphs.back();
    }
    void replay(Commands& commands) {
        if (!runtime_.on_owner()) return runtime_.invoke([&]() -> decltype(auto) { return replay(commands); });
        if (recording_) throw std::logic_error("cannot replay during DG2 recording");
        if (commands.runtime != &runtime_) throw std::invalid_argument("DG2 graph belongs to a different runtime");
        freeze();
        for (auto& segment : commands.segments) {
            if (segment.last && !segment.last->ready_locked()) throw std::logic_error("previous DG2 graph replay is still in flight");
            for (auto& signal : segment.signals) {
                // Preserve old immutable points before rearming their cache line.
                if (signal.last) signal.last->finish_locked();
                if (signal.inline_value) {
                    signal.event->tag->store(0, std::memory_order_release);
                    _mm_clflush(signal.event->tag);
                }
            }
            _mm_mfence();
            for (const auto& upload : segment.uploads) {
                runtime_.gpu.cpu_access(*upload.memory, true);
                std::memcpy(upload.memory->data, upload.source, upload.bytes);
            }
            submit(segment, false, commands.queue);
        }
        if (commands.segments.empty()) barrier();
    }
    bool wait(uint64_t ns = 30000000000ull) {
        auto point = checkpoint();
        if (!point) return true;
        if (!point->needs_readbacks) return point->wait(ns);
        const bool complete = point->finished.load(std::memory_order_acquire) || point->gpu.wait(ns);
        runtime_.invoke([&] { if (complete) point->finish_locked(); collect(); });
        return complete;
    }
    std::shared_ptr<Point> checkpoint() {
        if (!dirty_.load(std::memory_order_acquire) && !copies_pending_.load(std::memory_order_acquire)) {
            if (recording_) throw std::logic_error("coarse completion during DG2 recording");
            return latest_; // Acquire observes a foreign recorder's published immutable tail.
        }
        if (!runtime_.on_owner()) return runtime_.invoke([&]() -> decltype(auto) { return checkpoint(); });
        if (recording_) throw std::logic_error("coarse completion during DG2 recording");
        freeze(); return latest_;
    }
    bool observe_wait(uint64_t ns) const {
        if (dirty_.load(std::memory_order_acquire) || copies_pending_.load(std::memory_order_acquire)) return false;
        auto point = runtime_.invoke([&] { return latest_; });
        if (point && !point->wait(ns)) return false;
        return runtime_.invoke([&] {
            return !dirty_.load(std::memory_order_acquire) && !copies_pending_.load(std::memory_order_acquire) && latest_ == point;
        });
    }
    Stream* native_handle() const noexcept { return const_cast<Stream*>(this); }
private:
    void record_operation() {
        // ponytail: bound eager batches by operation count; explicit signals,
        // waits, readbacks and queue switches flush their tail without a timer.
        // Recorded graphs retain their explicit event boundaries instead.
        if (!recording_ && ++operations_ >= runtime_.batch_ops) freeze();
    }
    void copy_impl(void* destination, const void* source, size_t bytes, bool defer_readback) {
        if (!bytes) return;
        if (!destination || !source) throw std::invalid_argument("null DG2 copy pointer");
        activate();
        auto from = runtime_.address(source, bytes), to = runtime_.address(destination, bytes);
        const bool readback = runtime_.host_address(destination) || !to;
        if (!from) {
            Lease memory;
            if (!recording_) for (const auto& cached : upload_cache_)
                if (cached.use_count() == 1 && cached->bytes >= bytes) { memory = cached; break; }
            if (!memory) {
                memory = runtime_.staging(bytes);
                // ponytail: retain at most eight upload buffers; larger overlapping
                // bursts fall back to the existing shared staging pool.
                if (!recording_ && upload_cache_.size() < 8) upload_cache_.push_back(memory);
            }
            runtime_.gpu.cpu_access(*memory, true); std::memcpy(memory->data, source, bytes);
            from = memory->gpu; segment_.leases.push_back(memory);
            if (recording_) segment_.uploads.push_back({source, memory, bytes});
        }
        if (!to) {
            auto memory = runtime_.staging(bytes); to = memory->gpu;
            segment_.leases.push_back(memory); segment_.copies.push_back({destination, memory, bytes});
        }
        runtime_.gpu.copy(*to, *from, bytes); dirty_.store(true, std::memory_order_release);
        record_operation();
        if (!recording_ && !defer_readback && readback) freeze();
    }
    void activate() {
        if (runtime_.active == this) return;
        // Another producer may switch the shared recorder. It can submit only
        // committed commands; the pending-copy array belongs to its own producer.
        if (runtime_.active) runtime_.active->freeze_recorded();
        runtime_.gpu.begin(queue_, !recording_ || recording_->in_order); runtime_.active = this;
    }
    void collect() {
        if (pending_.empty()) return;
        std::array<native::Queue, 16> queues{};
        std::array<uint64_t, 16> through{};
        while (!pending_.empty()) {
            // Bound lookahead even when a gated head has a large pending tail.
            // One batch covers the next queues; reuse it beyond the lookahead
            // while those queues keep supplying the completed prefix.
            size_t count = 0, inspected = 0;
            for (const auto& pending : pending_) {
                const auto& queue = pending.point->queue;
                if (std::find(queues.begin(), queues.begin() + count, queue) == queues.begin() + count)
                    queues[count++] = queue;
                if (++inspected == queues.size()) break;
            }
            runtime_.gpu.completed(queues.data(), through.data(), count);
            while (!pending_.empty()) {
                const auto& point = pending_.front().point;
                const auto found = std::find(queues.begin(), queues.begin() + count, point->queue);
                if (found == queues.begin() + count) break;
                // Never wait for a tail or hold ready work to fill a batch.
                if (point->gpu.sequence() > through[size_t(found - queues.begin())]) return;
                point->finish_locked();
                if (pending_.front().owned) runtime_.gpu.discard(pending_.front().commands);
                pending_.pop_front();
            }
        }
    }
    void submit(Segment& segment, bool owned) { submit(segment, owned, queue_); }
    void submit(Segment& segment, bool owned, const native::Queue& queue) {
        collect();
        if (sequence_ == UINT64_MAX) throw std::overflow_error("DG2 stream sequence exhausted");
        std::array<std::shared_ptr<Point>, 16> dependencies{};
        size_t count = 0;
        const auto depend_on = [&](const std::shared_ptr<Point>& point) {
            if (point->queue == queue) return; // The native queue already orders its own submissions.
            for (size_t i = 0; i < count; ++i) {
                if (dependencies[i]->queue != point->queue) continue;
                if (dependencies[i]->gpu.sequence() < point->gpu.sequence()) dependencies[i] = point;
                return;
            }
            if (count == dependencies.size()) throw std::invalid_argument("DG2 submission exceeds 16 predecessor queues");
            dependencies[count++] = point;
        };
        // A graph retains its recording queue and queue-bound scratch/private memory.
        // Join only across queue transitions; same-queue submissions are already ordered.
        if (latest_) depend_on(latest_);
        for (auto* event : segment.waits) {
            if (!event->point) throw std::logic_error("DG2 dependency has not been submitted");
            depend_on(event->point);
        }
        std::vector<native::Completion> waits;
        waits.reserve(count);
        for (size_t i = 0; i < count; ++i) waits.push_back(dependencies[i]->gpu);
        for (auto& signal : segment.signals) if (signal.inline_value) {
            if (sequence_ == UINT64_MAX) throw std::overflow_error("DG2 stream sequence exhausted");
            signal.last = std::make_shared<Point>(runtime_, readbacks_);
            signal.last->needs_readbacks = !readbacks_->pending.empty();
            signal.last->queue = queue; signal.last->sequence = ++sequence_;
            signal.last->tag_memory = signal.event->tag_memory; signal.last->tag = signal.event->tag;
        }
        if (sequence_ == UINT64_MAX) throw std::overflow_error("DG2 stream sequence exhausted");
        auto point = std::make_shared<Point>(runtime_, readbacks_);
        point->needs_readbacks = !readbacks_->pending.empty() || !segment.copies.empty();
        point->queue = queue; point->sequence = ++sequence_;
        point->leases = segment.leases;
        pending_.push_back({segment.commands, point, owned});
        if (!segment.copies.empty()) readbacks_->pending.push_back({point->sequence, {}, segment.copies});
        point->gpu = runtime_.gpu.submit(segment.commands, queue, waits);
        if (!segment.copies.empty()) readbacks_->pending.back().completion = point->gpu;
        for (auto& signal : segment.signals) {
            if (signal.inline_value) signal.last->gpu = point->gpu;
            else signal.last = point;
            signal.event->point = signal.last; signal.event->kind = Event::Kind::gpu;
            signal.event->notify();
        }
        segment.last = point; latest_ = point;
    }
    void release_copy_pin() {
        if (copy_pin_) { runtime_.pending_copy_batches.fetch_sub(1, std::memory_order_seq_cst); copy_pin_ = false; }
    }
    void flush_copies_locked() {
        if (copy_error_) std::rethrow_exception(copy_error_);
        const auto count = std::exchange(copy_count_, size_t(0));
        if (count) dirty_.store(true, std::memory_order_release);
        copies_pending_.store(false, std::memory_order_release);
        // Reset before append: the operation cap can recursively freeze this stream.
        try {
            for (size_t i = 0; i < count; ++i) {
                activate();
                const auto& copy = copies_[i];
                if (!copy.source) {
                    const auto* source = copy.bytes <= copy.host.size() ? copy.host.data() : copy.large_host.data();
                    copy_impl(reinterpret_cast<void*>(copy.destination), source, copy.bytes, false);
                } else {
                    runtime_.gpu.copy(copy.destination, copy.source, copy.bytes);
                    dirty_.store(true, std::memory_order_release); record_operation();
                }
            }
        } catch (...) { release_copy_pin(); copy_error_ = std::current_exception(); throw; }
        release_copy_pin();
    }
    void freeze() {
        flush_copies_locked();
        freeze_recorded();
    }
    void freeze_recorded() {
        if (runtime_.active != this) return;
        // Validation can activate an empty recorder. Release it at the boundary
        // so the next recording/eager operation selects its own ordering mode.
        if (!dirty_.load(std::memory_order_acquire)) { runtime_.active = nullptr; return; }
        runtime_.gpu.barrier(); segment_.commands = runtime_.gpu.finish();
        runtime_.active = nullptr;
        if (recording_) {
            recording_->segments.push_back(std::move(segment_));
        } else submit(segment_, true);
        segment_ = {}; operations_ = 0; dirty_.store(false, std::memory_order_release);
    }
    struct Pending { native::Commands commands; std::shared_ptr<Point> point; bool owned; };
    Runtime& runtime_;
    Program& program_;
    native::Queue queue_;
    Segment segment_;
    std::unique_ptr<Commands> recording_;
    std::deque<Pending> pending_;
    std::shared_ptr<Readbacks> readbacks_ = std::make_shared<Readbacks>();
    std::shared_ptr<Point> latest_;
    std::atomic<bool> dirty_{false};
    std::atomic<bool> copies_pending_{false};
    struct DeferredCopy {
        uint64_t destination, source; size_t bytes;
        std::array<std::byte, 16> host{}; // source==0 selects the producer-owned Host snapshot.
        std::vector<std::byte> large_host; // Reuse capacity across eager batches.
    };
    std::array<DeferredCopy, 64> copies_{};
    size_t copy_count_ = 0;
    bool copy_pin_ = false;
    uint64_t copy_epoch_ = UINT64_MAX;
    std::vector<Runtime::Block> copy_blocks_;
    size_t destination_hint_ = SIZE_MAX, source_hint_ = SIZE_MAX;
    std::exception_ptr copy_error_;
    std::vector<Lease> upload_cache_;
    size_t operations_ = 0;
    uint64_t sequence_ = 0;
};
inline void Runtime::discard(Commands& commands) {
        if (!this->on_owner()) return this->invoke([&]() -> decltype(auto) { return discard(commands); });
    
    auto found = std::find_if(graphs.begin(), graphs.end(), [&](const auto& graph) { return graph.get() == &commands; });
    if (found == graphs.end()) throw std::invalid_argument("foreign DG2 graph");
    for (auto& segment : commands.segments) {
        if (segment.last && !segment.last->ready_locked()) throw std::logic_error("DG2 graph is still in flight");
        gpu.discard(segment.commands);
    }
    graphs.erase(found);
}
// The transport itself validates the calibrated A770. These are the DG2 kernel
// limits exercised by Strata's native checks, not a generic Intel capability probe.
class DeviceView {
public:
    explicit DeviceView(Runtime& runtime) : runtime_(runtime) {}
    bool has(sycl::aspect aspect) const noexcept {
        return aspect == sycl::aspect::fp16 || aspect == sycl::aspect::atomic64 ||
            aspect == sycl::aspect::usm_host_allocations || aspect == sycl::aspect::usm_device_allocations ||
            aspect == sycl::aspect::usm_shared_allocations;
    }
    bool is_gpu() const noexcept { return true; }
    auto get_backend() const noexcept { return sycl::backend::ext_oneapi_level_zero; }
    template<class Info> auto get_info() const {
        namespace d = sycl::info::device;
        if constexpr (std::is_same_v<Info, d::name>) return std::string("Intel(R) Arc(TM) A770 Graphics");
        else if constexpr (std::is_same_v<Info, d::vendor>) return std::string("Intel");
        else if constexpr (std::is_same_v<Info, d::vendor_id>) return uint32_t(0x8086);
        else if constexpr (std::is_same_v<Info, d::global_mem_size>) return runtime_.capacity;
        else if constexpr (std::is_same_v<Info, d::local_mem_size>) return uint64_t(65536);
        else if constexpr (std::is_same_v<Info, d::max_work_group_size>) return size_t(1024);
        else if constexpr (std::is_same_v<Info, d::max_compute_units>) return uint32_t(512);
        else if constexpr (std::is_same_v<Info, d::max_clock_frequency>) return uint32_t(0); // Unknown; callers use their existing fallback.
        else if constexpr (std::is_same_v<Info, d::sub_group_sizes>) return std::vector<size_t>{8,16,32};
        else if constexpr (std::is_same_v<Info, d::max_work_item_sizes<3>>) return sycl::range<3>(64,1024,1024);
        else if constexpr (std::is_same_v<Info, sycl::ext::oneapi::experimental::info::device::architecture>)
            return sycl::ext::oneapi::experimental::architecture::intel_gpu_acm_g10;
        else if constexpr (std::is_same_v<Info, sycl::ext::oneapi::experimental::info::device::matrix_combinations>) {
            namespace m = sycl::ext::oneapi::experimental::matrix;
            return std::vector<m::combination>{
                {8,8,16,8,8,16,m::matrix_type::fp16,m::matrix_type::fp16,m::matrix_type::fp32,m::matrix_type::fp32},
                {8,8,16,8,8,16,m::matrix_type::bf16,m::matrix_type::bf16,m::matrix_type::fp32,m::matrix_type::fp32}};
        }
        else static_assert(!std::is_same_v<Info,Info>, "DG2 device query has not been implemented");
    }
private:
    Runtime& runtime_;
};
} // namespace strata::l0::dg2_backend
