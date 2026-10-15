// Windows: cl /EHsc /std:c++17 /O2 /DWIN64 /I<patched-ezpp-directory> tools\test_ezpp_clock.cpp
#include <chrono>
#include <cmath>
#include <cstdio>
#include <thread>
#include "ezpp.hpp"
#undef int64_t

int main() {
    using Clock = std::chrono::steady_clock;
    for (const int delay : {20, 80, 120, 250, 1050}) {
        const auto start = Clock::now();
        const auto before = ezpp::time_now();
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        const auto measured = ezpp::time_now() - before;
        const double expected = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
        std::printf("sleep=%d ezpp=%lld steady_clock=%.3f ms\n", delay,
                    static_cast<long long>(measured), expected);
        if (measured <= 0 || std::abs(static_cast<double>(measured) - expected) > 5.0) return 1;
    }
    return 0;
}
