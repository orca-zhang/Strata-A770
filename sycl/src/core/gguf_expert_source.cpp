// src/core/gguf_expert_source.cpp - see the header. Plain C++, no device code.
#include "strata/core/gguf_expert_source.hpp"
#include "strata/kernels/cpu/expert_layout.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <future>
#include <limits>
#include <fcntl.h>
#ifdef _WIN32
// No unistd.h / pread on Windows: overlapped file handles permit independent positional reads.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif
#include <dpct/dpct.hpp>
#include <sycl/sycl.hpp>
#include <atomic>

namespace strata::core {

#ifdef _WIN32
namespace {
// A synchronous file handle serializes ReadFile calls even with explicit OVERLAPPED offsets.
// Each outstanding read needs its own event; finish it before the stack OVERLAPPED goes away.
int64_t portable_pread(int fd, void* buf, size_t count, uint64_t offset) {
    HANDLE h = (HANDLE) _get_osfhandle(fd);
    if (h == INVALID_HANDLE_VALUE) return -1;
    OVERLAPPED ov{};
    ov.Offset = (DWORD) (offset & 0xFFFFFFFFull);
    ov.OffsetHigh = (DWORD) (offset >> 32);
    ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (ov.hEvent == nullptr) return -1;
    DWORD done = 0;
    bool ok = ReadFile(h, buf, (DWORD) std::min<size_t>(count, MAXDWORD), &done, &ov) != FALSE;
    if (ok || GetLastError() == ERROR_IO_PENDING) ok = GetOverlappedResult(h, &ov, &done, TRUE) != FALSE;
    const DWORD error = ok ? ERROR_SUCCESS : GetLastError();
    CloseHandle(ov.hEvent);
    return ok || error == ERROR_HANDLE_EOF ? (int64_t) done : -1;
}
int portable_open(const char* name) {
    HANDLE h = CreateFileA(name, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
    if (h == INVALID_HANDLE_VALUE) return -1;
    const int fd = _open_osfhandle((intptr_t) h, _O_RDONLY | _O_BINARY);
    if (fd < 0) CloseHandle(h);
    return fd;
}
int portable_close(int fd) { return _close(fd); }
}  // namespace
#else
namespace {
int64_t portable_pread(int fd, void* buf, size_t count, uint64_t offset) {
    return ::pread(fd, buf, count, (off_t) offset);
}
int portable_open(const char* name) { return ::open(name, O_RDONLY | O_CLOEXEC); }
int portable_close(int fd) { return ::close(fd); }
}  // namespace
#endif

namespace {
constexpr size_t kRing = 512;   // blobs alive at once: the prompt path holds a layer's worth of streamed experts
static_assert(kRing <= std::numeric_limits<uint16_t>::max()); // where_ stores slot + 1
// ponytail: stay below Arc's 4 GiB USM allocation ceiling; make configurable only if another driver needs it.
constexpr uint64_t kMirrorChunk = 2ull << 30;

int gguf_lookahead_reads() {
    static const int v = [] {
        const char* e = std::getenv("STRATA_LOOKAHEAD_READ");
        if (!e || !*e) return 8;
        const int n = std::atoi(e);
        return n < 0 ? 0 : n > 128 ? 128 : n;
    }();
    return v;
}
}

GgufExpertSource::~GgufExpertSource() { close(); }

void GgufExpertSource::close() {
    inputs_.clear();
    for (uint8_t* chunk : mirror_chunks_) sycl::free(chunk, dpct::get_in_order_queue());
    mirror_chunks_.clear(); mirror_bytes_ = 0; mirror_count_ = 0; warmed_.store(0, std::memory_order_relaxed);
    warm_hits_.store(0, std::memory_order_relaxed);
    file_reads_.store(0, std::memory_order_relaxed);
    warm_stamp_.reset();
    epoch_.store(0, std::memory_order_relaxed);
    last_layer_.store(-1, std::memory_order_relaxed);
    mirror_ptr_.clear(); layer_first_.clear();
    for (int fd : fds_) if (fd >= 0) portable_close(fd);
    fds_.clear(); names_.clear(); layer_fd_.clear(); ring_.clear(); ring_key_.clear(); where_.clear();
    ring_next_ = 0;
    reads_ = 0;
    n_layers_ = n_expert_ = 0;
}

bool GgufExpertSource::open(const std::string& shard1, int64_t n_layers, int64_t n_expert, std::string& err) {
    close();
    if (n_layers <= 0 || n_expert <= 0 ||
        (uint64_t) n_layers > (uint64_t) std::numeric_limits<int64_t>::max() / 3 ||
        (uint64_t) n_layers > where_.max_size() / (uint64_t) n_expert) {
        err = "--stream-experts: invalid expert geometry";
        return false;
    }
    const auto& lay = strata::kernels::cpu::expert_layout();
    if (!lay.native || lay.gguf_off.size() < (size_t) (3 * n_layers)) {
        err = "--stream-experts needs a native (IQ) pack whose native_experts.txt carries the GGUF tensor offsets";
        return false;
    }
    for (int64_t l = 0; l < n_layers; ++l)
        for (int r = 0; r < 3; ++r)
            if (lay.gguf_off[(size_t) (3 * l + r)] == 0) {
                err = "--stream-experts: layer " + std::to_string(l) + " has no GGUF offset for its experts";
                return false;
            }
    shard_ = shard1;
    const size_t cut = shard1.find_last_of("/\\");
    dir_ = cut == std::string::npos ? std::string() : shard1.substr(0, cut + 1);
    n_layers_ = n_layers;
    n_expert_ = n_expert;
    layer_fd_.assign((size_t) (3 * n_layers), -1);
    for (int64_t l = 0; l < n_layers; ++l)
        for (int r = 0; r < 3; ++r)
            if (fd_of(l, r, err) < 0) { close(); return false; }
    ring_.resize(kRing);
    for (auto& b : ring_) b.resize((size_t) lay.max_blob);
    ring_key_.assign(kRing, -1);
    where_.assign((size_t) (n_layers * n_expert), 0);
    warm_stamp_.reset(new std::atomic<uint32_t>[(size_t) (n_layers * n_expert)]());
    static const char* roles[] = {"gate", "up", "down"};
    for (int64_t l = 0; l < n_layers; ++l)
        for (int r = 0; r < 3; ++r)
            inputs_.push_back({"expert blk." + std::to_string(l) + ".ffn_" + roles[r],
                               names_[(size_t) layer_fd_[(size_t) (3 * l + r)]]});
    return true;
}

// The file of role `role` (0 gate, 1 up, 2 down) of `layer`. ExpertLayout::gguf_file is per layer AND role
// (`3 * layer + role`) since upstream 0.1.31; indexing it by layer alone read a shard-2 layer from shard 1 (Swift 1.5,
// whose layers 13-47 are in shard 2: garbage IQ1_M scales, NaN logits - 2026-10-01).
int GgufExpertSource::fd_of(int64_t layer, int role, std::string& err) {
    const size_t i = (size_t) (3 * layer + role);
    if (layer_fd_[i] >= 0) return fds_[(size_t) layer_fd_[i]];
    const auto& lay = strata::kernels::cpu::expert_layout();
    std::string name = shard_;
    if (lay.gguf_file.size() > i && !lay.gguf_file[i].empty()) name = dir_ + lay.gguf_file[i];
    for (size_t k = 0; k < names_.size(); ++k)
        if (names_[k] == name) { layer_fd_[i] = (int) k; return fds_[k]; }
    const int fd = portable_open(name.c_str());
    if (fd < 0) { err = "--stream-experts: cannot open " + name; return -1; }
    names_.push_back(name);
    fds_.push_back(fd);
    layer_fd_[i] = (int) fds_.size() - 1;
    return fd;
}

const uint8_t* GgufExpertSource::blob(int64_t layer, int64_t expert) {
    if (layer < 0 || layer >= n_layers_ || expert < 0 || expert >= n_expert_ || ring_.empty()) return nullptr;
    if (!mirror_ptr_.empty()) {
        const uint8_t* p = mirror_ptr_[(size_t) (layer * n_expert_ + expert)];
        if (p != nullptr) return p;
    }
    const auto& lay = strata::kernels::cpu::expert_layout();
    const auto& fm = lay.fmt[(size_t) layer];
    const uint64_t blob = lay.bytes[(size_t) layer];
    // the arena loader's gather, for one expert: [gate | up | down] from the three tensors
    const uint64_t per[3] = {fm.up_off, fm.up_off, blob - fm.down_off};
    const uint64_t at[3] = {0, fm.up_off, fm.down_off};
    const int64_t key = layer * n_expert_ + expert;
    size_t slot;
    {
        std::lock_guard<std::mutex> lk(mu_);
        if (where_[(size_t) key] != 0) { ++reads_; return ring_[where_[(size_t) key] - 1].data(); }
        slot = ring_next_;
        ring_next_ = (ring_next_ + 1) % ring_.size();
        if (ring_key_[slot] >= 0) where_[(size_t) ring_key_[slot]] = 0;
        ring_key_[slot] = key;
        where_[(size_t) key] = (uint16_t) (slot + 1);
    }
    std::vector<uint8_t>& buf = ring_[slot];
    for (int r = 0; r < 3; ++r) {
        const int fd = fds_[(size_t) layer_fd_[(size_t) (3 * layer + r)]];
        const uint64_t src = lay.gguf_off[(size_t) (3 * layer + r)] + per[r] * (uint64_t) expert;
        uint64_t done = 0;
        while (done < per[r]) {
            const int64_t n = portable_pread(fd, buf.data() + at[r] + done, (size_t) (per[r] - done), src + done);
            if (n <= 0) { std::lock_guard<std::mutex> lk(mu_); where_[(size_t) key] = 0; ring_key_[slot] = -1; return nullptr; }
            done += (uint64_t) n;
        }
    }
    file_reads_.fetch_add(1, std::memory_order_relaxed);
    if (warm_stamp_) {
        const uint32_t s = warm_stamp_[(size_t) (layer * n_expert_ + expert)].load(std::memory_order_relaxed);
        const uint64_t epoch = epoch_.load(std::memory_order_relaxed);
        if (s != 0 && (uint64_t) s + 3 >= epoch + 1) warm_hits_.fetch_add(1, std::memory_order_relaxed);
    }
    ++reads_;
    return buf.data();
}

int64_t GgufExpertSource::mirror(const std::vector<std::pair<int64_t, int64_t>>& pairs, uint64_t cap, int threads,
                                 std::string& err) {
    const auto& lay = strata::kernels::cpu::expert_layout();
    std::vector<std::pair<int64_t, int64_t>> take;
    std::vector<std::pair<size_t, uint64_t>> loc;
    std::vector<uint64_t> chunk_bytes;
    uint64_t total = 0;
    for (const auto& [l, e] : pairs) {
        if (l < 0 || l >= n_layers_ || e < 0 || e >= n_expert_) continue;
        const uint64_t b = (lay.bytes[(size_t) l] + 255) / 256 * 256;
        if (b > cap - total) break;
        if (chunk_bytes.empty() || chunk_bytes.back() + b > kMirrorChunk) chunk_bytes.push_back(0);
        take.push_back({l, e});
        loc.push_back({chunk_bytes.size() - 1, chunk_bytes.back()});
        chunk_bytes.back() += b;
        total += b;
    }
    if (take.empty()) return 0;
    const auto started = std::chrono::steady_clock::now();
    std::vector<uint8_t*> chunks;
    chunks.reserve(chunk_bytes.size());
    std::atomic<bool> bad{false};
    bool failed = false;
    double allocate_ms = 0, read_ms = 0;
    std::future<void> reading;
    try {
        size_t begin = 0;
        for (size_t c = 0; c < chunk_bytes.size() && !bad.load(); ++c) {
            const auto tm = std::chrono::steady_clock::now();
            uint8_t* chunk = (uint8_t*) sycl::malloc_host(chunk_bytes[c], dpct::get_in_order_queue());
            allocate_ms += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tm).count();
            if (chunk == nullptr) {
                err = "mirror: no pinned host memory for " + std::to_string(chunk_bytes[c] >> 20) +
                      " MiB chunk (" + std::to_string(total >> 20) + " MiB total)";
                failed = true;
                break;
            }
            chunks.push_back(chunk);
            // ponytail: one chunk reads while the main thread allocates the next; no staging copy or memory pool.
            if (reading.valid()) reading.get();
            if (bad.load()) break;
            size_t end = begin;
            while (end < loc.size() && loc[end].first == c) ++end;
            reading = std::async(std::launch::async, [&, chunk, begin, end] {
                const auto tr = std::chrono::steady_clock::now();
                std::atomic<size_t> next{begin};
                std::vector<std::future<void>> readers;
                for (int i = 0; i < std::max(1, threads); ++i)
                    readers.emplace_back(std::async(std::launch::async, [&] {
                        for (size_t j; (j = next.fetch_add(1)) < end && !bad.load();) {
                            const auto [l, e] = take[j];
                            if (!read_into(l, e, chunk + loc[j].second, (size_t) lay.bytes[(size_t) l]))
                                bad.store(true);
                        }
                    }));
                for (auto& reader : readers) reader.get();
                read_ms += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tr).count();
            });
            begin = end;
        }
        if (reading.valid()) reading.get();
    } catch (const std::exception& ex) {
        err = std::string("mirror: ") + ex.what();
        failed = true;
    }
    // Even an allocation failure must finish the previous chunk's readers before freeing their destinations.
    if (reading.valid()) reading.wait();
    if (failed || bad.load()) {
        for (uint8_t* chunk : chunks) sycl::free(chunk, dpct::get_in_order_queue());
        if (!failed) err = "mirror: reading an expert from the GGUF failed";
        return -1;
    }
    std::fprintf(stderr, "strata mirror: allocated %.1f ms, read %.1f ms, total %.1f ms (overlapped)\n",
                 allocate_ms, read_ms,
                 std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count());
    for (uint8_t* chunk : mirror_chunks_) sycl::free(chunk, dpct::get_in_order_queue());
    mirror_chunks_ = std::move(chunks);
    mirror_bytes_ = total;
    mirror_count_ = (int64_t) take.size();
    mirror_ptr_.assign((size_t) (n_layers_ * n_expert_), nullptr);
    layer_first_.assign((size_t) n_layers_, nullptr);
    for (size_t j = 0; j < take.size(); ++j) {
        const auto [l, e] = take[j];
        const uint8_t* p = mirror_chunks_[loc[j].first] + loc[j].second;
        mirror_ptr_[(size_t) (l * n_expert_ + e)] = p;
        if (layer_first_[(size_t) l] == nullptr) layer_first_[(size_t) l] = p;
    }
    return (int64_t) take.size();
}

bool GgufExpertSource::pinned(int64_t layer, int64_t expert) const {
    if (mirror_ptr_.empty() || layer < 0 || layer >= n_layers_ || expert < 0 || expert >= n_expert_) return false;
    return mirror_ptr_[(size_t) (layer * n_expert_ + expert)] != nullptr;
}

const uint8_t* GgufExpertSource::device_alias(int64_t layer, int64_t expert) const {
    if (mirror_ptr_.empty() || layer < 0 || layer >= n_layers_) return nullptr;
    if (expert >= 0 && expert < n_expert_) {
        const uint8_t* p = mirror_ptr_[(size_t) (layer * n_expert_ + expert)];
        if (p != nullptr) return p;
    }
    return layer_first_[(size_t) layer];
}

void GgufExpertSource::begin_layer(int64_t layer, const int32_t* ids, int64_t k) {
    (void) ids;
    (void) k;
    if (last_layer_.exchange(layer, std::memory_order_relaxed) != layer)
        epoch_.fetch_add(1, std::memory_order_relaxed);
}

void GgufExpertSource::warm(int64_t layer, const int64_t* experts, int64_t n) {
    if (layer < 0 || layer >= n_layers_ || experts == nullptr || n <= 0) return;
    const int read_limit = gguf_lookahead_reads();
    if (read_limit <= 0) return;
    const uint32_t stamp = (uint32_t) epoch_.load(std::memory_order_relaxed) + 1;
    static thread_local std::vector<uint8_t> scratch;
    int reads = 0;
    for (int64_t i = 0; i < n && reads < read_limit; ++i) {
        const int64_t e = experts[i];
        if (e < 0 || e >= n_expert_) continue;
        if (!mirror_ptr_.empty() && mirror_ptr_[(size_t) (layer * n_expert_ + e)] != nullptr) continue;
        const size_t index = (size_t) (layer * n_expert_ + e);
        if (warm_stamp_) {
            const uint32_t s = warm_stamp_[index].load(std::memory_order_relaxed);
            if (s != 0 && (uint64_t) s + 3 >= stamp) continue;
        }
        {
            const int64_t key = layer * n_expert_ + e;
            std::lock_guard<std::mutex> lk(mu_);
            if (where_[(size_t) key] != 0) continue;
        }
        const size_t max_blob = (size_t) strata::kernels::cpu::expert_layout().max_blob;
        if (scratch.size() < max_blob) scratch.resize(max_blob);
        // ponytail: warm by cached read; no ring insertion, no residency change, no half-filled blob race.
        if (read_into(layer, e, scratch.data(), scratch.size())) {
            if (warm_stamp_) warm_stamp_[index].store(stamp, std::memory_order_relaxed);
            warmed_.fetch_add(1, std::memory_order_relaxed);
            ++reads;
        }
    }
}

bool GgufExpertSource::warms() const {
    return gguf_lookahead_reads() > 0 && n_layers_ > 0 && mirror_count_ < n_layers_ * n_expert_;
}

int64_t GgufExpertSource::warm_candidates() const {
    const int reads = gguf_lookahead_reads();
    // ponytail: 4x read slack covers mirrored/ring skips without paying for all 8 submitted tokens.
    return reads > 0 ? (int64_t) reads * 4 : 0;
}

bool GgufExpertSource::read_into(int64_t layer, int64_t expert, uint8_t* dst, size_t bytes) const {
    if (layer < 0 || layer >= n_layers_ || expert < 0 || expert >= n_expert_ || dst == nullptr) return false;
    const auto& lay = strata::kernels::cpu::expert_layout();
    const auto& fm = lay.fmt[(size_t) layer];
    const uint64_t blob = lay.bytes[(size_t) layer];
    if (bytes < blob) return false;
    const uint64_t per[3] = {fm.up_off, fm.up_off, blob - fm.down_off};
    const uint64_t at[3] = {0, fm.up_off, fm.down_off};
    for (int r = 0; r < 3; ++r) {
        const int fd = fds_[(size_t) layer_fd_[(size_t) (3 * layer + r)]];
        const uint64_t src = lay.gguf_off[(size_t) (3 * layer + r)] + per[r] * (uint64_t) expert;
        uint64_t done = 0;
        while (done < per[r]) {
            const int64_t n = portable_pread(fd, dst + at[r] + done, (size_t) (per[r] - done), src + done);
            if (n <= 0) return false;
            done += (uint64_t) n;
        }
    }
    return true;
}

}  // namespace strata::core
