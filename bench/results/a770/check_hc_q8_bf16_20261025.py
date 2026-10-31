"""CPU-only arithmetic and integrity check for the published HC Q8 prerequisite."""
import hashlib
import json
import math
from pathlib import Path
import statistics

HERE = Path(__file__).resolve().parent
STEM = 'hc-q8-bf16-2026-10-25'


def quality(rows):
    expected = {(c,p,a,t,f) for c in range(3) for p in range(2) for a in range(2)
                for t in (1,2,4,6,8) for f in ('lo','mixed')}
    assert len(rows) == 120
    assert {(r['case'],r['profile'],r['apply'],r['T'],r['field']) for r in rows} == expected
    assert all(math.isfinite(r['max_abs']) and r['max_abs'] >= 0 and
               math.isfinite(r['nrmse']) and r['nrmse'] >= 0 and
               math.isfinite(r['cosine']) for r in rows)
    return all(r['nrmse'] <= (0.1 if r['profile'] else 0.05) and
               r['cosine'] >= (0.98 if r['profile'] else 0.995) for r in rows)


def compare(runs):
    result = {}
    for i in (1,3):
        a,b,z = runs[i-1:i+2]
        result[b['label']] = {}
        for x,y,w in zip(a['rows'],b['rows'],z['rows']):
            assert x['T'] == y['T'] == w['T']
            baseline = math.sqrt(x['us']*w['us'])
            result[b['label']][str(x['T'])] = dict(baseline=baseline,candidate=y['us'],
                throughput_change_pct=100*(baseline/y['us']-1),
                baseline_drift_pct=100*(w['us']/x['us']-1),precision_passed=quality(b['errors']))
    return result


def check(d, verify_files=True):
    assert d['complete'] and not d['production_changed'] and not d['model_performance_measured']
    assert not d['model_quality_measured'] and not d['vram_saving_measured'] and not d['target_achieved']
    weights = d['weights']
    assert len(weights['rows']) == len({r['name'] for r in weights['rows']}) == 194
    assert sum(r['values']*2 for r in weights['rows']) == weights['bf16_bytes'] == 1271398400
    assert sum(r['q8_bytes'] for r in weights['rows']) == weights['q8_bytes'] == 675430400
    assert weights['potential_weight_bytes_saved'] == weights['bf16_bytes']-weights['q8_bytes']
    passes = []
    for r in weights['rows']:
        assert r['values'] > 0 and r['weight_energy'] > 0 and r['error_energy'] >= 0
        assert r['q8_bytes']*32 == r['values']*34
        assert r['relative_l2_error'] == (r['error_energy']/r['weight_energy'])**0.5
        assert math.isfinite(r['max_abs_error']) and 0 <= r['max_abs_error'] <= r['max_abs_weight']
        ok = r['relative_l2_error'] <= 0.01 and r['max_abs_error'] <= 0.01*r['max_abs_weight']
        assert r['passes_weight_screen'] == ok
        passes.append(ok)
        for key in ('source_sha256','q8_sha256','restored_sha256'):
            assert len(r[key]) == 64 and set(r[key]) <= set('0123456789abcdef')
    assert weights['passes_weight_screen'] == all(passes)
    assert [(c['name'],c['gain_floor_pct']) for c in d['cohorts']] == [('screen',3),('confirmation',1)]
    checks = batches = 0
    for cohort in d['cohorts']:
        assert [r['label'] for r in cohort['runs']] == ['A1','B1','A2','B2','A3']
        for run in cohort['runs']:
            compressed = run['label'].startswith('B')
            assert run['exit_code'] == 0 and run['summary'] == dict(checks=189 if compressed else 129,compressed=compressed)
            checks += run['summary']['checks']
            assert [r['T'] for r in run['rows']] == [1,4,6]
            assert [r['T'] for r in run['samples']] == [1,4,6]
            assert len(run['errors']) == (120 if compressed else 0)
            for r,s in zip(run['rows'],run['samples']):
                assert len(s['us']) == 11 and all(math.isfinite(v) and v > 0 for v in s['us'])
                assert math.isfinite(r['us']) and r['us'] > 0
                assert abs(r['us']-statistics.median(s['us'])) < 0.000000502
                batches += len(s['us'])
        assert cohort['comparison'] == compare(cohort['runs'])
        pairs = [p for group in cohort['comparison'].values() for p in group.values()]
        assert cohort['passes_gate'] == all(p['precision_passed'] and abs(p['baseline_drift_pct']) <= 3 and
            p['throughput_change_pct'] >= cohort['gain_floor_pct'] for p in pairs)
        assert cohort['passes_storage_prerequisite'] == all(p['precision_passed'] and abs(p['baseline_drift_pct']) <= 3 and
            p['throughput_change_pct'] >= -3 for p in pairs)
    assert d['checks'] == checks == 1530 and d['retained_batches'] == batches == 330
    assert d['passes_independent_1pct_confirmation'] == d['cohorts'][1]['passes_gate']
    if verify_files:
        sha = lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
        assert sha(Path(__file__)) == d['checker_sha256']
        assert sha(HERE/(STEM+'.md')) == d['report_sha256']
    print('PASS HC Q8 weight and activation errors, both frozen gates, all batches, gains/drift and report integrity')


if __name__ == '__main__':
    check(json.loads((HERE/(STEM+'.json')).read_text(encoding='utf-8')))
