#pragma once
#include <sycl/sycl.hpp>
#include <dpct/dpct.hpp>
#include <variant>
#include <cstdlib>
#include <cstdio>

#if defined(STRATA_HAS_LEVEL_ZERO_EVENTS)
#include <level_zero/ze_api.h>
#include <sycl/ext/oneapi/backend/level_zero.hpp>
#endif

namespace strata::core {

class L0QueueSync {
public:
    void init(sycl::queue* q, const char* label = "queue") {
#if defined(STRATA_HAS_LEVEL_ZERO_EVENTS)
        static const bool enabled = [] {
            const char* v = std::getenv("STRATA_L0_NATIVE_SYNC");
            return v == nullptr || std::atoi(v) != 0;
        }();
        enabled_ = enabled;
        cmd_list_ = nullptr;
        cmd_queue_ = nullptr;
        if (!enabled_ || !q || q->get_backend() != sycl::backend::ext_oneapi_level_zero) {
            return;
        }
        try {
            auto nq = sycl::get_native<sycl::backend::ext_oneapi_level_zero>(*q);
            if (std::holds_alternative<ze_command_list_handle_t>(nq)) {
                cmd_list_ = std::get<ze_command_list_handle_t>(nq);
            } else if (std::holds_alternative<ze_command_queue_handle_t>(nq)) {
                cmd_queue_ = std::get<ze_command_queue_handle_t>(nq);
            }
            if (active()) {
                static bool logged = false;
                if (!logged) {
                    std::fprintf(stderr, "strata: Level Zero native synchronization active for %s (%s)\n",
                                 label, cmd_list_ ? "immediate command list" : "command queue");
                    logged = true;
                }
            }
        } catch (...) {
            cmd_list_ = nullptr;
            cmd_queue_ = nullptr;
        }
#else
        (void) q;
        (void) label;
#endif
    }

    bool active() const {
#if defined(STRATA_HAS_LEVEL_ZERO_EVENTS)
        return enabled_ && (cmd_list_ != nullptr || cmd_queue_ != nullptr);
#else
        return false;
#endif
    }

    bool wait(sycl::queue* q, uint64_t timeout_ns = 20000000000ULL) {
#if defined(STRATA_HAS_LEVEL_ZERO_EVENTS)
        if (active()) {
            ze_result_t zr;
            if (cmd_list_) {
                zr = zeCommandListHostSynchronize((ze_command_list_handle_t) cmd_list_, timeout_ns);
            } else {
                zr = zeCommandQueueSynchronize((ze_command_queue_handle_t) cmd_queue_, timeout_ns);
            }
            return (zr == ZE_RESULT_SUCCESS);
        }
#endif
        if (!q) return true;
        try {
            q->wait();
            return true;
        } catch (...) {
            return false;
        }
    }

private:
#if defined(STRATA_HAS_LEVEL_ZERO_EVENTS)
    bool enabled_ = false;
    void* cmd_list_ = nullptr;
    void* cmd_queue_ = nullptr;
#endif
};

}  // namespace strata::core

