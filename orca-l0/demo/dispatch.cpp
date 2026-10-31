// Same native module, device, context, data and kernel geometry for all three submission paths.
#include <orca_l0/runtime.hpp>
#include <sycl/sycl.hpp>
#include <sycl/ext/oneapi/backend/level_zero.hpp>
#include <sycl/ext/oneapi/experimental/graph.hpp>
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

using namespace orca::l0;
namespace graph_api = sycl::ext::oneapi::experimental;
constexpr auto backend = sycl::backend::ext_oneapi_level_zero;
constexpr auto keep = sycl::ext::oneapi::level_zero::ownership::keep;
using Clock = std::chrono::steady_clock;

static double us(Clock::time_point a, Clock::time_point b) {
    return std::chrono::duration<double, std::micro>(b - a).count();
}
static double median(std::vector<double> values) {
    std::sort(values.begin(), values.end());
    const auto n = values.size();
    return n % 2 ? values[n / 2] : (values[n / 2 - 1] + values[n / 2]) / 2;
}
static int repetitions(const char* text) {
    int value = 0;
    const auto end = text + std::strlen(text);
    auto parsed = std::from_chars(text, end, value);
    if (parsed.ec != std::errc{} || parsed.ptr != end || value < 5 || value > 100)
        throw std::invalid_argument("repetitions must be an integer in [5,100]");
    return value;
}

int main(int argc, char** argv) try {
    if (argc < 2 || argc > 3) throw std::invalid_argument("usage: orca-l0-demo inference.spv [repetitions=15]");
    const int repeats = argc == 3 ? repetitions(argv[2]) : 15;
    constexpr uint32_t launches = 400, local = 64;
    Runtime runtime;
    const auto module = runtime.module(argv[1]);
    // Borrowing imports keep Orca's native context/module/kernel ownership intact.
    auto device = sycl::make_device<backend>(runtime.native_device());
    auto context = sycl::make_context<backend>({runtime.native_context(), {device}, keep});
    auto bundle = sycl::make_kernel_bundle<backend, sycl::bundle_state::executable>({module, keep}, context);
    sycl::queue queue(context, device, sycl::property::queue::in_order{});
    std::cout << "# device=" << runtime.properties().name << ", kernels=" << launches
              << ", repetitions=" << repeats << ", identical_native_binary=true\n"
              << "# times include input copy, all dependent kernels, result copy and completion wait\n"
              << "# standalone dispatch demo; no full-model tok/s inference\n"
              << "elements,round,backend,submit_us,total_us\n" << std::fixed << std::setprecision(3);

    for (const std::size_t elements : {std::size_t{64}, std::size_t{1 << 20}}) {
        const auto bytes = elements * sizeof(float);
        auto* initial = static_cast<float*>(runtime.allocate(bytes, Memory::host));
        auto* result = static_cast<float*>(runtime.allocate(bytes, Memory::host));
        auto* values = static_cast<float*>(runtime.allocate(bytes));
        for (std::size_t i = 0; i < elements; ++i) initial[i] = static_cast<float>(i % 7);
        const auto native_kernel = runtime.kernel(module, "advance");
        const auto imported_handle = runtime.kernel(module, "advance");
        auto imported_kernel = sycl::make_kernel<backend>({bundle, imported_handle, keep}, context);
        arguments(native_kernel, values, uint32_t{1});
        const auto copied = runtime.event();
        auto& native = runtime.commands();
        native.copy(values, initial, bytes);
        native.barrier();
        for (uint32_t i = 0; i < launches; ++i) {
            native.launch(native_kernel, {static_cast<uint32_t>(elements / local), 1, 1}, {local, 1, 1});
            native.barrier();
        }
        native.copy(result, values, bytes, copied);
        native.close();

        auto eager = [&] {
            queue.memcpy(values, initial, bytes);
            for (uint32_t i = 0; i < launches; ++i) {
                queue.submit([&](sycl::handler& handler) {
                    handler.set_args(values, uint32_t{1});
                    handler.parallel_for(sycl::nd_range<1>{elements, local}, imported_kernel);
                });
            }
            queue.memcpy(result, values, bytes);
        };
        graph_api::command_graph graph(context, device);
        graph.begin_recording(queue);
        eager();
        graph.end_recording(queue);
        auto executable = graph.finalize();

        struct Sample { double submit, total; };
        auto measure = [&](int mode) -> Sample {
            check(zeEventHostReset(copied), "demo output event reset");
            const auto begin = Clock::now();
            if (mode == 0) eager();
            else if (mode == 1) queue.ext_oneapi_graph(executable);
            else native.submit();
            const auto submitted = Clock::now();
            if (mode == 2) {
                if (!native.wait()) throw std::runtime_error("native demo timeout");
                check(zeEventHostSynchronize(copied, 30000000000ULL), "demo output wait");
            } else queue.wait_and_throw();
            const auto end = Clock::now();
            for (std::size_t i = 0; i < elements; ++i)
                if (result[i] != initial[i] + launches) throw std::runtime_error("demo exact parity failed");
            return {us(begin, submitted), us(begin, end)};
        };
        for (int warmup = 0; warmup < 3; ++warmup)
            for (int mode = 0; mode < 3; ++mode) measure(mode);
        std::vector<double> eager_times, graph_times, native_times, paired_changes;
        for (int round = 0; round < repeats; ++round) {
            // A0/B/A1 brackets native replay with the graph baseline; keep every raw sample.
            const auto a0 = measure(1), b = measure(2), a1 = measure(1), e = measure(0);
            for (const auto& row : {std::make_pair("sycl_graph_a0", a0), std::make_pair("orca_l0", b),
                                    std::make_pair("sycl_graph_a1", a1), std::make_pair("sycl_eager", e)})
                std::cout << elements << ',' << round << ',' << row.first << ','
                          << row.second.submit << ',' << row.second.total << '\n';
            eager_times.push_back(e.total);
            graph_times.push_back(a0.total);
            graph_times.push_back(a1.total);
            native_times.push_back(b.total);
            if (std::abs(a1.total / a0.total - 1) <= 0.03)
                paired_changes.push_back(100 * (std::sqrt(a0.total * a1.total) / b.total - 1));
        }
        std::cout << "# elements=" << elements << " median_total_us: eager=" << median(eager_times)
                  << " sycl_graph=" << median(graph_times) << " orca_l0=" << median(native_times)
                  << " stable_pairs=" << paired_changes.size() << '/' << repeats;
        if (paired_changes.size() >= 5)
            std::cout << " paired_throughput_change_vs_graph_pct=" << median(paired_changes);
        else std::cout << " result=inconclusive (adjacent baseline drift exceeded 3%)";
        std::cout << '\n';
    }
    std::cout << "# PASS exact CPU reference for every output of every measured path\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << "FAIL " << error.what() << '\n';
    return 1;
}
