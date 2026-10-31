"""Recompute the IQ4_XS GRF screen, independent confirmation and native-resource gates."""
import copy
import json
import math
from pathlib import Path
import statistics
import sys

from check_iq4xs_register_lut_20261028 import comparisons, sha

H = Path(__file__).resolve().parent
R = H.parents[2]
P = R/'build-sycl-a770-iq4xs-large-grf-20261028'
OUT = H/'iq4xs-grf-2026-10-28.json'
SHAPES = {'head': (2560, 248320), 'qkv': (2560, 10240), 'shared': (2560, 640), 'ssm': (6144, 2560)}
ORDER = [('A1', 0), ('C1', 2), ('B1', 1), ('C2', 2), ('A2', 0),
         ('C3', 2), ('B2', 1), ('C4', 2), ('A3', 0)]
SELECTED = {'qkv': [6], 'shared': [3, 5, 6]}


def eligible(comparison, selected, arms):
    return {shape: [nc for nc in ncs if all(comparison[shape][arm][str(nc)]['throughput_change_pct'] >= 1 and
        abs(comparison[shape][arm][str(nc)]['baseline_drift_pct']) <= 3 for arm in arms)] for shape, ncs in selected.items()}


def check_rows(runs, shapes):
    assert [(r['label'], r['mode'], r['shape']) for r in runs] == [
        (label, mode, shape) for label, mode in ORDER for shape in shapes]
    for r in runs:
        assert r['exit_code'] == 0
        assert [v['nc'] for v in r['rows']] == [v['nc'] for v in r['samples']] == list(range(1, 7))
        for nc, row, sample in zip(range(1, 7), r['rows'], r['samples']):
            assert row['checks'] == 8*nc and len(sample['us']) == 11
            assert all(math.isfinite(v) and v > 0 for v in sample['us'])
            assert math.isfinite(row['us']) and abs(row['us'] - statistics.median(sample['us'])) < .00000051


def confirmation_comparison(runs):
    result = {}
    for shape in SELECTED:
        arms = {r['label']: r for r in runs if r['shape'] == shape}
        result[shape] = {}
        for a, candidate, z, key in [('A1', 'B1', 'A2', 'B1_vs_original'), ('A2', 'B2', 'A3', 'B2_vs_original'),
                                    ('C1', 'B1', 'C2', 'B1_vs_128'), ('C3', 'B2', 'C4', 'B2_vs_128')]:
            result[shape][key] = {}
            for nc in range(1, 7):
                before, value, after = (arms[label]['rows'][nc-1]['us'] for label in (a, candidate, z))
                base = math.sqrt(before*after)
                result[shape][key][str(nc)] = dict(baseline=base, candidate=value,
                    throughput_change_pct=100*(base/value-1), baseline_drift_pct=100*(after/before-1))
    return result


def check(d):
    assert d['scope'] == 'iq4xs-grf-device-operator-confirmation' and d['complete']
    assert not any(d[k] for k in ('production_changed', 'adopted', 'model_gain_confirmed', 'target_achieved'))
    assert (d['minimum_gain_pct'], d['maximum_baseline_drift_pct'], d['noise_below_pct']) == (1, 3, .3)
    screen, confirm, native = (d[k] for k in ('screen', 'confirmation', 'native'))
    check_rows(screen['runs'], SHAPES); check_rows(confirm['runs'], SELECTED)
    assert [(r['ni'], r['no'], r['mode'], r['exit_code']) for r in screen['checks']] == [
        (ni, 7, mode, 0) for ni in (512, 768, 2560) for mode in (0, 2, 1)]
    for shape, (ni, no) in SHAPES.items():
        t = screen['weights']['tensors'][shape]
        assert (t['ni'], t['no'], t['bytes']) == (ni, no, ni//256*no*136)
    assert screen['comparison'] == comparisons(screen['runs'])
    assert screen['eligible_grf'] == eligible(screen['comparison'], {k:range(3, 7) for k in SHAPES},
        ('B1_vs_original', 'B2_vs_original', 'B1_vs_scalar', 'B2_vs_scalar'))
    assert screen['exploratory_128'] == eligible(screen['comparison'], {k:range(3, 7) for k in SHAPES},
        tuple(f'C{i}_vs_original' for i in range(1, 5)))
    assert {k:v for k,v in screen['eligible_grf'].items() if v} == confirm['selected'] == SELECTED
    assert confirm['comparison'] == confirmation_comparison(confirm['runs'])
    assert confirm['eligible'] == eligible(confirm['comparison'], SELECTED,
        ('B1_vs_original', 'B2_vs_original', 'B1_vs_128', 'B2_vs_128'))
    assert native['property_applied'] and native['complete']
    assert sorted((v['nc'], v['requested_grf']) for v in native['modules']) == [(nc, g) for nc in range(3, 9) for g in (128, 256)]
    for v in native['modules']:
        assert v['resources']['grf_count'] == v['requested_grf'] and v['resources']['simd_size'] == 32
    assert d['runtime_flags']['STRATA_IQ4_XS_SG16'] == '2'


def build():
    paths = [P/'screen-results.json', P/'confirmation/results.json', P/'native-recovery.json']
    screen, confirm, native = [json.loads(p.read_text(encoding='utf8')) for p in paths]
    assert all(v['complete'] for v in (screen, confirm, native))
    d = dict(complete=True, scope='iq4xs-grf-device-operator-confirmation', production_changed=False, adopted=False,
        model_gain_confirmed=False, target_achieved=False, minimum_gain_pct=1, maximum_baseline_drift_pct=3, noise_below_pct=.3,
        hardware='A770 16GB / PCIe3.0 / Windows / DPC++2025.3.2', runtime_flags=screen['runtime_flags'],
        screen={k:screen[k] for k in ('checks', 'runs', 'weights', 'comparison', 'eligible_grf', 'exploratory_128', 'rule')},
        confirmation={k:confirm[k] for k in ('selected', 'runs', 'comparison', 'eligible', 'rule')},
        native={k:native[k] for k in ('complete', 'property_applied', 'modules', 'note', 'rule')},
        external_headers_sha256={Path(p).name:digest for p,digest in screen['external_headers_sha256'].items()},
        files_sha256=screen['files_sha256'] | confirm['files_sha256'] | native['files_sha256'] |
            {p.relative_to(R).as_posix():sha(p) for p in paths})
    d['screen']['weights']['source'] = Path(d['screen']['weights']['source']).name
    d['screen']['weights'].pop('source_mtime_ns', None)
    check(d); return d


def main():
    if '--write' in sys.argv:
        assert not OUT.exists(); OUT.write_text(json.dumps(build(), indent=2), encoding='utf8')
    d = json.loads(OUT.read_text(encoding='utf8')); check(d)
    if '--self-check' in sys.argv:
        for kind in ('missing', 'nan', 'eligibility', 'native'):
            bad = copy.deepcopy(d)
            if kind == 'missing': bad['confirmation']['runs'].pop()
            if kind == 'nan': bad['screen']['runs'][0]['samples'][0]['us'][0] = float('nan')
            if kind == 'eligibility': bad['confirmation']['eligible']['qkv'] = [6]
            if kind == 'native': bad['native']['modules'][0]['resources']['grf_count'] = 64
            try: check(bad)
            except AssertionError: pass
            else: raise AssertionError('accepted '+kind)
    if '--local' in sys.argv:
        assert d == build()
        for path, digest in d['files_sha256'].items(): assert sha(R/path) == digest, path
        original = json.loads((P/'screen-results.json').read_text(encoding='utf8'))
        for path, digest in original['external_headers_sha256'].items(): assert sha(Path(path)) == digest, path
    print('PASS45 screen+18 independent processes,2376+1188 batches,12 actual native kernels;operator scope only')


if __name__ == '__main__': main()
