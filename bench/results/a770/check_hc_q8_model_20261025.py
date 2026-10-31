"""Offline accounting for the HC storage, limited quality and original2K/128 screen."""
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import statistics
import sys

H = Path(__file__).resolve().parent
R = H.parents[2]
STEM = 'hc-q8-model-2026-10-25'


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def close(a, b):
    assert math.isfinite(a) and math.isclose(a, b, rel_tol=1e-12, abs_tol=1e-9), (a, b)


def check(d):
    assert d['complete'] and not d['adopted'] and not d['target_achieved'] and not d['production_changed']
    storage = d['storage']
    assert storage['matrices'] == 192 and storage['bf16_bytes'] == 1258291200 and storage['q8_bytes'] == 668467200
    assert storage['unit_checks'] == 189 and storage['restore_checks'] == 6 and storage['candidate_equal_scattered']
    assert storage['free_mib_difference'] == storage['runs'][1]['free_mib'] - storage['runs'][0]['free_mib'] == 558
    for run in storage['runs']:
        assert run['exit_code'] == 0 and run['expert_slots'] == 6297 and len(run['rows']) == 4
        for name in ('Code2K', 'Prose2K'):
            rows = [r for r in run['rows'] if r['case'] == name]
            assert len(rows) == 2 and rows[0]['output_sha256'] == rows[1]['output_sha256']
            for row in rows:
                assert row['generated'] == 128 and row['prompt_tokens'] == 2048 and row['reused'] == 0 and row['finish'] == 'length'
    q = d['quality']
    assert [r['label'] for r in q['runs']] == ['A1', 'B1', 'A2']
    assert all(r['exit_code'] == 0 for r in q['runs'])
    a, b, z = [r['cases'] for r in q['runs']]
    assert a == z and len(a) == 9 and sum(r['tokens'] for r in a.values()) == 758
    delta = 0
    for name in a:
        n = a[name]['tokens']
        assert n == b[name]['tokens'] and 0 <= q['top1_same_count'][name] <= n
        for cases in (a, b):
            assert math.isfinite(cases[name]['nll'])
            close(cases[name]['ppl'], math.exp(cases[name]['nll'] / n))
        change = 100 * math.expm1((b[name]['nll'] - a[name]['nll']) / n)
        close(q['comparison']['cases'][name]['ppl_change_pct'], change)
        close(q['comparison']['cases'][name]['top1_agreement'], q['top1_same_count'][name] / n)
        delta += b[name]['nll'] - a[name]['nll']
    aggregate = 100 * math.expm1(delta / 758)
    close(q['comparison']['aggregate_ppl_change_pct'], aggregate)
    assert q['passes_quality_screen'] == (aggregate <= 1 and all(r['ppl_change_pct'] <= 3 for r in q['comparison']['cases'].values()))
    perf = d['performance']
    assert [r['label'] for r in perf['runs']] == ['A1', 'B1', 'A2']
    schedule = [(name, warm) for warm in (True, False, False, False) for name in ('Code2K', 'Prose2K')]
    for run in perf['runs']:
        assert run['exit_code'] == 0 and [(r['case'], r['warmup']) for r in run['rows']] == schedule
        if run['label'] == 'B1':
            assert 6500 <= run['expert_slots'] <= 6750 and run['free_mib'] >= 1536
        else:
            assert run['expert_slots'] == 6297 and run['free_mib'] >= 1500
        for row in run['rows']:
            assert row['generated'] == 128 and row['prompt_tokens'] == 2048 and row['reused'] == 0 and row['finish'] == 'length'
            assert math.isfinite(row['decode_ms']) and row['decode_ms'] > 0 and 0 <= row['accepted'] <= row['offered']
        for name in ('Code2K', 'Prose2K'):
            hashes = [r['output_sha256'] for r in run['rows'] if r['case'] == name]
            assert len(set(hashes)) == 1
            if run['label'].startswith('A'):
                assert hashes[0] == d['reference_output_sha256'][name]
    for name, p in perf['comparison'].items():
        ms = [statistics.median(r['decode_ms'] for r in run['rows'] if r['case'] == name and not r['warmup']) for run in perf['runs']]
        base = math.sqrt(ms[0] * ms[2])
        for key, value in dict(baseline=base, candidate=ms[1], throughput_change_pct=100*(base/ms[1]-1),
            baseline_drift_pct=100*(ms[2]/ms[0]-1), baseline_tps=128000/base, candidate_tps=128000/ms[1],
            candidate_min_tps=min(128000/r['decode_ms'] for r in perf['runs'][1]['rows'] if r['case'] == name and not r['warmup'])).items():
            close(p[key], value)
    assert perf['passes_screen'] == all(p['throughput_change_pct'] >= 3 and abs(p['baseline_drift_pct']) <= 3 for p in perf['comparison'].values())
    assert perf['screen_reached_stable40'] == all(p['candidate_min_tps'] >= 40 for p in perf['comparison'].values())
    assert len(perf['telemetry']) == 24
    assert [(r['label'], r['case'], r['warmup']) for r in perf['telemetry']] == [(run['label'], row['case'], row['warmup']) for run in perf['runs'] for row in run['rows']]
    assert all(r['samples'] > 0 and 0 < r['min_mhz'] <= r['max_mhz'] for r in perf['telemetry'])


def main():
    d = json.loads((H / (STEM + '.json')).read_text(encoding='utf-8'))
    assert sha(Path(__file__)) == d['checker_sha256'] and sha(H / (STEM + '.md')) == d['report_sha256']
    check(d)
    if '--local' in sys.argv:
        folder = R / d['source_directory']
        for name, digest in d['source_results_sha256'].items():
            path = folder / name
            assert sha(path) == digest
        for script, data in [('check.py', 'checks/results.json'), ('quality.py', 'quality/results.json'), ('run-settled.py', 'performance-settled/results.json')]:
            spec = importlib.util.spec_from_file_location('hc_raw', folder / script)
            m = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(m)
            m.audit(json.loads((folder / data).read_text(encoding='utf-8')))
    print('PASS HC storage/quality/2K performance accounting, all samples and gates')


if __name__ == '__main__':
    main()
