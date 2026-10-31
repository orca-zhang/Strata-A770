"""Recompute tail256/64; --adoption checks the later deployment, --local also audits raw records."""
import argparse
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import statistics
import sys

H = Path(__file__).resolve().parent
R = H.parents[2]


def close(a, b):
    assert math.isclose(a, b, rel_tol=1e-12, abs_tol=1e-8), (a, b)


def pair(values, result):
    a, x, z = values
    close(result['baseline'], math.sqrt(a * z))
    close(result['candidate'], x)
    close(result['throughput_change_pct'], 100 * (math.sqrt(a * z) / x - 1))
    close(result['baseline_drift_pct'], 100 * (z / a - 1))


def adoption(local):
    d = json.loads((H / 'prefill-cache-tail-adoption-2026-10-26.json').read_text(encoding='utf-8'))
    assert d['complete'] and d['adopted'] and d['adoption_basis'] == 'explicit user request'
    assert not d['production_performance_confirmed'] and not d['target_achieved']
    assert d['requests'] == 7 and d['output_tokens_checked'] == 560
    assert d['workspace_mib'] == 32 and d['eager'] and d['expert_cache'] == 'auto'
    assert d['value_bits'] == 4 and d['value_residuals'] == 0
    base = d['production_baseline']
    assert base['exit_code'] == 0 and base['free_mib'] >= 1024
    assert d['production_candidate']['exit_code'] == 0 and d['production_candidate']['free_mib'] >= 1024
    assert d['functional_expert_cache'] == '6000'
    assert [r['flag'] for r in d['runs']] == ['0', '1']
    assert len({r['expert_slots'] for r in d['runs']}) == 1
    assert d['previous_short']['generated'] == 16
    assert d['previous_short']['output_sha256'] == d['runs'][0]['rows'][0]['output_sha256']
    for run in d['runs']:
        assert run['exit_code'] == 0 and run['expert_slots'] == 6297 and run['free_mib'] >= 1500
        assert [r['case'] for r in run['rows']] == ['Short', 'Code2K', 'Prose2K']
        for i, row in enumerate(run['rows']):
            assert row['generated'] == (16 if i == 0 else 128) and row['reused'] == 0 and row['finish'] == 'length'
            assert row['output_sha256'] == d['runs'][0]['rows'][i]['output_sha256']
            if i == 0:
                assert row['cache_tail'] is None and 1 < row['prompt_tokens'] <= 65
                continue
            assert row['cache_tail']['tokens'] == row['prompt_tokens'] - 1
            assert row['cache_tail']['mass'] == min(row['prompt_tokens'] - 1, 256) * 480
            assert row['cache_tail']['limit'] == (64 if run['flag'] == '1' else 256)
            assert row['prompt_tokens'] == 2048
    assert all(d['runs'][0]['rows'][i]['cache_tail']['digest'] != d['runs'][1]['rows'][i]['cache_tail']['digest'] for i in (1, 2))
    if local:
        raw = R / 'build-sycl-a770-prefill-cache-tail-production-v3-20261026'
        assert hashlib.sha256((raw / 'validation-v3/results.json').read_bytes()).hexdigest() == d['raw_results_sha256']
        spec = importlib.util.spec_from_file_location('tail_production_audit', raw / 'finish_v3.py')
        m = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(m)
        m.audit(json.loads((raw / 'validation-v3/results.json').read_text(encoding='utf-8')))
    print('PASS requested adoption, production auto-cache/eager, complete outputs, short-prompt identity and normalized routing mass')


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--local', action='store_true')
    ap.add_argument('--adoption', action='store_true', help='check the subsequent user-requested production adoption')
    ap.add_argument('--cohort', choices=('screen', 'confirm'), default='screen')
    args = ap.parse_args()
    if args.adoption:
        return adoption(args.local)
    suffix = '' if args.cohort == 'screen' else '-confirm'
    d = json.loads((H / ('prefill-cache-tail-2026-10-26' + suffix + '.json')).read_text(encoding='utf-8'))
    assert d['complete'] and d['scope'] == args.cohort and not d['adopted'] and not d['target_achieved']
    assert d['requests'] == 24 and d['output_tokens_checked'] == 3072 and d['linked_libraries_unchanged'] == 13
    assert [r['label'] for r in d['runs']] == ['A1', 'B1', 'A2']
    assert [r['flag'] for r in d['runs']] == ['0', '1', '0']
    for run in d['runs']:
        assert run['exit_code'] == 0 and run['expert_slots'] == 6297 and run['free_mib'] >= 1500
        schedule = [(name, warm) for warm in (True, False, False, False) for name in d['cases']]
        assert [(r['case'], r['warmup']) for r in run['rows']] == schedule
        for row in run['rows']:
            assert row['generated'] == 128 and row['prompt_tokens'] == 2048 and row['reused'] == 0 and row['finish'] == 'length'
            assert row['decode_ms'] > 0 and row['prompt_ms'] > 0 and 0 <= row['accepted'] <= row['offered']
            assert row['output_sha256'] == d['reference_output_sha256'][row['case']]
            tail, seed = row['cache_tail'], row['cache_seed']
            assert tail['tokens'] == 2047 and tail['mass'] == seed['tail_total'] == 122880
            assert tail['limit'] == (64 if run['label'] == 'B1' else 256) and len(tail['digest']) == 16
            assert seed['cap'] == 4 and 0 <= seed['batches'] <= 4 and 0 <= seed['swaps'] <= 32 * seed['batches']
    for name, result in d['comparison'].items():
        groups = [[r for r in run['rows'] if r['case'] == name and not r['warmup']] for run in d['runs']]
        assert all(len(rows) == 3 for rows in groups)
        ms = [statistics.median(r['decode_ms'] for r in rows) for rows in groups]
        pair(ms, result)
        close(result['baseline_tps'], 128000 / math.sqrt(ms[0] * ms[2]))
        close(result['candidate_tps'], 128000 / ms[1])
        close(result['candidate_min_tps'], min(128000 / r['decode_ms'] for r in groups[1]))
        totals = [statistics.median(r['prompt_ms'] + r['decode_ms'] for r in rows) for rows in groups]
        pair(totals, result['prompt_plus_decode'])
        assert result['prompt_ms'] == [statistics.median(r['prompt_ms'] for r in rows) for rows in groups]
    assert d['passes_screen'] == all(v['throughput_change_pct'] >= 1 and abs(v['baseline_drift_pct']) <= 3 for v in d['comparison'].values())
    assert len(d['telemetry']) == 24 and all(v['samples'] > 0 for v in d['telemetry'])
    if args.local:
        raw = R / 'build-sycl-a770-prefill-cache-tail-20261026'
        assert hashlib.sha256((raw / args.cohort / 'results.json').read_bytes()).hexdigest() == d['raw_results_sha256']
        sys.argv = [str(raw / 'run.py'), '--out', args.cohort]
        spec = importlib.util.spec_from_file_location('tail_raw_audit', raw / 'run.py')
        m = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(m)
        m.audit(json.loads((raw / args.cohort / 'results.json').read_text(encoding='utf-8')))
    print('PASS original2K/full128, tail256/64 normalized mass, all samples, paired gate and prompt+decode accounting')


if __name__ == '__main__':
    main()
