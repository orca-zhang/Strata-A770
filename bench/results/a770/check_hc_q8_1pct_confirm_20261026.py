"""Recompute both HC-Q8 cohorts under the user's updated 1% rule."""
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import statistics
import sys

H = Path(__file__).resolve().parent
STEM = 'hc-q8-model-1pct-confirm-2026-10-26'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read(path):
    return json.loads(path.read_text(encoding='utf-8'))


def load(name):
    spec = importlib.util.spec_from_file_location(name, H / (name + '.py'))
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


def passes(perf):
    assert set(perf['comparison']) == {'Code2K', 'Prose2K'}
    return all(p['throughput_change_pct'] >= 1 and abs(p['baseline_drift_pct']) <= 3
               for p in perf['comparison'].values())


def check(d):
    original = read(H / 'hc-q8-model-2026-10-25.json')
    load('check_hc_q8_model_20261025').check(original)
    load('check_hc_q8_1pct_update_20261025').check()
    assert d['complete'] and not d['production_changed'] and not d['target_achieved']
    assert d['previous_public_sha256'] == sha(H / 'hc-q8-model-2026-10-25.json')
    assert d['rule_update_sha256'] == sha(H / 'hc-q8-model-1pct-update-2026-10-25.json')
    assert d['minimum_gain_pct'] == 1 and d['maximum_baseline_drift_pct'] == 3
    interrupted = d['interrupted_attempt']
    assert interrupted['complete_requests'] == 15 and interrupted['excluded_from_confirmation']
    perf = d['confirmation']
    assert [r['label'] for r in perf['runs']] == ['A1', 'B1', 'A2']
    schedule = [(name, warm) for warm in (True, False, False, False) for name in ('Code2K', 'Prose2K')]
    for run in perf['runs']:
        candidate = run['label'] == 'B1'
        assert run['exit_code'] == 0 and run['expert_slots'] == (6666 if candidate else 6297)
        assert run['free_mib'] >= (1536 if candidate else 1500)
        assert [(r['case'], r['warmup']) for r in run['rows']] == schedule
        previous = original['performance']['runs'][int(candidate)]['rows']
        for row in run['rows']:
            assert row['generated'] == 128 and row['prompt_tokens'] == 2048 and row['reused'] == 0
            assert row['finish'] == 'length' and math.isfinite(row['decode_ms']) and row['decode_ms'] > 0
            assert 0 <= row['accepted'] <= row['offered']
            assert row['output_sha256'] == next(r['output_sha256'] for r in previous if r['case'] == row['case'])
    assert set(perf['comparison']) == {'Code2K', 'Prose2K'}
    for name, p in perf['comparison'].items():
        ms = [statistics.median(r['decode_ms'] for r in run['rows'] if r['case'] == name and not r['warmup']) for run in perf['runs']]
        base = math.sqrt(ms[0] * ms[2])
        expected = dict(baseline=base, candidate=ms[1], throughput_change_pct=100*(base/ms[1]-1),
            baseline_drift_pct=100*(ms[2]/ms[0]-1), baseline_tps=128000/base, candidate_tps=128000/ms[1],
            candidate_min_tps=min(128000/r['decode_ms'] for r in perf['runs'][1]['rows'] if r['case'] == name and not r['warmup']))
        assert p == expected
    assert d['passes_confirmation'] == (passes(original['performance']) and passes(perf))
    assert len(perf['telemetry']) == 24
    assert [(t['label'], t['case'], t['warmup']) for t in perf['telemetry']] == [(run['label'], r['case'], r['warmup']) for run in perf['runs'] for r in run['rows']]
    assert all(t['samples'] > 0 and 0 < t['min_mhz'] <= t['max_mhz'] for t in perf['telemetry'])
    extension = d['enlarged_cache_quality']
    if extension is not None:
        assert d['passes_confirmation']
        assert extension['exit_code'] == 0 and extension['expert_slots'] == 6666 and extension['targets'] == 758
        prior_scores = {name: row['scores_sha256'] for name, row in original['quality']['runs'][1]['cases'].items()}
        assert set(extension['scores_sha256']) == set(prior_scores)
        assert extension['scores_equal_prior_q8'] == (extension['scores_sha256'] == prior_scores)
        assert extension['passes_quality'] == (extension['scores_equal_prior_q8'] and extension['comparison']['aggregate_ppl_change_pct'] <= 1
            and all(r['ppl_change_pct'] <= 3 for r in extension['comparison']['cases'].values()))
        if extension['scores_equal_prior_q8']:
            assert extension['comparison'] == original['quality']['comparison']
    assert d['candidate_accepted'] == (d['passes_confirmation'] and extension is not None and extension['passes_quality'])


def main():
    d = read(H / (STEM + '.json'))
    assert sha(Path(__file__)) == d['checker_sha256']
    assert sha(H / (STEM + '.md')) == d['report_sha256']
    check(d)
    print('PASS HC-Q8 two-cohort 1% decision, all samples and enlarged-cache quality accounting; accepted', d['candidate_accepted'])


def self_check():
    # In-memory replay of the old published cohort, never a new measurement or confirmation record.
    original = read(H / 'hc-q8-model-2026-10-25.json')
    fixture = dict(complete=True, production_changed=False, target_achieved=False,
        previous_public_sha256=sha(H / 'hc-q8-model-2026-10-25.json'),
        rule_update_sha256=sha(H / 'hc-q8-model-1pct-update-2026-10-25.json'),
        minimum_gain_pct=1, maximum_baseline_drift_pct=3, confirmation=original['performance'],
        passes_confirmation=True, enlarged_cache_quality=None, candidate_accepted=False,
        interrupted_attempt=dict(complete_requests=15, excluded_from_confirmation=True))
    check(fixture)
    fixture['enlarged_cache_quality'] = dict(exit_code=0, expert_slots=6666, targets=758,
        scores_equal_prior_q8=True, passes_quality=True, comparison=original['quality']['comparison'],
        scores_sha256={name: row['scores_sha256'] for name, row in original['quality']['runs'][1]['cases'].items()})
    fixture['candidate_accepted'] = True
    check(fixture)
    fixture['confirmation']['runs'][1]['rows'][2]['decode_ms'] = -1
    try:
        check(fixture)
    except AssertionError:
        print('PASS checker self-check: preserved-cohort fixture and rejection of negative decode time; no new measurement')
    else:
        raise AssertionError('invalid timing accepted')


if __name__ == '__main__':
    self_check() if '--self-check' in sys.argv else main()
