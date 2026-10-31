#pragma once
#include "strata/l0_sync.hpp"
#include <sycl/ext/oneapi/experimental/graph.hpp>
#include <map>
#include <memory>
#include <mutex>

#if STRATA_SYCL_ORCA_L0
#include <orca_l0/runtime.hpp>
#endif

namespace strata::core {

using VerifyGraph = sycl::ext::oneapi::experimental::command_graph<
    sycl::ext::oneapi::experimental::graph_state::executable>;

#if STRATA_SYCL_ORCA_L0
inline std::mutex orca_graph_mutex;
inline std::map<const VerifyGraph*, ze_command_list_handle_t> orca_graph_handles;
using OrcaGraphHandle = std::shared_ptr<ze_command_list_handle_t>;
#else
using OrcaGraphHandle = std::shared_ptr<void*>;
#endif

inline bool orca_replay_enabled() {
    static const bool enabled = [] {
        const char* v = std::getenv("STRATA_ORCA_L0");
        return v && std::atoi(v) != 0;
    }();
    return enabled;
}

inline void check_orca_replay(sycl::queue& q) {
    if (!orca_replay_enabled()) return;
#if STRATA_SYCL_ORCA_L0
    if (q.get_backend() != sycl::backend::ext_oneapi_level_zero || !q.is_in_order())
        throw sycl::exception(sycl::make_error_code(sycl::errc::invalid),
                              "Orca L0 replay requires an in-order Level Zero queue");
    // Other device families may split graph copies onto a private UR copy list, which this bridge cannot export.
    if (q.get_device().get_info<sycl::info::device::name>().find("A770") == std::string::npos)
        throw sycl::exception(sycl::make_error_code(sycl::errc::feature_not_supported),
                              "Experimental Orca graph export currently supports Arc A770 only");
    // Export only the self-contained in-order compute list: older UR graphs also need private reset/copy lists.
    for (const char* name : {"UR_L0_USE_IMMEDIATE_COMMANDLISTS", "UR_L0_CMD_BUFFER_USE_IMMEDIATE_APPEND_PATH",
                             "UR_L0_USE_DRIVER_INORDER_LISTS", "UR_L0_USE_DRIVER_COUNTER_BASED_EVENTS"}) {
        const char* value = std::getenv(name);
        if (!value || std::atoi(value) != 1)
            throw sycl::exception(sycl::make_error_code(sycl::errc::invalid),
                                  std::string("Orca L0 graph export requires ") + name + "=1 before process startup");
    }
#else
    (void)q;
    throw sycl::exception(sycl::make_error_code(sycl::errc::feature_not_supported),
                          "STRATA_ORCA_L0=1 requires -DSTRATA_SYCL_ORCA_L0=ON");
#endif
}

inline OrcaGraphHandle record_orca_handle(sycl::queue& q) {
    if (!orca_replay_enabled()) return {};
    check_orca_replay(q);
#if STRATA_SYCL_ORCA_L0
    auto handle = std::make_shared<ze_command_list_handle_t>(nullptr);
    q.submit([=](sycl::handler& cgh) {
        cgh.ext_codeplay_enqueue_native_command([=](sycl::interop_handle ih) {
            *handle = ih.ext_codeplay_get_native_graph<sycl::backend::ext_oneapi_level_zero>();
        });
    });
    return handle;
#else
    return {};
#endif
}

inline void bind_orca_handle(const VerifyGraph* graph, const OrcaGraphHandle& handle) {
#if STRATA_SYCL_ORCA_L0
    if (!handle) return;
    if (!*handle) throw sycl::exception(sycl::make_error_code(sycl::errc::runtime), "Orca graph handle unavailable");
    std::lock_guard<std::mutex> lock(orca_graph_mutex);
    orca_graph_handles[graph] = *handle;
#else
    (void)graph; (void)handle;
#endif
}

inline void forget_orca_handle(const VerifyGraph* graph) {
#if STRATA_SYCL_ORCA_L0
    std::lock_guard<std::mutex> lock(orca_graph_mutex);
    orca_graph_handles.erase(graph);
#else
    (void)graph;
#endif
}

#if STRATA_SYCL_ORCA_L0
template<class F>
inline auto orca_native_queue(sycl::queue& q, F operation) {
    try {
        return std::visit(operation, sycl::get_native<sycl::backend::ext_oneapi_level_zero>(q));
    } catch (const orca::l0::Error& e) {
        throw sycl::exception(sycl::make_error_code(sycl::errc::runtime), e.what());
    }
}
#endif

inline void submit_verify_graph(sycl::queue& q, VerifyGraph& graph) {
    if (!orca_replay_enabled()) { q.ext_oneapi_graph(graph); return; }
#if STRATA_SYCL_ORCA_L0
    // ponytail: host-drain pending SYCL work on the SAME queue/context; native events can restore overlap in a full backend.
    // The verifier must use wait_verify_queue/verify_queue_empty until this replay completes.
    q.wait_and_throw();
    ze_command_list_handle_t list;
    {
        // ponytail: a mutex registry preserves the frozen Verifier ABI; store handles in Verifier on a full rebuild to remove this lock.
        std::lock_guard<std::mutex> lock(orca_graph_mutex);
        const auto it = orca_graph_handles.find(&graph);
        if (it == orca_graph_handles.end())
            throw sycl::exception(sycl::make_error_code(sycl::errc::invalid), "Orca graph was not registered");
        list = it->second;
    }
    orca_native_queue(q, [=](auto target) { orca::l0::replay(target, list); });
    static const bool logged = [] {
        std::fprintf(stderr, "strata: Orca L0 native graph replay active (SYCL still records kernels)\n");
        return true;
    }();
    (void)logged;
#else
    check_orca_replay(q);
#endif
}

inline bool wait_verify_queue(sycl::queue* q, L0QueueSync& sync,
                              uint64_t timeout_ns = 20000000000ULL) {
#if STRATA_SYCL_ORCA_L0
    if (q && orca_replay_enabled())
        return orca_native_queue(*q, [=](auto target) { return orca::l0::wait(target, timeout_ns); });
#endif
    return sync.wait(q, timeout_ns);
}

inline bool wait_verify_queue(sycl::queue* q) {
#if STRATA_SYCL_ORCA_L0
    if (orca_replay_enabled())
        return orca_native_queue(*q, [](auto target) { return orca::l0::wait(target); });
#endif
    q->wait_and_throw();
    return true;
}

inline bool verify_queue_empty(sycl::queue* q) {
#if STRATA_SYCL_ORCA_L0
    if (orca_replay_enabled())
        return orca_native_queue(*q, [](auto target) { return orca::l0::wait(target, 0); });
#endif
    return q->ext_oneapi_empty();
}

} // namespace strata::core
