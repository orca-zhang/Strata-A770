#include "strata/spec/draft_policy.hpp"
#include <cstdint>
#include <cstdio>
#include <vector>
using strata::spec::DraftPolicy;
void baseline(DraftPolicy& policy, int T, int64_t n, int64_t cap, bool timed_round) {
    std::vector<int32_t> produced((size_t)n);
    struct { int64_t max_new; } o{cap};
    const bool from_sfx = false;
    const int a = T - 1, sfx_match = 0;
    const double round_ms = 12345.0;
    if (timed_round) policy.observe(from_sfx, T, a, sfx_match, round_ms);
}
void candidate(DraftPolicy& policy, int T, int64_t n, int64_t cap, bool timed_round) {
    std::vector<int32_t> produced((size_t)n);
    struct { int64_t max_new; } o{cap};
    const bool from_sfx = false;
    const int a = T - 1, sfx_match = 0;
    const double round_ms = 12345.0;
    if (timed_round && (int64_t) produced.size() < o.max_new)
                policy.observe(from_sfx, T, a, sfx_match, round_ms);
}
int main() {
    int pairs = 0, differences = 0;
    for (int T : {1, 4, 6}) for (int cap : {1, 4, 6})
        for (int n : {cap - 1, cap, cap + 1}) for (bool timed : {false, true}) {
            DraftPolicy old(6), fixed(6);
            baseline(old, T, n, cap, timed);
            candidate(fixed, T, n, cap, timed);
            const bool before = old.cost_ms(T) == 12345.0;
            const bool after = fixed.cost_ms(T) == 12345.0;
            if (before != timed || after != (timed && n < cap)) return 1;
            differences += before != after;
            ++pairs;
        }
    std::printf("SUMMARY {\"pairs\":%d,\"checks\":%d,\"terminal_differences\":%d}\n", pairs, pairs*2, differences);
}
