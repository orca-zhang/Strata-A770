"""Recompute all ESIMD screens and confirmations, using only the Python standard library."""
from pathlib import Path
import hashlib
import json
import math
import re
import statistics

H = Path(__file__).resolve().parent
d = json.loads((H / 'results.json').read_text(encoding='utf-8'))
assert d['decision'] == 'adopted_coop32'
samples = fingerprints = checks = eligible = pairs = 0
oracle = None
drifts = []


def compare(runs, T):
    global pairs, eligible
    gains = []
    rows = []
    for keep in range(1, T + 1):
        ratios = []
        row_pairs = []
        for run in runs:
            medians = [run['medians'][f'{T}/{keep}/{arm}'] for arm in range(5)]
            for arm in (1, 3):
                a, b, c = medians[arm-1:arm+2]
                base = math.sqrt(a*c)
                drift = 100*(c/a-1)
                row_pairs.append({'baseline': base, 'candidate': b,
                    'throughput_change_pct': 100*(base/b-1), 'baseline_drift_pct': drift})
                pairs += 1
                drifts.append(abs(drift))
                if abs(drift) <= 3:
                    eligible += 1
                    ratios.append(base/b)
        gain = 100*(math.exp(statistics.mean(map(math.log, ratios)))-1) if ratios else None
        gains.append(gain)
        rows.append({'keep': keep, 'pairs': row_pairs, 'eligible': len(ratios), 'pooled_gain_pct': gain})
    pooled = 100*(math.exp(statistics.mean(math.log1p(g/100) for g in gains))-1) if all(g is not None for g in gains) else None
    return {'T': T, 'rows': rows, 'pooled_gain_pct': pooled,
            'passed': all(g is not None and g > 0 for g in gains) and pooled >= 1}


def equal(a, b):
    if isinstance(a, float):
        assert math.isclose(a, b, rel_tol=1e-9, abs_tol=1e-9), (a, b)
    elif isinstance(a, dict):
        assert a.keys() == b.keys()
        for key in a:
            equal(a[key], b[key])
    elif isinstance(a, list):
        assert len(a) == len(b)
        for v, w in zip(a, b, strict=True):
            equal(v, w)
    else:
        assert a == b


for name, cohort in d['cohorts'].items():
    assert cohort['complete']
    for entry in cohort['screen'] + cohort['confirm']:
        if 'checks' in entry:
            assert entry['checks'] == 71
            checks += entry['checks']
        for run in entry['runs']:
            assert len(run['samples']) == 450 and len(run['fingerprints']) == 50
            if oracle is None:
                oracle = run['fingerprints']
            assert run['fingerprints'] == oracle, 'Bit fingerprints differ across candidates/processes'
            samples += len(run['samples'])
            fingerprints += len(run['fingerprints'])
            for T in (4, 6):
                for keep in range(1, T+1):
                    for arm in range(5):
                        batch = [v for v in run['samples'] if (v['T'], v['keep'], v['arm']) == (T, keep, arm)]
                        assert sorted(v['index'] for v in batch) == list(range(9))
                        assert all(math.isfinite(v['us']) and v['us'] > 0 for v in batch)
                        equal(statistics.median(v['us'] for v in batch), run['medians'][f'{T}/{keep}/{arm}'])
        for cap in entry['capacities']:
            equal(compare(entry['runs'], cap['T']), cap)
    if name == 'packed':
        assert len(cohort['screen']) == 4 and not cohort['confirm'] and not cohort['decision']
        assert all(not c['passed'] for e in cohort['screen'] for c in e['capacities'])
    else:
        assert len(cohort['confirm']) == 1 and len(cohort['confirm'][0]['runs']) == 2
        assert set(cohort['decision']) == {'4', '6'}
        for T in (4, 6):
            chosen = cohort['decision'][str(T)]
            expected = next(c for c in cohort['confirm'][0]['capacities'] if c['T'] == T)
            equal({k: v for k, v in chosen.items() if k not in ('mode', 'name')}, expected)
            assert expected['passed'] and chosen['name'] == ('coop32' if name == 'cooperative' else 'production_coop32')
assert samples == d['samples'] == 4950
assert fingerprints == d['fingerprint_sets'] == 550
assert checks == d['screen_checks'] == 497
assert pairs == 220
equal(max(drifts), d['max_abs_baseline_drift_pct'])
assert d['production_checks']['passed']
assert d['production_checks']['output']['commit-layers'].count('42 cases, 0 failures') == 4
assert '156 cases, 0 failures' in d['production_checks']['output']['window']
assert '44 cases, 0 total failures' in d['production_checks']['output']['window']
assert 'gdn_parity OK' in d['production_checks']['output']['selftest']
for row in d['prototype_native']:
    for key in ('asm', 'ze_info'):
        assert hashlib.sha256((H / row[key]).read_bytes()).hexdigest() == row[key + '_sha256']
    assembly = (H / row['asm']).read_text()
    metadata = (H / row['ze_info']).read_text()
    assert int(re.search(r'grf_count:\s*(\d+)', metadata)[1]) == row['grf']
    slm = re.search(r'slm_size:\s*(\d+)', metadata)
    assert (int(slm[1]) if slm else 0) == row['slm_bytes']
    assert ('per_thread_memory_buffers' in metadata) == row['has_scratch_allocation']
    assert len(re.findall(r'\bsend\.ugm\s+\(', assembly)) == row['static_ugm_send_instructions']
    assert row['grf'] == 256 and not row['has_scratch_allocation']
    assert row['static_ugm_send_instructions'] == (266 if row['variant'] == 'esimd' else 73)
    assert row['slm_bytes'] == (0 if row['variant'] == 'esimd' else 1024)
print(f'PASS: {samples} samples, {fingerprints} identical fingerprints, {pairs} pairs ({eligible} eligible), '
      f'{checks} recorded screen checks; final coop32 adopted; native evidence verified')
