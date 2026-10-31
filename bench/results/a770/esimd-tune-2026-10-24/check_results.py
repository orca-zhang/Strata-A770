"""Recompute the frozen ESIMD screen; no GPU, model or dependencies needed."""
import json
import math
from pathlib import Path
import statistics

d = json.loads(Path(__file__).with_name('results.json').read_text(encoding='utf-8'))
assert d['decision'] == 'not_adopted' and not d['confirm'] and not d['selected_capacities']
assert len(d['variants']) == 4
samples = checks = fingerprints = eligible = 0
drifts = []
oracle = None
for variant in d['variants']:
    assert variant['checks'] == 71 and len(variant['runs']) == 1
    checks += variant['checks']
    run = variant['runs'][0]
    assert len(run['samples']) == 450 and len(run['fingerprints']) == 50
    if oracle is None:
        oracle = run['fingerprints']
    assert run['fingerprints'] == oracle
    samples += len(run['samples'])
    fingerprints += len(run['fingerprints'])
    for capacity in variant['capacities']:
        T = capacity['T']
        shape_gains = []
        assert len(capacity['rows']) == T
        for row in capacity['rows']:
            keep = row['keep']
            assert len({v[3] for v in run['fingerprints'] if (int(v[0]), int(v[1])) == (T, keep)}) == 1
            medians = []
            for arm in range(5):
                batch = [v for v in run['samples'] if (v['T'], v['keep'], v['arm']) == (T, keep, arm)]
                assert sorted(v['index'] for v in batch) == list(range(9))
                assert all(math.isfinite(v['us']) and v['us'] > 0 for v in batch)
                medians.append(statistics.median(v['us'] for v in batch))
                assert math.isclose(medians[-1], run['medians'][f'{T}/{keep}/{arm}'])
            ratios = []
            for arm, pair in zip((1, 3), row['pairs'], strict=True):
                base = math.sqrt(medians[arm-1] * medians[arm+1])
                drift = 100 * (medians[arm+1] / medians[arm-1] - 1)
                assert math.isclose(pair['baseline'], base)
                assert math.isclose(pair['candidate'], medians[arm])
                assert math.isclose(pair['baseline_drift_pct'], drift)
                assert math.isclose(pair['throughput_change_pct'], 100 * (base / medians[arm] - 1))
                drifts.append(abs(drift))
                if abs(drift) <= 3:
                    ratios.append(base / medians[arm])
            assert len(ratios) == row['eligible']
            eligible += len(ratios)
            gain = 100 * (math.exp(statistics.mean(map(math.log, ratios))) - 1) if ratios else None
            assert gain is None and row['pooled_gain_pct'] is None or gain is not None and math.isclose(gain, row['pooled_gain_pct'])
            shape_gains.append(gain)
        pooled = 100 * (math.exp(statistics.mean(math.log1p(g/100) for g in shape_gains)) - 1)
        assert math.isclose(pooled, capacity['pooled_gain_pct'])
        passed = all(g > 0 for g in shape_gains) and pooled >= 1
        assert capacity['passed'] == passed and not passed
assert samples == d['samples'] == 1800
assert checks == d['correctness_checks'] == 284
assert fingerprints == d['fingerprint_sets'] == 200
assert eligible == d['eligible_pairs'] == 80
assert math.isclose(max(drifts), d['max_abs_baseline_drift_pct'])
assert not d['invalid_graph_attempt']['usable_for_performance']
assert len(d['invalid_graph_attempt']['samples']) == 351
print('PASS: 1800 samples,200 fingerprints,80 pairs,284 recorded checks;all four candidates rejected;graph attempt excluded')
