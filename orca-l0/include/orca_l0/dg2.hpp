#pragma once

// Experimental DG2 compute without Level Zero or an Intel user-mode driver.
// Ordered native queues share one Runtime; internal command/argument storage is pooled.
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <type_traits>
#include <array>
#include <initializer_list>
#include <utility>

namespace orca::l0::drm::detail {
struct RecordingHeap;
struct CaptureHeap;
struct AllocationPool;
}

namespace orca::l0::dg2 {
struct Allocation {
    void* data = nullptr;
    uint64_t gpu = 0;
    size_t bytes = 0;
    uint32_t handle = 0;
    bool imported = false;
    bool device_local = false;
    // Copies retain identity, so a recycled native handle/VA cannot alias a stale allocation.
    std::shared_ptr<const int> identity;
};
struct Argument {
    const void* data;
    size_t bytes;
    bool captured_pointers = false;
    static Argument local(size_t bytes) { return {nullptr, bytes}; }
    template<class T> static Argument value(const T& v) {
        static_assert(std::is_trivially_copyable_v<T>);
        return {&v, sizeof(v)};
    }
    // Compiler captures may contain CPU pointers, including metadata-marked aggregate fields.
    static Argument capture(const void* data, size_t bytes) { return {data, bytes, true}; }
};
class Commands {
public:
    Commands() = default;
private:
    friend class Runtime;
    // Identity survives a Runtime so a stale handle cannot alias a later Runtime.
    std::shared_ptr<const int> owner_;
    size_t index_ = SIZE_MAX;
    uint64_t generation_ = 0;
};
class Queue {
public:
    Queue() = default;
    friend bool operator==(const Queue& a, const Queue& b) noexcept {
        return a.owner_ == b.owner_ && a.index_ == b.index_;
    }
    friend bool operator!=(const Queue& a, const Queue& b) noexcept { return !(a == b); }
private:
    friend class Runtime;
    std::shared_ptr<const int> owner_;
    size_t index_ = SIZE_MAX;
};
struct NativeCompletion;
class Completion {
public:
    Completion() = default;
    uint64_t sequence() const noexcept { return sequence_; }
    // Read-only observation of this exact point from worker/watchdog threads.
    // These calls never publish Runtime progress or retire resources. wait()
    // returns false on timeout; new calls on invalid/closing-runtime points throw.
    bool ready() const;
    bool wait(uint64_t timeout_ns = 5000000000ull) const;
private:
    friend class Runtime;
    std::shared_ptr<const int> owner_;
    size_t queue_ = SIZE_MAX;
    uint64_t sequence_ = 0;
    std::shared_ptr<NativeCompletion> observer_;
};
class HostGate {
public:
    HostGate() = default;
    // Idempotent release. May run on a callback
    // thread; keep a separate handle copy there and finish it before Runtime teardown.
    void signal() const;
private:
    friend class Runtime;
    struct State;
    std::shared_ptr<State> state_;
};
struct MemoryInfo {
    uint64_t total = 0, available = 0;
    // Windows clamps its process budget to global local-memory availability; Linux reports region availability,
    // which the kernel may restrict for unprivileged callers.
    bool process_budget = false;
    bool global_availability = false;
};
class Runtime {
public:
    // Linux: i915 render node (default /dev/dri/renderD128).
    // Windows: nullptr selects the single calibrated A770/driver combination.
    explicit Runtime(const char* render_node = nullptr);
    ~Runtime();
    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;
    Queue default_queue() const;
    Queue create_queue();
    // Experimental DG2 BCS: copy/fill and synchronization only; no compute kernels.
    Queue create_copy_queue();
    // Gates start unsignaled. Reusable gates record a GPU memory wait and keep
    // the same address across reset/replay; ordinary gates remain one-shot.
    HostGate create_gate(bool reusable = false);
    // Owner-only: every submitted use of this reusable gate must be observed complete.
    void reset_gate(const HostGate&);
    // Record a leading host dependency, before kernels/copies. Keep at most 16
    // distinct gates per list; frozen lists retain their gates until discard.
    void depend_on(const HostGate&);
    // Select a queue before recording; its scratch/private memory is isolated.
    // Requires an empty open recording; other frozen lists may be in flight.
    // With in_order=false, the caller records barriers between dependent work.
    // Explicit barriers, completion writes and required state workarounds remain.
    // device_parameters=false keeps indirect arguments host-backed for eager work.
    // true preserves the calibrated mirror policy for reusable recordings.
    void begin(const Queue&, bool in_order = true, bool device_parameters = true);
    Allocation allocate(size_t bytes);
    // Device-local GPU storage, without a CPU mapping. Transfer through GPU kernels.
    Allocation allocate_device(size_t bytes);
    // Mapped transfer storage with per-queue use tracking. Copy/fill retain its
    // residency; checked CPU access is allowed once its own uses are observed idle.
    Allocation allocate_staging(size_t bytes);
    // Borrow committed write-back read/write host pages; address and size must be 64 KiB aligned.
    // Keep the mapping alive and unchanged until all GPU use and this Runtime end.
    // Use the returned GPU VA for arguments; importing does not move pages into VRAM.
    Allocation import_host(void* data, size_t bytes);
    // Release a public allocation after all queues have owner-observed completion.
    // Discard lists that use it first. Imported pages remain owned by the caller;
    // internal pooled/staging allocations cannot be released through this API.
    // Copied/stale/foreign handles are invalid after release.
    void release(Allocation&);
    // Dynamic OS availability, capped by total minus this Runtime's local allocations.
    // Static region capacity/query layout is cached; no L0/Sysman is involved.
    MemoryInfo memory_info();
    // Convert a resident CPU mapping/interior pointer, or preserve an existing GPU VA. Null stays null.
    uint64_t gpu_address(const void* pointer) const;
    // Cached metadata: required 1D size, or the largest dividing power of two up to 256.
    uint32_t suggest_group_size(const std::string& zebin, const std::string& kernel, uint32_t global);
    // Bind a driver-baked src/drm/memory.cl module before recording memory operations.
    // Keep this native file unchanged for the Runtime's lifetime, like append's modules.
    // With local_thread_memory, initialize scratch/private pools on a separate
    // native queue and read back only their guards for explicit diagnostics.
    // local_isa uploads immutable kernel code into local VRAM; the memory
    // module stays host-backed to bootstrap those uploads without recursion.
    void set_memory_module(const std::string& zebin, bool local_thread_memory = true, bool local_isa = false);
    // Record asynchronous GPU transfers. Addresses and entire ranges must belong to this Runtime.
    // Exact self-copy is a no-op; partially overlapping copies are rejected.
    void copy(uint64_t destination, uint64_t source, size_t bytes);
    // Pitched spans are validated together; bounding spans must not overlap.
    void copy_2d(uint64_t destination, size_t destination_pitch, uint64_t source, size_t source_pitch,
                 size_t width, size_t height);
    // Repeat a captured power-of-two pattern (1..128 bytes), including a partial final pattern.
    void fill(uint64_t destination, const void* pattern, size_t pattern_bytes, size_t bytes);
    void barrier();
    // In-list completion: flush preceding writes, then store an aligned 64-bit value.
    // The caller owns/reset-orders the tag and retains its allocation and waiters.
    // Observing a tag does not retire the list's queue fence or backing storage.
    void signal_value(uint64_t address, uint64_t value);
    // GPU-side equality wait on an aligned 32-bit word, at this point in the list.
    void wait_value(uint64_t address, uint32_t value);
    // Ordered raw CS-domain ticks to an aligned registered 64-bit destination.
    // Observe queue completion before reading. Calibrate frequency/valid bits
    // externally for each driver/platform; this API does not return nanoseconds.
    void write_global_timestamp(uint64_t address);
    // Arguments are copied while recording; pointers contain GPU VAs, not CPU mappings.
    void append(const std::string& zebin, const std::string& kernel,
                uint32_t local_x, uint32_t groups_x, const std::vector<Argument>& arguments);
    // Hardware/OpenCL x,y,z order; SYCL range<3> uses the reverse z,y,x order.
    void append(const std::string& zebin, const std::string& kernel,
                std::array<uint32_t, 3> local, std::array<uint32_t, 3> groups,
                const std::vector<Argument>& arguments);
    void append(const std::string& zebin, const std::string& kernel,
                std::array<uint32_t, 3> local, std::array<uint32_t, 3> groups,
                const Argument* arguments, size_t argument_count);
    void append(const std::string& zebin, const std::string& kernel,
                uint32_t local_x, uint32_t groups_x, std::initializer_list<Argument> arguments) {
        append(zebin, kernel, {local_x, 1, 1}, {groups_x, 1, 1}, arguments.begin(), arguments.size());
    }
    void append(const std::string& zebin, const std::string& kernel,
                std::array<uint32_t, 3> local, std::array<uint32_t, 3> groups,
                std::initializer_list<Argument> arguments) {
        append(zebin, kernel, local, groups, arguments.begin(), arguments.size());
    }
    void close();
    // Close and freeze the current list, then start an empty recording. Handles
    // are borrowed from this Runtime; copies refer to the same immutable list.
    Commands finish();
    // Return a completed or never-submitted frozen list to the internal pools.
    // Invalidates every copied handle. Observe completion before discarding.
    void discard(Commands&);
    // Retained GPU allocation/import bytes, including idle internal pools.
    // This counts backing storage, not physical residency or an OS memory budget.
    uint64_t allocated_bytes() const;
    // Multiple different frozen lists may be in flight on the same ordered queue.
    // A list must complete before resubmitting it. Return the queue sequence.
    uint64_t submit();
    uint64_t submit(const Commands&);
    // Lists are bound to their recording queue. GPU dependencies refer to
    // already submitted immutable completion points; up to 16 per submission.
    Completion submit(const Commands&, const Queue&, std::initializer_list<Completion> waits = {});
    Completion submit(const Commands&, const Queue&, const std::vector<Completion>& waits);
    bool ready();
    bool ready(const Commands&);
    // One query collects completion progress for all earlier queue submissions.
    uint64_t completed();
    uint64_t completed(const Queue&);
    // Snapshot up to 16 queues without waiting. Duplicate queues share one query.
    // Linux uses one timeline ioctl for pending queues; no heap allocation.
    void completed(const Queue* queues, uint64_t* sequences, size_t count);
    bool ready(const Completion&);
    // A timeout keeps the list in flight. A subsequent wait may finish it.
    void wait(uint64_t timeout_ns = 5000000000ull);
    void wait(const Commands&, uint64_t timeout_ns = 5000000000ull);
    void wait(const Completion&, uint64_t timeout_ns = 5000000000ull);
    // Finish the list before recording another; allocations and compiled images stay valid.
    void reset();
    void cpu_access(const Allocation&, bool write);
    // Explicit diagnostic outside timed replay; requires an idle completed list.
    void check_scratch_guards();
    // Initialization-only: free obsolete unbound/unretained pools and descriptors.
    // Requires idle healthy queues and an empty open recording; preserves pool indices.
    uint64_t trim_thread_memory();
private:
    Allocation allocate_storage(size_t, bool device_local = false);
    void transfer_storage(uint64_t destination, uint64_t source, size_t bytes, uint8_t pattern = 0);
    size_t allocation_index(const Allocation&) const;
    bool storage_idle(const Allocation&) const;
    void storage_access(const Allocation&);
    void use_storage(const Allocation&);
    void use_address(uint64_t);
    std::pair<Allocation, uint32_t> capture(drm::detail::RecordingHeap&, size_t, bool device_local = false);
    std::pair<std::shared_ptr<const Allocation>, uint32_t> capture(
        drm::detail::CaptureHeap&, drm::detail::AllocationPool&, size_t);
    size_t command_index(const Commands&) const;
    size_t queue_index(const Queue&) const;
    size_t completion_index(const Completion&) const;
    void completed_queues(const size_t*, uint64_t*, size_t);
    uint64_t submit_recording(size_t, size_t, const Completion* = nullptr, size_t = 0);
    uint64_t completed_queue(size_t);
    void wait_sequence(size_t, uint64_t, uint64_t);
    struct State;
    std::unique_ptr<State> state_;
};
} // namespace orca::l0::dg2
