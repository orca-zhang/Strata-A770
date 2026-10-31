"""Recompute the fixed-five-step Down screen; --local also checks retained native/raw evidence."""
import copy
import hashlib
import json
import math
from pathlib import Path
import statistics
import sys

from check_gu_schedule_20261028 import comparisons, eligible

H = Path(__file__).resolve().parent; R = H.parents[2]
RAW = R/'build-sycl-a770-down-unroll-20261028/screen-results.json'
NATIVE = RAW.with_name('native-recovery.json')
OUT = H/'down-unroll-2026-10-28.json'
ORDER = [('A1',0),('C1',1),('B1',2),('C2',1),('A2',0),('C3',1),('B2',2),('C4',1),('A3',0)]


def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()


def check(d):
    assert d['complete'] and not any(d[k] for k in ('production_changed','adopted','model_gain_confirmed','target_achieved'))
    assert d['scope'] == 'native-chain-operator-screen'
    assert (d['minimum_gain_pct'],d['maximum_baseline_drift_pct'],d['noise_below_pct']) == (1,3,.3)
    assert [(c['label'],c['mode'],c['exit_code']) for c in d['checks']] == [
        (f'check-{fixture}-{mode}',mode,0) for fixture in ('random','outlier') for mode in (0,1,2)]
    assert [(r['label'],r['mode'],r['layout']) for r in d['runs']] == [
        (label,mode,layout) for label,mode in ORDER for layout in ('mixed4','mixed8','vram')]
    for run in d['runs']:
        assert run['exit_code'] == 0 and len(run['rows']) == len(run['samples']) == 6
        for t,row,sample in zip(range(1,7),run['rows'],run['samples']):
            assert row['T'] == sample['T'] == t and row['checks'] == 8*t and row['grid_groups'] == 0
            assert row['groups'] == (10 if t == 1 else 5*t) and row['entries'] == 10*t
            assert len(sample['us']) == 11 and all(math.isfinite(v) and v > 0 for v in sample['us'])
            assert math.isfinite(row['us']) and row['us'] > 0
            assert abs(row['us']-statistics.median(sample['us'])) < .00000051
    assert d['comparison'] == comparisons(d['runs'])
    assert d['eligible_T'] == eligible(d['comparison'],('B1_vs_original','B2_vs_original','B1_vs_helper','B2_vs_helper'))
    assert d['native_audit_recovered'] and d['instruction_bytes_unchanged'] and d['original_native_failure_preserved']
    assert sorted(v['candidate'] for v in d['native_modules']) == [False,True]
    for v in d['native_modules']:
        assert v['resources'] == dict(grf_count=128,simd_size=16,slm_size=None,spill_size=None)
        assert v['assembly_bytes'] > 0 and v['branch_lines'] >= 0


def build():
    raw = json.loads(RAW.read_text(encoding='utf8')); native = json.loads(NATIVE.read_text(encoding='utf8'))
    assert native['complete'] and native['instruction_bytes_unchanged'] and native['original_failed']
    failed = json.loads(RAW.with_name('native-results.json').read_text(encoding='utf8')); assert not failed['complete']
    d = {k:raw[k] for k in ('complete','production_changed','adopted','checks','runs','comparison','eligible_T','rule')}
    files = [RAW,NATIVE,H/'check_gu_schedule_20261028.py',Path(__file__)]
    d.update(scope='native-chain-operator-screen',model_gain_confirmed=False,target_achieved=False,
        hardware='A770 16GB / PCIe3.0 / Windows / driver32.0.101.9034 / DPC++2025.3.2',
        minimum_gain_pct=1,maximum_baseline_drift_pct=3,noise_below_pct=.3,
        native_audit_recovered=True,instruction_bytes_unchanged=True,original_native_failure_preserved=True,
        native_modules=native['modules'],raw_file=RAW.relative_to(R).as_posix(),raw_sha256=sha(RAW),
        native_file=NATIVE.relative_to(R).as_posix(),native_sha256=sha(NATIVE),
        files_sha256=raw['files_sha256']|native['files_sha256']|{p.relative_to(R).as_posix():sha(p) for p in files})
    check(d); return d


def main():
    if '--write' in sys.argv:
        assert not OUT.exists(); OUT.write_text(json.dumps(build(),indent=2),encoding='utf8')
    d = json.loads(OUT.read_text(encoding='utf8')); check(d)
    if '--self-check' in sys.argv:
        for kind in ('missing','nan','mode','eligible','native'):
            bad = copy.deepcopy(d)
            if kind == 'missing': bad['runs'].pop()
            if kind == 'nan': bad['runs'][0]['samples'][0]['us'][0] = float('nan')
            if kind == 'mode': bad['runs'][3]['mode'] = 128
            if kind == 'eligible': bad['eligible_T'] = [1]
            if kind == 'native': bad['instruction_bytes_unchanged'] = False
            try: check(bad)
            except AssertionError: pass
            else: raise AssertionError('accepted '+kind)
    if '--local' in sys.argv:
        assert d == build()
        for path,digest in d['files_sha256'].items(): assert sha(R/path) == digest,path
    print('PASS 6 edge/27 timing processes,1782 batches,dual controls,1%/3% gates and recovered native audit;operator only')


if __name__ == '__main__':
    if '--write' in sys.argv or '--local' in sys.argv:
        sys.path.insert(0,str(R/'build-sycl-a770-u4s4-grouped-20261027'))
        from lease import gpu_lock
        with gpu_lock(): main()
    else: main()
