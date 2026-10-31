// Standalone check of the same SYCL/Orca boundary used by the real verifier.
#include "strata/orca_replay.hpp"
#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>

int main() try {
    using namespace strata::core;
    namespace exp = sycl::ext::oneapi::experimental;
    sycl::queue q(sycl::gpu_selector_v, sycl::property::queue::in_order{});
    check_orca_replay(q);
    constexpr size_t n = 257;
    auto* data = sycl::malloc_device<float>(n, q);
    if (!data) throw std::bad_alloc();
    // Two different graphs mirror the verify/commit alternation. Keep graphs alive until the queue drains.
    {
        exp::command_graph first(q.get_context(), q.get_device());
        first.begin_recording(q);
        for (int i = 0; i < 8; ++i)
            q.parallel_for(sycl::range<1>(n), [=](sycl::id<1> j) { data[j] += 1.0f; });
        auto window_handle = record_orca_handle(q);
        first.end_recording(q);
        auto window = first.finalize();
        bind_orca_handle(&window, window_handle);
        exp::command_graph second(q.get_context(), q.get_device());
        second.begin_recording(q);
        q.parallel_for(sycl::range<1>(n), [=](sycl::id<1> j) { data[j] *= 2.0f; });
        auto commit_handle = record_orca_handle(q);
        second.end_recording(q);
        auto commit = second.finalize();
        bind_orca_handle(&commit, commit_handle);
        L0QueueSync sync;
        sync.init(&q, "orca-replay-check");
        std::array<float, n> host;
        for (int round = 0; round < 32; ++round) {
            q.fill(data, float(round), n); // Deliberately pending SYCL work before native replay.
            submit_verify_graph(q, window);
            if (!wait_verify_queue(&q, sync)) throw std::runtime_error("window timeout");
            submit_verify_graph(q, commit);
            if (!wait_verify_queue(&q, sync)) throw std::runtime_error("commit timeout");
            if (!verify_queue_empty(&q)) throw std::runtime_error("completed queue still busy");
            q.memcpy(host.data(), data, sizeof(host)).wait_and_throw(); // Return to SYCL after native wait.
            if (!std::all_of(host.begin(), host.end(), [=](float v) { return v == (round + 8) * 2.0f; }))
                throw std::runtime_error("SYCL/Orca output mismatch");
        }
        forget_orca_handle(&commit);
        forget_orca_handle(&window);
    }
    sycl::free(data, q);
    std::cout << "PASS 32 alternating window/commit replays; pending SYCL input, exact output, native completion\n";
} catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
}
