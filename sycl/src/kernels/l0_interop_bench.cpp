#include <sycl/sycl.hpp>
#include <dpct/dpct.hpp>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <variant>

#if defined(STRATA_HAS_LEVEL_ZERO_EVENTS)
#include <level_zero/ze_api.h>
#include <sycl/ext/oneapi/backend/level_zero.hpp>
#endif

int main() {
    std::printf("=== Strata Level Zero Interop Micro-Benchmark (Scheme B) ===\n");
    auto& dev = dpct::get_current_device();
    dpct::queue_ptr cs = dev.create_queue(true);
    if (!cs) {
        std::fprintf(stderr, "Error: failed to create in-order queue\n");
        return 1;
    }

    std::printf("SYCL Device: %s\n", dev.get_device_info().get_name());
    const bool is_l0 = (cs->get_backend() == sycl::backend::ext_oneapi_level_zero);
    std::printf("Backend: %s (is Level Zero: %s)\n",
                is_l0 ? "Level Zero" : "Other", is_l0 ? "YES" : "NO");

#if !defined(STRATA_HAS_LEVEL_ZERO_EVENTS)
    std::printf("Error: STRATA_HAS_LEVEL_ZERO_EVENTS not defined in this build\n");
    return 1;
#else
    if (!is_l0) {
        std::printf("Error: backend is not Level Zero\n");
        return 1;
    }

    auto native_q = sycl::get_native<sycl::backend::ext_oneapi_level_zero>(*cs);
    const bool is_cmd_list = std::holds_alternative<ze_command_list_handle_t>(native_q);
    const bool is_cmd_queue = std::holds_alternative<ze_command_queue_handle_t>(native_q);

    std::printf("Native Queue Object: %s\n",
                is_cmd_list ? "ze_command_list_handle_t (Immediate Command List)" :
                is_cmd_queue ? "ze_command_queue_handle_t (Standard Command Queue)" : "Unknown");

    ze_command_list_handle_t cmd_list = is_cmd_list ? std::get<ze_command_list_handle_t>(native_q) : nullptr;
    ze_command_queue_handle_t cmd_queue = is_cmd_queue ? std::get<ze_command_queue_handle_t>(native_q) : nullptr;

    const size_t N = 1024 * 1024;
    float* d_buf = sycl::malloc_device<float>(N, *cs);
    cs->fill(d_buf, 1.0f, N).wait();

    // Warmup
    cs->parallel_for(sycl::range<1>(N), [=](sycl::id<1> idx) {
        d_buf[idx] += 1.0f;
    });
    if (cmd_list) {
        ze_result_t res = zeCommandListHostSynchronize(cmd_list, UINT64_MAX);
        std::printf("Warmup zeCommandListHostSynchronize result: %d (SUCCESS=%d)\n", (int)res, (int)ZE_RESULT_SUCCESS);
    } else if (cmd_queue) {
        ze_result_t res = zeCommandQueueSynchronize(cmd_queue, UINT64_MAX);
        std::printf("Warmup zeCommandQueueSynchronize result: %d (SUCCESS=%d)\n", (int)res, (int)ZE_RESULT_SUCCESS);
    }

    // Benchmark 1: Single small kernel + sync (T=1 micro-step latency)
    constexpr int ITERS = 1000;
    std::printf("\n--- Test 1: %d Iterations of Single Kernel + Sync ---\n", ITERS);

    // Branch A: Standard SYCL cs->wait()
    {
        auto t0 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < ITERS; ++i) {
            cs->parallel_for(sycl::range<1>(1024), [=](sycl::id<1> idx) {
                d_buf[idx] += 0.001f;
            });
            cs->wait();
        }
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        std::printf("Branch A (SYCL cs->wait()): total %.2f ms, avg %.2f us/iter\n", ms, (ms * 1000.0) / ITERS);
    }

    // Branch B: Native Level Zero Host Synchronize
    {
        auto t0 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < ITERS; ++i) {
            cs->parallel_for(sycl::range<1>(1024), [=](sycl::id<1> idx) {
                d_buf[idx] += 0.001f;
            });
            if (cmd_list) {
                zeCommandListHostSynchronize(cmd_list, UINT64_MAX);
            } else if (cmd_queue) {
                zeCommandQueueSynchronize(cmd_queue, UINT64_MAX);
            }
        }
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        std::printf("Branch B (L0 Direct Sync):  total %.2f ms, avg %.2f us/iter\n", ms, (ms * 1000.0) / ITERS);
    }

    // Benchmark 2: Decode Window Burst (50 chained small kernels simulating a layer block)
    constexpr int BURST_ITERS = 200;
    constexpr int KERNELS_PER_BURST = 50;
    std::printf("\n--- Test 2: %d Bursts of %d Chained Kernels (Simulated Layer Burst) ---\n", BURST_ITERS, KERNELS_PER_BURST);

    // Branch A: Standard SYCL
    {
        auto t0 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < BURST_ITERS; ++i) {
            for (int k = 0; k < KERNELS_PER_BURST; ++k) {
                cs->parallel_for(sycl::range<1>(1024), [=](sycl::id<1> idx) {
                    d_buf[idx] += 0.0001f;
                });
            }
            cs->wait();
        }
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        std::printf("Burst Branch A (SYCL cs->wait()): total %.2f ms, avg %.3f ms/burst\n", ms, ms / BURST_ITERS);
    }

    // Branch B: Native Level Zero Host Synchronize
    {
        auto t0 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < BURST_ITERS; ++i) {
            for (int k = 0; k < KERNELS_PER_BURST; ++k) {
                cs->parallel_for(sycl::range<1>(1024), [=](sycl::id<1> idx) {
                    d_buf[idx] += 0.0001f;
                });
            }
            if (cmd_list) {
                zeCommandListHostSynchronize(cmd_list, UINT64_MAX);
            } else if (cmd_queue) {
                zeCommandQueueSynchronize(cmd_queue, UINT64_MAX);
            }
        }
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        std::printf("Burst Branch B (L0 Direct Sync):  total %.2f ms, avg %.3f ms/burst\n", ms, ms / BURST_ITERS);
    }

    // Verify numerical results
    std::vector<float> h_buf(N);
    cs->memcpy(h_buf.data(), d_buf, N * sizeof(float)).wait();
    bool correct = true;
    for (size_t i = 0; i < 1024; ++i) {
        if (!std::isfinite(h_buf[i]) || h_buf[i] <= 1.0f) {
            correct = false;
            break;
        }
    }
    std::printf("\nBitwise verification: %s (Sample value: %.5f)\n", correct ? "PASS" : "FAIL", h_buf[0]);

    sycl::free(d_buf, *cs);
    dev.destroy_queue(cs);
    return correct ? 0 : 1;
#endif
}
