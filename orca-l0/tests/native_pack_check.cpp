#include <orca_l0/compiler/program.hpp>
#include <array>
#include <chrono>
#include <cstdio>
#include <vector>
#include "runtime_dependencies.hpp"
#ifdef ORCA_L0_DIRECT_DRIVER
#include <orca_l0/direct_driver.hpp>
#endif

int main(int argc, char** argv) try {
    if (argc != 2 && !(argc == 3 && std::string(argv[2]) == "--bench"))
        throw std::invalid_argument("usage: native-pack-check <pack with advance kernel> [--bench]");
    const bool bench = argc == 3;
    const int launches = bench ? 400 : 16, passes = bench ? 35 : 3, warmup = bench ? 5 : 0;
    using Clock = std::chrono::steady_clock;
    const auto us = [](Clock::time_point a, Clock::time_point b) {
        return std::chrono::duration<double, std::micro>(b - a).count();
    };
    check_runtime_dependencies();
    const auto start = Clock::now();
    orca::l0::Runtime runtime;
    const auto initialized = Clock::now();
    orca::l0::compiler::Program program(runtime, argv[1]);
    const auto kernel = program.kernel("advance", 2);
    const auto loaded = Clock::now();
    auto* device = static_cast<float*>(runtime.allocate(64 * sizeof(float)));
    std::array<float, 64> input{}, result{};
    for (unsigned i = 0; i < input.size(); ++i) input[i] = float(i);
    auto& recorded = runtime.commands(orca::l0::Ordering::in_order);
    recorded.copy(device, input.data(), sizeof(input));
    orca::l0::arguments(kernel, device, uint32_t{7});
    for (int i = 0; i < launches; ++i) recorded.launch(kernel, {1, 1, 1}, {64, 1, 1});
    recorded.copy(result.data(), device, sizeof(result));
    recorded.close();
    const auto ready = Clock::now();
    std::vector<double> complete_samples, submit_samples;
    for (int pass = 0; pass < passes; ++pass) {
        for (unsigned i = 0; i < input.size(); ++i) input[i] = float(i) + float(pass) * 0.125f;
        const auto begin = Clock::now();
        recorded.submit();
        const auto submitted = Clock::now();
        if (!recorded.wait()) throw std::runtime_error("native replay timed out");
        const auto done = Clock::now();
        for (unsigned i = 0; i < result.size(); ++i)
            if (result[i] != input[i] + 7 * launches) throw std::runtime_error("native output mismatch");
        if (bench && pass >= warmup) {
            submit_samples.push_back(us(begin, submitted));
            complete_samples.push_back(us(begin, done));
            std::printf("SAMPLE %d %.3f %.3f\n", pass - warmup, submit_samples.back(), complete_samples.back());
        }
    }
    if (bench) {
        const auto median = [](std::vector<double> values) {
            std::sort(values.begin(), values.end());
            return (values[(values.size()-1)/2] + values[values.size()/2]) / 2;
        };
        std::printf("METRIC initialization_us=%.3f module_kernel_us=%.3f preparation_us=%.3f submit_us=%.3f complete_us=%.3f warmup=%d retained=%zu\n",
            us(start, initialized), us(initialized, loaded), us(loaded, ready),
            median(submit_samples), median(complete_samples), warmup, complete_samples.size());
    }
    try { (void)program.kernel("advance", 3); }
    catch (const std::runtime_error&) {
        check_runtime_dependencies();
#ifdef ORCA_L0_DIRECT_DRIVER
        std::printf("DIRECT %s\n", orca::l0::direct_driver_path());
#endif
        std::printf("PASS %zu exact output elements, native graph replay, ABI mismatch and module audit\n", result.size() * passes);
        return 0;
    }
    throw std::runtime_error("wrong ABI argument count accepted");
} catch (const std::exception& e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
}
