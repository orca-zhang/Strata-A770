"""Check the original 2K/128 output-budget screen; --local audits frozen artifacts."""
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import statistics
import sys

H = Path(__file__).resolve().parent
R = H.parents[2]


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def check(d):
    assert d['complete'] and not d['adopted'] and not d['target_achieved'] and not d['production_changed']
    assert d['requests'] == 24 and d['output_tokens_checked'] == 3072 and d['short_boundary_checks'] == 9
    assert d['cases'] == ['Code2K', 'Prose2K']
    assert [r['label'] for r in d['runs']] == ['A1', 'B1', 'A2']
    assert [r['tail_budget'] for r in d['runs']] == [False, True, False]
    schedule = [(name, True) for name in d['cases']] + [(name, False) for _ in range(3) for name in d['cases']]
    for run in d['runs']:
        assert run['expert_slots'] == 6297 and run['exit_code'] == 0
        assert (run['verify_capacity'], run['mtp_capacity'], run['lookup']) == (6, 4, 3)
        assert [(r['case'], r['warmup']) for r in run['rows']] == schedule
        assert [r['generated'] for r in run['boundary_checks']] == [1, 2, 3]
        for row in run['boundary_checks']:
            assert row['output_sha256'] == run['short_prefix_sha256'][str(row['generated'])]
            assert row['finish'] == 'length' and row['reused'] == 0
        for row in run['rows']:
            assert row['generated'] == 128 and row['prompt_tokens'] == 2048 and row['reused'] == 0
            assert row['finish'] == 'length' and row['decode_ms'] > 0 and 0 <= row['accepted'] <= row['offered']
            assert row['output_sha256'] == d['reference_output_sha256'][row['case']]
        for case in d['cases']:
            rows = [r for r in run['rows'] if r['case'] == case and not r['warmup']]
            speeds = [128000 / r['decode_ms'] for r in rows]
            summary = run['summary'][case]
            assert summary['decode_tps'] == statistics.median(speeds)
            assert summary['decode_tps_range'] == [min(speeds), max(speeds)]
            assert summary['accepted'] == [r['accepted'] for r in rows]
            assert summary['offered'] == [r['offered'] for r in rows]
    for case in d['cases']:
        a, b, z = [run['summary'][case]['decode_tps'] for run in d['runs']]
        expected = dict(baseline=1000 / math.sqrt(a * z), candidate=1000 / b,
            throughput_change_pct=100 * (b / math.sqrt(a * z) - 1), baseline_drift_pct=100 * (a / z - 1))
        for key, value in expected.items():
            assert math.isclose(d['comparison'][case][key], value, rel_tol=1e-12, abs_tol=1e-9)
    assert d['passes_screen'] == all(abs(p['baseline_drift_pct']) <= 3 and p['throughput_change_pct'] >= 3
                                    for p in d['comparison'].values())
    assert len(d['telemetry']) == 24
    assert [(t['label'], t['case'], t['warmup']) for t in d['telemetry']] == [
        (run['label'], row['case'], row['warmup']) for run in d['runs'] for row in run['rows']]
    assert all(t['samples'] > 0 and 0 < t['min_mhz'] <= t['max_mhz'] for t in d['telemetry'])


def main():
    d = json.loads((H / 'decode2k-tail-budget-2026-10-25.json').read_text(encoding='utf-8'))
    assert sha(Path(__file__)) == d['checker_sha256']
    assert sha(H / 'decode2k-tail-budget-2026-10-25.md') == d['report_sha256']
    check(d)
    if '--local' in sys.argv:
        folder = R / d['source_directory']
        assert sha(folder / 'results.json') == d['source_results_sha256']
        spec = importlib.util.spec_from_file_location('tail_raw', folder / 'run.py')
        m = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(m)
        m.audit(json.loads((folder / 'results.json').read_text(encoding='utf-8')))
    print('PASS tail-budget screen:24 full2K/128 outputs,9 short boundaries, all samples, medians, drift/gains and telemetry')


if __name__ == '__main__':
    main()
