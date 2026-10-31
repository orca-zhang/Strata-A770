"""Offline accounting for the A770 existing-API group-row operator screen."""
import copy
import hashlib
import json
import math
from pathlib import Path
import statistics
import sys

H = Path(__file__).resolve().parent
STEM = 'group-concurrency-2026-10-26'
ORDER = [('A1', 0), ('G4-1', 4), ('A2', 0), ('G8-1', 8), ('A3', 0),
         ('G4-2', 4), ('A4', 0), ('G8-2', 8), ('A5', 0)]
MEMORY = ('mapped', 'vram')


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def check(d):
    assert d['complete'] and not d['adopted'] and not d['target_achieved']
    assert d['minimum_gain_pct'] == 1 and d['maximum_baseline_drift_pct'] == 3
    runs = d['runs']
    assert [(r['label'], r['grid_groups'], r['memory']) for r in runs] == [
        (label, gy, mem) for label, gy in ORDER for mem in MEMORY]
    for run in runs:
        assert run['exit_code'] == 0 and len(run['rows']) == len(run['samples']) == 6
        assert run['extra_checks'] == {'empty': 1, 'graph': 6}
        for t, row, sample in zip(range(1, 7), run['rows'], run['samples']):
            assert row['T'] == sample['T'] == t and row['checks'] == t*8
            assert row['groups'] == (10 if t == 1 else t*5) and row['entries'] == t*10
            assert row['grid_groups'] == run['grid_groups']
            assert math.isfinite(row['us']) and row['us'] > 0
            assert len(sample['us']) == 11 and all(math.isfinite(v) and v > 0 for v in sample['us'])
            assert abs(row['us'] - statistics.median(sample['us'])) <= 0.000000501
    comparison = {str(gy): {mem: {str(t): [] for t in range(1, 7)} for mem in MEMORY} for gy in (4, 8)}
    for i in (1, 3, 5, 7):
        for mem in MEMORY:
            arms = [next(r for r in runs if r['label'] == ORDER[j][0] and r['memory'] == mem)
                    for j in (i-1, i, i+1)]
            for rows in zip(*(arm['rows'] for arm in arms)):
                assert len({row['T'] for row in rows}) == 1
                a, c, z = (row['us'] for row in rows)
                base = math.sqrt(a*z)
                pair = dict(baseline=base, candidate=c, throughput_change_pct=100*(base/c-1),
                            baseline_drift_pct=100*(z/a-1))
                pair['passes'] = pair['throughput_change_pct'] >= 1 and abs(pair['baseline_drift_pct']) <= 3
                comparison[str(ORDER[i][1])][mem][str(rows[0]['T'])].append(pair)
    selected = {mem: {str(t): [int(gy) for gy, memories in comparison.items()
                              if len(memories[mem][str(t)]) == 2 and all(p['passes'] for p in memories[mem][str(t)])]
                     for t in range(1, 7)} for mem in MEMORY}
    assert d['comparison'] == comparison and d['eligible_shapes'] == selected


def main():
    d = json.loads((H / (STEM + '.json')).read_text(encoding='utf-8'))
    assert sha(Path(__file__)) == d['checker_sha256']
    assert sha(H / (STEM + '.md')) == d['report_sha256']
    check(d)
    if '--self-check' in sys.argv:
        bad = copy.deepcopy(d)
        bad['runs'][0]['samples'][0]['us'][0] = -1
        try:
            check(bad)
        except AssertionError:
            pass
        else:
            raise AssertionError('negative timing accepted')
    print('PASS group-row complete-chain accounting:882 eager/empty,108 graph checks,1188 batches; both1% rounds; no model speed claim')
    print(json.dumps(d['eligible_shapes']))


if __name__ == '__main__':
    main()
