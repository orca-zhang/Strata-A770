// src/kernels/sampler_parity.cpp - P2.S2's test for the sampler chain.
//
// THE CHECK THAT MATTERS IS THAT THE ORDER IS OBSERVABLE.  A sampler in the wrong order still returns a valid
// token, so "it produced a token" proves nothing; and because temperature is MONOTONIC it does not change which
// tokens `top_k` keeps, so a top-k-only fixture cannot see the order either.  What it changes is `top_p`'s CUT:
// at T < 1 the distribution sharpens, the cumulative mass reaches p sooner, and fewer tokens survive.
//
// So this builds a fixture where that happens, computes the greedy pick under BOTH orders, and requires them to
// DIFFER - then requires the kernel to agree with the specified one.  Without the first half, the test would
// pass against either order.
#define DPCT_PROFILING_ENABLED
#include <sycl/sycl.hpp>
#include <dpct/dpct.hpp>
#include "strata/kernels/sampler.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <random>
#include <string>
#include <vector>

namespace {

void check(dpct::err0 e, const char *what) {
    if (e) { std::fprintf(stderr, "%s failed: %d\n", what, (int)e); std::exit(1); }
}

// The host reference for the specified order: top_k -> top_p -> temperature -> argmax.
// `temp_first` swaps the first and last stages, which is the intuitive-but-wrong order.
int reference_pick(const std::vector<float>& l, const strata::kernels::SamplerParams& p, bool temp_first) {
    const int nv = (int) l.size();
    const float inv_t = p.temperature > 0.0f ? 1.0f / p.temperature : 0.0f;
    auto val = [&](int v) { return temp_first ? l[(size_t) v] * inv_t : l[(size_t) v]; };

    std::vector<int> ids;
    const int k = p.top_k > 0 ? p.top_k : nv;
    std::vector<char> taken((size_t) nv, 0);
    for (int i = 0; i < k; ++i) {
        int best = -1;
        float bv = 0;
        for (int v = 0; v < nv; ++v) {
            if (taken[(size_t) v]) continue;
            if (best < 0 || val(v) > bv) { best = v; bv = val(v); }
        }
        taken[(size_t) best] = 1;
        ids.push_back(best);
    }
    if (p.top_p < 1.0f) {
        float mx = val(ids[0]);
        for (int v : ids) mx = std::fmax(mx, val(v));
        double sum = 0;
        for (int v : ids) sum += std::exp((double) val(v) - (double) mx);
        double cum = 0;
        int cut = (int) ids.size();
        for (size_t i = 0; i < ids.size(); ++i) {
            cum += std::exp((double) val(ids[i]) - (double) mx) / sum;
            if (cum >= (double) p.top_p) { cut = (int) i + 1; break; }
        }
        if (cut < p.min_keep) cut = p.min_keep < (int) ids.size() ? p.min_keep : (int) ids.size();
        ids.resize((size_t) cut);
    }
    return ids[0];      // greedy: the largest SURVIVING logit, and `ids` is in descending order
}

// The number of survivors AFTER top_p, in the given order - the quantity the order actually changes.
int reference_cut(const std::vector<float>& l, const strata::kernels::SamplerParams& p, bool temp_first) {
    const int nv = (int) l.size();
    const float inv_t = p.temperature > 0.0f ? 1.0f / p.temperature : 0.0f;
    auto val = [&](int v) { return temp_first ? l[(size_t) v] * inv_t : l[(size_t) v]; };
    const int k = p.top_k > 0 ? p.top_k : nv;
    std::vector<int> ids;
    std::vector<char> taken((size_t) nv, 0);
    for (int i = 0; i < k; ++i) {
        int best = -1; float bv = 0;
        for (int v = 0; v < nv; ++v) {
            if (taken[(size_t) v]) continue;
            if (best < 0 || val(v) > bv) { best = v; bv = val(v); }
        }
        taken[(size_t) best] = 1; ids.push_back(best);
    }
    if (p.top_p < 1.0f) {
        float mx = val(ids[0]);
        for (int v : ids) mx = std::fmax(mx, val(v));
        double sum = 0;
        for (int v : ids) sum += std::exp((double) val(v) - (double) mx);
        double cum = 0;
        int cut = (int) ids.size();
        for (size_t i = 0; i < ids.size(); ++i) {
            cum += std::exp((double) val(ids[i]) - (double) mx) / sum;
            if (cum >= (double) p.top_p) { cut = (int) i + 1; break; }
        }
        if (cut < p.min_keep) cut = p.min_keep < (int) ids.size() ? p.min_keep : (int) ids.size();
        return cut;
    }
    return (int) ids.size();
}

int run(const char* name, const std::vector<float>& logits, int n_tokens, const strata::kernels::SamplerParams& p,
        const std::vector<int>& want, const std::vector<int>& hist = {}, int hist_len = 0) {
    float* d_l = nullptr;
    int* d_o = nullptr;
    check(DPCT_CHECK_ERROR(d_l = sycl::malloc_device<float>(
                               logits.size(), dpct::get_in_order_queue())),
          "malloc logits");
    check(DPCT_CHECK_ERROR(d_o = sycl::malloc_device<int>(
                               (size_t)n_tokens, dpct::get_in_order_queue())),
          "malloc out");
    /*
    DPCT1114: cudaMemcpy is migrated to asynchronization memcpy, assuming
    in the original code the source host memory is pageable memory. If the
    memory is not pageable, call wait() on event return by memcpy API to ensure
    synchronization behavior.
    */
    check(DPCT_CHECK_ERROR((dpct::get_current_device().queues_wait_and_throw(), dpct::get_in_order_queue()).memcpy(
              d_l, logits.data(), logits.size() * sizeof(float)).wait()),
          "copy");
    // -1 in every output slot first: a row the kernel leaves unwritten can never match (a verify window reads
    // every row, so "no output" is a wrong answer, not a skipped one)
    check(
        DPCT_CHECK_ERROR((dpct::get_current_device().queues_wait_and_throw(), dpct::get_in_order_queue())
                             .memset(d_o, 0xFF, (size_t)n_tokens * sizeof(int))
                             .wait()),
        "fill out");
    int* d_h = nullptr;
    if (hist_len > 0) {
        check(DPCT_CHECK_ERROR(d_h = sycl::malloc_device<int>(
                                   hist.size(), dpct::get_in_order_queue())),
              "malloc hist");
        /*
        DPCT1114: cudaMemcpy is migrated to asynchronization memcpy,
        assuming in the original code the source host memory is pageable memory.
        If the memory is not pageable, call wait() on event return by memcpy API
        to ensure synchronization behavior.
        */
        check(DPCT_CHECK_ERROR((dpct::get_current_device().queues_wait_and_throw(), dpct::get_in_order_queue()).memcpy(
                  d_h, hist.data(), hist.size() * sizeof(int)).wait()),
              "copy hist");
    }
    strata::kernels::sample_tokens(d_l, n_tokens, (int) (logits.size() / n_tokens), d_h, hist_len, p, d_o,
                                   nullptr);
    std::vector<int> got((size_t) n_tokens);
    check(
        DPCT_CHECK_ERROR((dpct::get_current_device().queues_wait_and_throw(), dpct::get_in_order_queue())
                             .memcpy(got.data(), d_o, got.size() * sizeof(int))
                             .wait()),
        "back");
    int bad = 0;
    for (int t = 0; t < n_tokens; ++t) if (got[(size_t) t] != want[(size_t) t]) ++bad;
    std::printf("  %-34s %s (%d of %d differ)", name, bad ? "*** WRONG ***" : "matches", bad, n_tokens);
    if (bad) std::printf("   first: want %d got %d", want[0], got[0]);
    std::printf("\n");
    sycl::free(d_l, dpct::get_in_order_queue());
    sycl::free(d_o, dpct::get_in_order_queue());
    if (d_h) sycl::free(d_h, dpct::get_in_order_queue());
    return bad;
}

// The Philox draw, host side - a transcription of the kernel's `philox_uniform` so the SAMPLED pick (not
// just the greedy argmax) can be pinned against a reference.  `__umulhi(a, b)` is the high half of a 32x32
// multiply, spelled `(uint32_t)(((uint64_t) a * b) >> 32)` here.
struct PhiloxRound {
    uint32_t& c0; uint32_t& c1; uint32_t& c2; uint32_t& c3;
    void step(uint32_t k0, uint32_t k1) const {
        const uint32_t hi0 = (uint32_t) (((uint64_t) 0x9E3779B9u * c0) >> 32);
        const uint32_t hi1 = (uint32_t) (((uint64_t) 0xBB67AE85u * c2) >> 32);
        const uint32_t lo0 = 0x9E3779B9u * c0;
        const uint32_t lo1 = 0xBB67AE85u * c2;
        const uint32_t n0 = hi1 ^ c1 ^ k0;
        const uint32_t n1 = lo1;
        const uint32_t n2 = hi0 ^ c3 ^ k1;
        const uint32_t n3 = lo0;
        c0 = n0; c1 = n1; c2 = n2; c3 = n3;
    }
};

float host_philox_uniform(uint64_t seed, uint64_t counter) {
    uint32_t c0 = (uint32_t) counter, c1 = (uint32_t) (counter >> 32);
    uint32_t c2 = (uint32_t) seed, c3 = (uint32_t) (seed >> 32);
    PhiloxRound r{c0, c1, c2, c3};
    for (int i = 0; i < 10; ++i) r.step((uint32_t) i, 0u);
    return (float) (c0 >> 8) * (1.0f / 16777216.0f);
}

// The full SAMPLED chain, host side - the kernel's `sampler_kernel` in serial form, in llama.cpp's order:
// penalties on the raw logits during the top_k selection (ties to the lowest index), top_p's cut in double over
// the top_k list, the min_p prefix cut on its survivors, the temperature, and one Philox draw at
// (seed, counter + row).  One penalties stage (issue #53: this reference used to repeat the kernel's second one).
int sampled_reference(const std::vector<float>& l, const std::vector<int>& hist,
                      const strata::kernels::SamplerParams& p, int row) {
    auto penal = [&](float logit, int count) {
        if (count <= 0) return logit;
        if (logit <= 0.0f) logit *= p.penalty_repeat; else logit /= p.penalty_repeat;
        logit -= (float) count * p.penalty_freq + (count > 0 ? 1.0f : 0.0f) * p.penalty_present;
        return logit;
    };
    auto count = [&](int v) { int c = 0; for (int h : hist) if (h == v) ++c; return c; };
    const int nv = (int) l.size();
    const int KMAX = 64;                       // 1..64 as given; 0 (off) and wider keep the widest list, 64
    const int k = std::min(nv, (p.top_k > 0 && p.top_k < KMAX) ? p.top_k : KMAX);
    std::vector<int> sel_ids;
    std::vector<float> sel_logit;
    std::vector<char> taken((size_t) nv, 0);
    for (int i = 0; i < k; ++i) {
        int best = -1; float bv = 0;
        for (int v = 0; v < nv; ++v) {
            if (taken[(size_t) v]) continue;
            const float s = penal(l[(size_t) v], count(v));
            if (best < 0 || s > bv) { best = v; bv = s; }
        }
        taken[(size_t) best] = 1;
        sel_ids.push_back(best); sel_logit.push_back(bv);
    }
    // llama.cpp's order (issue #53): top_p over the whole top_k list, then min_p on its survivors
    const int n_sel = (int) sel_ids.size();
    int n_keep = n_sel;
    if (p.top_p < 1.0f) {
        double sum = 0.0;
        for (int i = 0; i < n_sel; ++i) sum += std::exp((double) sel_logit[(size_t) i] - (double) sel_logit[0]);
        double cum = 0.0;
        int cut = n_sel;
        for (int i = 0; i < n_sel; ++i) {
            cum += std::exp((double) sel_logit[(size_t) i] - (double) sel_logit[0]) / sum;
            if (cum >= (double) p.top_p) { cut = i + 1; break; }
        }
        if (cut < p.min_keep) cut = p.min_keep < n_sel ? p.min_keep : n_sel;
        n_keep = cut;
    }
    if (p.min_p > 0.0f) {
        const float thresh = sel_logit[0] + std::log(p.min_p);
        for (int i = 0; i < n_keep; ++i)
            if (sel_logit[(size_t) i] < thresh) { n_keep = i; break; }
    }
    const float inv_t = p.temperature > 0.0f ? 1.0f / p.temperature : 0.0f;
    auto scaled = [&](int i) { return sel_logit[(size_t) i] * inv_t; };   // one penalties stage, before (#53)
    float smx = scaled(0);
    for (int i = 1; i < n_keep; ++i) smx = std::fmax(smx, scaled(i));
    double sum = 0.0;
    for (int i = 0; i < n_keep; ++i) sum += std::exp((double) scaled(i) - (double) smx);
    const float u = host_philox_uniform(p.seed, p.counter + (uint64_t) row);
    double cum = 0.0;
    int pick = sel_ids[(size_t) (n_keep - 1)];
    for (int i = 0; i < n_keep; ++i) {
        cum += std::exp((double) scaled(i) - (double) smx) / sum;
        if ((double) u < cum) { pick = sel_ids[(size_t) i]; break; }
    }
    return pick;
}

// Survivors after the min_p + top_p cuts, in the sampled chain - the quantity an order or a threshold
// actually changes, used to assert a fixture can SEE the feature before asserting the kernel matches.
int sampled_cut(const std::vector<float>& l, const std::vector<int>& hist, const strata::kernels::SamplerParams& p) {
    auto penal = [&](float logit, int count) {
        if (count <= 0) return logit;
        if (logit <= 0.0f) logit *= p.penalty_repeat; else logit /= p.penalty_repeat;
        logit -= (float) count * p.penalty_freq + (count > 0 ? 1.0f : 0.0f) * p.penalty_present;
        return logit;
    };
    auto count = [&](int v) { int c = 0; for (int h : hist) if (h == v) ++c; return c; };
    const int nv = (int) l.size();
    const int KMAX = 64;
    const int k = std::min(nv, (p.top_k > 0 && p.top_k < KMAX) ? p.top_k : KMAX);
    std::vector<float> sel;
    std::vector<char> taken((size_t) nv, 0);
    for (int i = 0; i < k; ++i) {
        int best = -1; float bv = 0;
        for (int v = 0; v < nv; ++v) {
            if (taken[(size_t) v]) continue;
            const float s = penal(l[(size_t) v], count(v));
            if (best < 0 || s > bv) { best = v; bv = s; }
        }
        taken[(size_t) best] = 1; sel.push_back(bv);
    }
    int n_minp = (int) sel.size();
    if (p.min_p > 0.0f) {
        const float thresh = sel[0] + std::log(p.min_p);
        for (int i = 0; i < (int) sel.size(); ++i)
            if (sel[(size_t) i] < thresh) { n_minp = i; break; }
    }
    if (p.top_p >= 1.0f) return n_minp;
    double sum = 0.0;
    for (int i = 0; i < n_minp; ++i) sum += std::exp((double) sel[(size_t) i] - (double) sel[0]);
    double cum = 0.0;
    int cut = n_minp;
    for (int i = 0; i < n_minp; ++i) {
        cum += std::exp((double) sel[(size_t) i] - (double) sel[0]) / sum;
        if (cum >= (double) p.top_p) { cut = i + 1; break; }
    }
    if (cut < p.min_keep) cut = p.min_keep < n_minp ? p.min_keep : n_minp;
    return cut;
}

// ---- the kernel's own semantics, for the fixtures ----
//
// `sampled_reference` picks the first unpicked logit even when it is -inf or NaN; the kernels never pick either, and
// a round that finds nothing stores id 0 with a -inf logit (and later rounds treat id 0 as taken, as `sampler_kernel`
// does).  The mirror below follows the kernels, so rows with -inf, NaN, +inf and more requested than finite logits
// can be pinned exactly.  On rows without those it is `sampled_reference`.
struct SelList {
    std::vector<int> ids;
    std::vector<float> logit;
};

// The top_k list of `sampler_kernel` for one row: `window` is the counted history (the row's last penalty_last_n).
SelList mirror_select(const float* l, int nv, const std::vector<int>& window, const strata::kernels::SamplerParams& p,
                      int k) {
    std::vector<float> s((size_t) nv);
    for (int v = 0; v < nv; ++v) {
        int c = 0;
        for (int h : window) c += h == v;
        float x = l[v];
        if (c > 0) {
            if (x <= 0.0f) x *= p.penalty_repeat; else x /= p.penalty_repeat;
            x -= (float) c * p.penalty_freq + (c > 0 ? 1.0f : 0.0f) * p.penalty_present;
        }
        s[(size_t) v] = x;
    }
    SelList out;
    std::vector<char> taken((size_t) nv, 0);
    for (int i = 0; i < k; ++i) {
        int best = nv;
        float bv = -std::numeric_limits<float>::infinity();
        for (int v = 0; v < nv; ++v)
            if (!taken[(size_t) v] && s[(size_t) v] > bv) { bv = s[(size_t) v]; best = v; }
        const int id = best < nv ? best : 0;
        taken[(size_t) id] = 1;
        out.ids.push_back(id);
        out.logit.push_back(bv);
    }
    return out;
}

// The tail of `sampler_kernel` over a list (its first `k` entries): top_p, min_p, temperature, the Philox draw.
int mirror_pick(const SelList& sel, int k, const strata::kernels::SamplerParams& p, int row,
                double* probability = nullptr) {
    int n_keep = k;
    float mx = sel.logit[0];
    for (int i = 1; i < k; ++i) mx = std::fmax(mx, sel.logit[(size_t) i]);
    if (p.top_p < 1.0f) {
        double sum = 0.0;
        for (int i = 0; i < k; ++i) sum += std::exp((double) sel.logit[(size_t) i] - (double) mx);
        double cum = 0.0;
        int cut = k;
        for (int i = 0; i < k; ++i) {
            cum += std::exp((double) sel.logit[(size_t) i] - (double) mx) / sum;
            if (cum >= (double) p.top_p) { cut = i + 1; break; }
        }
        if (cut < p.min_keep) cut = p.min_keep < k ? p.min_keep : k;
        n_keep = cut;
    }
    if (p.min_p > 0.0f) {
        const float thresh = sel.logit[0] + std::log(p.min_p);
        for (int i = 0; i < n_keep; ++i)
            if (sel.logit[(size_t) i] < thresh) { n_keep = i; break; }
    }
    const float inv_t = p.temperature > 0.0f ? 1.0f / p.temperature : 0.0f;
    auto scaled = [&](int i) { return sel.logit[(size_t) i] * inv_t; };
    float smx = scaled(0);
    for (int i = 1; i < n_keep; ++i) smx = std::fmax(smx, scaled(i));
    double sum = 0.0;
    for (int i = 0; i < n_keep; ++i) sum += std::exp((double) scaled(i) - (double) smx);
    const float u = host_philox_uniform(p.seed, p.counter + (uint64_t) row);
    double cum = 0.0;
    int pi = n_keep > 0 ? n_keep - 1 : 0;
    int pick = sel.ids[(size_t) (n_keep > 0 ? n_keep - 1 : 0)];
    for (int i = 0; i < n_keep; ++i) {
        cum += std::exp((double) scaled(i) - (double) smx) / sum;
        if ((double) u < cum) { pick = sel.ids[(size_t) i]; pi = i; break; }
    }
    if (probability)
        *probability = n_keep > 0 ? std::exp((double) scaled(pi) - (double) smx) / sum : 1.0;
    return pick;
}

int sampled_k(int top_k, int nv) { return std::min(nv, (top_k > 0 && top_k < 64) ? top_k : 64); }

// Rows uploaded once and sampled under many parameter sets: `sample` returns the picks of one launch on `stream`
// (nullptr: the legacy stream), -1 prefilled as in `run`.
struct DeviceRows {
    float* l = nullptr;
    int* h = nullptr;
    int* o = nullptr;
    int n_tokens = 0, nv = 0, hist_len = 0;
    DeviceRows(const std::vector<float>& logits, int n_tokens_, const std::vector<int>& hist, int hist_len_)
        : n_tokens(n_tokens_), nv((int) (logits.size() / (size_t) n_tokens_)), hist_len(hist_len_) {
        check(DPCT_CHECK_ERROR(l = sycl::malloc_device<float>(
                                   logits.size(), dpct::get_in_order_queue())),
              "malloc logits");
        check(
            DPCT_CHECK_ERROR(o = sycl::malloc_device<int>(
                                 (size_t)n_tokens, dpct::get_in_order_queue())),
            "malloc out");
        /*
        DPCT1114: cudaMemcpy is migrated to asynchronization memcpy,
        assuming in the original code the source host memory is pageable memory.
        If the memory is not pageable, call wait() on event return by memcpy API
        to ensure synchronization behavior.
        */
        check(DPCT_CHECK_ERROR((dpct::get_current_device().queues_wait_and_throw(), dpct::get_in_order_queue()).memcpy(
                  l, logits.data(), logits.size() * sizeof(float)).wait()),
              "copy");
        if (hist_len > 0) {
            check(
                DPCT_CHECK_ERROR(h = sycl::malloc_device<int>(
                                     hist.size(), dpct::get_in_order_queue())),
                "malloc hist");
            /*
            DPCT1114: cudaMemcpy is migrated to asynchronization memcpy,
            assuming in the original code the source host memory is pageable
            memory. If the memory is not pageable, call wait() on event return
            by memcpy API to ensure synchronization behavior.
            */
            check(DPCT_CHECK_ERROR((dpct::get_current_device().queues_wait_and_throw(), dpct::get_in_order_queue()).memcpy(
                      h, hist.data(), hist.size() * sizeof(int)).wait()),
                  "copy hist");
        }
    }
    DeviceRows(const DeviceRows&) = delete;
    DeviceRows& operator=(const DeviceRows&) = delete;
    ~DeviceRows() {
        sycl::free(l, dpct::get_in_order_queue());
        sycl::free(o, dpct::get_in_order_queue());
        if (h) sycl::free(h, dpct::get_in_order_queue());
    }
    std::vector<int> sample(const strata::kernels::SamplerParams &p,
                            dpct::queue_ptr stream) {
        check(DPCT_CHECK_ERROR(
                  (dpct::get_current_device().queues_wait_and_throw(), dpct::get_in_order_queue())
                      .memset(o, 0xFF, (size_t)n_tokens * sizeof(int))
                      .wait()),
              "fill out");
        strata::kernels::sample_tokens(l, n_tokens, nv, h, hist_len, p, o, stream);
        if (stream != &dpct::get_in_order_queue())
            check(DPCT_CHECK_ERROR(stream->wait()), "stream sync");
        std::vector<int> got((size_t) n_tokens);
        check(DPCT_CHECK_ERROR(
                  (dpct::get_current_device().queues_wait_and_throw(), dpct::get_in_order_queue())
                      .memcpy(got.data(), o, got.size() * sizeof(int))
                      .wait()),
              "back");
        return got;
    }
};

// The counted window of row t: the last min(penalty_last_n, hist_len) entries (none without penalties).
std::vector<int> window_of(const std::vector<int>& hist, int hist_len, int t, int last_n) {
    if (hist_len <= 0 || last_n <= 0) return {};
    const int h = std::min(last_n, hist_len);
    const int* row = hist.data() + (size_t) t * hist_len;
    return std::vector<int>(row + (hist_len - h), row + hist_len);
}

// `--bench`: the sampled path alone at the engine's vocabulary, per call, on the path the environment selects.
void bench_sampled() {
    const int NV = 248320;
    dpct::queue_ptr s = &dpct::get_in_order_queue();
    check(DPCT_CHECK_ERROR(s = dpct::get_current_device().create_queue(true)),
          "stream");
    std::mt19937 rng(20);
    std::normal_distribution<float> g(0.0f, 3.0f);
    for (int T : {1, 4, 8}) {
        std::vector<float> l((size_t) NV * T);
        for (auto& v : l) v = g(rng);
        float* d_l = nullptr;
        int* d_o = nullptr;
        check(DPCT_CHECK_ERROR(d_l = sycl::malloc_device<float>(
                                   l.size(), dpct::get_in_order_queue())),
              "bench logits");
        check(DPCT_CHECK_ERROR(d_o = sycl::malloc_device<int>(
                                   (size_t)T, dpct::get_in_order_queue())),
              "bench out");
        /*
        DPCT1114: cudaMemcpy is migrated to asynchronization memcpy,
        assuming in the original code the source host memory is pageable memory.
        If the memory is not pageable, call wait() on event return by memcpy API
        to ensure synchronization behavior.
        */
        check(DPCT_CHECK_ERROR((dpct::get_current_device().queues_wait_and_throw(), dpct::get_in_order_queue()).memcpy(
                  d_l, l.data(), l.size() * sizeof(float)).wait()),
              "bench copy");
        for (int k : {20, 64}) {
            strata::kernels::SamplerParams p;
            p.top_k = k; p.top_p = 0.95f; p.temperature = 0.7f; p.seed = 1;
            for (int w = 0; w < 3; ++w) strata::kernels::sample_tokens(d_l, T, NV, nullptr, 0, p, d_o, s);
            check(DPCT_CHECK_ERROR(s->wait()), "bench warmup");
            dpct::event_ptr e0, e1;
            check(DPCT_CHECK_ERROR(e0 = new sycl::event()), "event");
            check(DPCT_CHECK_ERROR(e1 = new sycl::event()), "event");
            const int iters = 50;
            /*
            DPCT1024: The original code returned the error code that was
            further consumed by the program logic. This original code was
            replaced with 0. You may need to rewrite the program logic consuming
            the error code.
            */
            check(DPCT_CHECK_ERROR(dpct::sync_barrier(e0, s)), "record");
            for (int it = 0; it < iters; ++it) {
                p.counter = (uint64_t) it;
                strata::kernels::sample_tokens(d_l, T, NV, nullptr, 0, p, d_o, s);
            }
            /*
            DPCT1024: The original code returned the error code that was
            further consumed by the program logic. This original code was
            replaced with 0. You may need to rewrite the program logic consuming
            the error code.
            */
            check(DPCT_CHECK_ERROR(dpct::sync_barrier(e1, s)), "record");
            check(DPCT_CHECK_ERROR(e1->wait_and_throw()), "bench sync");
            float ms = 0.0f;
            check(DPCT_CHECK_ERROR(
                      ms = (e1->get_profiling_info<
                                sycl::info::event_profiling::command_end>() -
                            e0->get_profiling_info<
                                sycl::info::event_profiling::command_start>()) /
                           1000000.0f),
                  "elapsed");
            std::printf("  bench: n_vocab %d, rows %d, top_k %2d, top_p 0.95: %8.1f us per call\n", NV, T, k,
                        1000.0 * (double) ms / iters);
            dpct::destroy_event(e0);
            dpct::destroy_event(e1);
        }
        sycl::free(d_l, dpct::get_in_order_queue());
        sycl::free(d_o, dpct::get_in_order_queue());
    }
    dpct::get_current_device().destroy_queue(s);
}

}  // namespace

int main() {
    auto& q = dpct::get_in_order_queue();
    std::printf("device: %s fp64=%d\n", q.get_device().get_info<sycl::info::device::name>().c_str(), q.get_device().has(sycl::aspect::fp64));
    if (!q.get_device().has(sycl::aspect::fp64)) return 77;
    constexpr int T=4, NV=512, H=16;
    std::mt19937 rng(2026);
    std::vector<float> l(T*NV);
    std::vector<int> hist(T*H);
    for(auto& x:l) x=float(int(rng()%1000)-500)/32.0f;
    for(auto& x:hist) x=int(rng()%NV);
    int bad=0;
    for(int config=0;config<12;++config) {
        strata::kernels::SamplerParams p;
        p.top_k=config%2?64:20; p.top_p=config%3?0.9f:1.0f;
        p.min_p=config%2?0.05f:0.0f; p.temperature=0.7f;
        p.penalty_last_n=H; p.penalty_repeat=1.1f; p.penalty_freq=0.1f; p.penalty_present=0.2f;
        p.seed=123456789+config; p.counter=1000*config;
        std::vector<int> want(T);
        for(int t=0;t<T;++t) want[t]=sampled_reference({l.begin()+t*NV,l.begin()+(t+1)*NV}, {hist.begin()+t*H,hist.begin()+(t+1)*H},p,t);
        bad+=run("FP64 vs double CPU oracle",l,T,p,want,hist,H);
    }
    std::printf("FP64 smoke: %d failures / 48 draws\n",bad);
    return bad?1:0;
}
