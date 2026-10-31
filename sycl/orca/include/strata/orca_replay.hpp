#pragma once
#include <dpct/dpct.hpp>
#include "strata/l0_sync.hpp"
namespace strata::core {
using VerifyGraph = dpct::experimental::Graph;
// This query describes the legacy SYCL-export bridge. Native queues always use
// Orca, including eager execution and graphs containing device event waits.
inline bool orca_replay_enabled() { return false; }
inline void check_orca_replay(l0::Queue&) {}
inline std::nullptr_t record_orca_handle(l0::Queue&) { return nullptr; }
inline void bind_orca_handle(const VerifyGraph*, std::nullptr_t) {}
inline void forget_orca_handle(const VerifyGraph*) {}
inline void submit_verify_graph(l0::Queue& queue, VerifyGraph& graph) { queue.ext_oneapi_graph(graph); }
inline bool wait_verify_queue(l0::Queue* queue, L0QueueSync& sync,
                             uint64_t timeout_ns = 20000000000ULL) { return sync.wait(queue, timeout_ns); }
inline bool wait_verify_queue(l0::Queue* queue) { return !queue || queue->wait(); }
inline bool verify_queue_empty(l0::Queue* queue) { return !queue || queue->ext_oneapi_empty(); }
}
