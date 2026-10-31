"""Run the engine's actual staging-index methods without a GPU or model.

python tools/test_expert_stage_index.py [--bench]
Uses CXX (default: g++). Only the GPU-independent methods and their member
declarations are compiled; both backends must keep identical implementations.
"""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def methods(path):
    source = path.read_text(encoding="utf-8")
    claim = source[source.index("bool FileExpertSource::claim_stage("):source.index("// Fills buffer `v`")]
    publish = source[source.index("void FileExpertSource::publish_stage("):source.index("const uint8_t* FileExpertSource::staged_blob(")]
    return claim + publish


def main():
    implementation = methods(ROOT / "src/core/expert_source.cpp")
    assert implementation == methods(ROOT / "sycl/src/core/expert_source.cpp"), "staging backends diverged"
    header = (ROOT / "include/strata/core/expert_source.hpp").read_text(encoding="utf-8")
    members = header[header.index("    std::mutex stage_mu_;"):header.index("    std::atomic<int64_t> ram_reads_")]
    program = r'''
#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>
struct FileExpertSource {
    static constexpr uint64_t kStageAge = 3;
    bool claim_stage(int64_t key, size_t& v, bool& fill);
    void publish_stage(size_t v, int64_t layer, bool ok, double us);
    int64_t blobs_ = 48 * 512;
    std::vector<uint64_t> layer_blob_bytes_ = {16};
    std::atomic<uint64_t> file_read_bytes_{0};
'''+members+r'''
};
'''+implementation+r'''
int main(int argc, char**) {
    FileExpertSource s;
    s.stage_blob_ = 16;
    size_t v = 999;
    bool fill = false;
    assert(!s.claim_stage(0, v, fill) && fill && v == 0); // slot zero must not mean absent
    assert(s.claim_stage(0, v, fill) && !fill && s.stage_busy_[v]);
    s.publish_stage(v, 0, true, 0);
    for (int key = 1; key < 256; ++key) {
        assert(!s.claim_stage(key, v, fill) && fill);
        s.publish_stage(v, 0, true, 0);
    }
    assert(s.stage_buf_.size() == 256);
    assert(!s.claim_stage(256, v, fill) && fill && v == 0); // sequence age evicts the oldest
    s.publish_stage(v, 0, false, 0);                       // failed read invalidates the forward index
    assert(!s.claim_stage(256, v, fill) && fill);
    s.publish_stage(v, 0, true, 0);
    s.epoch_ += 3;
    assert(!s.claim_stage(257, v, fill) && fill);
    s.publish_stage(v, 0, true, 0);
    assert(s.claim_stage(256, v, fill) && !fill);          // another entry survived the eviction
    // Failed slots can be reused; busy slots cannot. Every key still points back to its own bytes.
    s.publish_stage(v, 0, false, 0);
    s.epoch_ += 3;
    for (int key = 258; key < 2048; ++key) {
        assert(!s.claim_stage(key, v, fill) && fill);
        if (key % 7 != 0) s.publish_stage(v, 0, key % 11 != 0, 0);
        for (size_t i = 0; i < s.stage_key_.size(); ++i) {
            if (s.stage_key_[i] < 0) continue;
            size_t hit = 999;
            bool needs_fill = true;
            assert(s.claim_stage(s.stage_key_[i], hit, needs_fill) && !needs_fill && hit == i);
        }
        s.epoch_ += 3;
    }
    // New geometry after close/reopen must not retain old key-to-slot mappings.
    s.stage_of_.clear(); s.stage_buf_.clear(); s.stage_key_.clear();
    s.stage_epoch_.clear(); s.stage_used_.clear(); s.stage_busy_.clear();
    s.blobs_ = 2;
    assert(!s.claim_stage(1, v, fill) && fill && v == 0);
    const auto count = s.stage_buf_.size();
    assert(!s.claim_stage(-1, v, fill) && !fill);
    assert(!s.claim_stage(2, v, fill) && !fill && s.stage_buf_.size() == count);
    std::puts("expert_stage_index: PASS (slot zero, hits, busy slots, eviction, failed reads, reopen)");
    if (argc > 1) {
        FileExpertSource b;
        b.stage_blob_ = 16;
        constexpr int slots = 512, queries = 4000000;
        for (int key = 0; key < slots; ++key) {
            const bool hit = b.claim_stage(key, v, fill);
            assert(!hit && fill); // keep slots busy during setup so none are evicted
        }
        uint64_t sum = 0;
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < queries; ++i) {
            const bool hit = b.claim_stage((i * 997u) % slots, v, fill);
            assert(hit && !fill);
            sum += v;
        }
        const double ns = std::chrono::duration<double, std::nano>(
            std::chrono::steady_clock::now() - start).count() / queries;
        std::printf("BENCH stage hits slots=%d ns/lookup=%.2f checksum=%llu\n", slots, ns,
                    (unsigned long long) sum);
        for (int count : {256, 512, 2048}) {
            for (bool shuffled : {false, true}) {
                FileExpertSource c;
                c.stage_blob_ = 16;
                for (int key = 0; key < count; ++key) {
                    assert(!c.claim_stage(key, v, fill) && fill);
                }
                for (int key = 0; key < count; ++key) c.publish_stage(key, 0, true, 0);
                // Both ordered LRU slots and a permuted order; all reads are already complete.
                if (shuffled) {
                    for (int i = 0; i < count; ++i)
                        assert(c.claim_stage((i * 997u) % count, v, fill) && !fill);
                }
                constexpr int misses = 100000;
                uint64_t checksum = 0;
                const auto t0 = std::chrono::steady_clock::now();
                for (int i = 0; i < misses; ++i) {
                    c.epoch_ += 3;
                    const int key = count + i % (c.blobs_ - count);
                    assert(!c.claim_stage(key, v, fill) && fill);
                    c.publish_stage(v, 0, true, 0);
                    checksum += v;
                }
                const double miss_ns = std::chrono::duration<double, std::nano>(
                    std::chrono::steady_clock::now() - t0).count() / misses;
                assert(c.stage_buf_.size() == (size_t)count);
                std::printf("BENCH stage misses slots=%d shuffled=%d ns/lookup=%.2f checksum=%llu\n",
                            count, shuffled, miss_ns, (unsigned long long) checksum);
            }
        }
    }
}
'''
    with tempfile.TemporaryDirectory(prefix="strata-stage-index-") as folder:
        source = Path(folder) / "check.cpp"
        binary = Path(folder) / ("check.exe" if os.name == "nt" else "check")
        source.write_text(program, encoding="utf-8")
        subprocess.run([os.environ.get("CXX", "g++"), "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
                        "-pthread", str(source), "-o", str(binary)], check=True)
        subprocess.run([str(binary), *sys.argv[1:]], check=True)


if __name__ == "__main__":
    main()
