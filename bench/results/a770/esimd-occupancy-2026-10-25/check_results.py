"""Recompute all paired ESIMD results and verify recorded native evidence; no GPU needed."""
from pathlib import Path
import hashlib
import json
import math
import re
import statistics

H = Path(__file__).resolve().parent
d = json.loads((H / 'results.json').read_text(encoding='utf-8'))
samples = fingerprints = pairs = eligible = checks = 0
drifts, oracle = [], None


def equal(a, b):
    if isinstance(a, float):
        assert math.isclose(a, b, rel_tol=1e-9, abs_tol=1e-9), (a, b)
    elif isinstance(a, dict):
        assert a.keys() == b.keys()
        for key in a:
            equal(a[key], b[key])
    elif isinstance(a, list):
        assert len(a) == len(b)
        for x, y in zip(a, b, strict=True):
            equal(x, y)
    else:
        assert a == b


def compare(runs, T):
    global pairs, eligible
    rows = []
    for keep in range(1, T + 1):
        results, ratios = [], []
        for run in runs:
            medians = [run['medians'][f'{T}/{keep}/{arm}'] for arm in range(5)]
            for arm in (1, 3):
                a, b, c = medians[arm - 1:arm + 2]
                baseline, drift = math.sqrt(a * c), 100 * (c / a - 1)
                results.append({'baseline': baseline, 'candidate': b,
                    'throughput_change_pct': 100 * (baseline / b - 1), 'baseline_drift_pct': drift})
                pairs += 1
                drifts.append(abs(drift))
                if abs(drift) <= 3:
                    eligible += 1
                    ratios.append(baseline / b)
        gain = 100 * (math.exp(statistics.mean(map(math.log, ratios))) - 1) if ratios else None
        rows.append({'keep': keep, 'pairs': results, 'eligible': len(ratios), 'pooled_gain_pct': gain})
    gains = [r['pooled_gain_pct'] for r in rows]
    pooled = 100 * (math.exp(statistics.mean(math.log1p(g / 100) for g in gains)) - 1) if all(g is not None for g in gains) else None
    return {'T': T, 'rows': rows, 'pooled_gain_pct': pooled,
        'passed': all(g is not None and g > 0 for g in gains) and pooled >= 1}


for name, cohort in d['cohorts'].items():
    assert cohort['complete'] and len(cohort['screen']) == (3 if name == 'prototype' else 1)
    for entry in cohort['screen'] + cohort['confirm']:
        assert len(entry['runs']) == (1 if 'checks' in entry else 2)
        if 'checks' in entry:
            assert entry['checks'] == 85
            checks += entry['checks']
        for run in entry['runs']:
            assert len(run['samples']) == 450 and len(run['fingerprints']) == 50
            if oracle is None:
                oracle = run['fingerprints']
            assert run['fingerprints'] == oracle, 'Full fingerprints differ'
            samples += len(run['samples'])
            fingerprints += len(run['fingerprints'])
            for T in (4, 6):
                for keep in range(1, T + 1):
                    for arm in range(5):
                        batch = [s for s in run['samples'] if (s['T'], s['keep'], s['arm']) == (T, keep, arm)]
                        assert sorted(s['index'] for s in batch) == list(range(9))
                        assert all(math.isfinite(s['us']) and s['us'] > 0 for s in batch)
                        equal(statistics.median(s['us'] for s in batch), run['medians'][f'{T}/{keep}/{arm}'])
        equal([compare(entry['runs'], T) for T in (4, 6)], entry['capacities'])
    options = [e for e in cohort['screen'] if all(c['passed'] for c in e['capacities'])]
    selected = max(options, key=lambda e: sum(math.log1p(c['pooled_gain_pct']/100) for c in e['capacities'])) if options else None
    assert len(cohort['confirm']) == int(selected is not None)
    if selected:
        confirmed = cohort['confirm'][0]
        assert confirmed['mode'] == selected['mode']
        assert set(cohort['decision']) == {'4', '6'}
        for cap in confirmed['capacities']:
            result = cohort['decision'][str(cap['T'])]
            assert result['mode'] == selected['mode']
            assert result['name'] == cohort['preparation']['variants'][str(result['mode'])]
            equal({k: v for k, v in result.items() if k not in ('mode', 'name')}, cap)
    else:
        assert cohort['decision'] == {}

assert samples == d['samples'] and fingerprints == d['fingerprint_sets']
assert pairs == d['pairs'] and eligible == d['eligible_pairs'] and checks == d['screen_checks']
equal(max(drifts), d['max_abs_baseline_drift_pct'])
assert len(d['correctness']) == 3 + int('final' in d['cohorts'])
for row in d['correctness']:
    assert 'PASS checks=85' in row['stdout'] and not row['stderr']
    assert hashlib.sha256(row['stdout'].encode()).hexdigest() == row['stdout_sha256']
for row in d['artifacts']:
    assert hashlib.sha256((H / row['file']).read_bytes()).hexdigest() == row['sha256'], row['file']
for row in d['native']:
    m = (H / row['ze_info']).read_text()
    assert int(re.search(r'grf_count:\s*(\d+)', m)[1]) == row['grf']
    assert int(re.search(r'slm_size:\s*(\d+)', m)[1]) == row['slm_bytes']
    assert ('per_thread_memory_buffers' in m) == row['has_scratch_allocation']
assert len([r for r in d['native'] if r['cohort'] == 'prototype']) == 8
if d['adopted']:
    assert d['decision'].startswith('adopted_')
    assert all(v['passed'] for v in d['cohorts']['final']['decision'].values())
    p = d['production_checks']
    assert p['passed'] and p['production_sha256'] == d['production_sha256']
    assert p['output']['commit-layers'].count('42 cases, 0 failures') == 4
    assert '156 cases, 0 failures' in p['output']['window'] and '44 cases, 0 total failures' in p['output']['window']
    assert 'gdn_parity OK' in p['output']['selftest']
    for T in (4, 6):
        old, new = [next(r for r in d['native'] if r['cohort'] == 'final' and r['kind'] == k and r['T'] == T)
                    for k in ('single-before', 'single-after')]
        assert (H / old['asm']).read_bytes() == (H / new['asm']).read_bytes()
else:
    assert d['decision'] == 'not_adopted'
    last = d['cohorts'].get('final', d['cohorts']['prototype'])['decision']
    assert set(last) != {'4', '6'} or not all(v['passed'] for v in last.values())
print(f'PASS: {samples} samples, {fingerprints} identical fingerprints, {pairs} pairs ({eligible} eligible), '
      f'{checks} recorded checks; selection, artifact hashes and native evidence verified')
