"""Recompute all five rejected ESIMD screens without a GPU or dependencies."""
from pathlib import Path
import hashlib
import json
import math
import statistics

H = Path(__file__).resolve().parent
d = json.loads((H / 'results.json').read_text(encoding='utf-8'))
assert d['decision'] == 'kept_fixed_geometry_baseline'
assert d['baseline_sha256'] == d['production_sha256'] == '1c1ed46aaf692a17a27effacca17a1c99c1d0981a877ad309da9fcd4b857fc9f'
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
    gains = [row['pooled_gain_pct'] for row in rows]
    pooled = 100 * (math.exp(statistics.mean(math.log1p(g / 100) for g in gains)) - 1) if all(g is not None for g in gains) else None
    return {'T': T, 'rows': rows, 'pooled_gain_pct': pooled,
        'passed': all(g is not None and g > 0 for g in gains) and pooled >= 1}


for name, cohort in d['cohorts'].items():
    assert cohort['complete'] and not cohort['confirm'] and not cohort['decision']
    assert len(cohort['screen']) == (3 if name == 'exchange' else 2)
    for entry in cohort['screen']:
        assert entry['checks'] == 71 and len(entry['runs']) == 1
        checks += entry['checks']
        for run in entry['runs']:
            assert len(run['samples']) == 450 and len(run['fingerprints']) == 50
            if oracle is None:
                oracle = run['fingerprints']
            assert run['fingerprints'] == oracle, 'Full fingerprints differ across candidates'
            samples += len(run['samples'])
            fingerprints += len(run['fingerprints'])
            for T in (4, 6):
                for keep in range(1, T + 1):
                    for arm in range(5):
                        batch = [s for s in run['samples'] if (s['T'], s['keep'], s['arm']) == (T, keep, arm)]
                        assert sorted(s['index'] for s in batch) == list(range(9))
                        assert all(math.isfinite(s['us']) and s['us'] > 0 for s in batch)
                        equal(statistics.median(s['us'] for s in batch), run['medians'][f'{T}/{keep}/{arm}'])
        for cap in entry['capacities']:
            equal(compare(entry['runs'], cap['T']), cap)
            assert not cap['passed']
assert samples == d['samples'] == 2250
assert fingerprints == d['fingerprint_sets'] == 250
assert checks == d['screen_checks'] == 355 and pairs == 100
equal(max(drifts), d['max_abs_baseline_drift_pct'])
for row in d['correctness']:
    assert 'PASS checks=71' in row['stdout'] and not row['stderr']
    # Stored text uses normalized newlines; the original log has a separate byte hash.
    assert hashlib.sha256(row['stdout'].encode()).hexdigest() == row['stdout_sha256']
assert len(d['correctness']) == 5
for row in d['candidate_patches']:
    assert hashlib.sha256((H / row['file']).read_bytes()).hexdigest() == row['sha256']
print(f'PASS: {samples} samples, {fingerprints} identical fingerprints, {pairs} pairs ({eligible} eligible), '
      f'{checks} recorded checks; all five candidates rejected; candidate patch/log hashes verified')
