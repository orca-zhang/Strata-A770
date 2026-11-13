// Synthetic byte-for-byte test of the real SYCL GGUF ring: no model or GPU work.
// Link this file with gguf_expert_source.cpp only; the tiny layout is local to this executable.
#ifdef NDEBUG
#undef NDEBUG
#endif

#include "strata/core/gguf_expert_source.hpp"
#include "strata/kernels/cpu/expert_layout.hpp"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <thread>
#include <vector>
namespace strata::kernels::cpu {
ExpertLayout test_layout;
const ExpertLayout& expert_layout() { return test_layout; }
}  // namespace strata::kernels::cpu
namespace strata::core {
bool ExpertSource::copy_blob(int64_t l, int64_t e, uint8_t* dst) {
    const uint8_t* b = blob(l, e);
    if (!b || !dst) return false;
    std::memcpy(dst, b, (size_t) strata::kernels::cpu::expert_layout().blob_bytes(l));
    return true;
}
}  // namespace strata::core
int main(int argc, char** argv) {
    using namespace strata::core;
    auto& lay = strata::kernels::cpu::test_layout;
    constexpr int layers = 2, experts = 600, piece = 16;
    lay.native = true; lay.max_blob = piece * 3;
    lay.bytes.assign(layers, piece * 3); lay.fmt.resize(layers);
    lay.gguf_off.resize(3 * layers);
    std::vector<uint8_t> bytes(32 + 3 * layers * experts * piece);
    for (int l = 0; l < layers; ++l) {
        lay.fmt[l].up_off = piece; lay.fmt[l].down_off = piece * 2;
        for (int r = 0; r < 3; ++r) {
            lay.gguf_off[3 * l + r] = 32 + (3 * l + r) * experts * piece;
            for (int e = 0; e < experts; ++e)
                for (int j = 0; j < piece; ++j)
                    bytes[lay.gguf_off[3 * l + r] + e * piece + j] = (uint8_t) (l * 83 + e * 19 + r * 31 + j);
        }
    }
    const auto path = std::filesystem::temp_directory_path() / ("strata-gguf-ring-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".bin");
    auto write = [&](size_t count) {
        std::ofstream out(path, std::ios::binary);
        out.write((const char*) bytes.data(), (std::streamsize) count);
        assert(out);
    };
    write(bytes.size());
    GgufExpertSource s;
    std::string err;
    assert(!s.open(path.string(), 0, experts, err));
    assert(!s.open(path.string(), layers, -1, err));
    assert(!s.open(path.string(), std::numeric_limits<int64_t>::max(), experts, err));
    assert(s.open(path.string(), layers, experts, err));
    assert(s.model_inputs().size() == 3 * layers);
    assert(s.model_inputs()[0] == std::make_pair(std::string("expert blk.0.ffn_gate"), path.string()));
    assert(s.model_inputs().back() == std::make_pair(std::string("expert blk.1.ffn_down"), path.string()));
    auto check = [&](int l, int e) {
        const auto* p = s.blob(l, e); assert(p);
        for (int r = 0; r < 3; ++r)
            assert(!std::memcmp(p + r * piece, bytes.data() + lay.gguf_off[3 * l + r] + e * piece, piece));
    };
    // Independent destinations exercise concurrent positional reads on the same file handle.
    std::vector<std::thread> readers;
    for (int worker = 0; worker < 8; ++worker)
        readers.emplace_back([&, worker] {
            uint8_t gathered[piece * 3];
            for (int i = 0; i < layers * experts; ++i) {
                const int l = (i + worker) % layers, e = (i * 997 + worker * 19) % experts;
                assert(s.read_into(l, e, gathered, sizeof gathered));
                for (int r = 0; r < 3; ++r)
                    assert(!std::memcmp(gathered + r * piece, bytes.data() + lay.gguf_off[3 * l + r] + e * piece, piece));
            }
        });
    for (auto& reader : readers) reader.join();
    for (int round = 0; round < 3; ++round)
        for (int l = 0; l < layers; ++l)
            for (int e = 0; e < experts; ++e) {
                check(l, e); const auto n = s.file_reads(); check(l, e); assert(s.file_reads() == n);
            }
    assert(!s.blob(-1, 0) && !s.blob(0, experts));
    s.close(); assert(!s.blob(0, 0) && s.model_inputs().empty());
    assert(s.open(path.string(), layers, experts, err));
    check(0, 0); assert(s.file_reads() == 1);
    for (int e = 1; e < 512; ++e) check(0, e);
    const int64_t resident[] = {0, 511};
    s.warm(0, resident, 2);
    assert(s.warmed() == 0);
    if (argc > 1 && !std::strcmp(argv[1], "--mirror")) {
        // Opt-in: USM allocation needs a GPU runtime; the default ring check stays CPU-only.
        const std::vector<std::pair<int64_t, int64_t>> ranked{{1, 599}, {0, 17}, {1, 2}, {0, 511}};
        assert(s.mirror(ranked, 3 * 256, 4, err) == 3);
        assert(s.mirrored_bytes() == 3 * 256 && !s.pinned(0, 511));
        const uint8_t* previous = nullptr;
        for (size_t i = 0; i < 3; ++i) {
            const auto [l, e] = ranked[i];
            check((int) l, (int) e);
            const auto* p = s.blob(l, e);
            assert(s.pinned(l, e) && (!previous || p == previous + 256));
            previous = p;
        }
        // A failed replacement must leave the existing mirror usable.
        write(bytes.size() - 1);
        assert(s.mirror(ranked, 4 * 256, 2, err) == -1);
        check(1, 599);
        assert(s.mirrored_bytes() == 3 * 256 && !s.pinned(0, 511));
        write(bytes.size());
        assert(s.mirror(ranked, 4 * 256, 1, err) == 4);
        for (const auto& [l, e] : ranked) check((int) l, (int) e);
        std::puts("gguf_expert_source_test: PASS (ranked cap, bytes, addresses, failed replacement)");
    } else if (argc > 1) {
        constexpr int queries = 4000000;
        uint64_t sum = 0;
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < queries; ++i) {
            const auto* p = s.blob(0, (i * 997u) % 512); assert(p); sum += p[1];
        }
        const double ns = std::chrono::duration<double, std::nano>(
            std::chrono::steady_clock::now() - start).count() / queries;
        std::printf("BENCH ring hits ns/lookup=%.2f checksum=%llu\n", ns, (unsigned long long) sum);
    }
    s.close();
    write(bytes.size() - 1);
    assert(s.open(path.string(), layers, experts, err));
    assert(!s.blob(layers - 1, experts - 1));
    assert(!s.blob(layers - 1, experts - 1));
    write(bytes.size()); check(layers - 1, experts - 1); // retry a failed key without clearing the index
    s.close();
    assert(s.open(path.string(), layers, experts, err)); check(layers - 1, experts - 1);
    s.close(); std::filesystem::remove(path);
    std::puts("gguf_expert_source_test: PASS (geometry, bytes, hits, wrap, failure, reopen)");
    return 0;
}
