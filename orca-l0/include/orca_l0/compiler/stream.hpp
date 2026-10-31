#pragma once
#include <orca_l0/compiler/sycl.hpp>
#include <utility>

namespace orca::l0::compiler {

// One submitter. Eager operations and recorded graphs use the same native
// stream, context, allocations and program; there is no runtime handoff.
class Stream {
public:
    Stream(Runtime& runtime, Program& program, bool copy_engine = false)
        : runtime_(runtime), program_(program), immediate_(runtime.commands(true, copy_engine, true)) {}
    ~Stream() {
        // Return completed queue resources to Runtime. On a driver error Runtime
        // keeps the handle for its final drain/cleanup.
        try { if (immediate_.wait(UINT64_MAX)) runtime_.discard(immediate_); } catch (...) {}
    }
    Stream(const Stream&) = delete;
    Stream& operator=(const Stream&) = delete;

    template<class Name = void, int Dimensions, class... Args>
    void parallel_for(sycl::nd_range<Dimensions> range, Args&&... arguments) {
        compiler::parallel_for<Name>(program_, target(), range, std::forward<Args>(arguments)...);
    }
    template<class Name = void, class... Args>
    void parallel_for(sycl::range<1> range, Args&&... arguments) {
        compiler::parallel_for<Name>(program_, target(), range, std::forward<Args>(arguments)...);
    }
    template<class F> void submit(F&& body) { std::forward<F>(body)(*this); }

    void copy(void* destination, const void* source, std::size_t bytes,
              ze_event_handle_t signal = nullptr,
              std::initializer_list<ze_event_handle_t> waits = {}) {
        target().copy(destination, source, bytes, signal, waits);
    }
    void barrier(ze_event_handle_t signal = nullptr,
                 std::initializer_list<ze_event_handle_t> waits = {}) {
        target().barrier(signal, waits);
    }
    void copy_2d(void* destination, std::size_t dst_pitch, const void* source, std::size_t src_pitch,
                 std::size_t width, std::size_t height) {
        target().copy_2d(destination, dst_pitch, source, src_pitch, width, height);
    }
    void fill(void* destination, const void* pattern, std::size_t pattern_bytes, std::size_t bytes) {
        target().fill(destination, pattern, pattern_bytes, bytes);
    }
    std::size_t max_fill_pattern() const noexcept {
        return (recording_ ? *recording_ : immediate_).max_fill_pattern();
    }
    void reset_event(ze_event_handle_t event) { target().reset_event(event); }
    // Raw device ticks; convert differences with Runtime::properties().timerResolution.
    void write_global_timestamp(uint64_t* destination) {
        if (!destination || reinterpret_cast<uintptr_t>(destination) % alignof(uint64_t))
            throw std::invalid_argument("invalid native timestamp destination");
        check(zeCommandListAppendWriteGlobalTimestamp(target().native_handle(), destination, nullptr, 0, nullptr),
              "zeCommandListAppendWriteGlobalTimestamp");
    }
    void begin_recording(Ordering order = Ordering::in_order) {
        if (recording_) throw std::logic_error("native stream is already recording");
        recording_ = &runtime_.commands(order);
    }
    Commands& end_recording() {
        if (!recording_) throw std::logic_error("native stream is not recording");
        recording_->close();
        auto& commands = *recording_;
        recording_ = nullptr;
        return commands;
    }
    void replay(const Commands& commands) {
        if (recording_) throw std::logic_error("cannot replay while native stream is recording");
        l0::replay(immediate_.native_handle(), commands.native_handle());
    }
    bool wait(uint64_t timeout_ns = 30000000000ULL) {
        if (recording_) throw std::logic_error("cannot wait while native stream is recording");
        return immediate_.wait(timeout_ns);
    }
    bool ready() { return wait(0); }
    ze_command_list_handle_t native_handle() const noexcept { return immediate_.native_handle(); }
private:
    Commands& target() { return recording_ ? *recording_ : immediate_; }
    Runtime& runtime_;
    Program& program_;
    Commands& immediate_;
    Commands* recording_ = nullptr;
};

} // namespace orca::l0::compiler
