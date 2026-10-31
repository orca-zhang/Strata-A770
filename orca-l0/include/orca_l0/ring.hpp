#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <type_traits>

namespace orca::l0 {

// ponytail: one producer and one consumer; use per-slot sequences for multiple producers.
// A false return is backpressure. Neither endpoint allocates, blocks or overwrites a live slot.
template<class T, std::size_t Capacity>
class Ring {
    static_assert(Capacity >= 2 && (Capacity & (Capacity - 1)) == 0,
                  "ring capacity must be a power of two >= 2");
    static_assert(std::is_trivially_copyable_v<T> && std::is_default_constructible_v<T>);
    static_assert(std::atomic<std::size_t>::is_always_lock_free,
                  "ring requires lock-free machine-word atomics");
public:
    bool try_push(const T& item) noexcept {
        const auto write = write_.load(std::memory_order_relaxed);
        if (write - cached_read_ == Capacity) {
            cached_read_ = read_.load(std::memory_order_acquire);
            if (write - cached_read_ == Capacity) return false;
        }
        slots_[write & (Capacity - 1)] = item;
        write_.store(write + 1, std::memory_order_release);
        return true;
    }
    bool try_pop(T& item) noexcept {
        const auto read = read_.load(std::memory_order_relaxed);
        if (read == cached_write_) {
            cached_write_ = write_.load(std::memory_order_acquire);
            if (read == cached_write_) return false;
        }
        item = slots_[read & (Capacity - 1)];
        read_.store(read + 1, std::memory_order_release);
        return true;
    }
private:
    alignas(64) std::atomic<std::size_t> write_{0};
    // Each cached opposite cursor belongs only to its endpoint. Refresh on
    // full/empty, avoiding a shared-cache-line read for every ready item.
    std::size_t cached_read_ = 0;
    alignas(64) std::atomic<std::size_t> read_{0};
    std::size_t cached_write_ = 0;
    alignas(64) std::array<T, Capacity> slots_{};
};

} // namespace orca::l0
