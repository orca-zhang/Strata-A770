"""Recompute the isolated GDN output model screen; --local also verifies frozen raw files."""
import argparse
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import statistics

H = Path(__file__).resolve().parent
R = H.parents[2]


def close(a, b):
    assert math.isclose(a, b, rel_tol=1e-12, abs_tol=1e-9), (a, b)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--local', action='store_true')
    args = ap.parse_args()
    d = json.loads((H / 'decode2k-gdn-output-2026-10-25.json').read_text(encoding='utf-8'))
    assert d['complete'] and d['scope'] == 'screen' and not d['adopted'] and not d['target_achieved']
    assert d['requests'] == 24 and d['output_tokens_checked'] == 3072 and d['linked_libraries_unchanged'] == 13
    assert [r['label'] for r in d['runs']] == ['A1', 'B1', 'A2']
    assert [r['mode'] for r in d['runs']] == ['baseline', 'candidate', 'baseline']
    for run in d['runs']:
        assert run['expert_slots'] == 6297 and run['exit_code'] == 0 and len(run['rows']) == 8
        schedule = [(n, True) for n in d['cases']] + [(n, False) for _ in range(3) for n in d['cases']]
        assert [(r['case'], r['warmup']) for r in run['rows']] == schedule
        for row in run['rows']:
            assert row['generated'] == 128 and row['prompt_tokens'] == 2048 and row['reused'] == 0
            assert row['finish'] == 'length' and row['decode_ms'] > 0 and 0 <= row['accepted'] <= row['offered']
            assert row['outputs_equal'] and row['output_sha256'] == d['reference_output_sha256'][row['case']]
        for name in d['cases']:
            rows = [r for r in run['rows'] if r['case'] == name and not r['warmup']]
            assert len(rows) == 3
            rates = [128000 / row['decode_ms'] for row in rows]
            s = run['summary'][name]
            close(s['decode_tps'], statistics.median(rates))
            close(s['decode_tps_range'][0], min(rates))
            close(s['decode_tps_range'][1], max(rates))
            assert s['accepted'] == [r['accepted'] for r in rows] and s['offered'] == [r['offered'] for r in rows]
    for name in d['cases']:
        a, x, z = [r['summary'][name]['decode_tps'] for r in d['runs']]
        pair = d['comparison'][name]
        close(pair['baseline'], 1000 / math.sqrt(a * z))
        close(pair['candidate'], 1000 / x)
        close(pair['throughput_change_pct'], 100 * (x / math.sqrt(a * z) - 1))
        close(pair['baseline_drift_pct'], 100 * (a / z - 1))
    assert d['passes_screen'] == all(abs(p['baseline_drift_pct']) <= 3 and p['throughput_change_pct'] >= 1
                                     for p in d['comparison'].values())
    assert [(pr['number'], pr['state'], pr['merged']) for pr in d['upstream']['pulls']] == [(30976, 'open', False), (26631, 'closed', True)]
    if args.local:
        raw = R / 'build-sycl-a770-decode2k-gdn-output-20261025'
        assert hashlib.sha256((raw / 'results.json').read_bytes()).hexdigest() == d['raw_results_sha256']
        spec = importlib.util.spec_from_file_location('gdn_raw_audit', raw / 'run.py')
        m = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(m)
        m.audit(json.loads((raw / 'results.json').read_text(encoding='utf-8')))
    print('PASS GDN output isolation, all24 original2K/128 outputs, all retained timings and paired gate')


if __name__ == '__main__':
    main()
