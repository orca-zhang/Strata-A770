"""Check the actual verifier key and map declarations without a GPU (CXX defaults to g++)."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    header = (ROOT / "include/strata/core/verify.hpp").read_text(encoding="utf-8")
    members = header[header.index("    using BatchKey ="):header.index("    uint64_t bm_tick_")]
    method = header[header.index("    BatchKey bkey("):header.index("    // batch_launch / batch_poll")]
    program = r'''
#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <new>
#include <vector>
static bool counting = false;
static size_t allocations = 0;
[[gnu::noinline]] void* operator new(size_t n) {
    if (counting) ++allocations;
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
[[gnu::noinline]] void operator delete(void* p) noexcept { std::free(p); }
[[gnu::noinline]] void operator delete(void* p, size_t) noexcept { std::free(p); }
using cudaGraphExec_t = uintptr_t;
struct Verifier {
    bool ar_off_ = false;
''' + members + method + r'''
};
int main(int argc, char**) {
    Verifier v;
    std::map<Verifier::BatchKey, std::vector<int>> seen;
    int rows[8] = {};
    size_t cases = 0;
    for (int S = 1, combinations = 3; S <= 8; ++S, combinations *= 3) {
        for (int bits = 0; bits < combinations; ++bits) {
            int digits = bits;
            for (int t = 0; t < S; ++t) { rows[t] = digits % 3; digits /= 3; }
            for (int base : {0, 8, 16}) for (bool flag : {false, true}) {
                v.ar_off_ = flag;
                counting = true;
                const auto key = v.bkey(rows, S, base);
                counting = false;
                assert(allocations == 0);
                assert(key == v.bkey(rows, S, base));
                std::vector<int> original{base};
                original.insert(original.end(), rows, rows + S);
                original.push_back(flag ? 1 : 0);
                const auto [it, added] = seen.emplace(key, original);
                assert(added || it->second == original); // different old keys must never alias
                assert(added);
                ++cases;
            }
        }
    }
    assert(cases == 59040 && seen.size() == cases);
    for (const auto& [key, original] : seen) {
        v.exec_bm_[key] = 17;
        v.commit_bm_[key] = 29;
        v.bm_used_[key] = 41;
        assert(v.exec_bm_.find(key)->second == 17 && v.commit_bm_.find(key)->second == 29);
        assert(v.bm_used_.find(key)->second == 41);
    }
    std::puts("batch_graph_key: PASS (59,040 layouts, lengths, row order, bases, flags; zero key allocations)");
    if (argc > 1) {
        uint64_t checksum = 0;
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < 2000000; ++i) {
            rows[0] = i % 3;
            const auto key = v.bkey(rows, 8, 16);
            checksum += v.exec_bm_.find(key)->second;
        }
        const double ns = std::chrono::duration<double, std::nano>(
            std::chrono::steady_clock::now() - start).count() / 2000000;
        std::printf("BENCH batch key + map hit: %.2f ns checksum=%llu\n", ns, (unsigned long long)checksum);
    }
}
'''
    with tempfile.TemporaryDirectory(prefix="strata-batch-key-") as folder:
        source = Path(folder) / "check.cpp"
        binary = Path(folder) / ("check.exe" if os.name == "nt" else "check")
        source.write_text(program, encoding="utf-8")
        subprocess.run([os.environ.get("CXX", "g++"), "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
                        str(source), "-o", str(binary)], check=True)
        subprocess.run([str(binary), *sys.argv[1:]], check=True)


if __name__ == "__main__":
    main()
