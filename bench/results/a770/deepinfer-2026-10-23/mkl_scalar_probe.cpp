#define DPCT_PROFILING_ENABLED
#include <sycl/sycl.hpp>
#include <dpct/dpct.hpp>
#include <dpct/blas_utils.hpp>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <stdexcept>
#include <vector>

using Clock = std::chrono::steady_clock;
double elapsed(Clock::time_point t) { return std::chrono::duration<double, std::micro>(Clock::now()-t).count(); }

int main() try {
    auto& q = dpct::get_in_order_queue();
    if (!q.is_in_order() || !q.has_property<sycl::property::queue::enable_profiling>())
        throw std::runtime_error("queue differs from the engine's profiling in-order queue");
    std::printf("DEVICE %s\n", q.get_device().get_info<sycl::info::device::name>().c_str());
    const auto context = q.get_context();
    float alpha = 1, beta = 0;
    auto query = [&](int allocations) {
        std::vector<double> times;
        for (int batch = 0; batch < 5; ++batch) {
            const auto t0 = Clock::now();
            for (int i = 0; i < 64; ++i)
                if (sycl::get_pointer_type(&alpha, context) != sycl::usm::alloc::unknown)
                    throw std::runtime_error("stack scalar classified as USM");
            times.push_back(elapsed(t0)/64);
        }
        std::sort(times.begin(), times.end());
        std::printf("QUERY {\"allocations\":%d,\"us\":%.6f}\n", allocations, times[2]);
        std::fflush(stdout);
    };
    constexpr int T = 4, N = 1280, K = 2560;
    auto* x = sycl::malloc_device<sycl::half>(T*K, q);
    auto* w = sycl::malloc_device<sycl::half>(N*K, q);
    auto* y = sycl::malloc_device<float>(T*N, q);
    if (!x || !w || !y) throw std::runtime_error("device allocation failed");
    q.fill(x, sycl::half(.125f), T*K);
    q.fill(w, sycl::half(.0625f), N*K).wait();
    dpct::blas::descriptor desc;
    desc.set_queue(&q);
    auto gemm = [&](bool direct) {
        if (direct)
            oneapi::mkl::blas::column_major::gemm(q, oneapi::mkl::transpose::trans,
                oneapi::mkl::transpose::nontrans, N, T, K, alpha, w, K, x, K, beta, y, N,
                oneapi::mkl::blas::compute_mode::unset);
        else
            dpct::blas::gemm(&desc, oneapi::mkl::transpose::trans, oneapi::mkl::transpose::nontrans,
                N, T, K, &alpha, w, dpct::library_data_t::real_half, K,
                x, dpct::library_data_t::real_half, K, &beta, y,
                dpct::library_data_t::real_float, N, dpct::compute_type::f32);
    };
    auto bench = [&](bool direct, int allocations) {
        for (int i = 0; i < 4; ++i) gemm(direct);
        q.wait_and_throw();
        std::vector<double> submit, total;
        for (int batch = 0; batch < 5; ++batch) {
            const auto t0 = Clock::now();
            for (int i = 0; i < 16; ++i) gemm(direct);
            submit.push_back(elapsed(t0)/16);
            q.wait_and_throw();
            total.push_back(elapsed(t0)/16);
        }
        std::vector<float> h(T*N);
        q.memcpy(h.data(), y, h.size()*sizeof(float)).wait();
        for (float v : h) if (v != 20.f) throw std::runtime_error("GEMM output mismatch");
        std::sort(submit.begin(), submit.end());
        std::sort(total.begin(), total.end());
        std::printf("GEMM {\"direct\":%s,\"allocations\":%d,\"submit_us\":%.6f,\"total_us\":%.6f}\n",
                    direct ? "true" : "false", allocations, submit[2], total[2]);
        std::fflush(stdout);
    };
    std::vector<void*> pinned;
    for (int count : {0, 24576}) {
        const auto t0 = Clock::now();
        while ((int)pinned.size() < count) {
            void* p = sycl::malloc_host(1u<<20, q);
            if (!p) throw std::runtime_error("host allocation failed");
            pinned.push_back(p);
        }
        std::printf("ALLOCATIONS %d %.1f ms\n", count, elapsed(t0)/1000);
        query(count);
        bench(false, count);
        bench(true, count);
        bench(false, count);
    }
    for (void* p : pinned) sycl::free(p, q);
    sycl::free(x, q); sycl::free(w, q); sycl::free(y, q);
    std::puts("PASS");
} catch (const std::exception& e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
}
