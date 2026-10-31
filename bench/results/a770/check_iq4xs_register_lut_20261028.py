"""Recompute the IQ4_XS operator screen; --local verifies immutable raw evidence."""
import copy
import hashlib
import json
import math
from pathlib import Path
import re
import statistics
import sys

H = Path(__file__).resolve().parent
R = H.parents[2]
RAW = R/'build-sycl-a770-iq4xs-register-lut-20261028/screen-results.json'
OUT = H/'iq4xs-register-lut-2026-10-28.json'
CONFIRM_RAW = RAW.parent/'confirm-scalar/results.json'
CONFIRM = H/'iq4xs-sg16-confirm-2026-10-28.json'
MODEL_RAW = R/'build-sycl-a770-iq4xs-small-sg16-v2-20261028/performance/results.json'
MODEL_RECOVERY = MODEL_RAW.parent/'audit-recovery.json'
MODEL = H/'iq4xs-sg16-model-2026-10-28.json'
SHAPES = {'head': (2560, 248320), 'qkv': (2560, 10240), 'shared': (2560, 640), 'ssm': (6144, 2560)}
ORDER = [('A1', 0), ('C1', 16), ('B1', 1), ('C2', 16), ('A2', 0),
         ('C3', 16), ('B2', 1), ('C4', 16), ('A3', 0)]


def sha(path):
    with path.open('rb') as f: return hashlib.file_digest(f, 'sha256').hexdigest()


def comparisons(runs):
    result = {}
    for shape in SHAPES:
        arms = {r['label']: r for r in runs if r['shape'] == shape}
        pairs = [('A1', arm, 'A2', arm+'_vs_original') for arm in ('C1', 'B1', 'C2')]
        pairs += [('A2', arm, 'A3', arm+'_vs_original') for arm in ('C3', 'B2', 'C4')]
        pairs += [('C1', 'B1', 'C2', 'B1_vs_scalar'), ('C3', 'B2', 'C4', 'B2_vs_scalar')]
        result[shape] = {}
        for a, candidate, z, key in pairs:
            result[shape][key] = {}
            for nc in range(1, 7):
                before, value, after = (arms[label]['rows'][nc-1]['us'] for label in (a, candidate, z))
                base = math.sqrt(before*after)
                result[shape][key][str(nc)] = dict(baseline=base, candidate=value,
                    throughput_change_pct=100*(base/value-1), baseline_drift_pct=100*(after/before-1))
    return result


def eligible(comparison, arms):
    return {shape: [nc for nc in range(1, 7) if all(
        comparison[shape][arm][str(nc)]['throughput_change_pct'] >= 1 and
        abs(comparison[shape][arm][str(nc)]['baseline_drift_pct']) <= 3 for arm in arms)] for shape in SHAPES}


def check(d):
    assert d['complete'] and not any(d[k] for k in ('production_changed', 'adopted', 'model_gain_confirmed', 'target_achieved'))
    assert d['scope'] == 'native-iq4xs-device-operator-screen'
    assert (d['minimum_gain_pct'], d['maximum_baseline_drift_pct'], d['noise_below_pct']) == (1, 3, .3)
    assert d['runtime_flags']['STRATA_IQ4_XS_SG16'] == '2'
    assert [(r['ni'], r['no'], r['mode'], r['exit_code']) for r in d['checks']] == [
        (ni, 7, mode, 0) for ni in (512, 768, 2560) for mode in (0, 16, 1)]
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
    assert d['eligible_lut'] == eligible(d['comparison'], ('B1_vs_original', 'B2_vs_original', 'B1_vs_scalar', 'B2_vs_scalar'))
    assert d['exploratory_scalar'] == eligible(d['comparison'], tuple(f'C{i}_vs_original' for i in range(1, 5)))


def build():
    d = json.loads(RAW.read_text(encoding='utf8'))
    d.update(scope='native-iq4xs-device-operator-screen', minimum_gain_pct=1, maximum_baseline_drift_pct=3,
        noise_below_pct=.3, model_gain_confirmed=False, target_achieved=False,
        hardware='A770 16GB / PCIe3.0 / Windows / DPC++2025.3.2',
        raw_file=RAW.relative_to(R).as_posix(), raw_sha256=sha(RAW))
    check(d); return d


def check_confirmation(d):
    assert d['complete'] and not any(d[k] for k in ('production_changed', 'adopted', 'model_gain_confirmed', 'target_achieved'))
    assert d['scope'] == 'independent-iq4xs-scalar-sg16-confirmation'
    assert (d['minimum_gain_pct'], d['maximum_baseline_drift_pct'], d['noise_below_pct']) == (1, 3, .3)
    assert d['selected'] == {'shared': [1, 3, 4, 5, 6], 'ssm': [1]}
    order = [('A1', 0), ('B1', 16), ('A2', 0), ('B2', 16), ('A3', 0)]
    assert [(r['label'], r['mode'], r['shape']) for r in d['runs']] == [
        (label, mode, shape) for label, mode in order for shape in d['selected']]
    for r in d['runs']:
        assert r['exit_code'] == 0
        assert [v['nc'] for v in r['rows']] == [v['nc'] for v in r['samples']] == list(range(1, 7))
        for nc, row, sample in zip(range(1, 7), r['rows'], r['samples']):
            assert row['checks'] == 8*nc and len(sample['us']) == 11
            assert all(math.isfinite(v) and v > 0 for v in sample['us'])
            assert math.isfinite(row['us']) and abs(row['us'] - statistics.median(sample['us'])) < .00000051
    result = {}
    for shape in d['selected']:
        arms = {r['label']: r for r in d['runs'] if r['shape'] == shape}
        result[shape] = {}
        for a, candidate, z in (('A1', 'B1', 'A2'), ('A2', 'B2', 'A3')):
            result[shape][candidate] = {}
            for nc in range(1, 7):
                before, value, after = (arms[label]['rows'][nc-1]['us'] for label in (a, candidate, z))
                base = math.sqrt(before*after)
                result[shape][candidate][str(nc)] = dict(baseline=base, candidate=value,
                    throughput_change_pct=100*(base/value-1), baseline_drift_pct=100*(after/before-1))
    assert d['comparison'] == result
    assert d['eligible'] == {shape: [nc for nc in ncs if all(
        result[shape][arm][str(nc)]['throughput_change_pct'] >= 1 and
        abs(result[shape][arm][str(nc)]['baseline_drift_pct']) <= 3 for arm in ('B1', 'B2'))]
        for shape, ncs in d['selected'].items()}


def confirmation():
    d = json.loads(CONFIRM_RAW.read_text(encoding='utf8'))
    d.update(scope='independent-iq4xs-scalar-sg16-confirmation', adopted=False, model_gain_confirmed=False,
        target_achieved=False, minimum_gain_pct=1, maximum_baseline_drift_pct=3, noise_below_pct=.3,
        raw_file=CONFIRM_RAW.relative_to(R).as_posix(), raw_sha256=sha(CONFIRM_RAW))
    check_confirmation(d); return d


def token_sha(tokens):
    return hashlib.sha256(json.dumps(tokens, separators=(',', ':')).encode()).hexdigest()


def model_comparison(d, labels):
    arms = {r['label']: r for r in d['runs']}
    result = {}
    for case in ('Code2K', 'Prose2K'):
        a, value, z = (arms[label]['summary'][case]['decode_ms'] for label in labels)
        base = math.sqrt(a*z)
        result[case] = dict(baseline=base, candidate=value, throughput_change_pct=100*(base/value-1),
            baseline_drift_pct=100*(z/a-1), candidate_min_tps=arms[labels[1]]['summary'][case]['min_tps'])
    return result


def model_passes(comparison):
    return all(v['throughput_change_pct'] >= 1 and abs(v['baseline_drift_pct']) <= 3 for v in comparison.values())


def check_model(d):
    assert d['scope'] == 'independent-small-iq4xs-sg16-model-screen' and d['complete']
    assert not any(d[k] for k in ('production_changed', 'adopted', 'target_achieved')) and d['quality_history_pending']
    assert (d['minimum_gain_pct'], d['maximum_baseline_drift_pct'], d['noise_below_pct']) == (1, 3, .3)
    assert (d['spec_min_p'], d['production_spec_min_p']) == (.5, .8)
    assert d['audit_recovery']['complete'] and d['audit_recovery']['results_sha256'] == d['raw_sha256']
    order = [('A1', False), ('B1', True), ('A2', False), ('A3', False), ('B2', True), ('A4', False)]
    assert [(r['label'], r['candidate']) for r in d['runs']] == order[:6 if d['passes_screen'] else 3]
    for run in d['runs']:
        assert run['exit_code'] == 0 and run['expert_slots'] == 6297 and run['ready_free_mib'] >= 1024
        assert run['initializer_generated'] == 16 and run['initializer_sha256'] == d['reference_output_sha256']['Short']
        assert run['repair_scan_counts'] == [48]*len(run['rows'])
        assert d['audit_recovery']['repair_counts'][run['label']] == len(run['rows'])
        assert bool(run['dispatch_columns']) == run['candidate'] and set(run['dispatch_columns']) <= {3, 4, 6}
        assert [(r['case'], r['warmup']) for r in run['rows']] == [
            (case, warm) for warm in (True, False, False, False) for case in ('Code2K', 'Prose2K')]
        for row in run['rows']:
            assert row['generated'] == 128 and row['prompt_tokens'] == 2048 and row['reused'] == 0 and row['finish'] == 'length'
            assert row['output_sha256'] == d['reference_output_sha256'][row['case']]
            assert math.isfinite(row['decode_ms']) and row['decode_ms'] > 0
        for case in ('Code2K', 'Prose2K'):
            values = [v['decode_ms'] for v in run['rows'] if v['case'] == case and not v['warmup']]
            median = statistics.median(values)
            assert run['summary'][case] == dict(decode_ms=median, tps=128000/median, min_tps=128000/max(values),
                median_absolute_deviation_ms=statistics.median(abs(v-median) for v in values))
    assert d['screen'] == model_comparison(d, ('A1', 'B1', 'A2')) and d['passes_screen'] == model_passes(d['screen'])
    assert d['confirmation'] == (model_comparison(d, ('A3', 'B2', 'A4')) if d['passes_screen'] else {})
    assert d['passes_confirmation'] == (d['passes_screen'] and model_passes(d['confirmation']))
    assert d['speed_target35'] == (d['passes_confirmation'] and all(r['summary'][case]['min_tps'] >= 35
        for r in d['runs'] if r['candidate'] for case in ('Code2K', 'Prose2K')))


def model():
    raw = json.loads(MODEL_RAW.read_text(encoding='utf8'))
    d = {k: raw[k] for k in ('complete', 'production_changed', 'adopted', 'target_achieved', 'quality_history_pending',
        'spec_min_p', 'production_spec_min_p', 'screen', 'passes_screen', 'confirmation', 'passes_confirmation',
        'speed_target35', 'telemetry', 'files_sha256', 'runtime_flags', 'rule')}
    d.update(scope='independent-small-iq4xs-sg16-model-screen', minimum_gain_pct=1, maximum_baseline_drift_pct=3,
        noise_below_pct=.3, raw_file=MODEL_RAW.relative_to(R).as_posix(), raw_sha256=sha(MODEL_RAW), runs=[],
        reference_output_sha256={k: token_sha(v) for k, v in raw['reference'].items()})
    d['audit_recovery'] = json.loads(MODEL_RECOVERY.read_text(encoding='utf8'))
    d['files_sha256'].update(d['audit_recovery']['files_sha256'])
    d['files_sha256'][MODEL_RECOVERY.relative_to(R).as_posix()] = sha(MODEL_RECOVERY)
    for run in raw['runs']:
        r = {k: run[k] for k in ('label', 'candidate', 'expert_slots', 'ready_free_mib', 'exit_code', 'summary', 'dispatch_columns')}
        r.update(initializer_generated=run['initializer']['generated'], initializer_sha256=token_sha(run['initializer']['tokens']),
            repair_scan_counts=[int(v) for v in re.findall(r'^DOWN_RETRY_SUMMARY checks=(\d+) experts=\d+ trigger_tiles=\d+$',
                (MODEL_RAW.parent/(run['label']+'.stderr')).read_text(encoding='utf8'), re.M)], rows=[])
        for row in run['rows']:
            r['rows'].append({k: row[k] for k in ('case', 'warmup', 'generated', 'prompt_tokens', 'prompt_ms',
                'decode_ms', 'ttft_ms', 'finish', 'reused', 'accepted', 'offered')} | {'output_sha256': token_sha(row['tokens'])})
        d['runs'].append(r)
    check_model(d); return d


def main():
    if '--write' in sys.argv:
        assert not OUT.exists(); OUT.write_text(json.dumps(build(), indent=2), encoding='utf8')
    if '--write-confirm' in sys.argv:
        assert not CONFIRM.exists(); CONFIRM.write_text(json.dumps(confirmation(), indent=2), encoding='utf8')
    if '--write-model' in sys.argv:
        assert not MODEL.exists(); MODEL.write_text(json.dumps(model(), indent=2), encoding='utf8')
    d = json.loads(OUT.read_text(encoding='utf8')); check(d)
    c = json.loads(CONFIRM.read_text(encoding='utf8')) if CONFIRM.exists() else None
    if c is not None: check_confirmation(c)
    md = json.loads(MODEL.read_text(encoding='utf8')) if MODEL.exists() else None
    if md is not None: check_model(md)
    if '--self-check' in sys.argv:
        for kind in ('missing', 'nan', 'mode', 'eligibility'):
            bad = copy.deepcopy(d)
            if kind == 'missing': bad['runs'].pop()
            if kind == 'nan': bad['runs'][0]['samples'][0]['us'][0] = float('nan')
            if kind == 'mode': bad['runs'][0]['mode'] = 1
            if kind == 'eligibility': bad['eligible_lut']['head'] = [1]
            try: check(bad)
            except AssertionError: pass
            else: raise AssertionError('accepted '+kind)
        if c is not None:
            bad = copy.deepcopy(c); bad['eligible']['ssm'] = [1]
            try: check_confirmation(bad)
            except AssertionError: pass
            else: raise AssertionError('accepted unconfirmed shape')
        if md is not None:
            for kind in ('output', 'joint_gain', 'short_output'):
                bad = copy.deepcopy(md)
                if kind == 'output': bad['runs'][1]['rows'][0]['output_sha256'] = '0'*64
                if kind == 'joint_gain': bad['passes_screen'] = not bad['passes_screen']
                if kind == 'short_output': bad['runs'][1]['rows'][0]['generated'] = 127
                try: check_model(bad)
                except AssertionError: pass
                else: raise AssertionError('accepted model '+kind)
    if '--local' in sys.argv:
        assert d == build()
        for path, digest in d['files_sha256'].items(): assert sha(R/path) == digest, path
        if c is not None:
            assert c == confirmation()
            for path, digest in c['files_sha256'].items(): assert sha(R/path) == digest, path
        if md is not None:
            assert md == model()
            for path, digest in md['files_sha256'].items(): assert sha(R/path) == digest, path
    print('PASS9 edge+36 timing processes,2376 retained batches,full shape/dual-control gates;no model acceptance')
    if c is not None: print('PASS independent10 processes/660 batches,prespecified scalarSG16 eligibility;no model acceptance')
    if md is not None: print('PASS original2K/full128 model samples,complete output digests and joint1%/3% gates;no adoption')


if __name__ == '__main__':
    main()
