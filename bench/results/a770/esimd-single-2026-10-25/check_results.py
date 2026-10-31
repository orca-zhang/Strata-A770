"""Recompute single-layer ESIMD measurements and verify evidence without a GPU."""
from pathlib import Path
import hashlib
import json
import math
import re
import statistics

H = Path(__file__).resolve().parent
d = json.loads((H / 'results.json').read_text(encoding='utf-8'))
samples = fingerprints = pairs = eligible = checks = 0
drifts, oracles = [], {}


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
    pooled = 100 * (math.exp(statistics.mean(math.log1p(g / 100) for g in gains))) - 100 if all(g is not None for g in gains) else None
    return {'T': T, 'rows': rows, 'pooled_gain_pct': pooled,
        'passed': all(g is not None and g > 0 for g in gains) and pooled >= 1}


for name, cohort in d['cohorts'].items():
    assert cohort['complete'] and {e['mode'] for e in cohort['screen']} == {1, 2}
    for entry in cohort['screen'] + cohort['confirm']:
        mode = entry['mode']
        assert len(entry['runs']) == (1 if 'checks' in entry else 2)
        if 'checks' in entry:
            assert entry['checks'] == (86 if mode == 1 else 77)
            checks += entry['checks']
        for run in entry['runs']:
            assert len(run['samples']) == 450 and len(run['fingerprints']) == 50
            if mode not in oracles:
                oracles[mode] = run['fingerprints']
            assert run['fingerprints'] == oracles[mode], 'Full fingerprints differ'
            samples += len(run['samples'])
            fingerprints += len(run['fingerprints'])
            for T in (4, 6):
                for keep in range(1, T + 1):
                    assert len({r[3] for r in run['fingerprints'] if (int(r[0]), int(r[1])) == (T, keep)}) == 1
                    for arm in range(5):
                        batch = [s for s in run['samples'] if (s['T'], s['keep'], s['arm']) == (T, keep, arm)]
                        assert sorted(s['index'] for s in batch) == list(range(9))
                        assert all(math.isfinite(s['us']) and s['us'] > 0 for s in batch)
                        equal(statistics.median(s['us'] for s in batch), run['medians'][f'{T}/{keep}/{arm}'])
        for cap in entry['capacities']:
            equal(compare(entry['runs'], cap['T']), cap)
    selected = {e['mode'] for e in cohort['screen'] if all(c['passed'] for c in e['capacities'])}
    assert {e['mode'] for e in cohort['confirm']} == selected
    assert set(cohort['decision']) == {str(mode) for mode in selected}
    for entry in cohort['confirm']:
        decision = cohort['decision'][str(entry['mode'])]
        equal(decision['capacities'], entry['capacities'])
        assert decision['passed'] == all(c['passed'] for c in entry['capacities'])

assert samples == d['samples'] and fingerprints == d['fingerprint_sets']
assert pairs == d['pairs'] and eligible == d['eligible_pairs'] and checks == d['screen_checks']
equal(max(drifts), d['max_abs_baseline_drift_pct'])
assert len(d['correctness']) == sum(len(c['screen']) for c in d['cohorts'].values())
for row in d['correctness']:
    assert f'PASS checks={86 if row["mode"] == 1 else 77}' in row['stdout'] and not row['stderr']
    assert hashlib.sha256(row['stdout'].encode()).hexdigest() == row['stdout_sha256']
for row in d['artifacts']:
    assert hashlib.sha256((H / row['file']).read_bytes()).hexdigest() == row['sha256']
for p in d['production_checks']:
    assert p['passed']
    assert p['output']['commit-layers'].count('42 cases, 0 failures') == 4
    assert '156 cases, 0 failures' in p['output']['window'] and '44 cases, 0 total failures' in p['output']['window']
    assert 'gdn_parity OK' in p['output']['selftest']
if d['decision'] == 'adopted_single_cooperative32':
    final = d['cohorts']['final']
    assert set(final['decision']) == {'1', '2'} and all(v['passed'] for v in final['decision'].values())
    assert d['production_checks'][-1]['production_sha256'] == d['production_sha256']
for row in d.get('native', []):
    a, m = [(H / row[key]).read_text() for key in ('asm', 'ze_info')]
    assert int(re.search(r'grf_count:\s*(\d+)', m)[1]) == row['grf'] == 256
    assert int(re.search(r'slm_size:\s*(\d+)', m)[1]) == row['slm_bytes'] == 1024
    assert ('per_thread_memory_buffers' in m) == row['has_scratch_allocation']
for T, expected in d.get('batch_native_assembly_equal', {}).items():
    before, after = [(H / f'native/batch-{kind}-t{T}.asm').read_bytes() for kind in ('before', 'after')]
    assert (before == after) == expected
    if d['decision'] == 'adopted_single_cooperative32':
        assert expected
print(f'PASS: {samples} samples, {fingerprints} fingerprint sets, {pairs} pairs ({eligible} eligible), '
      f'{checks} recorded checks; regression and artifact hashes verified')
