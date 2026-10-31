"""Recompute the GU scheduling screen; --local also checks frozen raw evidence."""
import copy
import hashlib
import json
import math
from pathlib import Path
import statistics
import sys

H = Path(__file__).resolve().parent
R = H.parents[2]
RAW = R / 'build-sycl-a770-gu-schedule-20261028/screen-results.json'
OUT = H / 'gu-schedule-2026-10-28.json'
ORDER = [('A1', 0), ('C1', 256), ('B1', 128), ('C2', 256), ('A2', 0),
         ('C3', 256), ('B2', 128), ('C4', 256), ('A3', 0)]
LAYOUTS = ('mixed4', 'mixed8', 'vram')


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def comparisons(runs):
    result = {}
    for layout in LAYOUTS:
        arms = {row['label']: row for row in runs if row['layout'] == layout}
        pairs = [('A1', arm, 'A2', arm + '_vs_original') for arm in ('C1', 'B1', 'C2')]
        pairs += [('A2', arm, 'A3', arm + '_vs_original') for arm in ('C3', 'B2', 'C4')]
        pairs += [('C1', 'B1', 'C2', 'B1_vs_helper'), ('C3', 'B2', 'C4', 'B2_vs_helper')]
        result[layout] = {}
        for before, candidate, after, key in pairs:
            result[layout][key] = {}
            for t in range(1, 7):
                a, b, c = (arms[label]['rows'][t - 1]['us'] for label in (before, candidate, after))
                result[layout][key][str(t)] = dict(baseline=math.sqrt(a*c), candidate=b,
                    throughput_change_pct=100*(math.sqrt(a*c)/b-1), baseline_drift_pct=100*(c/a-1))
    return result


def eligible(comparison, arms):
    return [t for t in range(1, 7) if all(
        comparison[layout][arm][str(t)]['throughput_change_pct'] >= 1 and
        abs(comparison[layout][arm][str(t)]['baseline_drift_pct']) <= 3
        for layout in ('mixed4', 'mixed8') for arm in arms)]


def check(d):
    assert d['complete'] and not any(d[k] for k in ('production_changed', 'adopted', 'model_gain_confirmed', 'target_achieved'))
    assert d['scope'] == 'native-chain-operator-screen'
    assert (d['minimum_gain_pct'], d['maximum_baseline_drift_pct'], d['noise_below_pct']) == (1, 3, 0.3)
    assert [(c['label'], c['mode'], c['exit_code']) for c in d['checks']] == [
        (f'check-{fixture}-{mode}', mode, 0) for fixture in ('random', 'outlier') for mode in (0, 256, dict(ORDER)['B1'])]
    assert [(r['label'], r['mode'], r['layout']) for r in d['runs']] == [
        (label, mode, layout) for label, mode in ORDER for layout in LAYOUTS]
    for run in d['runs']:
        assert run['exit_code'] == 0 and len(run['rows']) == len(run['samples']) == 6
        for t, row, sample in zip(range(1, 7), run['rows'], run['samples']):
            assert row['T'] == sample['T'] == t and row['checks'] == 8*t and row['grid_groups'] == 0
            assert row['groups'] == (10 if t == 1 else 5*t) and row['entries'] == 10*t
            assert len(sample['us']) == 11 and all(math.isfinite(v) and v > 0 for v in sample['us'])
            assert math.isfinite(row['us']) and row['us'] > 0
            assert abs(row['us'] - statistics.median(sample['us'])) < 0.00000051
    assert d['comparison'] == comparisons(d['runs'])
    assert d['eligible_T'] == eligible(d['comparison'], ('B1_vs_original', 'B2_vs_original', 'B1_vs_helper', 'B2_vs_helper'))
    assert d['helper256_exploratory_T'] == eligible(d['comparison'], tuple(f'C{i}_vs_original' for i in range(1, 5)))


def build():
    raw = json.loads(RAW.read_text(encoding='utf8'))
    d = {k: raw[k] for k in ('complete', 'production_changed', 'adopted', 'rule', 'checks', 'runs', 'comparison', 'eligible_T')}
    d.update(scope='native-chain-operator-screen', model_gain_confirmed=False, target_achieved=False,
        hardware='A770 16GB / PCIe3.0 / Windows / driver32.0.101.9034 / DPC++2025.3.2',
        minimum_gain_pct=1, maximum_baseline_drift_pct=3, noise_below_pct=0.3,
        helper256_exploratory_T=eligible(d['comparison'], tuple(f'C{i}_vs_original' for i in range(1, 5))),
        raw_file=RAW.relative_to(R).as_posix(), raw_sha256=sha(RAW), files_sha256=raw['files_sha256'])
    check(d)
    return d


def main():
    if '--write' in sys.argv:
        assert not OUT.exists()
        OUT.write_text(json.dumps(build(), indent=2), encoding='utf8')
    d = json.loads(OUT.read_text(encoding='utf8')); check(d)
    if '--self-check' in sys.argv:
        for kind in ('missing', 'nan', 'mode', 'eligibility'):
            bad = copy.deepcopy(d)
            if kind == 'missing': bad['runs'].pop()
            if kind == 'nan': bad['runs'][0]['samples'][0]['us'][0] = float('nan')
            if kind == 'mode': bad['runs'][3]['mode'] = 128
            if kind == 'eligibility': bad['eligible_T'] = [7]
            try: check(bad)
            except AssertionError: pass
            else: raise AssertionError('accepted ' + kind)
    if '--local' in sys.argv:
        assert d == build()
        for path, digest in d['files_sha256'].items(): assert sha(R / path) == digest, path
    print('PASS 6 edge processes,27 timing processes,1782 batches,dual controls and1%/3% gates;operator scope only')


if __name__ == '__main__':
    main()
