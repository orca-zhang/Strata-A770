#include "strata/core/verify.hpp"
#include <cstdio>
#include <stdexcept>

int main() try {
    auto& queue = dpct::get_in_order_queue();
    auto* value = sycl::malloc_shared<int>(1, queue);
    if (!value) throw std::bad_alloc();
    *value = 0;
    int expected = 0;
    auto check = [&] {
        queue.single_task([=] { ++*value; }).wait_and_throw();
        if (*value != ++expected) throw std::runtime_error("default queue lost during Verifier cleanup");
    };
    {
        strata::core::Verifier first;
        { strata::core::Verifier second; }
        check();
    }
    check();
    try {
        strata::core::Verifier first, second;
        throw std::runtime_error("exercise exception unwinding");
    } catch (const std::runtime_error&) {
    }
    check();
    sycl::free(value, queue);
    std::printf("PASS Verifier normal/nested/exception cleanup; default queue remains usable (%d checks)\n", expected);
    return 0;
} catch (const std::exception& error) {
    std::fprintf(stderr, "FAIL %s\n", error.what());
    return 1;
}
