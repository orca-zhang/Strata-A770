#pragma once
#include <level_zero/ze_api.h>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace orca::l0 {

class Error : public std::runtime_error {
public:
    Error(ze_result_t result, const std::string& operation);
    const ze_result_t result;
};
void check(ze_result_t result, const char* operation);

// Borrowed handles: caller keeps the queue, closed list, context and all inputs alive.
// Flush the source runtime before replay and use native completion before returning to it.
void replay(ze_command_queue_handle_t queue, ze_command_list_handle_t recorded);
void replay(ze_command_list_handle_t immediate, ze_command_list_handle_t recorded);
// One driver call for ready closed lists. Caller supplies dependencies between
// lists and keeps every referenced resource alive until native completion.
// The handle array itself is needed only for this call; no allocation or host wait.
void replay(ze_command_queue_handle_t queue, std::size_t count,
            const ze_command_list_handle_t* recorded);
void replay(ze_command_list_handle_t immediate, std::size_t count,
            const ze_command_list_handle_t* recorded);
bool wait(ze_command_queue_handle_t queue, uint64_t timeout_ns = 30000000000ULL);
bool wait(ze_command_list_handle_t immediate, uint64_t timeout_ns = 30000000000ULL);

// Arguments are explicit C ABI values: no SYCL closure or compiler-generated argument layout.
template<class... Args>
void arguments(ze_kernel_handle_t kernel, const Args&... values) {
    static_assert((std::is_trivially_copyable_v<Args> && ...));
    uint32_t index = 0;
    (check(zeKernelSetArgumentValue(kernel, index++, sizeof(Args), &values),
           "zeKernelSetArgumentValue"), ...);
}

enum class Memory { device, host, shared };
// Relaxed lists require explicit barriers/events for every data dependency.
enum class Ordering { explicit_dependencies, in_order, relaxed };
class Commands;

// Single owner. Resources remain valid until Runtime destruction; it must outlive its users.
class Runtime {
public:
    explicit Runtime(uint32_t device_index = 0);
    ~Runtime();
    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;

    const ze_device_properties_t& properties() const noexcept { return properties_; }
    const ze_driver_properties_t& driver_properties() const noexcept { return driver_properties_; }
    const std::vector<ze_driver_extension_properties_t>& extensions() const noexcept { return extensions_; }
    ze_driver_handle_t native_driver() const noexcept { return driver_; }
    ze_context_handle_t native_context() const noexcept { return context_; }
    ze_device_handle_t native_device() const noexcept { return device_; }
    void* allocate(std::size_t bytes, Memory kind = Memory::device, std::size_t alignment = 64);
    // Caller must complete every use (including on other queues) before freeing.
    void deallocate(void* pointer);
    ze_module_handle_t module(const std::filesystem::path& file,
                             ze_module_format_t format = ZE_MODULE_FORMAT_IL_SPIRV,
                             const char* build_flags = "");
    // Compile once with the same effective flags as module(), then release the
    // temporary module. The result includes the driver's native ABI metadata.
    std::vector<uint8_t> compile_native(const std::filesystem::path& file, const char* build_flags = "");
    std::string binary_identity() const;
    ze_kernel_handle_t kernel(ze_module_handle_t module, const char* name);
    ze_event_handle_t event();
    // Regular lists replay on the compute queue. Immediate copy lists use a copy engine if available.
    Commands& commands(bool immediate = false, bool copy_engine = false, bool in_order = false);
    Commands& commands(Ordering order);
    // Reclaim a list after every native use has completed on every queue.
    // Borrowers must release their references first.
    void discard(Commands& commands);

private:
    friend class Commands;
    void release() noexcept;
    ze_context_handle_t context_ = nullptr;
    ze_driver_handle_t driver_ = nullptr;
    ze_device_handle_t device_ = nullptr;
    ze_command_queue_handle_t queue_ = nullptr;
    uint32_t compute_ordinal_ = 0, copy_ordinal_ = 0;
    std::size_t compute_fill_limit_ = 0, copy_fill_limit_ = 0;
    ze_device_properties_t properties_{};
    ze_driver_properties_t driver_properties_{};
    std::vector<ze_driver_extension_properties_t> extensions_;
    std::vector<void*> allocations_;
    std::vector<ze_module_handle_t> modules_;
    std::vector<ze_kernel_handle_t> kernels_;
    std::vector<ze_event_pool_handle_t> pools_;
    std::vector<ze_event_handle_t> events_;
    std::vector<std::unique_ptr<Commands>> lists_;
};

class Commands {
public:
    ~Commands();
    Commands(const Commands&) = delete;
    Commands& operator=(const Commands&) = delete;

    void copy(void* dst, const void* src, std::size_t bytes,
              ze_event_handle_t signal = nullptr,
              std::initializer_list<ze_event_handle_t> waits = {});
    void copy_2d(void* dst, std::size_t dst_pitch, const void* src, std::size_t src_pitch,
                 std::size_t width, std::size_t height);
    void fill(void* dst, const void* pattern, std::size_t pattern_bytes, std::size_t bytes);
    std::size_t max_fill_pattern() const noexcept { return fill_limit_; }
    void barrier(ze_event_handle_t signal = nullptr,
                 std::initializer_list<ze_event_handle_t> waits = {});
    // Reset in command order, including on every recorded-list replay.
    void reset_event(ze_event_handle_t event);
    void launch(ze_kernel_handle_t kernel, ze_group_count_t groups,
                ze_group_count_t local, ze_event_handle_t signal = nullptr,
                std::initializer_list<ze_event_handle_t> waits = {});
    void close();
    void submit();
    // A timeout leaves the list in flight. Never reset/free its inputs until a successful wait.
    bool wait(uint64_t timeout_ns = 30000000000ULL);
    bool ready();
    void reset();
    ze_command_list_handle_t native_handle() const noexcept { return list_; }

private:
    friend class Runtime;
    Commands(Runtime& runtime, bool immediate, bool copy_engine, Ordering order);
    void recording() const;
    Runtime& runtime_;
    ze_command_list_handle_t list_ = nullptr;
    ze_fence_handle_t fence_ = nullptr;
    bool immediate_ = false, closed_ = false, in_flight_ = false;
    std::size_t fill_limit_ = 0;
};

// ponytail: a checked bump allocator; reclaim the entire arena with its Runtime, not individual slices.
class Arena {
public:
    Arena(Runtime& runtime, std::size_t bytes);
    void* take(std::size_t bytes, std::size_t alignment = 64);
    std::size_t used() const noexcept { return used_; }
private:
    std::byte* base_;
    std::size_t size_, used_ = 0;
};

} // namespace orca::l0
