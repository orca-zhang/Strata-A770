"""Recompute the original 2K/128 decay screen; --local audits frozen raw artifacts."""
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


def close(a, b):
    assert math.isclose(a, b, rel_tol=1e-12, abs_tol=1e-9), (a, b)


def check(d):
    assert d['complete'] and not d['adopted'] and not d['target_achieved'] and not d['production_changed']
    assert d['requests'] == 24 and d['output_tokens_checked'] == 3072
    assert [run['label'] for run in d['runs']] == ['A1', 'B1', 'A2']
    assert [run['adapt_decay'] for run in d['runs']] == [0.5, 0.75, 0.5]
    assert d['cases'] == ['Code2K', 'Prose2K']
    schedule = [(name, True) for name in d['cases']] + [(name, False) for _ in range(3) for name in d['cases']]
    for run in d['runs']:
        assert run['expert_slots'] == 6297 and run['exit_code'] == 0 and len(run['rows']) == 8
        assert [(row['case'], row['warmup']) for row in run['rows']] == schedule
        for row in run['rows']:
            assert row['generated'] == 128 and row['prompt_tokens'] == 2048 and row['reused'] == 0
            assert row['finish'] == 'length' and row['decode_ms'] > 0 and 0 <= row['accepted'] <= row['offered']
            assert row['output_sha256'] == d['reference_output_sha256'][row['case']]
        for name in d['cases']:
            rows = [row for row in run['rows'] if row['case'] == name and not row['warmup']]
            assert len(rows) == 3
            summary = run['summary'][name]
            speeds = [128000 / row['decode_ms'] for row in rows]
            close(summary['decode_tps'], statistics.median(speeds))
            close(summary['decode_tps_range'][0], min(speeds))
            close(summary['decode_tps_range'][1], max(speeds))
            assert summary['accepted'] == [row['accepted'] for row in rows]
            assert summary['offered'] == [row['offered'] for row in rows]
    for name in d['cases']:
        a, candidate, z = [run['summary'][name]['decode_tps'] for run in d['runs']]
        pair = d['comparison'][name]
        close(pair['baseline'], 1000 / math.sqrt(a * z))
        close(pair['candidate'], 1000 / candidate)
        close(pair['throughput_change_pct'], 100 * (candidate / math.sqrt(a * z) - 1))
        close(pair['baseline_drift_pct'], 100 * (a / z - 1))
    assert d['passes_screen'] == all(abs(p['baseline_drift_pct']) <= 3 and p['throughput_change_pct'] >= 3
                                     for p in d['comparison'].values())
    assert len(d['telemetry']) == 24
    assert [(t['label'], t['case'], t['warmup']) for t in d['telemetry']] == [
        (run['label'], row['case'], row['warmup']) for run in d['runs'] for row in run['rows']]
    assert all(t['samples'] > 0 and 0 < t['min_mhz'] <= t['max_mhz'] for t in d['telemetry'])


def main():
    d = json.loads((H / 'decode2k-decay-2026-10-25.json').read_text(encoding='utf-8'))
    assert sha(Path(__file__)) == d['checker_sha256']
    assert sha(H / 'decode2k-decay-2026-10-25.md') == d['report_sha256']
    check(d)
    if '--local' in sys.argv:
        folder = R / d['source_directory']
        assert sha(folder / 'results.json') == d['source_results_sha256']
        spec = importlib.util.spec_from_file_location('decay_raw', folder / 'run.py')
        m = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(m)
        m.audit(json.loads((folder / 'results.json').read_text(encoding='utf-8')))
    print('PASS decay-only screen,24 full2K/128 outputs, all samples, medians, drift/gains and telemetry')


if __name__ == '__main__':
    main()
