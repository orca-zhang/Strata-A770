// src/kernels/kv_stream_parity.cpp - KV streaming (kv_stream.hpp) against a fully resident pool (GPU, no model).
//
// Decodes a synthetic sequence twice: into a fully resident pool with the identity page table, and into a streamed
// layer (host copy + a small VRAM slot pool that must evict constantly). Every few cells a batch of 1-8 queries
// with selections shaped like the indexer's (whole 4-cell blocks, a partial threshold block, the tail block, half of
// them recent) is attended in both; `kv_stream_resolve` runs before the streamed attention. Checks:
//   1. the attention outputs are BITWISE equal (the streamed readers see exactly the resident values);
//   2. the residency map is consistent after every call (slot_block and page_table invert each other);
//   3. no call overflowed, and the hit/miss counters add up;
//   4. direct host-KV attention reads the host copy as an identity-layout pool;
//   5. a ring (the MTP drafter's layout) restored from the host copy reads the same values as the resident pool.
// INT8, FP16 and Q4_0 (PR #21) pools.
#define DPCT_PROFILING_ENABLED
#include <sycl/sycl.hpp>
#include <dpct/dpct.hpp>
#include "strata/kernels/kv_q4.hpp"
#include "strata/kernels/kv_q8.hpp"
#include "strata/kernels/kv_stream.hpp"
#include "strata/kernels/qsa.hpp"
#include "strata/kernels/qsa_decode_attn.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <set>
#include <string>
#include <vector>

namespace k = strata::kernels;

namespace {
int g_fail = 0;
void ck(dpct::err0 e, const char *w) {
    /*
    DPCT1009: SYCL reports errors using exceptions and does not use error
    codes. Please replace the "get_error_string_dummy(...)" with a real
    error-handling function.
    */
}
template <typename T> T* dalloc(size_t n) {
    T* p = nullptr;
    ck(DPCT_CHECK_ERROR(p = (T *)sycl::malloc_device(
                            n * sizeof(T) + 64, dpct::get_in_order_queue())),
       "malloc");
    ck(DPCT_CHECK_ERROR(
           (dpct::get_current_device().queues_wait_and_throw(), dpct::get_in_order_queue()).memset(p, 0, n * sizeof(T) + 64).wait()),
       "memset");
    return p;
}
template <typename T> T* halloc(size_t n) {   // pinned, mapped; returns the device pointer
    void* h = nullptr;
    void* d = nullptr;
    /*
    DPCT1048: The original value cudaHostAllocMapped is not meaningful in the
    migrated code and was removed or replaced with 0. You may need to check the
    migrated code.
    */
    ck(DPCT_CHECK_ERROR(h = (void *)sycl::malloc_host(
                            n * sizeof(T) + 64, dpct::get_in_order_queue())),
       "hostalloc");
    std::memset(h, 0, n * sizeof(T) + 64);
    ck(DPCT_CHECK_ERROR(d = (void *)h), "devptr");
    return (T*) d;
}

struct Pools {   // one K/V pool set of `pages` pages
    k::KvHostPools p;   // reused as a plain pointer bundle
    void alloc(int64_t pages, const k::QsaShapes& s, int fmt, bool host) {
        const size_t rows = (size_t) pages * s.n_head_kv * s.page_size;
        if (fmt == k::kKvQ4) {
            const size_t b = rows * k::kv_q4_bytes_per_head((int) s.head_dim);
            p.k_q4 = host ? halloc<uint8_t>(b) : dalloc<uint8_t>(b);
            p.v_q4 = host ? halloc<uint8_t>(b) : dalloc<uint8_t>(b);
        } else if (fmt == k::kKvInt8) {
            p.k_q = host ? halloc<int8_t>(rows * s.head_dim) : dalloc<int8_t>(rows * s.head_dim);
            p.v_q = host ? halloc<int8_t>(rows * s.head_dim) : dalloc<int8_t>(rows * s.head_dim);
            p.k_scale = host ? halloc<uint16_t>(rows * 4) : dalloc<uint16_t>(rows * 4);
            p.v_scale = host ? halloc<uint16_t>(rows * 4) : dalloc<uint16_t>(rows * 4);
        } else {
            p.k_pool = host ? halloc<uint16_t>(rows * s.head_dim) : dalloc<uint16_t>(rows * s.head_dim);
            p.v_pool = host ? halloc<uint16_t>(rows * s.head_dim) : dalloc<uint16_t>(rows * s.head_dim);
        }
    }
    k::QsaAttnPools attn(const int32_t* table) const {
        k::QsaAttnPools a;
        a.k_pool = p.k_pool; a.v_pool = p.v_pool; a.k_q = p.k_q; a.v_q = p.v_q; a.k_scale = p.k_scale;
        a.v_scale = p.v_scale; a.k_q4 = p.k_q4; a.v_q4 = p.v_q4; a.page_table = table;
        return a;
    }
};

void append(const Pools& pl, const int32_t* table, const int32_t* step, const float* kc, const float* vc,
            const k::QsaShapes& s, int fmt, const k::KvHostPools* host) {
    if (fmt == k::kKvQ4) k::kv_append_q4_step(pl.p.k_q4, pl.p.v_q4, table, step, kc, vc, s, nullptr, host);
    else if (fmt == k::kKvInt8) k::kv_append_q8_step(pl.p.k_q, pl.p.v_q, pl.p.k_scale, pl.p.v_scale, table, step, kc, vc, s, nullptr, host);
    else k::kv_append_step(pl.p.k_pool, pl.p.v_pool, table, step, kc, vc, s, nullptr, host);
}

// A selection like qsa_block_topk's: ascending cells, whole blocks, the tail block's cells, and one partial block.
std::vector<int32_t> selection(int64_t n_kv, int64_t width, std::mt19937& rng) {
    std::vector<int32_t> ids;
    if (n_kv <= width) {
        for (int64_t c = 0; c < n_kv; ++c) ids.push_back((int32_t) c);
        return ids;
    }
    const int64_t n_bid = n_kv / 4, tail = n_kv - n_bid * 4;
    std::set<int64_t> blocks;
    const int64_t want_full = (width - tail) / 4, part = (width - tail) % 4;
    std::uniform_int_distribution<int64_t> any(0, n_bid - 1), recent(std::max<int64_t>(0, n_bid - 2048), n_bid - 1);
    while ((int64_t) blocks.size() < want_full + (part ? 1 : 0)) blocks.insert(rng() % 2 ? recent(rng) : any(rng));
    int64_t partial_block = part ? *std::next(blocks.begin(), (long) (rng() % blocks.size())) : -1;
    for (int64_t b : blocks) {
        const int64_t take = b == partial_block ? part : 4;
        for (int64_t i = 0; i < take; ++i) ids.push_back((int32_t) (b * 4 + i));
    }
    for (int64_t c = n_bid * 4; c < n_kv; ++c) ids.push_back((int32_t) c);
    std::sort(ids.begin(), ids.end());
    return ids;
}

template <typename F> double time_ms(int it, F&& fn) {
    ck(DPCT_CHECK_ERROR(dpct::get_current_device().queues_wait_and_throw()), "bench wait");
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < it; ++i) fn(i);
    ck(DPCT_CHECK_ERROR(dpct::get_current_device().queues_wait_and_throw()), "bench wait");
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() / it;
}

void copy_q8_pool(const Pools& dst, const Pools& src, int64_t pages, const k::QsaShapes& s) {
    const size_t rows = (size_t) pages * s.n_head_kv * s.page_size;
    const size_t codes = rows * s.head_dim, scales = rows * (s.head_dim / k::KV_Q8_GROUP) * sizeof(uint16_t);
    auto& q = dpct::get_in_order_queue();
    ck(DPCT_CHECK_ERROR(q.memcpy(dst.p.k_q, src.p.k_q, codes)), "bench k copy");
    ck(DPCT_CHECK_ERROR(q.memcpy(dst.p.v_q, src.p.v_q, codes)), "bench v copy");
    ck(DPCT_CHECK_ERROR(q.memcpy(dst.p.k_scale, src.p.k_scale, scales)), "bench ks copy");
    ck(DPCT_CHECK_ERROR(q.memcpy(dst.p.v_scale, src.p.v_scale, scales)), "bench vs copy");
    ck(DPCT_CHECK_ERROR(q.wait()), "bench pool copy wait");
}

// Independent FP64 samples: comparing two readers alone cannot catch a shared attention bug.
bool attention_reference(const Pools& host, int fmt, const k::QsaShapes& s,
                         const std::vector<float>& q, const std::vector<int32_t>& ids,
                         const std::vector<int32_t>& steps, const std::vector<float>& out,
                         int n_q, int64_t cap) {
    auto load = [&](bool value, int cell, int head, int dim) -> double {
        const size_t row = (size_t(cell / s.page_size) * s.n_head_kv + head) * s.page_size + cell % s.page_size;
        auto half = [](uint16_t bits) { return double(sycl::bit_cast<sycl::half>(bits)); };
        if (fmt == k::kKvF16) return half((value ? host.p.v_pool : host.p.k_pool)[row * s.head_dim + dim]);
        if (fmt == k::kKvInt8)
            return (value ? host.p.v_q : host.p.k_q)[row * s.head_dim + dim] *
                   half((value ? host.p.v_scale : host.p.k_scale)[row * (s.head_dim / k::KV_Q8_GROUP) + dim / k::KV_Q8_GROUP]);
        const auto& block = reinterpret_cast<const k::block_q4_0*>(value ? host.p.v_q4 : host.p.k_q4)
                            [row * (s.head_dim / k::QK4_0) + dim / k::QK4_0];
        const int j = dim % k::QK4_0;
        return ((j < 16 ? block.qs[j] & 15 : block.qs[j - 16] >> 4) - 8) * half(block.d);
    };
    for (int b = 0; b < n_q; ++b) for (int h : {0, 11, 12, 23}) {
        const int width = steps[b * k::kStepCount + k::kStepWidth];
        const int kvh = h / (s.n_head / s.n_head_kv);
        std::vector<double> weights(width);
        double maximum = -1e300, normalizer = 0;
        for (int i = 0; i < width; ++i) {
            double dot = 0;
            for (int d = 0; d < s.head_dim; ++d)
                dot += q[(size_t(b) * s.n_head + h) * s.head_dim + d] * load(false, ids[b * cap + i], kvh, d);
            weights[i] = dot / std::sqrt(double(s.head_dim));
            maximum = std::max(maximum, weights[i]);
        }
        for (double& w : weights) { w = std::exp(w - maximum); normalizer += w; }
        for (int d : {0, 17, 255}) {
            double sum = 0;
            for (int i = 0; i < width; ++i) sum += weights[i] * load(true, ids[b * cap + i], kvh, d);
            const double want = normalizer > 0 ? sum / normalizer : 0;
            const float got = out[(size_t(b) * s.n_head + h) * s.head_dim + d];
            if (!std::isfinite(got) || std::abs(got - want) > 1e-4) return false;
        }
    }
    return true;
}

int bench_int8() {
    k::QsaShapes s = k::qsa_real_shapes();
    const int64_t N = 24576, n_blocks = (N + s.page_size - 1) / s.page_size, n_slots = 20480 / s.page_size;
    const int64_t cap = k::qsa_selection_width(k::kTopkMaxCells, s);
    const int S = 128, it = 1024;
    const int64_t H = s.n_head_kv, D = s.head_dim, NH = s.n_head;
    std::mt19937 rng(11);
    std::normal_distribution<float> nd(0.f, 0.03f);

    Pools ref, slots, host;
    ref.alloc(n_blocks, s, k::kKvInt8, false);
    slots.alloc(n_slots, s, k::kKvInt8, false);
    host.alloc(n_blocks, s, k::kKvInt8, true);
    const size_t rows = (size_t) n_blocks * H * s.page_size;
    for (size_t i = 0; i < rows * D; ++i) {
        host.p.k_q[i] = (int8_t) ((i * 17) % 127 - 63);
        host.p.v_q[i] = (int8_t) ((i * 29) % 127 - 63);
    }
    for (size_t i = 0; i < rows * (D / k::KV_Q8_GROUP); ++i) host.p.k_scale[i] = host.p.v_scale[i] = 0x3400;
    copy_q8_pool(ref, host, n_blocks, s);

    int32_t* ident = dalloc<int32_t>(n_blocks);
    {
        std::vector<int32_t> t(n_blocks);
        for (int64_t i = 0; i < n_blocks; ++i) t[i] = (int32_t) i;
        ck(DPCT_CHECK_ERROR(dpct::get_in_order_queue().memcpy(ident, t.data(), t.size() * 4).wait()), "bench ident");
    }
    k::KvStreamMap m;
    m.page_table = dalloc<int32_t>(n_blocks);
    m.slot_block = dalloc<int32_t>(n_slots); m.slot_stamp = dalloc<int32_t>(n_slots); m.slot_ref = dalloc<int32_t>(n_slots);
    m.miss_block = dalloc<int32_t>(n_slots); m.miss_slot = dalloc<int32_t>(n_slots);
    m.ctl = dalloc<int32_t>(k::kKvCtlInts);
    m.n_blocks = n_blocks; m.n_slots = n_slots;

    int32_t* ids = dalloc<int32_t>((size_t) S * cap);
    int32_t* steps = dalloc<int32_t>((size_t) S * k::kStepCount);
    float* q = dalloc<float>(NH * D);
    const uint64_t scr = k::qsa_decode_attn_scratch_floats(cap, s);
    float* scratch = dalloc<float>(scr);
    float* out = dalloc<float>(NH * D);
    std::vector<int32_t> hids((size_t) S * cap), hst((size_t) S * k::kStepCount);
    std::vector<float> hq(NH * D);
    for (int i = 0; i < S; ++i) {
        const auto sel = selection(N, cap, rng);
        std::copy(sel.begin(), sel.end(), hids.begin() + (size_t) i * cap);
        hst[(size_t) i * k::kStepCount + 0] = (int32_t) (N - 1);
        hst[(size_t) i * k::kStepCount + 1] = (int32_t) N;
        hst[(size_t) i * k::kStepCount + 2] = (int32_t) (N / s.idx_block);
        hst[(size_t) i * k::kStepCount + 3] = (int32_t) cap;
    }
    for (float& x : hq) x = nd(rng);
    auto& dq = dpct::get_in_order_queue();
    ck(DPCT_CHECK_ERROR(dq.memcpy(ids, hids.data(), hids.size() * 4).wait()), "bench ids");
    ck(DPCT_CHECK_ERROR(dq.memcpy(steps, hst.data(), hst.size() * 4).wait()), "bench steps");
    ck(DPCT_CHECK_ERROR(dq.memcpy(q, hq.data(), hq.size() * 4).wait()), "bench q");

    auto id0 = [&](int i) { return ids + (size_t) (i & (S - 1)) * cap; };
    auto st0 = [&](int i) { return steps + (size_t) (i & (S - 1)) * k::kStepCount; };
    const k::QsaAttnPools ref_attn = ref.attn(ident), slot_attn = slots.attn(m.page_table), host_attn = host.attn(ident);

    for (int i = 0; i < 16; ++i) k::qsa_decode_attn_batch(q, ref_attn, id0(i), st0(i), cap, s, scratch, out, 1, nullptr);
    const double resident = time_ms(it, [&](int i) {
        k::qsa_decode_attn_batch(q, ref_attn, id0(i), st0(i), cap, s, scratch, out, 1, nullptr);
    });
    for (int i = 0; i < 16; ++i) k::qsa_decode_attn_step(q, ref_attn, id0(i), st0(i), cap, s, scratch, out, nullptr);
    const double resident_step = time_ms(it, [&](int i) {
        k::qsa_decode_attn_step(q, ref_attn, id0(i), st0(i), cap, s, scratch, out, nullptr);
    });
    for (int i = 0; i < 16; ++i) k::qsa_decode_attn_batch(q, host_attn, id0(i), st0(i), cap, s, scratch, out, 1, nullptr);
    const double host_direct = time_ms(it, [&](int i) {
        k::qsa_decode_attn_batch(q, host_attn, id0(i), st0(i), cap, s, scratch, out, 1, nullptr);
    });
    k::kv_stream_reset(m, nullptr);
    for (int i = 0; i < 16; ++i) k::kv_stream_resolve(m, slot_attn, host.p, k::kKvInt8, id0(i), st0(i), 1, cap, s, nullptr);
    k::kv_stream_reset(m, nullptr);
    const double resolve_only = time_ms(it, [&](int i) {
        k::kv_stream_resolve(m, slot_attn, host.p, k::kKvInt8, id0(i), st0(i), 1, cap, s, nullptr);
    });
    k::kv_stream_reset(m, nullptr);
    for (int i = 0; i < 16; ++i) {
        k::kv_stream_resolve(m, slot_attn, host.p, k::kKvInt8, id0(i), st0(i), 1, cap, s, nullptr);
        k::qsa_decode_attn_batch(q, slot_attn, id0(i), st0(i), cap, s, scratch, out, 1, nullptr);
    }
    k::kv_stream_reset(m, nullptr);
    const double streamed = time_ms(it, [&](int i) {
        k::kv_stream_resolve(m, slot_attn, host.p, k::kKvInt8, id0(i), st0(i), 1, cap, s, nullptr);
        k::qsa_decode_attn_batch(q, slot_attn, id0(i), st0(i), cap, s, scratch, out, 1, nullptr);
    });
    k::kv_stream_reset(m, nullptr);
    const double streamed_step = time_ms(it, [&](int i) {
        k::kv_stream_resolve(m, slot_attn, host.p, k::kKvInt8, id0(i), st0(i), 1, cap, s, nullptr);
        k::qsa_decode_attn_step(q, slot_attn, id0(i), st0(i), cap, s, scratch, out, nullptr);
    });
    const k::KvStreamCounters c = k::kv_stream_counters(m);
    const double hit = c.lookups ? 100.0 * (double) (c.lookups - c.misses) / (double) c.lookups : 0.0;
    std::printf("kv_stream --bench int8: N=%lld cap=%lld slots=%lld calls=%d hit=%.2f%%\n",
                (long long) N, (long long) cap, (long long) n_slots, it, hit);
    std::printf("  resident attention batch %.4f ms/call\n", resident);
    std::printf("  resident attention step  %.4f ms/call\n", resident_step);
    std::printf("  streamed resolve only    %.4f ms/call\n", resolve_only);
    std::printf("  streamed resolve+batch   %.4f ms/call\n", streamed);
    std::printf("  streamed resolve+step    %.4f ms/call\n", streamed_step);
    std::printf("  direct host attention    %.4f ms/call\n", host_direct);
    return 0;
}

bool run(int fmt) {
    const char* name = fmt == k::kKvQ4 ? "q4_0" : fmt == k::kKvInt8 ? "int8" : "fp16";
    k::QsaShapes s = k::qsa_real_shapes();
    const int64_t N = 40000, n_blocks = (N + 3) / 4, n_slots = 8 * 516 + 700;   // must evict: slots < blocks
    const int64_t cap = k::qsa_selection_width(k::kTopkMaxCells, s), NQ = 8;
    const int64_t H = s.n_head_kv, D = s.head_dim, NH = s.n_head;
    std::mt19937 rng(7 + 4 * fmt);
    std::normal_distribution<float> nd(0.f, 1.f);

    Pools ref, slots, host;
    ref.alloc(n_blocks, s, fmt, false);
    slots.alloc(n_slots, s, fmt, false);
    host.alloc(n_blocks, s, fmt, true);
    int32_t* ident = dalloc<int32_t>(n_blocks);
    {
        std::vector<int32_t> t(n_blocks);
        for (int64_t i = 0; i < n_blocks; ++i) t[i] = (int32_t) i;
        /*
        DPCT1114: cudaMemcpy is migrated to asynchronization memcpy,
        assuming in the original code the source host memory is pageable memory.
        If the memory is not pageable, call wait() on event return by memcpy API
        to ensure synchronization behavior.
        */
        ck(DPCT_CHECK_ERROR((dpct::get_current_device().queues_wait_and_throw(), dpct::get_in_order_queue()).memcpy(ident, t.data(),
                                                              n_blocks * 4).wait()),
           "ident");
    }
    k::KvStreamMap m;
    m.page_table = dalloc<int32_t>(n_blocks);
    m.slot_block = dalloc<int32_t>(n_slots); m.slot_stamp = dalloc<int32_t>(n_slots); m.slot_ref = dalloc<int32_t>(n_slots);
    m.miss_block = dalloc<int32_t>(n_slots); m.miss_slot = dalloc<int32_t>(n_slots);
    m.ctl = dalloc<int32_t>(k::kKvCtlInts);
    m.n_blocks = n_blocks; m.n_slots = n_slots;
    k::kv_stream_reset(m, nullptr);

    float* kc = dalloc<float>(H * D);
    float* vc = dalloc<float>(H * D);
    int32_t* step = dalloc<int32_t>(k::kStepCount);
    int32_t* steps = dalloc<int32_t>(NQ * k::kStepCount);
    int32_t* ids = dalloc<int32_t>(NQ * cap);
    float* q = dalloc<float>(NQ * NH * D);
    const uint64_t scr = k::qsa_decode_attn_scratch_floats(cap, s);
    float* scratch = dalloc<float>(NQ * scr);
    float* out_ref = dalloc<float>(NQ * NH * D);
    float* out_str = dalloc<float>(NQ * NH * D);
    std::vector<float> hk(H * D), hv(H * D), hq(NQ * NH * D), a((size_t) NQ * NH * D), b2((size_t) NQ * NH * D);
    std::vector<int32_t> pt(n_blocks), sb(n_slots);

    int batches = 0, bad = 0;
    bool checked_host_attn = false;
    for (int64_t pos = 0; pos < N; ++pos) {
        for (auto& x : hk) x = nd(rng) * ((pos % 17 == 0) ? 30.f : 1.f);
        for (auto& x : hv) x = nd(rng);
        const int32_t st[4] = {(int32_t) pos, (int32_t) (pos + 1), (int32_t) ((pos + 1) / 4), 0};
        /*
        DPCT1114: cudaMemcpy is migrated to asynchronization memcpy,
        assuming in the original code the source host memory is pageable memory.
        If the memory is not pageable, call wait() on event return by memcpy API
        to ensure synchronization behavior.
        */
        ck(DPCT_CHECK_ERROR(
               (dpct::get_current_device().queues_wait_and_throw(), dpct::get_in_order_queue()).memcpy(kc, hk.data(), hk.size() * 4).wait()),
           "k");
        /*
        DPCT1114: cudaMemcpy is migrated to asynchronization memcpy,
        assuming in the original code the source host memory is pageable memory.
        If the memory is not pageable, call wait() on event return by memcpy API
        to ensure synchronization behavior.
        */
        ck(DPCT_CHECK_ERROR(
               (dpct::get_current_device().queues_wait_and_throw(), dpct::get_in_order_queue()).memcpy(vc, hv.data(), hv.size() * 4).wait()),
           "v");
        /*
        DPCT1114: cudaMemcpy is migrated to asynchronization memcpy,
        assuming in the original code the source host memory is pageable memory.
        If the memory is not pageable, call wait() on event return by memcpy API
        to ensure synchronization behavior.
        */
        ck(DPCT_CHECK_ERROR(
               (dpct::get_current_device().queues_wait_and_throw(), dpct::get_in_order_queue()).memcpy(step, st, sizeof(st)).wait()),
           "step");
        append(ref, ident, step, kc, vc, s, fmt, nullptr);
        append(slots, m.page_table, step, kc, vc, s, fmt, &host.p);
        if (pos % 131 != 130 && pos != N - 1) continue;
        // a batch: n_q queries at the last n_q positions (as a verify window), each with its own selection
        const int n_q = 1 + (int) (rng() % NQ);
        std::vector<int32_t> hids((size_t) NQ * cap, 0), hst(NQ * 4, 0);
        for (int t = 0; t < n_q; ++t) {
            const int64_t p = std::max<int64_t>(0, pos - (n_q - 1 - t)), n_kv = p + 1;
            const int64_t width = std::min<int64_t>(n_kv, cap);
            const std::vector<int32_t> sel = selection(n_kv, width, rng);
            if ((int64_t) sel.size() != width) { std::fprintf(stderr, "selection size %zu != %lld\n", sel.size(), (long long) width); return false; }
            std::copy(sel.begin(), sel.end(), hids.begin() + (size_t) t * cap);
            hst[t * 4 + 0] = (int32_t) p; hst[t * 4 + 1] = (int32_t) n_kv; hst[t * 4 + 2] = (int32_t) (n_kv / 4);
            hst[t * 4 + 3] = (int32_t) width;
        }
        for (auto& x : hq) x = nd(rng);
        /*
        DPCT1114: cudaMemcpy is migrated to asynchronization memcpy,
        assuming in the original code the source host memory is pageable memory.
        If the memory is not pageable, call wait() on event return by memcpy API
        to ensure synchronization behavior.
        */
        ck(DPCT_CHECK_ERROR((dpct::get_current_device().queues_wait_and_throw(), dpct::get_in_order_queue()).memcpy(ids, hids.data(),
                                                              hids.size() * 4).wait()),
           "ids");
        /*
        DPCT1114: cudaMemcpy is migrated to asynchronization memcpy,
        assuming in the original code the source host memory is pageable memory.
        If the memory is not pageable, call wait() on event return by memcpy API
        to ensure synchronization behavior.
        */
        ck(DPCT_CHECK_ERROR((dpct::get_current_device().queues_wait_and_throw(), dpct::get_in_order_queue()).memcpy(steps, hst.data(),
                                                              hst.size() * 4).wait()),
           "steps");
        /*
        DPCT1114: cudaMemcpy is migrated to asynchronization memcpy,
        assuming in the original code the source host memory is pageable memory.
        If the memory is not pageable, call wait() on event return by memcpy API
        to ensure synchronization behavior.
        */
        ck(DPCT_CHECK_ERROR(
               (dpct::get_current_device().queues_wait_and_throw(), dpct::get_in_order_queue()).memcpy(q, hq.data(), hq.size() * 4).wait()),
           "q");
        k::qsa_decode_attn_batch(q, ref.attn(ident), ids, steps, cap, s, scratch, out_ref, n_q, nullptr);
        k::kv_stream_resolve(m, slots.attn(m.page_table), host.p, fmt, ids, steps, n_q, cap, s, nullptr);
        k::qsa_decode_attn_batch(q, slots.attn(m.page_table), ids, steps, cap, s, scratch, out_str, n_q, nullptr);
        ck(DPCT_CHECK_ERROR(dpct::get_current_device().queues_wait_and_throw()),
           "batch");
        ck(DPCT_CHECK_ERROR(
               (dpct::get_current_device().queues_wait_and_throw(), dpct::get_in_order_queue())
                   .memcpy(a.data(), out_ref, (size_t)n_q * NH * D * 4)
                   .wait()),
           "a");
        ck(DPCT_CHECK_ERROR(
               (dpct::get_current_device().queues_wait_and_throw(), dpct::get_in_order_queue())
                   .memcpy(b2.data(), out_str, (size_t)n_q * NH * D * 4)
                   .wait()),
           "b");
        if (std::memcmp(a.data(), b2.data(), (size_t) n_q * NH * D * 4) != 0) {
            if (bad++ < 5) std::fprintf(stderr, "  %s pos %lld n_q %d: streamed attention differs\n", name, (long long) pos, n_q);
        }
        if ((batches == 0 || pos == N - 1) && !attention_reference(host, fmt, s, hq, hids, hst, a, n_q, cap)) {
            if (bad++ < 5) std::fprintf(stderr, "  %s pos %lld: FP64 attention reference differs\n", name, (long long) pos);
        }
        if (!checked_host_attn) {
            checked_host_attn = true;
            k::qsa_decode_attn_batch(q, host.attn(ident), ids, steps, cap, s, scratch, out_str, n_q, nullptr);
            ck(DPCT_CHECK_ERROR(dpct::get_current_device().queues_wait_and_throw()),
               "host attention");
            ck(DPCT_CHECK_ERROR(
                   (dpct::get_current_device().queues_wait_and_throw(), dpct::get_in_order_queue())
                       .memcpy(b2.data(), out_str, (size_t)n_q * NH * D * 4)
                       .wait()),
               "host b");
            if (std::memcmp(a.data(), b2.data(), (size_t) n_q * NH * D * 4) != 0) {
                if (bad++ < 5) std::fprintf(stderr, "  %s pos %lld n_q %d: host attention differs\n", name, (long long) pos, n_q);
            }
        }
        // the map inverts itself
        ck(DPCT_CHECK_ERROR((dpct::get_current_device().queues_wait_and_throw(), dpct::get_in_order_queue())
                                .memcpy(pt.data(), m.page_table, n_blocks * 4)
                                .wait()),
           "pt");
        ck(DPCT_CHECK_ERROR((dpct::get_current_device().queues_wait_and_throw(), dpct::get_in_order_queue())
                                .memcpy(sb.data(), m.slot_block, n_slots * 4)
                                .wait()),
           "sb");
        int64_t resident = 0;
        for (int64_t bk = 0; bk < n_blocks; ++bk) {
            if (pt[bk] < -1 || pt[bk] >= n_slots || (pt[bk] >= 0 && sb[pt[bk]] != bk)) {
                if (bad++ < 5) std::fprintf(stderr, "  map broken at block %lld (table %d)\n", (long long) bk, pt[bk]);
                break;
            }
            resident += pt[bk] >= 0;
        }
        for (int64_t sl = 0; sl < n_slots; ++sl)
            if (sb[sl] >= 0 && pt[sb[sl]] != sl) { if (bad++ < 5) std::fprintf(stderr, "  slot %lld not in the table\n", (long long) sl); break; }
        ++batches;
    }
    const k::KvStreamCounters c = k::kv_stream_counters(m);
    std::printf("  %s: %d batches, %llu block lookups, %llu misses (%.1f%% hit), overflow %d, %d failures\n",
                name, batches, (unsigned long long) c.lookups, (unsigned long long) c.misses,
                c.lookups ? 100.0 * (double) (c.lookups - c.misses) / (double) c.lookups : 0.0, (int) c.overflow, bad);
    if (c.overflow || c.calls != (uint64_t) batches || c.misses == 0 || c.misses >= c.lookups) ++bad;

    // 4. a ring over the same host copy: blocks [b1 - R, b1) restored, read through `b % R`
    {
        const int64_t R = 1500, b1 = n_blocks, b0 = b1 - R;
        Pools ring;
        ring.alloc(R, s, fmt, false);
        int32_t* rt = dalloc<int32_t>(n_blocks);
        k::kv_ring_table(rt, n_blocks, R, nullptr);
        k::kv_ring_restore(ring.attn(rt), host.p, fmt, b0, b1, R, s, nullptr);
        const int64_t width = cap;
        std::vector<int32_t> hids((size_t) cap), hst = {(int32_t) (N - 1), (int32_t) N, (int32_t) (N / 4), (int32_t) width};
        for (int64_t i = 0; i < width; ++i) hids[i] = (int32_t) (N - width + i);   // the window's last cells
        /*
        DPCT1114: cudaMemcpy is migrated to asynchronization memcpy,
        assuming in the original code the source host memory is pageable memory.
        If the memory is not pageable, call wait() on event return by memcpy API
        to ensure synchronization behavior.
        */
        ck(DPCT_CHECK_ERROR((dpct::get_current_device().queues_wait_and_throw(), dpct::get_in_order_queue()).memcpy(ids, hids.data(),
                                                              hids.size() * 4).wait()),
           "ids");
        /*
        DPCT1114: cudaMemcpy is migrated to asynchronization memcpy,
        assuming in the original code the source host memory is pageable memory.
        If the memory is not pageable, call wait() on event return by memcpy API
        to ensure synchronization behavior.
        */
        ck(DPCT_CHECK_ERROR(
               (dpct::get_current_device().queues_wait_and_throw(), dpct::get_in_order_queue()).memcpy(steps, hst.data(), 16).wait()),
           "steps");
        k::qsa_decode_attn_batch(q, ref.attn(ident), ids, steps, cap, s, scratch, out_ref, 1, nullptr);
        k::qsa_decode_attn_batch(q, ring.attn(rt), ids, steps, cap, s, scratch, out_str, 1, nullptr);
        ck(DPCT_CHECK_ERROR(dpct::get_current_device().queues_wait_and_throw()),
           "ring");
        ck(DPCT_CHECK_ERROR((dpct::get_current_device().queues_wait_and_throw(), dpct::get_in_order_queue())
                                .memcpy(a.data(), out_ref, (size_t)NH * D * 4)
                                .wait()),
           "a");
        ck(DPCT_CHECK_ERROR((dpct::get_current_device().queues_wait_and_throw(), dpct::get_in_order_queue())
                                .memcpy(b2.data(), out_str, (size_t)NH * D * 4)
                                .wait()),
           "b");
        const bool ok = std::memcmp(a.data(), b2.data(), (size_t) NH * D * 4) == 0;
        std::printf("  %s ring restore: %s\n", name, ok ? "identical" : "DIFFERS");
        if (!ok) ++bad;
    }
    return bad == 0;
}
}  // namespace

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--bench") return bench_int8();
    std::printf("kv_stream_parity: streamed vs resident KV, bitwise\n");
    const bool a = run(k::kKvInt8), b = run(k::kKvF16), c = run(k::kKvQ4);
    if (!a || !b || !c) ++g_fail;
    std::printf(g_fail ? "FAIL\n" : "PASS\n");
    return g_fail ? 1 : 0;
}
