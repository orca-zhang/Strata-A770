"""Recompute the current-EXE cache screen; --local also checks retained raw provenance."""
import copy
import hashlib
import json
import math
from pathlib import Path
import statistics
import sys

H = Path(__file__).resolve().parent; R = H.parents[2]
TASK = R/'build-sycl-a770-cache6250-current-20261028'
RAW = TASK/'performance-v2/results.json'
FAILED = R/'build-sycl-a770-cache6824-current-20261028/failure-audit.json'
OUT = H/'cache-current-2026-10-28.json'
CASES = ('Code2K', 'Prose2K')


def read(path): return json.loads(path.read_text(encoding='utf8'))
def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()


def summary(rows, case):
    values = [r['decode_ms'] for r in rows if r['case'] == case and not r['warmup']]
    assert len(values) == 3 and all(math.isfinite(v) and v > 0 for v in values)
    median = statistics.median(values)
    return dict(decode_ms=median,tps=128000/median,min_tps=128000/max(values),
        median_absolute_deviation_ms=statistics.median(abs(v-median) for v in values))


def comparison(runs):
    result = {}
    for case in CASES:
        a,b,c = (r['summary'][case]['decode_ms'] for r in runs)
        baseline = math.sqrt(a*c)
        result[case] = dict(baseline=baseline,candidate=b,throughput_change_pct=100*(baseline/b-1),
            baseline_drift_pct=100*(c/a-1),candidate_min_tps=runs[1]['summary'][case]['min_tps'])
    return result


def check(d):
    assert d['complete'] and not any(d[k] for k in ('production_changed','adopted','target_achieved',
        'passes_screen','confirmation_performed','passes_confirmation','sampled_memory_passed'))
    assert d['quality_history_pending'] and d['scope'] == 'current-exe-cache-capacity-model-screen'
    assert (d['minimum_gain_pct'],d['maximum_baseline_drift_pct'],d['noise_below_pct'],d['minimum_free_mib']) == (1,3,.3,1024)
    assert (d['spec_min_p'],d['production_spec_min_p']) == (.5,.8)
    assert [(r['label'],r['expert_slots']) for r in d['runs']] == [('A1',6297),('B1',6561),('A2',6297)]
    schedule = [(c,w) for w in (True,False,False,False) for c in CASES]
    for run in d['runs']:
        assert run['exit_code'] == 0 and run['ready_free_mib'] >= 1024
        assert [(r['case'],r['warmup']) for r in run['rows']] == schedule
        assert run['full_outputs_match'] and run['short_output_match'] and run['ordered_48_layer_scans'] == 8
        for row in run['rows']:
            assert row['generated'] == 128 and row['prompt_tokens'] == 2048
            assert row['output_matches'] and math.isfinite(row['decode_ms']) and row['decode_ms'] > 0
        assert run['summary'] == {case:summary(run['rows'],case) for case in CASES}
    assert d['screen'] == comparison(d['runs'])
    assert not all(v['throughput_change_pct'] >= 1 and abs(v['baseline_drift_pct']) <= 3 for v in d['screen'].values())
    expected = [(r['label'],c,w) for r in d['runs'] for c,w in schedule]
    for key in ('memory','telemetry'):
        assert [(r['label'],r['case'],r['warmup']) for r in d[key]] == expected
        assert all(r['samples'] > 0 for r in d[key])
    for row in d['memory']:
        assert math.isfinite(row['min_free_mib']) and 0 <= row['min_free_mib'] <= row['max_free_mib']
    assert not all(r['min_free_mib'] >= 1024 for r in d['memory'])
    for row in d['telemetry']:
        assert math.isfinite(row['min_mhz']) and 0 < row['min_mhz'] <= row['max_mhz']
    failure = d['larger_capacity_failure']
    assert failure == dict(actual_slots=6824,ready_free_mib=907,candidate_requests=0,
        candidate_speed_measured=False,audit_complete=True)
    assert d['script_failures_preserved'] == dict(import_model_requests=0,runner_model_requests=0)


def build():
    raw = read(RAW); failure = read(FAILED); calibrated = read(TASK/'calibration.json')
    assert raw['complete'] and failure['complete'] and calibrated['complete']
    assert not failure['candidate_speed_measured'] and calibrated['expert_slots'] == 6561
    d = {k:raw[k] for k in ('complete','production_changed','adopted','target_achieved','quality_history_pending',
        'spec_min_p','production_spec_min_p','screen','passes_screen','passes_confirmation','sampled_memory_passed','memory','telemetry')}
    d.update(scope='current-exe-cache-capacity-model-screen',confirmation_performed=bool(raw['confirmation']),
        hardware='A770 16GB / PCIe3.0 / Windows / driver32.0.101.9034 / DPC++2025.3.2',
        minimum_gain_pct=1,maximum_baseline_drift_pct=3,noise_below_pct=.3,minimum_free_mib=1024,runs=[],
        larger_capacity_failure=dict(actual_slots=failure['candidate_slots'],ready_free_mib=failure['candidate_ready_free_mib'],
            candidate_requests=0,candidate_speed_measured=False,audit_complete=failure['complete']),
        script_failures_preserved=dict(import_model_requests=read(TASK/'import-failure.json')['model_requests'],
            runner_model_requests=read(TASK/'run-failure.json')['model_requests']))
    for run in raw['runs']:
        # ponytail: publish timing/validation only; prompts, token IDs and local environment stay in raw evidence.
        arm = {k:run[k] for k in ('label','expert_slots','ready_free_mib','exit_code','summary')}
        arm.update(rows=[],full_outputs_match=True,short_output_match=run['initializer']['tokens'] == raw['reference']['Short'],
            ordered_48_layer_scans=8)
        for row in run['rows']:
            item = {k:row[k] for k in ('case','warmup','decode_ms','generated','prompt_tokens')}
            item['output_matches'] = row['tokens'] == raw['reference'][row['case']]
            arm['rows'].append(item)
        arm['full_outputs_match'] = all(r['output_matches'] for r in arm['rows']); d['runs'].append(arm)
    files = [RAW,FAILED,TASK/'calibration.json',TASK/'import-failure.json',TASK/'run-failure.json',Path(__file__)]
    d['files_sha256'] = {p.relative_to(R).as_posix():sha(p) for p in files}
    check(d); return d


def main():
    if '--write' in sys.argv:
        assert not OUT.exists(); OUT.write_text(json.dumps(build(),indent=2)+'\n',encoding='utf8')
    d = read(OUT); check(d)
    if '--self-check' in sys.argv:
        for kind in ('missing','nan','capacity','accept','memory'):
            bad = copy.deepcopy(d)
            if kind == 'missing': bad['runs'][1]['rows'].pop()
            if kind == 'nan': bad['runs'][1]['rows'][2]['decode_ms'] = float('nan')
            if kind == 'capacity': bad['runs'][1]['expert_slots'] = 6824
            if kind == 'accept': bad['passes_screen'] = True
            if kind == 'memory': bad['sampled_memory_passed'] = True
            try: check(bad)
            except AssertionError: pass
            else: raise AssertionError('accepted '+kind)
    if '--local' in sys.argv:
        assert d == build()
        for path,digest in d['files_sha256'].items(): assert sha(R/path) == digest,path
    print('PASS current6297/6561/6297 model screen,24 full outputs,paired1%/3% gates,memory rejection;no adoption')


if __name__ == '__main__':
    # ponytail: only small JSON/source files are read here; the full object audit holds its own GPU lease.
    main()
