// src/spec/draft_policy_test.cpp - DraftPolicy: when does a lookup window beat the MTP's?
//
// Simulated rounds with the costs measured on the RTX 5070 (window-cost: ~10 ms more per token) check that
//   1. with no lookup proposal the MTP window is kept;
//   2. lookup drafts that are mostly rejected stop being taken (their bucket's rate falls);
//   3. lookup drafts that are always accepted are taken, and the window grows with them;
//   4. match-length buckets learn separately (short matches failing does not stop long ones);
//   5. the policy never proposes a window beyond its cap.
#include "strata/spec/draft_policy.hpp"

#include <cstdio>
#include <limits>

using strata::spec::DraftPolicy;

namespace {
int g_fail = 0;
void check(bool ok, const char* what) {
    std::printf("  %-66s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok) ++g_fail;
}
double cost(int t) { return 19.0 + 10.5 * (t - 1); }   // ms per round, measured shape
}  // namespace

int main() {
    std::printf("draft_policy_test\n");
    {
        using strata::spec::mtp_confidence_window;
        const float p[] = {0.75f, 0.75f, 0.75f, 0.75f, 0.75f, 0.75f, 0.75f};
        for (int t = 1; t <= 8; ++t) {
            check(mtp_confidence_window(t, p, 0.5f) == t, "confidence: disabled cumulative floor keeps the old window");
            check(mtp_confidence_window(t, nullptr, 0.0f) == t, "confidence: both floors off do not read probabilities");
        }
        check(mtp_confidence_window(4, p, 0.5f, 0.5f) == 3,
              "confidence: cumulative floor cuts individually confident drafts");
        check(mtp_confidence_window(4, p, 0.0f, 0.5f) == 3,
              "confidence: cumulative rule works without a per-token floor");
        const float half[] = {0.5f, 0.5f, 0.5f};
        check(mtp_confidence_window(4, half, 0.5f, 0.25f) == 3,
              "confidence: equality at either threshold is accepted");
        check(mtp_confidence_window(4, half, 0.6f, 0.1f) == 1,
              "confidence: cumulative rule cannot bypass the per-token floor");
        const float partial[] = {0.75f, 0.75f, std::numeric_limits<float>::quiet_NaN()};
        check(mtp_confidence_window(3, partial, 0.0f, 0.5f) == 3,
              "confidence: early drafting reads only completed probabilities");
        const float stopped[] = {0.75f, 0.75f, 0.75f, 0, 0, 0, 0};
        check(mtp_confidence_window(8, stopped, 0.0f, 0.5f) ==
              mtp_confidence_window(8, p, 0.0f, 0.5f),
              "confidence: zeroed uncomputed suffix keeps the full-chain window");
        const float rejected[] = {0.25f, 0, 0};
        check(mtp_confidence_window(4, rejected, 0.0f, 0.5f) == 1,
              "confidence: the first rejected draft leaves only the fed-back row");
        check(mtp_confidence_window(5, p, 0.5f, 0.0f, 0.8f) == 4 &&
              mtp_confidence_window(4, p, 0.5f, 0.0f, 0.8f) == 4,
              "confidence: last-draft override still applies only to five rows");
        check(mtp_confidence_window(1, nullptr, 0.5f, 0.5f) == 1 &&
              mtp_confidence_window(4, nullptr, 0.5f, 0.5f) == 1,
              "confidence: no drafts or missing probabilities stay at one row");
        const float ones[] = {1, 1, 1, 1, 1, 1, 1};
        check(mtp_confidence_window(8, ones, 0.0f, 1.0f) == 8 &&
              mtp_confidence_window(4, p, 0.0f, 1.0f) == 1,
              "confidence: cumulative floor one retains only certain drafts");
        for (float bad : {-0.1f, 1.1f, std::numeric_limits<float>::infinity(),
                          std::numeric_limits<float>::quiet_NaN()}) {
            const float invalid[] = {1.0f, bad, 1.0f};
            check(mtp_confidence_window(4, invalid, 0.0f, 0.5f) == 2,
                  "confidence: invalid draft confidence ends the prefix");
        }
    }
    {
        DraftPolicy p(6);
        for (int i = 0; i < 50; ++i) p.observe(false, 4, 2, 0, cost(4));   // MTP windows of 4: 3 tokens each
        const DraftPolicy::Pick k = p.choose(4, 0, 0);
        check(!k.lookup && k.t == 4, "no proposal: the MTP window");
    }
    {
        DraftPolicy p(6);
        for (int i = 0; i < 50; ++i) p.observe(false, 4, 2, 0, cost(4));
        for (int t = 2; t <= 6; ++t) p.observe(false, t, 0, 0, cost(t));
        for (int i = 0; i < 40; ++i) p.observe(true, 6, 0, 4, cost(6));      // short matches, all rejected
        check(p.lookup_rate(4) < 0.15, "rejected short-match drafts: their rate falls below 0.15");
        check(!p.choose(4, 5, 4).lookup, "rejected short-match drafts: no longer taken");
        for (int i = 0; i < 40; ++i) p.observe(true, 6, 5, 30, cost(6));     // long matches, all accepted
        check(p.lookup_rate(30) > 0.9, "accepted long-match drafts: their rate rises above 0.9");
        const DraftPolicy::Pick k = p.choose(4, 5, 30);
        check(k.lookup && k.t == 6, "accepted long matches: the full lookup window is taken");
        check(!p.choose(4, 5, 4).lookup, "buckets are separate: short matches still not taken");
        check(p.choose(4, 20, 30).t <= 6, "never beyond the window cap");
    }
    {
        DraftPolicy p(8);
        for (int i = 0; i < 50; ++i) p.observe(false, 3, 2, 0, cost(3));      // a very good MTP: 3 of 3 tokens
        for (int t = 2; t <= 8; ++t) p.observe(false, t, t - 1, 0, cost(t));
        for (int i = 0; i < 40; ++i) p.observe(true, 4, 2, 8, cost(4));       // lookup at q ~ 0.67
        check(!p.choose(3, 7, 8).lookup, "a mediocre lookup does not replace a strong MTP window");
    }
    {
        DraftPolicy p(6);
        for (int i = 0; i < 50; ++i) p.observe(false, 4, 3, 0, cost(4));      // a near-perfect MTP, only size 4 seen
        const DraftPolicy::Pick k = p.choose(4, 5, 40);
        check(k.lookup && k.t == 6, "an unmeasured size is probed for a confident lookup");
        for (int i = 0; i < 3; ++i) p.observe(true, 6, 5, 40, 3.0 * cost(6));   // it turns out very expensive
        check(!p.choose(4, 5, 40).lookup, "after the probes, the measured cost decides");
    }
    {
        // --lookup-chain: rows cheap (UMA-like) and the chain always accepted -> chained; rows dear or the chain
        // always rejected -> not
        auto cheap = [](int t) { return 40.0 + 2.0 * (t - 1); };
        DraftPolicy p(8);
        for (int t = 2; t <= 8; ++t)
            for (int i = 0; i < 5; ++i) p.observe(false, t, 1, 0, cheap(t));
        for (int i = 0; i < 30; ++i) p.observe(false, 4, 2, 0, cheap(4));
        for (int i = 0; i < 30; ++i) p.observe_chain(4, 3, 6, 12, cheap(7));    // every chained token accepted
        check(p.chain(4, 0.9, 3, 12) == 3, "chain: cheap rows, always accepted -> all 3 chained");
        DraftPolicy q(8);
        for (int t = 2; t <= 8; ++t)
            for (int i = 0; i < 5; ++i) q.observe(false, t, 1, 0, cost(t));
        for (int i = 0; i < 30; ++i) q.observe_chain(4, 3, 3, 4, cost(7));       // reached, never accepted
        check(q.chain(4, 0.9, 3, 4) == 0, "chain: dear rows, never accepted -> none");
        check(q.chain(4, 0.9, 0, 4) == 0 && q.chain(8, 1.0, 3, 40) == 0, "chain: nothing proposed / no room -> none");
    }
    {
        DraftPolicy p(6);
        for (int i = 0; i < 7; ++i) p.observe(false, 4, 0, 0, 40.0);
        check(p.choose_mtp(4) == 4, "MTP: first eight windows retain the threshold policy");
        p.observe(false, 4, 0, 0, 40.0);
        check(p.choose_mtp(4) == 1, "MTP: rejected drafts favor the cheapest prefix");
        check(p.choose_mtp(1) == 1 && p.choose_mtp(0) == 1, "MTP: no drafts stays at one row");
        for (int i = 0; i < 7; ++i) p.observe(false, 4, 0, 0, 40.0);
        check(p.choose_mtp(4) == 4, "MTP: round sixteen refreshes the full threshold window");
        p.observe(false, 4, 0, 0, 40.0);
        check(p.choose_mtp(4) == 1, "MTP: selection resumes after the refresh");
        p.observe(false, 1, 0, 0, 100.0);
        check(p.choose_mtp(4) > 1, "MTP: a measured expensive prefix overrides its prior cost");
    }
    {
        DraftPolicy p(6);
        for (int i = 0; i < 20; ++i) p.observe(true, 4, 0, 30, 40.0);
        check(p.choose_mtp(4) == 4, "MTP: suffix results do not train MTP prefix acceptance");
        for (double bad : {0.0, -1.0, std::numeric_limits<double>::infinity(),
                           std::numeric_limits<double>::quiet_NaN()})
            for (int i = 0; i < 10; ++i) p.observe(false, 4, 0, 0, bad);
        check(p.choose_mtp(4) == 4, "MTP: invalid times do not count as observations");
        for (int i = 0; i < 8; ++i) p.observe(false, 4, 3, 0, 40.0);
        check(p.choose_mtp(4) == 4, "MTP: all accepted drafts keep the long window");
        check(p.choose_mtp(2) <= 2, "MTP: selection never exceeds the probability threshold cap");
        DraftPolicy single(1);
        for (int i = 0; i < 20; ++i) single.observe(false, 1, 0, 0, 1.0);
        check(single.choose_mtp(8) == 1, "MTP: one-row verifier stays at one row");
    }
    {
        DraftPolicy p(6);
        for (int i = 0; i < 8; ++i) p.observe(false, 4, 1, 0, 40.0);
        check(p.choose_mtp(4) == 2, "MTP: partial acceptance learns the committed prefix");
        // All shorter prefixes cost enough that none clears the 3% switching margin.
        DraftPolicy near(4);
        for (int t = 1; t <= 4; ++t)
            for (int i = 0; i < 8; ++i) near.observe(false, t, t - 1, 0, t == 4 ? 4.0 : t / 1.02);
        check(near.choose_mtp(4) == 4, "MTP: a two-percent predicted gain does not switch windows");
    }
    std::printf(g_fail ? "FAIL\n" : "PASS\n");
    return g_fail ? 1 : 0;
}
