#include "orca_queue.hpp"
#include <chrono>
#include <cstdio>

int main(int argc, char** argv) try {
    if (argc != 2) throw std::invalid_argument("usage: orca-timestamp-check image-directory");
    strata::l0::Runtime runtime;
    strata::l0::Program program(runtime, argv[1]);
    strata::l0::Queue queue(runtime, program);
    const auto clock = runtime.properties();
    if (!clock.timerResolution || !clock.timestampValidBits || clock.timestampValidBits > 64)
        throw std::runtime_error("invalid device clock properties");
    const uint64_t mask = clock.timestampValidBits == 64 ? UINT64_MAX : (uint64_t{1} << clock.timestampValidBits) - 1;
    auto* data = static_cast<unsigned char*>(runtime.allocate(4 * 1024 * 1024, strata::l0::Memory::host));
#ifdef STRATA_ORCA_DG2
    strata::l0::dg2_backend::HostEvent gate_storage(runtime);
    const auto gate = gate_storage.handle();
#else
    strata::l0::NativeEvent gate_storage(runtime);
    const auto gate = gate_storage.handle;
#endif
    struct ReleaseGate {
        strata::l0::EventHandle event;
        ~ReleaseGate() { try { strata::l0::signal_event(event); } catch (...) {} }
    } release_gate{gate}; // Release a failed check before the queue destructor drains it.
    for (auto kind : {strata::l0::Memory::host, strata::l0::Memory::device}) {
    auto* ticks = static_cast<uint64_t*>(runtime.allocate(16, kind));
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
            queue.memset(ticks, 0, 16).wait_and_throw();
            data[0] = data[4 * 1024 * 1024 - 1] = 0;
#ifdef STRATA_ORCA_DG2
            gate_storage.reset();
#else
            orca::l0::check(zeEventHostReset(gate), "reset timestamp gate");
            if (graph) orca::l0::check(zeEventHostReset(first.native()), "reset timestamp marker");
#endif
            const auto start = std::chrono::steady_clock::now();
            if (graph) queue.replay(graph); else append();
            first.wait_and_throw();
            const auto hold_start = std::chrono::steady_clock::now();
            std::this_thread::sleep_for(std::chrono::milliseconds(15));
            const double hold_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - hold_start).count();
            strata::l0::signal_event(gate);
            queue.wait_and_throw();
            const double wall_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
            uint64_t output[2]{};
            queue.memcpy(output, ticks, sizeof(output)).wait_and_throw();
            const double gpu_ms = double((output[1] - output[0]) & mask) * clock.timerResolution / 1e6;
            if (!output[0] || !output[1] || gpu_ms < 0.9 * hold_ms || gpu_ms > wall_ms + 1 ||
                data[0] != 0x5a || data[4 * 1024 * 1024 - 1] != 0x5a) {
                std::fprintf(stderr, "timestamp mismatch mode=%d replay=%d ticks=%llu,%llu gpu=%.6f hold=%.6f wall=%.6f bytes=%u,%u\n",
                    mode, replay, (unsigned long long)output[0], (unsigned long long)output[1], gpu_ms, hold_ms, wall_ms,
                    unsigned(data[0]), unsigned(data[4 * 1024 * 1024 - 1]));
                throw std::runtime_error("timestamp replay/order/scale mismatch");
            }
            std::printf("PASS memory=%s mode=%d replay=%d device_ms=%.6f wall_ms=%.6f period_ns=%.9f bits=%u\n",
                        kind == strata::l0::Memory::host ? "host" : "device", mode, replay,
                        gpu_ms, wall_ms, double(clock.timerResolution), clock.timestampValidBits);
        }
    }
#ifdef STRATA_ORCA_DG2
    uint64_t unregistered = 0;
    for (auto* bad : {static_cast<uint64_t*>(nullptr), reinterpret_cast<uint64_t*>(reinterpret_cast<uintptr_t>(ticks) + 1),
                      ticks + 2, &unregistered}) {
        bool rejected = false;
        try { queue.write_global_timestamp(bad); } catch (const std::invalid_argument&) { rejected = true; }
        if (!rejected) throw std::runtime_error("invalid timestamp destination accepted");
    }
#endif
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
