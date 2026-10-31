"""Recompute the exact IQ4_XS stream-dot screen; --local also audits raw files."""
import copy
import json
import math
from pathlib import Path
import statistics
import sys

from check_iq4xs_register_lut_20261028 import comparisons, sha

H = Path(__file__).resolve().parent
R = H.parents[2]
RAW = R/'build-sycl-a770-iq4xs-stream-dot-v2-20261028/screen-results.json'
OUT = H/'iq4xs-stream-dot-2026-10-28.json'
SHAPES = {'head': (2560, 248320), 'qkv': (2560, 10240), 'shared': (2560, 640), 'ssm': (6144, 2560)}
ORDER = [('A1', 0), ('C1', 2), ('B1', 1), ('C2', 2), ('A2', 0),
         ('C3', 2), ('B2', 1), ('C4', 2), ('A3', 0)]


def eligible(comparison, arms):
    return {shape: [nc for nc in range(3, 7) if all(
        comparison[shape][arm][str(nc)]['throughput_change_pct'] >= 1 and
        abs(comparison[shape][arm][str(nc)]['baseline_drift_pct']) <= 3 for arm in arms)] for shape in SHAPES}


def check(d):
    assert d['complete'] and not any(d[k] for k in ('production_changed', 'adopted', 'model_gain_confirmed', 'target_achieved'))
    assert d['scope'] == 'iq4xs-stream-dot-device-operator-screen'
    assert (d['minimum_gain_pct'], d['maximum_baseline_drift_pct'], d['noise_below_pct']) == (1, 3, .3)
    assert d['runtime_flags']['STRATA_IQ4_XS_SG16'] == '2'
    assert [(r['ni'], r['no'], r['mode'], r['exit_code']) for r in d['checks']] == [
        (ni, 7, mode, 0) for ni in (512, 768, 2560) for mode in (0, 2, 1)]
    assert [(r['label'], r['mode'], r['shape']) for r in d['runs']] == [
        (label, mode, shape) for label, mode in ORDER for shape in SHAPES]
    for shape, (ni, no) in SHAPES.items():
        t = d['weights']['tensors'][shape]
        assert (t['ni'], t['no'], t['bytes']) == (ni, no, ni//256*no*136)
    for r in d['runs']:
        assert r['exit_code'] == 0 and (r['ni'], r['no']) == SHAPES[r['shape']]
        assert [v['nc'] for v in r['rows']] == [v['nc'] for v in r['samples']] == list(range(1, 7))
        for nc, row, sample in zip(range(1, 7), r['rows'], r['samples']):
            assert row['checks'] == 8*nc and len(sample['us']) == 11
            assert all(math.isfinite(v) and v > 0 for v in sample['us'])
            assert math.isfinite(row['us']) and abs(row['us'] - statistics.median(sample['us'])) < .00000051
    assert d['comparison'] == comparisons(d['runs'])
    assert d['eligible_stream'] == eligible(d['comparison'], ('B1_vs_original', 'B2_vs_original', 'B1_vs_scalar', 'B2_vs_scalar'))
    assert d['exploratory_control'] == eligible(d['comparison'], tuple(f'C{i}_vs_original' for i in range(1, 5)))


def build():
    d = json.loads(RAW.read_text(encoding='utf8'))
    d.update(scope='iq4xs-stream-dot-device-operator-screen', minimum_gain_pct=1, maximum_baseline_drift_pct=3,
        noise_below_pct=.3, model_gain_confirmed=False, target_achieved=False,
        hardware='A770 16GB / PCIe3.0 / Windows / DPC++2025.3.2',
        raw_file=RAW.relative_to(R).as_posix(), raw_sha256=sha(RAW))
    # ponytail: publish measurements and digests; the raw local archive retains the source path and timestamps.
    d['weights']['source'] = Path(d['weights']['source']).name
    d['weights'].pop('source_mtime_ns', None)
    check(d)
    return d


def main():
    if '--write' in sys.argv:
        assert not OUT.exists()
        OUT.write_text(json.dumps(build(), indent=2), encoding='utf8')
    d = json.loads(OUT.read_text(encoding='utf8')); check(d)
    if '--self-check' in sys.argv:
        for kind in ('missing', 'nan', 'mode', 'eligibility', 'inactive'):
            bad = copy.deepcopy(d)
            if kind == 'missing': bad['runs'].pop()
            if kind == 'nan': bad['runs'][0]['samples'][0]['us'][0] = float('nan')
            if kind == 'mode': bad['runs'][0]['mode'] = 1
            if kind == 'eligibility': bad['eligible_stream']['head'] = [3]
            if kind == 'inactive': bad['eligible_stream']['head'] = [1]
            try: check(bad)
            except AssertionError: pass
            else: raise AssertionError('accepted '+kind)
    if '--local' in sys.argv:
        assert d == build()
        for path, digest in d['files_sha256'].items(): assert sha(R/path) == digest, path
    print('PASS9 edge+36 timing processes/2376 batches;active NC3..6 dual controls;no model acceptance')


if __name__ == '__main__':
    main()
