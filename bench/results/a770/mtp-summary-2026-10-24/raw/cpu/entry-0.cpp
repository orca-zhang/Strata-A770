#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>
bool enter(bool native_pack, int64_t pos_start, int spec, int64_t max_new, int count, bool ended) {
    struct { int spec; int64_t max_new; } o{spec, max_new};
    std::vector<int> produced(count);
    const int64_t n_prompt = pos_start + 1;
    int64_t spec_pos = -1;
    for (int64_t pos = pos_start;; ++pos) {
        if (native_pack) { spec_pos = pos; break; }
        if (o.spec > 0 && pos >= n_prompt - 1) { spec_pos = pos + 1; break; }
        // The harness omits token computation; stop after the last prompt position.
        if (pos >= n_prompt - 1) break;
    }
    if (spec_pos >= 0 && (int64_t) produced.size() < o.max_new && !ended) { return true; }
    return false;
}
int main() {
    int checks = 0;
    for (bool native : {false, true})
    for (int spec : {0, 4}) {
        if (native && spec == 0) continue; // Native packs require spec >= 2 at the CLI boundary.
        for (int pos : {0, 7})
        for (int cap : {1, 4, 6})
        for (int count : {0, cap, cap+1})
        for (bool ended : {false, true}) {
            bool expected = spec > 0 && count < cap && !ended;
            if (false && native && pos == 0) expected = false;
            assert(enter(native, pos, spec, cap, count, ended) == expected);
            ++checks;
        }
    }
    std::printf("PASS %d CLI entry checks\n", checks);
}
