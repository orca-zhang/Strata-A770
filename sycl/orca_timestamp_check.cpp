#include "orca_queue.hpp"
#include <chrono>
#include <cstdio>

int main(int argc, char** argv) try {
    if (argc != 2) throw std::invalid_argument("usage: orca-timestamp-check image-directory");
    orca::l0::Runtime runtime;
    orca::l0::compiler::Program program(runtime, argv[1]);
    strata::l0::Queue queue(runtime, program);
    const auto& clock = runtime.properties();
    if (!clock.timerResolution || !clock.timestampValidBits || clock.timestampValidBits > 64)
        throw std::runtime_error("invalid device clock properties");
    const uint64_t mask = clock.timestampValidBits == 64 ? UINT64_MAX : (uint64_t{1} << clock.timestampValidBits) - 1;
    auto* ticks = static_cast<uint64_t*>(runtime.allocate(16, orca::l0::Memory::host));
    auto* data = static_cast<unsigned char*>(runtime.allocate(4 * 1024 * 1024, orca::l0::Memory::host));
    const auto gate = runtime.event();
    struct ReleaseGate {
        ze_event_handle_t event;
        ~ReleaseGate() { zeEventHostSignal(event); }
    } release_gate{gate}; // Release a failed check before the queue destructor drains it.
    strata::l0::Event first;
    auto append = [&] {
        queue.write_global_timestamp(ticks);
        first = queue.ext_oneapi_submit_barrier();
        queue.depends_on(strata::l0::Event::borrow(gate));
        queue.fill(data, static_cast<unsigned char>(0x5a), 4 * 1024 * 1024);
        queue.write_global_timestamp(ticks + 1);
    };
    for (int mode = 0; mode != 3; ++mode) {
        std::shared_ptr<strata::l0::NativeGraph> graph;
        if (mode) {
            queue.begin_recording(mode == 2);
            append();
            graph = queue.end_recording();
        }
        for (int replay = 0; replay != 2; ++replay) {
            ticks[0] = ticks[1] = 0;
            data[0] = data[4 * 1024 * 1024 - 1] = 0;
            orca::l0::check(zeEventHostReset(gate), "reset timestamp gate");
            if (graph) orca::l0::check(zeEventHostReset(first.native()), "reset timestamp marker");
            const auto start = std::chrono::steady_clock::now();
            if (graph) queue.replay(graph); else append();
            first.wait_and_throw();
            const auto hold_start = std::chrono::steady_clock::now();
            std::this_thread::sleep_for(std::chrono::milliseconds(15));
            const double hold_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - hold_start).count();
            orca::l0::check(zeEventHostSignal(gate), "release timestamp gate");
            queue.wait_and_throw();
            const double wall_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
            const double gpu_ms = double((ticks[1] - ticks[0]) & mask) * clock.timerResolution / 1e6;
            if (!ticks[0] || !ticks[1] || gpu_ms < 0.9 * hold_ms || gpu_ms > wall_ms + 1 ||
                data[0] != 0x5a || data[4 * 1024 * 1024 - 1] != 0x5a)
                throw std::runtime_error("timestamp replay/order/scale mismatch");
            std::printf("PASS mode=%d replay=%d device_ms=%.6f wall_ms=%.6f period_ns=%llu bits=%u\n",
                        mode, replay, gpu_ms, wall_ms, static_cast<unsigned long long>(clock.timerResolution), clock.timestampValidBits);
        }
    }
    // Exercise the same wrap subtraction independently of today's counter position.
    if (((uint64_t{3} - uint64_t{254}) & uint64_t{255}) != 5)
        throw std::runtime_error("timestamp wrap arithmetic mismatch");
    bool rejected = false;
    try { queue.write_global_timestamp(nullptr); } catch (const std::invalid_argument&) { rejected = true; }
    if (!rejected) throw std::runtime_error("null timestamp destination accepted");
    return 0;
} catch (const std::exception& e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
}
