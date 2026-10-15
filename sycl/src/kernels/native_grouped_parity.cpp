// src/kernels/native_grouped_parity.cpp - native_expert_grouped's group stride (grid_groups) and its fused SwiGLU +
// q8_1 pass, against the launches before them (native_grouped_set_v1: a block row per possible group, SwiGLU and the
// q8_1 quantization as two kernels over all cap_entries).  Synthetic blobs, no model.
//
//     build/native_grouped_parity           the checks (a ctest; needs the GPU)
//     build/native_grouped_parity --bench   the checks, then v1/new timings of 48 calls in a graph (a window's layers)
//     build/native_grouped_parity --bench-a770-mixed  one mixed VRAM/mapped plan against two source-specific calls
//
// Every gate/up format with every down format; calls of 0..cap groups of 1..T entries each (an empty call is what the
// verify window's PCIe call usually is), entries from 0 and from past 0 (the PCIe call's follow the VRAM call's),
// scattered destinations, stale scratch, grid_groups 0 (= cap), 1, 2, 3, 4 and above cap: the output must be BITWISE
// equal to v1's, the rows the call must not write included (both start from the same NaN pattern).
//
// Random bytes are valid codes for every format here (every grid index is in range); only the fp16 block scales are
// set, small enough that the SwiGLU outputs keep a finite fp16 q8_1 scale.
#define DPCT_PROFILING_ENABLED
#include <sycl/sycl.hpp>
#include <dpct/dpct.hpp>
#include "strata/kernels/iq_kernels.hpp"
#include "strata/kernels/native_mmvq.hpp"
#include "strata/kernels/verify_kernels.hpp"
#include "strata/kernels/resident_plan_mirror.hpp"

#include <algorithm>
#include <stdexcept>
#include <cmath>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <numeric>
#include <random>
#include <string>
#include <vector>

namespace k = strata::kernels;

namespace {

int g_fail = 0;

// ponytail: check the real device plan against a host grouping, including the 64-entry boundary.
void check_resident_plan(dpct::queue_ptr s) {
    constexpr int NE = 256, CAP = k::kVerifyMaxT * 10;
    constexpr int PO = (4 + CAP + 1 + 2 * CAP + 1) & ~1;
    constexpr int WORDS = PO + 4 * CAP + CAP + 2;
    auto* ids = sycl::malloc_device<int32_t>(CAP, *s);
    auto* res = sycl::malloc_device<int32_t>(NE, *s);
    auto* offsets = sycl::malloc_device<unsigned long long>(NE, *s);
    auto* mirror = sycl::malloc_device<unsigned long long>(NE, *s);
    auto* plan = sycl::malloc_device<int32_t>(WORDS, *s);
    auto* skip = sycl::malloc_device<uint32_t>(1, *s);
    auto* cache = sycl::malloc_device<uint8_t>(NE * 96, *s);
    auto* host = sycl::malloc_host<uint8_t>(NE * 96, *s);
    if (!ids || !res || !offsets || !mirror || !plan || !skip || !cache || !host)
        throw std::bad_alloc();
    std::vector<int32_t> hi(CAP), hr(NE), got(WORDS), ref(WORDS);
    std::vector<unsigned long long> ho(NE), hm(NE);
    for (int e = 0; e < NE; ++e) {
        ho[e] = (unsigned long long) (e * 96);
        hm[e] = (unsigned long long) (host + e * 96);
    }
    s->memcpy(offsets, ho.data(), NE * sizeof(ho[0]));
    s->memcpy(mirror, hm.data(), NE * sizeof(hm[0]));
    for (int mode : {0, 1, 2}) {
        for (int e = 0; e < NE; ++e) hr[e] = mode == 2 || (mode == 1 && e % 3 == 0) ? -1 : e;
        s->memcpy(res, hr.data(), NE * sizeof(hr[0]));
        k::resident_plan_set_mirror(res, mode ? mirror : nullptr);
        for (bool unique : {false, true}) for (int n : {0, 10, 20, 30, 40, 50, 60, 64, 70, CAP}) {
            for (int i = 0; i < n; ++i) hi[i] = unique ? (i * 17) % NE : (i * 7) % 23;
            std::fill(ref.begin(), ref.end(), -12345);
            auto* start = ref.data() + 4;
            auto* dst = start + CAP + 1;
            auto* tok = dst + CAP;
            int groups = 0, entries = 0;
            for (int i = 0; i < n; ++i) {
                if (std::find(hi.begin(), hi.begin() + i, hi[i]) != hi.begin() + i) continue;
                const auto ptr = hr[hi[i]] < 0 ? hm[hi[i]] : (unsigned long long) (cache + ho[hr[hi[i]]]);
                std::memcpy(ref.data() + PO + 2 * groups, &ptr, sizeof(ptr));
                start[groups++] = entries;
                for (int j = i; j < n; ++j) if (hi[j] == hi[i]) { dst[entries] = j; tok[entries++] = j / 10; }
            }
            start[groups] = entries;
            ref[PO + 4 * CAP] = entries;
            ref[0] = groups; ref[1] = entries; ref[2] = 0;
            std::fill(got.begin(), got.end(), -12345);
            s->memcpy(plan, got.data(), WORDS * sizeof(int32_t));
            s->memcpy(ids, hi.data(), CAP * sizeof(int32_t));
            k::resident_plan(ids, n, 10, res, NE, cache, offsets, 96, plan, CAP, skip, 73, s);
            s->memcpy(got.data(), plan, WORDS * sizeof(int32_t)).wait_and_throw();
            uint32_t flag = 0;
            s->memcpy(&flag, skip, sizeof(flag)).wait_and_throw();
            const bool ok = got == ref && flag == 73;
            std::printf("resident plan n=%d %s %s: %s\n", n, unique ? "unique" : "repeated",
                        mode == 0 ? "VRAM" : mode == 1 ? "mirror+VRAM" : "mirror", ok ? "PASS" : "FAIL");
            if (!ok) ++g_fail;
        }
    }
    k::resident_plan_set_mirror(nullptr, nullptr);
    hr[0] = -1;
    hi[0] = 0;
    s->memcpy(res, hr.data(), NE * sizeof(hr[0]));
    s->memset(skip, 0, sizeof(*skip));
    k::resident_plan(ids, 1, 10, res, NE, cache, offsets, 96, plan, CAP, skip, 73, s);
    uint32_t fallback = 0;
    s->memcpy(&fallback, skip, sizeof(fallback)).wait_and_throw();
    const bool fallback_ok = fallback == (73u | k::kResidentPlanHostBit);
    std::printf("resident plan missing mirror fallback: %s\n", fallback_ok ? "PASS" : "FAIL");
    if (!fallback_ok) ++g_fail;
    for (const auto& bad : {std::pair<int, int>{CAP + 1, 10}, {129, 10}, {-1, 10}, {10, 0}}) {
        bool refused = false;
        try { k::resident_plan(ids, bad.first, bad.second, res, NE, cache, offsets, 96, plan, CAP, skip, 73, s); }
        catch (const std::invalid_argument&) { refused = true; }
        if (!refused) ++g_fail;
        std::printf("resident plan invalid n=%d k=%d: %s\n", bad.first, bad.second, refused ? "PASS" : "FAIL");
    }
    for (void* p : {static_cast<void*>(ids), static_cast<void*>(res), static_cast<void*>(offsets),
                   static_cast<void*>(mirror), static_cast<void*>(plan), static_cast<void*>(skip),
                   static_cast<void*>(cache), static_cast<void*>(host)}) sycl::free(p, *s);
}

void ck(dpct::err0 e, const char *what) {
}

template<typename T>
T* dalloc(size_t n) {
    T* p = nullptr;
    ck(DPCT_CHECK_ERROR(p = (T *)sycl::malloc_device(
                            n * sizeof(T) + 256, dpct::get_in_order_queue())),
       "malloc");
    ck(DPCT_CHECK_ERROR(
           (dpct::get_current_device().queues_wait_and_throw(), dpct::get_in_order_queue()).memset(p, 0, n * sizeof(T) + 256).wait()),
       "memset");
    return p;
}

const char* name_of(int t) {
    switch (t) {
        case 16: return "IQ2_XXS";
        case 17: return "IQ2_XS";
        case 18: return "IQ3_XXS";
        case 20: return "IQ4_NL";
        case 21: return "IQ3_S";
        case 22: return "IQ2_S";
        case 23: return "IQ4_XS";
        case 29: return "IQ1_M";
        case 42: return "Q2_0";
        case 12: return "Q4_K";
        case 13: return "Q5_K";
        case 7: return "Q5_1";
        case 8: return "Q8_0";
        default: return "?";
    }
}
int block_values(int t) { return t == 20 || t == 7 || t == 8 ? 32 : t == 42 ? 64 : 256; }

// `rows` rows of `n` values of format t: random bytes, then a finite fp16 scale in every block
std::vector<uint8_t> random_rows(int t, int64_t rows, int64_t n, std::mt19937& rng) {
    const size_t rb = k::iq_row_bytes(t, n), bs = k::iq_row_bytes(t, block_values(t));
    std::vector<uint8_t> w((size_t) rows * rb);
    std::uniform_int_distribution<int> byte(0, 255), ex(2, 8), man(0, 1023), sgn(0, 1);
    for (auto& b : w) b = (uint8_t) byte(rng);
    for (size_t o = 0; o < w.size(); o += bs) {
        if (t == 29) {
            // IQ1_M: the fp16 scale is the top nibbles of its four scale words (bytes 48..55), the sign and the
            // exponent's top bits in the last: 0x1 / 0x2 there (0x9 / 0xA negative) keeps it in 2^-11 .. 2^-3
            const uint8_t nib = (uint8_t) ((sgn(rng) ? 0x8 : 0x0) | (1 + (byte(rng) & 1)));
            w[o + 55] = (uint8_t) ((w[o + 55] & 0x0F) | (nib << 4));
        } else {   // the other formats start their block with the fp16 scale: 2^-13 .. 2^-6
            // (Q4_K / Q5_K: 2^-14 .. 2^-11, their 6-bit sub-scales multiply it by up to 63)
            const int e = (t == 12 || t == 13) ? 1 + ex(rng) % 4 : ex(rng);
            const uint16_t h = (uint16_t) ((sgn(rng) ? 0x8000 : 0) | (e << 10) | man(rng));
            std::memcpy(&w[o], &h, 2);
            if (t == 12 || t == 13 || t == 7) {   // Q4_K / Q5_K: dmin, Q5_1: m - the second fp16, as small
                const int em = (t == 7) ? ex(rng) : 1 + ex(rng) % 4;
                const uint16_t m = (uint16_t) ((sgn(rng) ? 0x8000 : 0) | (em << 10) | man(rng));
                std::memcpy(&w[o + 2], &m, 2);
            }
        }
    }
    return w;
}

// one window's worth of expert calls on one format pair: blobs, T tokens' q8_1 activations, cap = T * K entries
struct Setup {
    k::NativeExpertLayout L;
    int T = 0, K = 0, cap = 0, n_blobs = 0;
    size_t slot = 0;
    uint8_t* blobs = nullptr;
    uint8_t* xq = nullptr;
    uint8_t* scr = nullptr;
    size_t scr_bytes = 0;
    float* out = nullptr;
    size_t out_floats = 0;
    unsigned long long* ptr = nullptr;
    int32_t *start = nullptr, *n = nullptr, *dst = nullptr, *tok = nullptr;

    Setup(int gu, int dt, int64_t H, int64_t FF, int tokens, int k_per_token,
          int blobs_n, std::mt19937 &rng, dpct::queue_ptr s, bool host_blobs = false) {
        L = k::native_expert_layout(gu, dt, H, FF);
        T = tokens; K = k_per_token; cap = T * K; n_blobs = blobs_n;
        slot = (L.bytes + 255) & ~(size_t) 255;
        if (host_blobs) {
            blobs = sycl::malloc_host<uint8_t>(slot * (size_t) n_blobs, dpct::get_in_order_queue());
            if (!blobs) { std::fprintf(stderr, "host blob allocation failed\n"); std::exit(2); }
        } else {
            blobs = dalloc<uint8_t>(slot * (size_t) n_blobs);
        }
        for (int b = 0; b < n_blobs; ++b) {
            // gate rows | up rows (format gu, H values each) | down rows (format dt, FF values each)
            auto blob = random_rows(gu, 2 * FF, H, rng);
            const auto down = random_rows(dt, H, FF, rng);
            blob.insert(blob.end(), down.begin(), down.end());
            if (blob.size() != L.bytes) { std::fprintf(stderr, "blob %zu != %zu bytes\n", blob.size(), L.bytes); std::exit(2); }
            /*
            DPCT1114: cudaMemcpy is migrated to asynchronization memcpy,
            assuming in the original code the source host memory is pageable
            memory. If the memory is not pageable, call wait() on event return
            by memcpy API to ensure synchronization behavior.
            */
            if (host_blobs) {
                std::memcpy(blobs + slot * (size_t)b, blob.data(), blob.size());
            } else {
                ck(DPCT_CHECK_ERROR((dpct::get_current_device().queues_wait_and_throw(), dpct::get_in_order_queue()).memcpy(
                       blobs + slot * (size_t)b, blob.data(), blob.size()).wait()),
                   "blob");
            }
        }
        std::normal_distribution<float> nd(0.f, 1.f);
        std::vector<float> x((size_t) T * H);
        for (auto& v : x) v = nd(rng);
        float* dx = dalloc<float>(x.size());
        /*
        DPCT1114: cudaMemcpy is migrated to asynchronization memcpy,
        assuming in the original code the source host memory is pageable memory.
        If the memory is not pageable, call wait() on event return by memcpy API
        to ensure synchronization behavior.
        */
        ck(DPCT_CHECK_ERROR(
               (dpct::get_current_device().queues_wait_and_throw(), dpct::get_in_order_queue()).memcpy(dx, x.data(), x.size() * 4).wait()),
           "x");
        xq = dalloc<uint8_t>((size_t) T * (H / 32) * 36);
        k::quantize_q8_1_rows(dx, T, H, xq, s);
        ck(DPCT_CHECK_ERROR(s->wait()), "quantize");
        sycl::free(dx, dpct::get_in_order_queue());
        scr_bytes = k::native_expert_scratch_bytes(cap, FF);
        scr = dalloc<uint8_t>(scr_bytes);
        out_floats = (size_t) cap * H;
        out = dalloc<float>(out_floats);
        ptr = dalloc<unsigned long long>(cap);
        start = dalloc<int32_t>(cap + 1);
        n = dalloc<int32_t>(1);
        dst = dalloc<int32_t>(cap);
        tok = dalloc<int32_t>(cap);
    }
    ~Setup() {
        sycl::free(blobs, dpct::get_in_order_queue());
            sycl::free(xq, dpct::get_in_order_queue());
            sycl::free(scr, dpct::get_in_order_queue());
            sycl::free(out, dpct::get_in_order_queue());
        sycl::free(ptr, dpct::get_in_order_queue());
            sycl::free(start, dpct::get_in_order_queue());
            sycl::free(n, dpct::get_in_order_queue());
            sycl::free(dst, dpct::get_in_order_queue());
            sycl::free(tok, dpct::get_in_order_queue());
    }
    // a plan: `groups` groups of 1..T entries from entry `base` on (as many as fit in cap)
    int plan(int groups, int base, std::mt19937& rng) {
        std::uniform_int_distribution<int> size(1, T);
        std::vector<int> sizes;
        int used = base;
        for (int g = 0; g < groups; ++g) {
            const int m = std::min(size(rng), cap - used);
            if (m <= 0) break;
            sizes.push_back(m);
            used += m;
        }
        return plan_sizes(sizes, base, rng);
    }
    // groups of these sizes from entry `base` on, each its own blob (as long as there are enough: a window's groups
    // are different experts); scattered destinations, random tokens
    int plan_sizes(const std::vector<int>& sizes, int base, std::mt19937& rng) {
        std::uniform_int_distribution<int> tk(0, T - 1);
        std::vector<int> order(n_blobs);
        std::iota(order.begin(), order.end(), 0);
        std::shuffle(order.begin(), order.end(), rng);
        std::vector<unsigned long long> p;
        std::vector<int32_t> st(1, base), tk_v, ds(cap);
        for (const int m : sizes) {
            p.push_back((unsigned long long) (blobs + slot * (size_t) order[p.size() % order.size()]));
            st.push_back(st.back() + m);
        }
        if (st.back() > cap) { std::fprintf(stderr, "plan: %d entries > cap %d\n", st.back(), cap); std::exit(2); }
        const int ng = (int) p.size();
        std::iota(ds.begin(), ds.end(), 0);
        std::shuffle(ds.begin(), ds.end(), rng);                  // scattered rows of `out`
        for (int e = 0; e < cap; ++e) tk_v.push_back(tk(rng));
        /*
        DPCT1114: cudaMemcpy is migrated to asynchronization memcpy,
        assuming in the original code the source host memory is pageable memory.
        If the memory is not pageable, call wait() on event return by memcpy API
        to ensure synchronization behavior.
        */
        if (ng) ck(DPCT_CHECK_ERROR((dpct::get_current_device().queues_wait_and_throw(), dpct::get_in_order_queue()).memcpy(
                       ptr, p.data(), p.size() * 8).wait()),
                   "ptr");
        /*
        DPCT1114: cudaMemcpy is migrated to asynchronization memcpy,
        assuming in the original code the source host memory is pageable memory.
        If the memory is not pageable, call wait() on event return by memcpy API
        to ensure synchronization behavior.
        */
        ck(DPCT_CHECK_ERROR((dpct::get_current_device().queues_wait_and_throw(), dpct::get_in_order_queue()).memcpy(start, st.data(),
                                                              st.size() * 4).wait()),
           "start");
        ck(DPCT_CHECK_ERROR(
               (dpct::get_current_device().queues_wait_and_throw(), dpct::get_in_order_queue()).memcpy(n, &ng, 4).wait()),
           "n");
        /*
        DPCT1114: cudaMemcpy is migrated to asynchronization memcpy,
        assuming in the original code the source host memory is pageable memory.
        If the memory is not pageable, call wait() on event return by memcpy API
        to ensure synchronization behavior.
        */
        ck(DPCT_CHECK_ERROR((dpct::get_current_device().queues_wait_and_throw(), dpct::get_in_order_queue()).memcpy(dst, ds.data(),
                                                              ds.size() * 4).wait()),
           "dst");
        /*
        DPCT1114: cudaMemcpy is migrated to asynchronization memcpy,
        assuming in the original code the source host memory is pageable memory.
        If the memory is not pageable, call wait() on event return by memcpy API
        to ensure synchronization behavior.
        */
        ck(DPCT_CHECK_ERROR((dpct::get_current_device().queues_wait_and_throw(), dpct::get_in_order_queue()).memcpy(tok, tk_v.data(),
                                                              tk_v.size() * 4).wait()),
           "tok");
        return ng;
    }
    void call(int64_t grid_groups, dpct::queue_ptr s) {
        k::native_expert_grouped(L, ptr, start, n, dst, tok, cap, cap, xq, scr, out, s, grid_groups);
    }
    // `out` after one call from the NaN pattern, the scratch holding `junk` (what earlier calls left there)
    std::vector<uint32_t> result(bool v1, int64_t grid_groups, int junk,
                                 dpct::queue_ptr s, int repeats = 1) {
        ck(DPCT_CHECK_ERROR((dpct::get_current_device().queues_wait_and_throw(), dpct::get_in_order_queue())
                                .memset(out, 0xFF, out_floats * 4)
                                .wait()),
           "memset");
        ck(DPCT_CHECK_ERROR(
               (dpct::get_current_device().queues_wait_and_throw(), dpct::get_in_order_queue()).memset(scr, junk, scr_bytes).wait()),
           "memset");
        k::native_grouped_set_v1(v1);
        for (int i = 0; i < repeats; ++i) call(grid_groups, s);
        k::native_grouped_set_v1(false);
        ck(DPCT_CHECK_ERROR(s->wait()), "sync");
        std::vector<uint32_t> o(out_floats);
        ck(DPCT_CHECK_ERROR((dpct::get_current_device().queues_wait_and_throw(), dpct::get_in_order_queue())
                                .memcpy(o.data(), out, o.size() * 4)
                                .wait()),
           "out");
        return o;
    }
};

void check(int gu, int dt, int64_t H, int64_t FF, dpct::queue_ptr s,
           std::mt19937 &rng) {
    Setup S(gu, dt, H, FF, 6, 4, 9, rng, s);   // 6 tokens x 4 experts: cap 24 entries
    int calls = 0, bad = 0;
    size_t written = 0;
    for (const int base : {0, 7}) {
        for (const int groups : {0, 1, 2, 3, 5, 8, 24}) {
            const int ng = S.plan(groups, base, rng);
            k::native_gu_sg16_set(false);
            const auto ref = S.result(true, 0, 0x00, s);
            k::native_gu_sg16_set(true);
            size_t w = 0;
            for (const uint32_t v : ref) w += v != 0xFFFFFFFFu;
            written += w;
            for (const int64_t gy : {(int64_t) 0, (int64_t) 1, (int64_t) 2, (int64_t) 3, (int64_t) 4, (int64_t) S.cap + 3}) {
                const auto got = S.result(false, gy, (calls & 1) ? 0x7F : 0x00, s);
                ++calls;
                size_t diff = 0;
                for (size_t i = 0; i < ref.size(); ++i) diff += ref[i] != got[i];
                if (diff) {
                    std::printf("  %-8s/%-7s base %d, %d groups, grid_groups %lld: %zu of %zu floats differ  FAIL\n",
                                name_of(gu), name_of(dt), base, ng, (long long) gy, diff, ref.size());
                    ++bad;
                }
            }
        }
    }
    std::printf("%-8s/%-7s %5lld x %4lld  %d calls: %s (%zu output floats written in all)\n", name_of(gu), name_of(dt),
                (long long) H, (long long) FF, calls, bad ? "FAIL" : "bitwise equal to v1", written);
    if (written == 0) { std::printf("  nothing was written: the check checked nothing  FAIL\n"); ++bad; }
    g_fail += bad;
}

void host_smoke(dpct::queue_ptr s, bool full_layer, bool full_window) {
    std::mt19937 rng(316);
    const int tokens = full_layer || full_window ? 6 : 1;
    const int per_token = full_layer || full_window ? 10 : 1;
    const int experts = tokens * per_token;
    const int layers = full_window ? 48 : 1;
    Setup S(22, 42, 2560, 640, tokens, per_token, experts, rng, s, true);
    S.plan_sizes(std::vector<int>(experts, 1), 0, rng);
    std::printf("host_smoke: launching %d IQ2_S/Q2_0 experts (%zu bytes each) x %d queued layers\n",
                experts, S.L.bytes, layers);
    std::fflush(stdout);
    const auto got = S.result(false, experts, 0, s, layers);
    size_t finite = 0;
    for (uint32_t bits : got) {
        float value;
        std::memcpy(&value, &bits, sizeof(value));
        finite += std::isfinite(value);
    }
    if (finite != got.size()) {
        std::fprintf(stderr, "host_smoke: %zu of %zu outputs are finite\n", finite, got.size());
        ++g_fail;
    } else {
        std::printf("host_smoke: PASS (%zu finite outputs)\n", finite);
    }
}

// ------------------------------------------------------------------------------------------------ --bench
// A window's 48 calls, one per layer, each with ITS OWN experts as the layers have: copies of the Setup's blobs in one
// arena, so the weights stream from DRAM as in the engine.  (48 calls re-reading the same 28 experts - 73 MB, about an
// Ada L2 - would read part of them from L2; the engine's layers share none.)
struct Layers {
    uint8_t* arena = nullptr;
    uint8_t* mapped_arena = nullptr;
    std::vector<unsigned long long*> ptr;           // per layer: its groups' blobs (device)
    Layers(Setup& S, int ng, bool mapped = false, int mapped_tail = 0) {
        const int mapped_n = mapped ? ng : std::clamp(mapped_tail, 0, ng);
        const int resident_n = ng - mapped_n;
        if (resident_n > 0) arena = dalloc<uint8_t>(S.slot * (size_t) (48 * resident_n));
        if (mapped_n > 0)
            mapped_arena = sycl::malloc_host<uint8_t>(S.slot * (size_t) (48 * mapped_n),
                                                       dpct::get_in_order_queue());
        if ((resident_n > 0 && !arena) || (mapped_n > 0 && !mapped_arena)) throw std::bad_alloc();
        for (int l = 0; l < 48; ++l) {
            std::vector<unsigned long long> p;
            for (int j = 0; j < ng; ++j) {
                const bool on_host = j >= resident_n;
                uint8_t* d = on_host
                                 ? mapped_arena + S.slot * (size_t) (l * mapped_n + j - resident_n)
                                 : arena + S.slot * (size_t) (l * resident_n + j);
                ck(DPCT_CHECK_ERROR((dpct::get_current_device().queues_wait_and_throw(), dpct::get_in_order_queue()).memcpy(
                       d,
                       S.blobs + S.slot * (size_t)((l * std::max(ng, 1) + j) % S.n_blobs),
                       S.L.bytes).wait()),
                   "blob copy");
                p.push_back((unsigned long long) d);
            }
            unsigned long long* dp = dalloc<unsigned long long>(p.size());
            /*
            DPCT1114: cudaMemcpy is migrated to asynchronization memcpy,
            assuming in the original code the source host memory is pageable
            memory. If the memory is not pageable, call wait() on event return
            by memcpy API to ensure synchronization behavior.
            */
            ck(DPCT_CHECK_ERROR((dpct::get_current_device().queues_wait_and_throw(), dpct::get_in_order_queue()).memcpy(
                   dp, p.data(), p.size() * 8).wait()),
               "ptr");
            ptr.push_back(dp);
        }
    }
    Layers(const Layers&) = delete;
    Layers& operator=(const Layers&) = delete;
    ~Layers() {
        for (auto *p : ptr)
            DPCT_CHECK_ERROR(sycl::free(p, dpct::get_in_order_queue()));
        if (arena) sycl::free(arena, dpct::get_in_order_queue());
        if (mapped_arena) sycl::free(mapped_arena, dpct::get_in_order_queue());
    }
};

// the 48 calls of one variant captured in one graph
dpct::experimental::command_graph_exec_ptr
make_graph(dpct::queue_ptr s, Setup &S, const Layers &Ls, bool v1, int64_t gy) {
    k::native_grouped_set_v1(v1);
    dpct::experimental::command_graph_ptr g;
    dpct::experimental::command_graph_exec_ptr ge;
    ck(DPCT_CHECK_ERROR(dpct::experimental::begin_recording(s)), "capture");
    for (int l = 0; l < 48; ++l)
        k::native_expert_grouped(S.L, Ls.ptr[l], S.start, S.n, S.dst, S.tok, S.cap, S.cap, S.xq, S.scr, S.out, s, gy);
    ck(DPCT_CHECK_ERROR(dpct::experimental::end_recording(s, &g)), "capture");
    ck(DPCT_CHECK_ERROR(
           ge = new sycl::ext::oneapi::experimental::command_graph<
               sycl::ext::oneapi::experimental::graph_state::executable>(
               g->finalize())),
       "instantiate");
    ck(DPCT_CHECK_ERROR(delete (g)), "graph");
    k::native_grouped_set_v1(false);
    return ge;
}

// one launch of the graph: microseconds per call
float launch_us(dpct::queue_ptr s,
                dpct::experimental::command_graph_exec_ptr ge,
                dpct::event_ptr e0, dpct::event_ptr e1) {
    /*
    DPCT1024: The original code returned the error code that was further
    consumed by the program logic. This original code was replaced with 0. You
    may need to rewrite the program logic consuming the error code.
    */
    ck(DPCT_CHECK_ERROR(dpct::sync_barrier(e0, s)), "record");
    ck(DPCT_CHECK_ERROR(s->ext_oneapi_graph(*ge)), "launch");
    /*
    DPCT1024: The original code returned the error code that was further
    consumed by the program logic. This original code was replaced with 0. You
    may need to rewrite the program logic consuming the error code.
    */
    ck(DPCT_CHECK_ERROR(dpct::sync_barrier(e1, s)), "record");
    ck(DPCT_CHECK_ERROR(e1->wait_and_throw()), "bench");
    float ms = 0;
    ck(DPCT_CHECK_ERROR(
           ms = (e1->get_profiling_info<
                     sycl::info::event_profiling::command_end>() -
                 e0->get_profiling_info<
                     sycl::info::event_profiling::command_start>()) /
                1000000.0f),
       "elapsed");
    return 1e3f * ms / 48.0f;
}

float median(std::vector<float> v) {
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

// The two variants' launches ALTERNATE (v1-new, new-v1, ...) after a warm-up, and each reports its median: a card
// under sustained load steps its clock down after a fraction of a second (an RTX 4080 SUPER went from 80 to 89 us
// per gate/up call within one run), and whichever variant ran before the step would look faster.
constexpr int kWarmPairs = 20, kPairs = 40;

void bench(dpct::queue_ptr s, std::mt19937 &rng) {
    std::printf("\n--bench: microseconds per call, a window's 48 calls in a graph, each layer with its own experts (the\n"
                "weights stream from DRAM as in the engine); v1 / new (grid_groups): medians of %d launches each,\n"
                "alternating, after %d warm-up pairs - idle GPU assumed\n", kPairs, kWarmPairs);
    // a verify window's shape: 4 tokens x 10 experts, 2560 x 768 IQ3_S / IQ4_XS experts
    Setup S(21, 23, 2560, 768, 4, 10, 32, rng, s);
    std::vector<int> vram(16, 1);
    vram.insert(vram.end(), 12, 2);                                   // 28 groups, 40 entries
    const struct { const char* what; std::vector<int> sizes; int64_t gy; } rows[] = {
        {"no group (the PCIe call at pcie_frac 0)", {}, 4},
        {"1 group of 2 entries (a PCIe call)", {2}, 4},
        {"28 groups, 40 entries (a VRAM call)", vram, 0},
    };
    dpct::event_ptr e0, e1;
    ck(DPCT_CHECK_ERROR(e0 = new sycl::event()), "event");
    ck(DPCT_CHECK_ERROR(e1 = new sycl::event()), "event");
    for (const auto& r : rows) {
        const int ng = S.plan_sizes(r.sizes, 0, rng);
        const Layers Ls(S, ng);                                       // 48 x 28 experts: 3.7 GB for the VRAM call
        dpct::experimental::command_graph_exec_ptr ga = make_graph(s, S, Ls,
                                                                   true, 0),
                                                   gb = make_graph(s, S, Ls,
                                                                   false, r.gy);
        std::vector<float> ta, tb;
        for (int i = 0; i < kWarmPairs + kPairs; ++i) {
            const bool a_first = i % 2 == 0;
            const float x = launch_us(s, a_first ? ga : gb, e0, e1), y = launch_us(s, a_first ? gb : ga, e0, e1);
            if (i < kWarmPairs) continue;
            ta.push_back(a_first ? x : y);
            tb.push_back(a_first ? y : x);
        }
        ck(DPCT_CHECK_ERROR(delete (ga)), "graph");
        ck(DPCT_CHECK_ERROR(delete (gb)), "graph");
        std::printf("  %-42s %8.2f / %8.2f us (grid_groups %lld)\n", r.what, median(ta), median(tb), (long long) r.gy);
    }
    dpct::destroy_event(e0);
    dpct::destroy_event(e1);
}

// ponytail: host clocks and the existing queue wait keep this benchmark usable on Windows EAGER.
void bench_a770(dpct::queue_ptr s, std::mt19937& rng, bool single = false, bool stage = false) {
    using Clock = std::chrono::steady_clock;
    const int tokens = single ? 1 : 4, groups = single ? 10 : 20;
    std::printf("A770 expert benchmark: 48 distinct layers, T=%d, %d groups / %d entries; median of 5 eager windows\n",
                tokens, groups, tokens * 10);
    for (int gu : {22, 16, 29}) {
        check(gu, 42, 2560, 640, s, rng);
        Setup S(gu, 42, 2560, 640, tokens, 10, groups, rng, s);
        const int ng = S.plan_sizes(std::vector<int>(groups, single ? 1 : 2), 0, rng);
        std::vector<float> reference(S.out_floats), actual(S.out_floats);
        for (bool mapped : {false, true}) {
            Layers Ls(S, ng, mapped);
            uint8_t* staging = stage ? dalloc<uint8_t>(S.L.bytes * (size_t) groups) : nullptr;
            auto* stage_ptr = stage ? dalloc<unsigned long long>(groups) : nullptr;
            auto run = [&](bool staged) {
                for (int l = 0; l < 48; ++l) {
                    if (staged) {
                        s->memcpy(stage_ptr, Ls.ptr[l], groups * sizeof(unsigned long long));
                        k::fetch_blobs(stage_ptr, S.n, staging, (int64_t) S.L.bytes, groups, s);
                        k::rebase_ptrs(stage_ptr, S.n, staging, (int64_t) S.L.bytes, s);
                    }
                    k::native_expert_grouped(S.L, staged ? stage_ptr : Ls.ptr[l], S.start, S.n, S.dst, S.tok,
                                            S.cap, S.cap, S.xq, S.scr, S.out, s);
                }
                s->wait_and_throw();
            };
            run(false);
            s->memcpy(actual.data(), S.out, actual.size() * sizeof(float)).wait();
            if (!mapped) reference = actual;
            else if (std::memcmp(reference.data(), actual.data(), actual.size() * sizeof(float))) {
                std::fprintf(stderr, "mapped/VRAM output differs for type %d\n", gu);
                ++g_fail;
            }
            if (stage) {
                run(true);
                s->memcpy(actual.data(), S.out, actual.size() * sizeof(float)).wait();
                if (std::memcmp(reference.data(), actual.data(), actual.size() * sizeof(float))) {
                    std::fprintf(stderr, "staged/direct output differs for type %d\n", gu);
                    ++g_fail;
                } else std::printf("type %d staged/direct: bitwise equal\n", gu);
            }
            std::vector<float> times, direct_times;
            for (int i = 0; i < 7; ++i) {
                if (stage) {
                    const auto start = Clock::now();
                    run(false);
                    if (i >= 2) direct_times.push_back((float) std::chrono::duration<double, std::milli>(Clock::now() - start).count());
                }
                const auto start = Clock::now();
                run(stage);
                if (i >= 2) times.push_back((float) std::chrono::duration<double, std::milli>(Clock::now() - start).count());
            }
            std::printf("type %d / Q2_0, %s%s: %.3f ms/window\n", gu, mapped ? "mapped" : "VRAM",
                        stage ? " staged" : "", median(times));
            if (stage) std::printf("  direct %.3f ms/window; staging speedup %.2fx\n", median(direct_times), median(direct_times) / median(times));
            std::fflush(stdout);
            if (stage) { sycl::free(stage_ptr, *s); sycl::free(staging, *s); }
        }
    }
}

void bench_a770_mixed(dpct::queue_ptr s, std::mt19937& rng) {
    using Clock = std::chrono::steady_clock;
    std::printf("A770 mixed-plan benchmark: one mixed launch vs VRAM then mapped launches; 48 layers, "
                "median of 16 alternating windows\n");
    for (int tokens : {1, 3, 4}) {
        const int cap = tokens * 10;
        // NO_HOST logs: T=3 averages 23.4 distinct groups and T=4 averages 29.3.
        const int groups = tokens == 1 ? 10 : tokens == 3 ? 23 : 29;
        const int mapped_entries = std::max(1, (int) std::lround(cap * 0.17));
        std::vector<int> group_sweep;
        for (double fraction : {0.10, 0.20, 0.30}) {
            const int count = std::clamp((int) std::lround(groups * fraction), 1, mapped_entries);
            if (group_sweep.empty() || group_sweep.back() != count) group_sweep.push_back(count);
        }
        for (int mapped_groups : group_sweep) {
            const int resident_entries = cap - mapped_entries;
            const int resident_groups = groups - mapped_groups;
            auto partition = [](int entries, int n) {
                std::vector<int> sizes((size_t) n, entries / n);
                for (int i = 0; i < entries % n; ++i) ++sizes[(size_t) i];
                return sizes;
            };
            std::vector<int> sizes = partition(resident_entries, resident_groups);
            const std::vector<int> mapped_sizes = partition(mapped_entries, mapped_groups);
            sizes.insert(sizes.end(), mapped_sizes.begin(), mapped_sizes.end());
            for (int gu : {22, 16, 29}) {
                Setup S(gu, 42, 2560, 640, tokens, 10, groups, rng, s);
                const int ng = S.plan_sizes(sizes, 0, rng);
                Layers Ls(S, ng, false, mapped_groups);
                int32_t *n_resident = dalloc<int32_t>(1), *n_mapped = dalloc<int32_t>(1);
                s->memcpy(n_resident, &resident_groups, sizeof(resident_groups));
                s->memcpy(n_mapped, &mapped_groups, sizeof(mapped_groups)).wait();
                auto run = [&](bool split) {
                    for (int l = 0; l < 48; ++l) {
                        if (!split) {
                            k::native_expert_grouped(S.L, Ls.ptr[l], S.start, S.n, S.dst, S.tok, S.cap, S.cap,
                                                     S.xq, S.scr, S.out, s, 0);
                        } else {
                            k::native_expert_grouped(S.L, Ls.ptr[l], S.start, n_resident, S.dst, S.tok, S.cap, S.cap,
                                                     S.xq, S.scr, S.out, s, 0);
                            k::native_expert_grouped(S.L, Ls.ptr[l] + resident_groups, S.start + resident_groups,
                                                     n_mapped, S.dst, S.tok, S.cap, S.cap, S.xq, S.scr, S.out, s, 4);
                        }
                    }
                    s->wait_and_throw();
                };
                auto output = [&](bool split) {
                    s->memset(S.out, 0xFF, S.out_floats * sizeof(float));
                    s->memset(S.scr, 0, S.scr_bytes).wait();
                    run(split);
                    std::vector<uint32_t> result(S.out_floats);
                    s->memcpy(result.data(), S.out, result.size() * sizeof(result[0])).wait();
                    return result;
                };
                const auto mixed = output(false), split = output(true);
                const bool equal = mixed == split;
                if (!equal) ++g_fail;
                run(false);
                run(true);
                std::vector<float> mixed_ms, split_ms;
                for (int i = 0; i < 20; ++i) {
                    const bool split_first = i & 1;
                    const auto time = [&](bool mode) {
                        const auto begin = Clock::now();
                        run(mode);
                        return (float) std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
                    };
                    const float a = time(split_first), b = time(!split_first);
                    if (i >= 4) {
                        (split_first ? split_ms : mixed_ms).push_back(a);
                        (split_first ? mixed_ms : split_ms).push_back(b);
                    }
                }
                const float one = median(mixed_ms), two = median(split_ms);
                std::printf("  T=%d %-7s: %d/%d mapped entries, %d/%d groups; mixed %.3f ms, split %.3f ms, "
                            "split speedup %+0.2f%%; parity %s\n",
                            tokens, name_of(gu), mapped_entries, cap, mapped_groups, groups, one, two,
                            100.0f * (one / two - 1.0f), equal ? "PASS" : "FAIL");
                std::fflush(stdout);
                sycl::free(n_resident, *s);
                sycl::free(n_mapped, *s);
            }
        }
    }
}

}  // namespace

int main(int argc, char** argv) {
    const bool do_bench = argc > 1 && std::string(argv[1]) == "--bench";
    const bool do_host_smoke = argc > 1 && std::string(argv[1]) == "--host-smoke";
    const bool do_host_layer_smoke = argc > 1 && std::string(argv[1]) == "--host-layer-smoke";
    const bool do_host_window_smoke = argc > 1 && std::string(argv[1]) == "--host-window-smoke";
    dpct::queue_ptr s;
    ck(DPCT_CHECK_ERROR(s = dpct::get_current_device().create_queue(true)),
       "stream");
    if (argc > 1 && std::string(argv[1]) == "--plan-only") {
        check_resident_plan(s);
        dpct::get_current_device().destroy_queue(s);
        return g_fail ? 1 : 0;
    }
    if (do_host_smoke || do_host_layer_smoke || do_host_window_smoke) {
        host_smoke(s, do_host_layer_smoke, do_host_window_smoke);
        dpct::get_current_device().destroy_queue(s);
        return g_fail ? 1 : 0;
    }
    std::mt19937 rng(316);
    if (argc > 1 && (std::string(argv[1]) == "--bench-a770" || std::string(argv[1]) == "--bench-a770-single" ||
                    std::string(argv[1]) == "--bench-a770-stage" ||
                    std::string(argv[1]) == "--bench-a770-mixed")) {
        if (std::string(argv[1]) == "--bench-a770-mixed") bench_a770_mixed(s, rng);
        else bench_a770(s, rng, std::string(argv[1]) == "--bench-a770-single",
                        std::string(argv[1]) == "--bench-a770-stage");
        dpct::get_current_device().destroy_queue(s);
        return g_fail ? 1 : 0;
    }
    check_resident_plan(s);
    for (int gu : {16, 17, 18, 21, 22, 23, 29, 42, 12, 13, 8})        // STRATA_GU_FMTS
        for (int dt : {20, 23, 42, 7, 8}) check(gu, dt, 512, 256, s, rng);   // STRATA_D_FMTS; IQ4_XS: n_ff % 256
    check(21, 20, 2560, 640, s, rng);                                   // a model's shapes
    check(21, 23, 2560, 768, s, rng);
    if (do_bench) bench(s, rng);
    std::printf("native_grouped_parity: %d failures\n", g_fail);
    dpct::get_current_device().destroy_queue(s);
    return g_fail ? 1 : 0;
}
