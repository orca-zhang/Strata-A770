"""Publish/recompute the isolated mapped-first model screen without GPU or binary reads."""
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
RAW = R / 'build-sycl-a770-mapped-first-20261026'
STEM = 'mapped-first-2026-10-26'
CASES = ('Code2K', 'Prose2K')


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def fingerprint(tokens):
    return hashlib.sha256(json.dumps(tokens, separators=(',', ':')).encode('ascii')).hexdigest()


def compare(runs):
    result = {}
    for case in CASES:
        ms = [statistics.median(r['decode_ms'] for r in run['rows'] if r['case'] == case and not r['warmup']) for run in runs]
        baseline = math.sqrt(ms[0] * ms[2])
        result[case] = dict(baseline=baseline, candidate=ms[1], throughput_change_pct=100 * (baseline / ms[1] - 1),
            baseline_drift_pct=100 * (ms[2] / ms[0] - 1), baseline_tps=128000 / baseline,
            candidate_tps=128000 / ms[1], candidate_min_tps=min(128000 / r['decode_ms'] for r in runs[1]['rows'] if r['case'] == case and not r['warmup']))
    return result


def passes(comparison):
    return set(comparison) == set(CASES) and all(math.isfinite(r[k]) for r in comparison.values() for k in r) and all(
        r['throughput_change_pct'] >= 1 and abs(r['baseline_drift_pct']) <= 3 for r in comparison.values())


def check(d):
    assert d['complete'] and not d['production_changed'] and not d['target_achieved'] and not d['adopted']
    assert d['minimum_gain_pct'] == 1 and d['maximum_baseline_drift_pct'] == 3
    assert [r['label'] for r in d['runs']] == ['A1', 'B1', 'A2']
    assert len(d['plan_checks']) == 3 and all(r['passes'] == 65 and r['exit_code'] == 0 for r in d['plan_checks'])
    for run in d['runs']:
        assert run['expert_slots'] == 6297 and run['free_mib'] >= 1500 and run['exit_code'] == 0
        assert [(r['case'], r['warmup']) for r in run['rows']] == [(n, w) for w in (True, False, False, False) for n in CASES]
        assert run['dispatch_witnesses'] and all(r['active'] == int(run['label'] == 'B1') and r['parallel'] == r['mirror'] == 1 for r in run['dispatch_witnesses'])
        for row in run['rows']:
            assert row['generated'] == 128 and row['prompt_tokens'] == 2048 and row['reused'] == 0 and row['finish'] == 'length'
            assert math.isfinite(row['decode_ms']) and row['decode_ms'] > 0
            assert math.isfinite(row['prompt_ms']) and row['prompt_ms'] > 0
            assert 0 <= row['accepted'] <= row['offered']
            assert row['output_sha256'] == d['reference_sha256'][row['case']]
    assert d['comparison'] == compare(d['runs']) and d['passes_screen'] == passes(d['comparison'])


def publish():
    raw = json.loads((RAW / 'results.json').read_text(encoding='utf-8'))
    assert raw['complete']
    prior = json.loads((R / 'build-sycl-a770-hc-q8-model-arena-20261025/performance-settled/results.json').read_text(encoding='utf-8'))
    reference = {n: next(r['tokens'] for r in prior['runs'][0]['rows'] if r['case'] == n) for n in CASES}
    runs = []
    for run in raw['runs']:
        result = {k: run[k] for k in ('label', 'exit_code', 'expert_slots', 'free_mib')}
        result['dispatch_witnesses'] = [dict(zip(('entries', 'active', 'parallel', 'mirror'), map(int, values))) for values in re.findall(
            r'^PLAN_ORDER entries=(\d+) active=([01]) parallel=([01]) mirror=([01])$', (RAW / (run['label'] + '.stderr')).read_text(encoding='utf-8'), re.M)]
        result['rows'] = []
        for row in run['rows']:
            assert row['tokens'] == reference[row['case']] and len(row['tokens']) == 128
            result['rows'].append({k: v for k, v in row.items() if k not in ('tokens', 'done')} | dict(output_sha256=fingerprint(row['tokens'])))
        runs.append(result)
    d = {k: raw[k] for k in ('complete', 'production_changed', 'target_achieved', 'rule', 'plan_checks', 'comparison', 'passes_screen')}
    d.update(adopted=False, minimum_gain_pct=1, maximum_baseline_drift_pct=3, runs=runs,
        hardware='A77016GB / PCIe3.0 / E5-2696v4 / Windows / DPC++2025.3.2 / driver32.0.101.9034',
        reference_sha256={n: fingerprint(v) for n, v in reference.items()},
        fingerprint_format='SHA256 of ASCII JSON integer array with separators comma and colon',
        raw_file=(RAW / 'results.json').relative_to(R).as_posix(), raw_sha256=sha(RAW / 'results.json'),
        raw_audit='The runner completed device-plan, full-output, dispatch and source/binary hash audit under its exclusive GPU lock and exited0. Publication reads only small reports/results; no large binary reads or GPU commands.',
        audited_exe_sha256=raw['files_sha256'][(RAW / 'model/strata.exe').relative_to(R).as_posix()])
    check(d)
    lines = ['# A770 mapped-first expert scheduling screen', '', d['hardware'] + '.', '',
        'Following the actual2K ezpp diagnostic, test a stable partition of the real GPU expert plan: mapped-host experts first,then VRAM experts. Retain first-occurrence order within each class and entry order within each expert. Weight bytes, dot-product arithmetic, precision, grid size and destination scatter remain unchanged. This targets earlier PCIe requests in the mixed-residency path that the previous VRAM-only candidate did not reach.', '',
        'One isolated matched-ABI frozen HC-arena EXE,original BF16,6297slots,FP16KV,full vocabulary,verify6/MTP4/lookup3,min-p0.5,context/prefill8192,MTPwindow1024,promptcache0,reserve1536MiB. A1/B1/A2 differ only by the default-off scheduling flag. Profiling is disabled. Each process waits30seconds outside timing,runs Short16,one warmup/case and three retained interleaved cycles. No Sysman monitor in this cohort. All24 original2048-input/full128-output requests must equal the originalBF16 reference; all lows and warmups are retained.', '',
        '195 GPU-plan checks cover default,enabled and enabled-with-serial-fallback modes: VRAM,mixed,allmapped,unique/repeated experts,empty plans,the64-entry boundary,missing-mirror fallback and invalid sizes. Full plan bytes,group starts,pointers,token indices,destinations and untouched sentinels are checked. First-use host witnesses prove the real mirrored parallel dispatch has the candidate enabled only in B; they do not count device-side residency.', '',
        '| Case | Baseline tok/s | Candidate tok/s | Paired gain | Baseline time drift | Candidate minimum tok/s |',
        '| --- | ---: | ---: | ---: | ---: | ---: |']
    for case, row in d['comparison'].items():
        lines.append(f"| {case} | {row['baseline_tps']:.3f} | {row['candidate_tps']:.3f} | {row['throughput_change_pct']:+.3f}% | {row['baseline_drift_pct']:+.3f}% | {row['candidate_min_tps']:.3f} |")
    lines += ['', f"Both-case1% gain / <=3% time-drift screen: **{'PASS; independent confirmation required' if d['passes_screen'] else 'FAIL; do not adopt'}**. This experiment changes no production binary or configuration. Stable40tok/s remains unmet.", '',
        '| Arm | Case | Warmup | Decode ms | Accepted / offered |', '| --- | --- | --- | ---: | ---: |']
    for run in runs:
        for row in run['rows']:
            lines.append(f"| {run['label']} | {row['case']} | {row['warmup']} | {row['decode_ms']:.1f} | {row['accepted']} / {row['offered']} |")
    lines += ['', d['raw_audit'], '', f'[Data]({STEM}.json). Offline check:', '', '```powershell',
        'python bench/results/a770/check_mapped_first_20261026.py --self-check', '```', '']
    report = H / (STEM + '.md')
    report.write_text('\n'.join(lines), encoding='utf-8')
    d.update(checker_sha256=sha(Path(__file__)), report_sha256=sha(report))
    (H / (STEM + '.json')).write_text(json.dumps(d, indent=2), encoding='utf-8')


def main():
    if '--publish' in sys.argv:
        publish()
    d = json.loads((H / (STEM + '.json')).read_text(encoding='utf-8'))
    assert d['checker_sha256'] == sha(Path(__file__)) and d['report_sha256'] == sha(H / (STEM + '.md'))
    check(d)
    if '--self-check' in sys.argv:
        for change in ('nan', 'missing', 'dispatch', 'output'):
            bad = copy.deepcopy(d)
            if change == 'nan': bad['runs'][1]['rows'][2]['decode_ms'] = float('nan')
            if change == 'missing': bad['runs'][0]['rows'].pop()
            if change == 'dispatch': bad['runs'][1]['dispatch_witnesses'][0]['active'] = 0
            if change == 'output': bad['runs'][2]['rows'][0]['output_sha256'] = 'incorrect'
            try:
                check(bad)
            except AssertionError:
                pass
            else:
                raise AssertionError('accepted ' + change)
        boundary = {n: dict(throughput_change_pct=1, baseline_drift_pct=3) for n in CASES}
        assert passes(boundary)
        boundary['Prose2K']['throughput_change_pct'] = 0.999
        assert not passes(boundary)
    print('PASS195 GPU-plan checks,24 full-output fingerprints,dispatch witnesses,all medians and1% gate; adoption=False')


if __name__ == '__main__':
    main()
