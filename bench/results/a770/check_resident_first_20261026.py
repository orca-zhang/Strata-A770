"""Publish/recheck the monitored opposite expert order; reuse the mapped-first accounting."""
import copy
import json
import math
from pathlib import Path
import re
import sys

import check_mapped_first_20261026 as c

H = Path(__file__).resolve().parent
R = H.parents[2]
RAW = R / 'build-sycl-a770-resident-first-20261026'
STEM = 'resident-first-2026-10-26'


def check(d):
    c.check(d)
    assert d['monitor_stopped_by_runner'] and d['monitor_exit'] != 0
    rows = [(run['label'], row) for run in d['runs'] for row in run['rows']]
    assert len(d['telemetry']) == len(rows) == 24
    for sample, (label, row) in zip(d['telemetry'], rows):
        assert (sample['label'], sample['case'], sample['warmup']) == (label, row['case'], row['warmup'])
        assert sample['samples'] > 0 and all(math.isfinite(sample[k]) for k in ('min_mhz', 'max_mhz'))
        assert 0 <= sample['min_mhz'] <= sample['max_mhz']


def publish():
    raw = json.loads((RAW / 'results.json').read_text(encoding='utf-8'))
    assert raw['complete']
    prior = json.loads((R / 'build-sycl-a770-hc-q8-model-arena-20261025/performance-settled/results.json').read_text(encoding='utf-8'))
    reference = {n: next(r['tokens'] for r in prior['runs'][0]['rows'] if r['case'] == n) for n in c.CASES}
    runs = []
    for run in raw['runs']:
        result = {k: run[k] for k in ('label', 'exit_code', 'expert_slots', 'free_mib')}
        result['dispatch_witnesses'] = [dict(zip(('entries', 'active', 'parallel', 'mirror'), map(int, values))) for values in re.findall(
            r'^PLAN_ORDER entries=(\d+) active=([01]) parallel=([01]) mirror=([01])$', (RAW / (run['label'] + '.stderr')).read_text(encoding='utf-8'), re.M)]
        result['rows'] = []
        for row in run['rows']:
            assert row['tokens'] == reference[row['case']] and len(row['tokens']) == 128
            result['rows'].append({k: v for k, v in row.items() if k not in ('tokens', 'done')} | dict(output_sha256=c.fingerprint(row['tokens'])))
        runs.append(result)
    d = {k: raw[k] for k in ('complete', 'production_changed', 'target_achieved', 'rule', 'plan_checks', 'comparison', 'passes_screen',
        'telemetry', 'monitor_stopped_by_runner', 'monitor_exit')}
    d.update(adopted=False, minimum_gain_pct=1, maximum_baseline_drift_pct=3, runs=runs,
        hardware='A77016GB / PCIe3.0 / E5-2696v4 / Windows / DPC++2025.3.2 / driver32.0.101.9034',
        reference_sha256={n: c.fingerprint(v) for n, v in reference.items()},
        fingerprint_format='SHA256 of ASCII JSON integer array with separators comma and colon',
        raw_file=(RAW / 'results.json').relative_to(R).as_posix(), raw_sha256=c.sha(RAW / 'results.json'),
        raw_audit='Runner completed all device-plan/full-output/dispatch/frequency/source/binary audits under the exclusive GPU lock and exited0. Publication reads small files only.',
        audited_exe_sha256=raw['files_sha256'][(RAW / 'model/strata.exe').relative_to(R).as_posix()])
    check(d)
    lines = ['# A770 resident-first expert scheduling screen', '', d['hardware'] + '.', '',
        'Opposite order after the mapped-first screen failed: stable-partition VRAM experts before mapped-host experts in the actual parallel mirrored GPU plan. Preserve first-occurrence order within each class,entry order,weight bytes,dot products,precision,group-grid size and destination scatter. Default-off;serial and host fallback remain unchanged.', '',
        'Matched frozen HC-arena EXE,originalBF16,6297slots,FP16KV,full vocabulary,verify6/MTP4/lookup3,min-p0.5,context/prefill8192,MTPwindow1024,promptcache0,reserve1536MiB. Same EXE A1/B1/A2,30seconds settle,Short16,one warmup/case plus three retained interleaved cycles. Profiling disabled. All24 original2048/full128 requests equal the originalBF16 reference. Keep every low value and warmup. All195 GPU-plan checks pass,including default/enabled/serial,mixed/allmapped/VRAM,repeated/unique,empty,64-entry boundary,missing mirror and invalid sizes. Real dispatch witnesses are enabled only in B; these host witnesses do not count device-side residency.', '',
        'Separate read-only1Hz Sysman monitoring across all arms; no clock or power writes. Every request has successful frequency samples during decode. Sampling does not identify the cause of an individual slow request. Monitor overhead is shared and is not separately quantified.', '',
        '| Case | Baseline tok/s | Candidate tok/s | Paired gain | Baseline time drift | Candidate minimum tok/s |',
        '| --- | ---: | ---: | ---: | ---: | ---: |']
    for case, row in d['comparison'].items():
        lines.append(f"| {case} | {row['baseline_tps']:.3f} | {row['candidate_tps']:.3f} | {row['throughput_change_pct']:+.3f}% | {row['baseline_drift_pct']:+.3f}% | {row['candidate_min_tps']:.3f} |")
    lines += ['', f"Both-case1% gain / <=3% baseline time-drift screen: **{'PASS; independent original-binary confirmation required' if d['passes_screen'] else 'FAIL; do not adopt'}**. This experiment changes no production binary or configuration. Stable40tok/s remains unmet.", '',
        '| Arm | Case | Warmup | Decode ms | Accepted / offered | MHz min–max | Throttle sampled |',
        '| --- | --- | --- | ---: | ---: | ---: | --- |']
    index = 0
    for run in runs:
        for row in run['rows']:
            t = d['telemetry'][index]
            lines.append(f"| {run['label']} | {row['case']} | {row['warmup']} | {row['decode_ms']:.1f} | {row['accepted']} / {row['offered']} | {t['min_mhz']:.0f}–{t['max_mhz']:.0f} | {t['throttle_observed']} |")
            index += 1
    lines += ['', d['raw_audit'], '', f'[Data]({STEM}.json). Offline check:', '', '```powershell',
        'python bench/results/a770/check_resident_first_20261026.py --self-check', '```', '']
    report = H / (STEM + '.md')
    report.write_text('\n'.join(lines), encoding='utf-8')
    d.update(checker_sha256=c.sha(Path(__file__)), accounting_checker_sha256=c.sha(Path(c.__file__)), report_sha256=c.sha(report))
    (H / (STEM + '.json')).write_text(json.dumps(d, indent=2), encoding='utf-8')


def main():
    if '--publish' in sys.argv:
        publish()
    d = json.loads((H / (STEM + '.json')).read_text(encoding='utf-8'))
    assert d['checker_sha256'] == c.sha(Path(__file__)) and d['report_sha256'] == c.sha(H / (STEM + '.md'))
    assert d['accounting_checker_sha256'] == c.sha(Path(c.__file__))
    check(d)
    if '--self-check' in sys.argv:
        bad = copy.deepcopy(d)
        bad['telemetry'][0]['samples'] = 0
        try:
            check(bad)
        except AssertionError:
            pass
        else:
            raise AssertionError('missing frequency coverage accepted')
    print('PASS195 GPU-plan checks,24 complete outputs,dispatch,frequency coverage,all medians and1% gate; adoption=False')


if __name__ == '__main__':
    main()
