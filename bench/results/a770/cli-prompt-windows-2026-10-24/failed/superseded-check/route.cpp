#include <cassert>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
int main() {
    int checks = 0;
    for (bool native_pack : {false, true})
    for (int n_prompt : {1, 2, 7, 8, 23})
    for (int short_read : {0, 1, 6, 7, 22, 8192})
    for (int until : {0, 1, 3, 999}) {
        struct { int prefill_until, short_read; } o{until, short_read};
        const int64_t n_batched = (o.prefill_until > 0 && o.prefill_until < n_prompt - 1) ? o.prefill_until : n_prompt - 1;
        const bool prompt_windows = native_pack && n_batched > 0 && n_batched == n_prompt - 1 && n_batched <= o.short_read;
        const bool partial = until > 0 && until < n_prompt - 1;
        assert(prompt_windows == (native_pack && n_prompt > 1 && !partial && n_prompt <= short_read + 1));
        ++checks;
    }
    std::printf("PASS %d CLI prompt route checks\n", checks);
}
