#pragma once
#include <orca_queue.hpp>
namespace strata::core {
class L0QueueSync {
public:
    void init(l0::Queue* queue, const char* = "queue") { queue_ = queue; }
    bool active() const { return queue_ != nullptr; }
    bool wait(l0::Queue* queue, uint64_t timeout_ns = 20000000000ULL) {
        return !queue || queue->wait(timeout_ns);
    }
private:
    l0::Queue* queue_ = nullptr;
};
}
