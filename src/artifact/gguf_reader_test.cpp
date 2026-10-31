// src/artifact/gguf_reader_test.cpp - the reader refuses what it cannot arbitrate (synthetic GGUF, no model,
// no GPU).
//
//   1. two tensors with distinct names: the file opens and find() sees both;
//   2. the same name twice: refused at open, and the error names the tensor and the file - GGUF has no
//      index to say which of the two a name means, so the lookup must not silently pick one.
//
// The fixture is a minimal GGUF v3 written here (header, no metadata, two F32[8] tensors, 32-byte
// alignment), so the test needs neither gguf-py nor a shard.  The file is closed before it is removed:
// on Windows an open mapping keeps it.
#include "strata/artifact/gguf_reader.hpp"

#include <cstdint>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

namespace {
int g_fail = 0;
void check(bool ok, const char* what) {
    std::printf("  %-66s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok) ++g_fail;
}

template <class T> void put(std::vector<uint8_t>& b, T v) {
    const auto n = b.size();
    b.resize(n + sizeof(T));
    std::memcpy(b.data() + n, &v, sizeof(T));
}
void put_str(std::vector<uint8_t>& b, const std::string& s) {
    put<uint64_t>(b, s.size());
    b.insert(b.end(), s.begin(), s.end());
}

// A GGUF v3 file of F32[8] tensors named `names`, laid out 32 bytes apart from a 32-byte-aligned data start.
std::filesystem::path write_gguf(const std::vector<std::string>& names) {
    std::vector<uint8_t> b;
    put<uint32_t>(b, 0x46554747u);   // "GGUF"
    put<uint32_t>(b, 3);
    put<uint64_t>(b, names.size());  // n_tensors
    put<uint64_t>(b, 0);             // n_kv
    for (size_t i = 0; i < names.size(); ++i) {
        put_str(b, names[i]);
        put<uint32_t>(b, 1);         // n_dims
        put<uint64_t>(b, 8);
        put<uint32_t>(b, 0);         // F32
        put<uint64_t>(b, 32 * i);    // offset from data_start
    }
    b.resize((b.size() + 31) / 32 * 32 + 32 * names.size(), 0);
    const auto path = std::filesystem::temp_directory_path() / "strata_gguf_reader_test.gguf";
    std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char*>(b.data()), (std::streamsize)b.size());
    return path;
}
}  // namespace

int main(int argc, char** argv) {
    std::printf("gguf_reader_test\n");
    {
        const auto path = write_gguf({"blk.0.attn_q.weight", "blk.0.attn_k.weight"});
        std::string err;
        size_t n = 0;
        bool both = false;
        try {
            strata::GgufFile g(path.string());
            n = g.tensors().size();
            both = g.find("blk.0.attn_q.weight") && g.find("blk.0.attn_k.weight");
        } catch (const std::exception& e) {
            err = e.what();
        }
        std::filesystem::remove(path);
        check(err.empty() && n == 2 && both, "distinct names: the file opens and find() sees both tensors");
    }
    {
        const auto path = write_gguf({"blk.0.attn_q.weight", "blk.0.attn_q.weight"});
        std::string err;
        try {
            strata::GgufFile g(path.string());
        } catch (const std::exception& e) {
            err = e.what();
        }
        std::filesystem::remove(path);
        check(!err.empty(), "the same name twice: refused at open");
        check(err.find("blk.0.attn_q.weight") != std::string::npos, "  the error names the tensor");
        check(err.find(path.filename().string()) != std::string::npos, "  the error names the file");
        if (!err.empty()) std::printf("  (%s)\n", err.c_str());
    }
    {
        // Shuffled long names, SSO names and embedded NULs exercise borrowed-name lifetimes and exact comparisons.
        std::vector<std::string> names = {"", "a", "aa", "z", std::string("a\0b", 3), std::string("a\0c", 3)};
        for (int i = 0; i < 4096; ++i) names.push_back("blk." + std::to_string(i) + ".ffn_gate_exps.weight");
        std::shuffle(names.begin(), names.end(), std::mt19937(42));
        const auto path = write_gguf(names);
        {
            strata::GgufFile g(path.string());
            auto model = strata::GgufModel::open(path.string());
            auto moved = std::move(model);
            bool correct = true;
            for (size_t i = 0; i < names.size(); ++i) {
                const auto* t = g.find(names[i]);
                size_t shard = 99;
                const auto* m = moved.find(names[i], &shard);
                correct &= t == &g.tensors()[i] && t->offset == i * 32 &&
                           m == &moved.shard(0).tensors()[i] && shard == 0;
            }
            size_t shard = 99;
            check(correct, "shuffled directory: exact names, offsets, file order and model moves");
            const std::string wrapped = "[blk.17.ffn_gate_exps.weight]";
            const std::string_view borrowed(wrapped.data() + 1, wrapped.size() - 2);
            check(g.find(borrowed) && moved.find(borrowed), "borrowed, non-null-terminated names work without a string copy");
            check(!g.find("absent") && !g.find("zz") && !g.find(std::string("a\0", 2)) &&
                  !moved.find("absent", &shard) && shard == 99, "missing names and strict prefixes do not match");
            if (argc == 2 && std::strcmp(argv[1], "--bench") == 0) {
                constexpr size_t queries = 200000;
                auto bench = [&](const char* label, const auto& index) {
                    uint64_t sum = 0;
                    const auto start = std::chrono::steady_clock::now();
                    for (size_t i = 0; i < queries; ++i) {
                        const auto* t = index.find(names[(i * 997) % names.size()]);
                        if (t) sum += t->offset;
                    }
                    const double ns = std::chrono::duration<double, std::nano>(
                        std::chrono::steady_clock::now() - start).count() / queries;
                    std::printf("BENCH %s entries=%zu queries=%zu ns/lookup=%.2f checksum=%llu\n",
                                label, names.size(), queries, ns, (unsigned long long) sum);
                };
                bench("GgufFile", g);
                bench("GgufModel", moved);
            }
        }
        std::filesystem::remove(path);
    }
    {
        const auto path = write_gguf({});
        {
            strata::GgufFile g(path.string());
            strata::GgufModel model({path.string()});
            check(!g.find("") && !model.find(""), "empty tensor directories return nullptr");
        }
        std::filesystem::remove(path);
    }
    std::printf(g_fail ? "gguf_reader_test: %d FAILED\n" : "gguf_reader_test: all passed\n", g_fail);
    return g_fail ? 1 : 0;
}
