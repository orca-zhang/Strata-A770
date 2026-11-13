#include <orca_l0/runtime.hpp>
#include <orca_l0/ring.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <future>
#include <iostream>
#include <limits>
#include <string>
#include <thread>

using namespace orca::l0;
using Clock = std::chrono::steady_clock;

static void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
template<class Exception, class F> static void rejects(F action) {
    try { action(); } catch (const Exception&) { return; }
    throw std::runtime_error("expected rejection was missing");
}
static double micros(Clock::time_point begin, Clock::time_point end) {
    return std::chrono::duration<double, std::micro>(end - begin).count();
}

int main(int argc, char** argv) try {
    if (argc == 2 && std::string(argv[1]) == "--allocations") {
        if (const char* poison = std::getenv("ORCA_L0_POISON_ALLOCATIONS"); poison && std::string(poison) == "1")
            throw std::invalid_argument("concurrent allocation check requires poisoning disabled");
        Runtime rt;
        auto allocations = [&](Memory kind) {
            for (size_t i = 0; i < 128; ++i) {
                auto* p = rt.allocate(16384 + (i % 8) * 4096, kind);
                rt.deallocate(p);
            }
        };
        auto host = std::async(std::launch::async, allocations, Memory::host);
        auto device = std::async(std::launch::async, allocations, Memory::device);
        host.get(); device.get();
        std::cout << "PASS concurrent host/device allocation ownership\n";
        return 0;
    }
    if (argc < 2 || argc > 3 || (argc == 3 && std::string(argv[2]) != "native"))
        throw std::invalid_argument("usage: orca-l0-check module_file [native]");
    Runtime rt;
    std::cout << "Device: " << rt.properties().name << " (device ID " << rt.properties().deviceId << ")\n";
    rejects<std::out_of_range>([] { Runtime missing(UINT32_MAX); });
    rejects<std::invalid_argument>([&] { rt.allocate(0); });
    rejects<std::invalid_argument>([&] { rt.allocate(16, Memory::device, 3); });
    auto* temporary = rt.allocate(64);
    rt.deallocate(temporary);
    rt.deallocate(nullptr);
    rejects<std::invalid_argument>([&] { rt.deallocate(temporary); });
    if (const char* poison = std::getenv("ORCA_L0_POISON_ALLOCATIONS"); poison && std::string(poison) == "1") {
        constexpr size_t count = 67; // Include the bytes after the last complete float.
        auto* readback = static_cast<uint8_t*>(rt.allocate(count, Memory::host));
        for (const auto kind : {Memory::device, Memory::host, Memory::shared}) {
            auto* data = rt.allocate(count, kind);
            std::fill(readback, readback + count, uint8_t{0});
            auto& copy = rt.commands(true, false, true);
            copy.copy(readback, data, count);
            require(copy.wait(), "allocation poison readback timeout");
            require(std::all_of(readback, readback + count, [](uint8_t value) { return value == 0xff; }),
                    "allocation poison missed bytes");
            rt.discard(copy);
            rt.deallocate(data);
        }
        rt.deallocate(readback);
        std::cout << "PASS allocation poison: device/host/shared and odd byte count\n";
    }
    rejects<std::runtime_error>([&] { rt.module(std::filesystem::path(argv[1]) / "missing"); });
    auto module = rt.module(argv[1], argc == 3 ? ZE_MODULE_FORMAT_NATIVE : ZE_MODULE_FORMAT_IL_SPIRV);
    rejects<Error>([&] { rt.kernel(module, "kernel_does_not_exist"); });
    Arena arena(rt, 1024 * 1024);
    auto* odd = arena.take(3);
    (void)odd;
    auto* aligned = arena.take(7, 256);
    require(reinterpret_cast<std::uintptr_t>(aligned) % 256 == 0, "arena absolute alignment");
    auto used = arena.used();
    rejects<std::bad_alloc>([&] { arena.take(std::numeric_limits<std::size_t>::max()); });
    require(arena.used() == used, "failed arena request consumed capacity");
    rejects<std::invalid_argument>([&] { arena.take(1, 3); });

    constexpr uint32_t width = 67, rows = 41;
    struct Parameters { uint32_t width, rows; float scale, epsilon; };
    static_assert(sizeof(Parameters) == 16);
    auto* params = static_cast<Parameters*>(rt.allocate(sizeof(Parameters), Memory::host));
    *params = {width, rows, 1.0f, 1e-5f};
    auto* device_params = static_cast<Parameters*>(arena.take(sizeof(Parameters)));
    auto* input = static_cast<float*>(rt.allocate(width * sizeof(float), Memory::host));
    auto* gamma = static_cast<float*>(rt.allocate(width * sizeof(float), Memory::host));
    auto* matrix = static_cast<float*>(rt.allocate(2 * rows * width * sizeof(float), Memory::host));
    auto* result = static_cast<float*>(rt.allocate(rows * sizeof(float), Memory::host));
    auto* d_input = static_cast<float*>(arena.take(width * sizeof(float)));
    auto* d_gamma = static_cast<float*>(arena.take(width * sizeof(float)));
    auto* norm = static_cast<float*>(arena.take(width * sizeof(float)));
    auto* d_matrix = static_cast<float*>(arena.take(2 * rows * width * sizeof(float)));
    auto* projections = static_cast<float*>(arena.take(2 * rows * sizeof(float)));
    auto* d_result = static_cast<float*>(arena.take(rows * sizeof(float)));
    float sum = 0;
    for (uint32_t i = 0; i < width; ++i) {
        input[i] = static_cast<float>(static_cast<int>(i % 17) - 8) / 8;
        gamma[i] = 0.5f + static_cast<float>(i % 7) / 8;
        sum += input[i] * input[i];
    }
    for (uint32_t i = 0; i < 2 * rows * width; ++i)
        matrix[i] = static_cast<float>(static_cast<int>(i % 19) - 9) / 64;

    auto& upload = rt.commands(true, true);
    upload.copy(d_input, input, width * sizeof(float));
    upload.copy(d_gamma, gamma, width * sizeof(float));
    upload.copy(d_matrix, matrix, 2 * rows * width * sizeof(float));
    require(upload.wait(), "initial upload timeout");
    const auto uploaded = rt.event();
    const auto output_ready = rt.event();
    const auto norm_kernel = rt.kernel(module, "rms_norm");
    const auto projection_kernel = rt.kernel(module, "gate_up");
    const auto activation_kernel = rt.kernel(module, "swiglu");
    arguments(norm_kernel, d_input, d_gamma, norm, device_params);
    arguments(projection_kernel, d_matrix, norm, projections, device_params);
    arguments(activation_kernel, projections, d_result, device_params);
    auto& inference = rt.commands();
    inference.barrier(nullptr, {uploaded});
    inference.launch(norm_kernel, {1, 1, 1}, {64, 1, 1});
    inference.barrier();
    inference.launch(projection_kernel, {(2 * rows + 31) / 32, 1, 1}, {32, 1, 1});
    inference.barrier();
    inference.launch(activation_kernel, {(rows + 31) / 32, 1, 1}, {32, 1, 1});
    inference.barrier();
    inference.copy(result, d_result, rows * sizeof(float), output_ready);
    inference.close();
    rejects<std::logic_error>([&] { inference.barrier(); });
    rejects<std::logic_error>([&] { upload.close(); });
    for (int step = 0; step < 3; ++step) {
        params->scale = 1.0f + 0.25f * step;
        check(zeEventHostReset(uploaded), "upload event reset");
        check(zeEventHostReset(output_ready), "readback event reset");
        upload.copy(device_params, params, sizeof(Parameters));
        upload.barrier(uploaded);
        inference.submit();
        require(inference.wait(), "inference timeout");
        check(zeEventHostSynchronize(output_ready, 30000000000ULL), "readback event wait");
        require(upload.wait(), "parameter upload timeout");
        const float factor = params->scale / std::sqrt(sum / width + params->epsilon);
        float error = 0;
        for (uint32_t row = 0; row < rows; ++row) {
            float gate = 0, up = 0;
            for (uint32_t i = 0; i < width; ++i) {
                const float x = input[i] * factor * gamma[i];
                gate += matrix[row * width + i] * x;
                up += matrix[(rows + row) * width + i] * x;
            }
            const float reference = gate / (1.0f + std::exp(-gate)) * up;
            require(std::isfinite(result[row]), "nonfinite inference result");
            error = std::max(error, std::abs(reference - result[row]));
        }
        require(error < 2e-5f, "RMSNorm/GEMV/SwiGLU parity failed");
        std::cout << "Replay " << step << " max_abs_error=" << error << '\n';
    }
    std::cout << "PASS device arena, explicit ABI, copy/compute event dependency, dynamic parameter replay\n";

    // Signal the gate even when a check throws, before Runtime's draining destructor runs.
    const auto gate = rt.event();
    struct SignalOnExit {
        ze_event_handle_t event;
        ~SignalOnExit() { zeEventHostSignal(event); }
    } signal_on_exit{gate};
    auto& blocked = rt.commands();
    blocked.barrier(nullptr, {gate});
    blocked.close();
    blocked.submit();
    require(!blocked.ready() && !blocked.wait(1000), "unsignaled gate did not preserve timeout");
    rejects<std::logic_error>([&] { blocked.reset(); });
    rejects<std::logic_error>([&] { blocked.submit(); });
    check(zeEventHostSignal(gate), "host gate signal");
    require(blocked.wait(), "host-gated list did not complete");
    blocked.reset();
    blocked.close();
    blocked.submit();
    require(blocked.wait(), "reset list did not complete");
    std::cout << "PASS host event, timeout, in-flight resubmit/reset rejection, list reset\n";

    constexpr uint32_t elements = 64, launches = 400;
    auto* values = static_cast<float*>(rt.allocate(elements * sizeof(float), Memory::host));
    auto* device_values = static_cast<float*>(arena.take(elements * sizeof(float)));
    for (uint32_t i = 0; i < elements; ++i) values[i] = 0;
    upload.copy(device_values, values, elements * sizeof(float));
    require(upload.wait(), "benchmark initialization timeout");
    const auto advance = rt.kernel(module, "advance");
    arguments(advance, device_values, uint32_t{1});
    auto& burst = rt.commands();
    const auto record_start = Clock::now();
    for (uint32_t i = 0; i < launches; ++i) {
        burst.launch(advance, {1, 1, 1}, {elements, 1, 1});
        burst.barrier();
    }
    burst.close();
    const auto record_end = Clock::now();
    // Already recorded dispatches must retain delta=1, not silently change to delta=99.
    arguments(advance, device_values, uint32_t{99});
    for (int i = 0; i < 3; ++i) { burst.submit(); require(burst.wait(), "warmup timeout"); }
    constexpr int repeats = 30;
    double submit_us = 0, complete_us = 0;
    for (int i = 0; i < repeats; ++i) {
        auto begin = Clock::now();
        burst.submit();
        auto submitted = Clock::now();
        require(burst.wait(), "burst timeout");
        auto end = Clock::now();
        submit_us += micros(begin, submitted);
        complete_us += micros(begin, end);
    }

    // The execution thread exclusively owns submissions; completion sequence gates caller buffer reuse.
    struct Request { uint64_t sequence = 0; Commands* commands = nullptr; };
    struct Completion { uint64_t sequence = 0; bool success = false; };
    Ring<Request, 8> requests;
    Ring<Completion, 8> completions;
    std::exception_ptr worker_error;
    std::thread worker([&] {
        for (;;) {
            Request request;
            while (!requests.try_pop(request)) std::this_thread::yield();
            if (!request.commands) break;
            bool success = false;
            try {
                if (!worker_error) {
                    request.commands->submit();
                    success = request.commands->wait();
                    if (!success) throw std::runtime_error("worker GPU timeout");
                }
            } catch (...) { worker_error = std::current_exception(); }
            while (!completions.try_push({request.sequence, success})) std::this_thread::yield();
        }
    });
    constexpr uint64_t jobs = 24;
    uint64_t sent = 0, received = 0;
    bool ordered = true;
    while (received < jobs) {
        if (sent < jobs && requests.try_push({sent, &burst})) ++sent;
        Completion completion;
        if (completions.try_pop(completion)) {
            ordered = ordered && completion.sequence == received && completion.success;
            ++received;
        }
    }
    while (!requests.try_push({jobs, nullptr})) std::this_thread::yield();
    worker.join();
    if (worker_error) std::rethrow_exception(worker_error);
    require(ordered, "worker completion sequence failed");
    const auto values_ready = rt.event();
    upload.copy(values, device_values, elements * sizeof(float), values_ready);
    require(upload.wait(), "benchmark readback timeout");
    check(zeEventHostSynchronize(values_ready, 30000000000ULL), "benchmark readback event wait");
    for (uint32_t i = 0; i < elements; ++i)
        require(values[i] == launches * (3 + repeats + jobs), "dispatch snapshot/barrier/worker parity failed");
    std::cout << "PASS 400 dependent kernels, captured arguments, 24 ring-driven GPU replays\n";
    std::cout << "400 kernels: record_us=" << micros(record_start, record_end)
              << " mean_submit_us=" << submit_us / repeats
              << " mean_submit_and_wait_us=" << complete_us / repeats
              << " samples=" << repeats << " (microbenchmark, not model throughput)\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << "FAIL " << error.what() << '\n';
    return 1;
}
