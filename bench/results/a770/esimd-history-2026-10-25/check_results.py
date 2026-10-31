"""Recompute ESIMD screening, confirmation and native evidence without a GPU."""
from pathlib import Path
import hashlib
import json
import math
import re
import statistics

H = Path(__file__).resolve().parent
d = json.loads((H / 'results.json').read_text(encoding='utf-8'))
assert d['decision'] == 'adopted_history_block_store'
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
    assert cohort['complete'] and len(cohort['screen']) == {'layout': 3, 'history': 2, 'final': 1}[name]
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
        for cap in entry['capacities']:
            equal(compare(entry['runs'], cap['T']), cap)
    selected = {}
    for T in (4, 6):
        options = [(e, next(c for c in e['capacities'] if c['T'] == T)) for e in cohort['screen']]
        options = [(e, c) for e, c in options if c['passed']]
        if options:
            selected[str(T)] = max(options, key=lambda v: v[1]['pooled_gain_pct'])[0]['mode']
    assert {e['mode'] for e in cohort['confirm']} == set(selected.values())
    assert cohort['decision'].keys() == selected.keys()
    for T, mode in selected.items():
        result = cohort['decision'][T]
        assert result['mode'] == mode
        confirm = next(e for e in cohort['confirm'] if e['mode'] == mode)
        cap = next(c for c in confirm['capacities'] if c['T'] == int(T))
        equal({k: v for k, v in result.items() if k not in ('mode', 'name')}, cap)
history = d['cohorts']['history']
for stage in ('screen', 'confirm'):
    assert all(c['passed'] for c in next(e for e in history[stage] if e['mode'] == 1)['capacities'])
assert all(c['passed'] for e in d['cohorts']['final']['screen'] + d['cohorts']['final']['confirm'] for c in e['capacities'])
assert samples == d['samples'] == 5400 and fingerprints == d['fingerprint_sets'] == 600
assert pairs == 240 and checks == d['screen_checks'] == 510
equal(max(drifts), d['max_abs_baseline_drift_pct'])
assert len(d['correctness']) == 6
for row in d['correctness']:
    assert 'PASS checks=85' in row['stdout'] and not row['stderr']
    assert hashlib.sha256(row['stdout'].encode()).hexdigest() == row['stdout_sha256']
for row in d['patches']:
    assert hashlib.sha256((H / row['file']).read_bytes()).hexdigest() == row['sha256']
assert hashlib.sha256((H / 'adoption-policy.json').read_bytes()).hexdigest() == d['adoption_policy_sha256']
equal(json.loads((H / 'adoption-policy.json').read_text()), d['adoption_policy'])
p = d['production_checks']
assert p['passed'] and p['production_sha256'] == d['production_sha256']
assert p['output']['commit-layers'].count('42 cases, 0 failures') == 4
assert '156 cases, 0 failures' in p['output']['window'] and '44 cases, 0 total failures' in p['output']['window']
assert 'gdn_parity OK' in p['output']['selftest']
assert len(d['native']) == 4
for row in d['native']:
    for key in ('asm', 'ze_info'):
        assert hashlib.sha256((H / row[key]).read_bytes()).hexdigest() == row[key + '_sha256']
    a, m = [(H / row[key]).read_text() for key in ('asm', 'ze_info')]
    assert int(re.search(r'grf_count:\s*(\d+)', m)[1]) == row['grf'] == 256
    assert int(re.search(r'slm_size:\s*(\d+)', m)[1]) == row['slm_bytes'] == 1024
    assert ('per_thread_memory_buffers' in m) == row['has_scratch_allocation']
    for op, count in row['op_counts'].items():
        assert len(re.findall(r'\b' + re.escape(op) + r'\s+\(', a)) == count
print(f'PASS: {samples} samples, {fingerprints} identical fingerprints, {pairs} pairs ({eligible} eligible), '
      f'{checks} recorded checks; GDN regression, patch hashes and native evidence verified')
