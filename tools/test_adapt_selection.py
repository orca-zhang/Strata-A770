"""Check the Intel engine's actual cache selector against the original greedy scan (no GPU).

CXX defaults to g++; pass --bench to time the full selection, including both sorts.
"""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    source = (ROOT / "sycl/src/program/generate.cpp").read_text(encoding="utf-8")
    declarations = source[source.index("        struct AdaptSwap {"):source.index("        // the VRAM tier follows")]
    selection = source[source.index("                for (int32_t i = 0; i < (int32_t) host_res.size(); ++i) {"):
                       source.index("            } else for (int64_t l = 0; l < g.n_layers; ++l) {")]
    finish = source[source.index("            std::sort(adapt_swaps.begin(), adapt_swaps.end(),"):
                    source.index("            if (!resident_stage_swaps(src, xcache,")]
    # The reference deliberately restarts the original range loop for every candidate.
    reference = selection.replace("std::fill(adapt_cursor.begin(), adapt_cursor.end(), 0);", "")
    reference = reference.replace(
        "for (size_t& j = adapt_cursor[adapt_size_class[(size_t) layer]]; j < adapt_vict.size(); ++j) {\n"
        "                        auto& v = adapt_vict[j];", "for (auto& v : adapt_vict) {")
    reference = reference.replace("                        ++j;\n", "")
    assert "adapt_cursor" not in reference and "for (auto& v : adapt_vict)" in reference
    candidate = selection.replace("auto& v = adapt_vict[j];", "auto& v = adapt_vict[j]; ++visits;")
    reference = reference.replace("for (auto& v : adapt_vict) {", "for (auto& v : adapt_vict) { ++visits;")
    save = source[source.index("        auto save_profile ="):source.index("        const int S = o.spec;")]
    after_response = source[source.index("            if (!profile_saved && o.expert_profile_save_min"):
                            source.index('        save_profile("exit");')].removesuffix("        }\n")
    program = r'''
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <tuple>
#include <vector>
using Clock = std::chrono::steady_clock;
struct Geometry { int64_t n_layers, n_expert; };
struct Options { int adapt_swaps; double expert_profile_save_min = 10; std::string expert_profile_save = "test"; };
struct Layout { std::vector<int64_t> sizes; int64_t blob_bytes(int64_t l) const { return sizes[l]; } };
namespace strata::kernels::cpu { Layout layout; const Layout& expert_layout() { return layout; } }
struct Cache { std::vector<int64_t> offsets{0}; int64_t bytes_of(int i) const { return offsets[i]; } };
struct Drive { struct { std::vector<float> usage; } d; };
size_t visits = 0;
''' + declarations[:declarations.index("        std::vector<AdaptSwap> adapt_swaps;")] + r'''
std::vector<AdaptSwap> select(bool old, Geometry g, const Options& o, const Drive& drive,
    const std::vector<int32_t>& host_res, const Cache& xcache, const std::vector<float>& adapt_cost_weight) {
''' + declarations[declarations.index("        std::vector<AdaptSwap> adapt_swaps;"):] + "if (old) {\n" + reference + "} else {\n" + candidate + "}\n" + finish + r'''
    return adapt_swaps;
}
int writes = 0;
bool write_ok = true;
namespace strata::core {
std::vector<int> rank_learned_profile(int64_t, int64_t, const std::vector<uint8_t>& resident,
    const std::vector<double>&, const std::vector<int>&) {
    assert((resident == std::vector<uint8_t>{1, 1, 0})); // includes the swap in flight
    return {};
}
bool write_expert_profile(const std::string&, int64_t, int64_t, const std::vector<int>&, std::string&) {
    ++writes;
    return write_ok;
}
}
int main(int argc, char**) {
    std::mt19937 rng(8723);
    for (int trial = 0; trial < 4006; ++trial) {
        const bool large = trial >= 4000;
        Geometry g{large ? 48 : 1 + rng() % 16, large ? 512 : 1 + rng() % 32};
        Options o{large ? 32 : 1 + int(rng() % (g.n_layers * g.n_expert))};
        auto& sizes = strata::kernels::cpu::layout.sizes;
        sizes.resize(g.n_layers);
        for (auto& s : sizes) s = 64 * (1 + rng() % (large ? 3 : 8));
        if (large && trial % 3 == 0) std::fill(sizes.begin(), sizes.end(), 1024); // no slot fits
        Drive drive;
        std::vector<int32_t> host_res(g.n_layers * g.n_expert, -1);
        std::vector<float> adapt_cost_weight(g.n_layers, 1.0f);
        for (auto& w : adapt_cost_weight) w = (1 + rng() % 4) * 0.5f;
        Cache cache;
        for (size_t i = 0; i < host_res.size(); ++i) {
            // Many ties and exact .5 threshold boundaries, empty/all-resident tiers, padded slots.
            const bool resident = trial == 0 || (trial != 1 && rng() % 100 < (large ? 24 : 50));
            if (resident) {
                host_res[i] = int32_t(cache.offsets.size() - 1);
                cache.offsets.push_back(cache.offsets.back() + 64 * (1 + rng() % 8));
            }
            drive.d.usage.push_back(float(rng() % 40) * 0.25f);
        }
        visits = 0;
        const auto expected = select(true, g, o, drive, host_res, cache, adapt_cost_weight);
        const size_t old_visits = visits;
        visits = 0;
        const auto actual = select(false, g, o, drive, host_res, cache, adapt_cost_weight);
        assert(actual.size() == expected.size() && visits <= old_visits);
        for (size_t i = 0; i < actual.size(); ++i) {
            const auto& a = actual[i]; const auto& b = expected[i];
            assert(std::tie(a.gain,a.layer,a.in,a.out,a.out_layer) == std::tie(b.gain,b.layer,b.in,b.out,b.out_layer));
        }
        if (large && argc > 1) {
            std::printf("shape 48x512 trial=%d swaps=%zu visits=%zu -> %zu", trial, actual.size(), old_visits, visits);
            for (bool old : {true, false}) {
                std::vector<double> times;
                for (int repeat = 0; repeat < 11; ++repeat) {
                    const auto t = Clock::now();
                    assert(select(old, g, o, drive, host_res, cache, adapt_cost_weight).size() == actual.size());
                    times.push_back(std::chrono::duration<double, std::micro>(Clock::now() - t).count());
                }
                std::sort(times.begin(), times.end());
                std::printf(" %s_us=%.1f", old ? "old" : "new", times[5]);
            }
            std::puts("");
        }
    }
    Geometry g{1, 3}; Options o{32};
    std::vector<double> heat{3, 2, 1};
    std::vector<int32_t> host_res{0, -1, -1};
    std::vector<std::pair<int32_t,int32_t>> pending{{1, 1}};
    std::vector<int> profile_loaded;
    auto profile_saved_at = Clock::now();
    bool profile_saved = false;
    const char* finish = "length";
    int produced_n = 1;
''' + save + "auto response = [&] {\n" + after_response + r'''};
    finish = "cancel"; response(); assert(writes == 0);
    finish = "length"; produced_n = 0; response(); assert(writes == 0);
    produced_n = 1; o.expert_profile_save_min = 0; response(); assert(writes == 0);
    o.expert_profile_save_min = 10;
    save_profile("interval"); assert(writes == 1 && !profile_saved);
    write_ok = false; response(); assert(writes == 2 && !profile_saved);
    write_ok = true; response(); assert(writes == 3 && profile_saved);
    response(); assert(writes == 3);
    save_profile("interval"); assert(writes == 4);
    o.expert_profile_save_min = 0; save_profile("exit"); assert(writes == 5);
    heat.clear(); profile_saved = false; response(); assert(writes == 5);
    o.expert_profile_save_min = 10; response(); assert(writes == 5 && !profile_saved);
    std::puts("PASS: 4,006 exact old/new cache selections; profile first response, interval, retry, cancel, zero, pending, disabled");
}
'''
    with tempfile.TemporaryDirectory(prefix="strata-adapt-") as folder:
        cpp = Path(folder) / "check.cpp"
        binary = Path(folder) / ("check.exe" if os.name == "nt" else "check")
        cpp.write_text(program, encoding="utf-8")
        subprocess.run([os.environ.get("CXX", "g++"), "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
                        str(cpp), "-o", str(binary)], check=True)
        subprocess.run([str(binary), *sys.argv[1:]], check=True)


if __name__ == "__main__":
    main()
