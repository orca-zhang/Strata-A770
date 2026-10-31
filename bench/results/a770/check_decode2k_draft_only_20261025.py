"""Recompute the fixed-cache draft-only cohort; --local verifies frozen raw evidence."""
import argparse
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import statistics

H = Path(__file__).resolve().parent
R = H.parents[2]


def close(a, c):
    assert math.isclose(a, c, rel_tol=1e-12, abs_tol=1e-9), (a, c)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--local', action='store_true')
    args = ap.parse_args()
    d = json.loads((H / 'decode2k-draft-only-2026-10-25.json').read_text(encoding='utf-8'))
    assert d['complete'] and [r['label'] for r in d['runs']] == ['A1', 'B1', 'A2', 'B2', 'A3']
    assert d['requests'] == 40 and d['output_tokens_checked'] == 5120
    for run in d['runs']:
        assert run['exit_code'] == 0 and run['expert_slots'] == 5771 and len(run['rows']) == 8
        assert sum(row['warmup'] for row in run['rows']) == 2
        for row in run['rows']:
            assert row['generated'] == 128 and row['prompt_tokens'] == 2048 and row['reused'] == 0 and row['finish'] == 'length'
            assert row['output_sha256'] == d['reference_output_sha256'][row['case']]
            assert 0 <= row['accepted'] <= row['offered'] and row['decode_ms'] > 0
        for name in d['cases']:
            rows = [r for r in run['rows'] if r['case'] == name and not r['warmup']]
            assert len(rows) == 3
            tps = [128000 / r['decode_ms'] for r in rows]
            summary = run['summary'][name]
            close(summary['decode_tps'], 128000 / statistics.median(r['decode_ms'] for r in rows))
            close(summary['decode_tps_range'][0], min(tps))
            close(summary['decode_tps_range'][1], max(tps))
    for index in (1, 3):
        before, candidate, after = d['runs'][index-1:index+2]
        for name in d['cases']:
            a, x, c = [r['summary'][name]['decode_tps'] for r in (before, candidate, after)]
            p = d['comparison'][candidate['label']][name]
            close(p['throughput_change_pct'], 100 * (x / math.sqrt(a*c) - 1))
            close(p['baseline_drift_pct'], 100 * (a/c - 1))
            assert p['drift_eligible'] == (abs(p['baseline_drift_pct']) <= 3)
            assert p['median_tps_reaches_40'] == (x >= 40)
    confirmed = all(p['drift_eligible'] and p['throughput_change_pct'] >= 3 for group in d['comparison'].values() for p in group.values())
    target = confirmed and all(128000 / row['decode_ms'] >= 40 for run in d['runs'] if run['label'].startswith('B') for row in run['rows'] if not row['warmup'])
    assert d['confirmed_gain'] == confirmed and d['target_achieved'] == target
    if args.local:
        raw = R / 'build-sycl-a770-decode2k-draft-only-20261025'
        assert hashlib.sha256((raw / 'results.json').read_bytes()).hexdigest() == d['raw_results_sha256']
        spec = importlib.util.spec_from_file_location('draft_audit', raw / 'run.py')
        obj = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(obj)
        obj.audit(json.loads((raw / 'results.json').read_text(encoding='utf-8')))
    print('PASS all40 original2K/128 outputs, retained ranges, paired gains, baseline drift and stable40 gate')


if __name__ == '__main__':
    main()
