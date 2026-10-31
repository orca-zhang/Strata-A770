"""Offline check of actual2K ezpp diagnostics; never accepts a speed result."""
import copy
import hashlib
import json
import math
from pathlib import Path
import statistics
import sys

H = Path(__file__).resolve().parent
STEM = 'ezpp-decode2k-2026-10-26'
PRIMARY = ['verify.submit.post.experts', 'verify.submit.pre.attn', 'verify.submit.pre.hc0',
    'verify.submit.pre.hc1', 'verify.submit.pre.shared', 'verify.submit.pre.route',
    'verify.submit.post.combine', 'verify.submit.post.plan', 'verify.submit.post.misses',
    'verify.submit.input', 'verify.submit.head']


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def summaries(runs):
    result = {}
    for run in runs:
        cases = result[run['label']] = {}
        for name in ('Code2K', 'Prose2K'):
            rows = [r for r in run['rows'] if r['case'] == name and not r['warmup']]
            median = lambda scope: statistics.median(r['scopes'][scope]['milliseconds'] for r in rows)
            row = cases[name] = dict(decode_ms=statistics.median(r['decode_ms'] for r in rows),
                verify_ms=median('verify.run (submission + GPU wait)'), draft_ms=median('mtp.draft'),
                pending_ms=median('cache.apply_pending'), join_ms=median('cache.adapt_join'))
            if run['label'] == 'graph':
                row['verify_elapsed_pct'] = 100 * row['verify_ms'] / row['decode_ms']
                row['draft_elapsed_pct'] = 100 * row['draft_ms'] / row['decode_ms']
            else:
                row['primary_scope_ms'] = {scope: median(scope) for scope in PRIMARY}
                row['combined_hc_ms'] = statistics.median(sum(r['scopes'][scope]['milliseconds']
                    for scope in ('verify.submit.pre.hc0', 'verify.submit.pre.hc1')) for r in rows)
    return result


def check(d):
    assert d['complete'] and not d['production_changed'] and not d['target_achieved']
    assert not d['performance_acceptance'] and not d['eager_shares_apply_to_graph']
    assert d['minimum_gain_pct'] == 1 and d['maximum_baseline_drift_pct'] == 3
    assert [r['label'] for r in d['runs']] == ['graph', 'eager-phases']
    schedule = [(name, warm) for warm in (True, False, False) for name in ('Code2K', 'Prose2K')]
    for run in d['runs']:
        assert run['exit_code'] == 0 and run['expert_slots'] == 6297 and run['free_mib'] >= 1500
        assert [(r['case'], r['warmup']) for r in run['rows']] == schedule
        for row in run['rows']:
            assert row['generated'] == 128 and row['prompt_tokens'] == 2048 and row['reused'] == 0
            assert row['finish'] == 'length' and math.isfinite(row['decode_ms']) and row['decode_ms'] > 0
            assert math.isfinite(row['prompt_ms']) and row['prompt_ms'] > 0
            assert 0 <= row['accepted'] <= row['offered']
            assert row['output_sha256'] == d['reference_sha256'][row['case']]
            scopes = row['scopes']
            for value in scopes.values():
                assert math.isfinite(value['milliseconds']) and value['milliseconds'] >= 0
                assert isinstance(value['calls'], int) and value['calls'] > 0
            calls = scopes['verify.run (submission + GPU wait)']['calls']
            assert calls == scopes['mtp.draft']['calls'] == scopes['cache.apply_pending']['calls']
            if run['label'] == 'eager-phases':
                for scope in PRIMARY:
                    assert scopes[scope]['calls'] == calls * (1 if scope in ('verify.submit.input', 'verify.submit.head') else 48)
    assert d['summaries'] == summaries(d['runs'])
    assert len(d['clock_checks']) == 5
    assert [v['requested_ms'] for v in d['clock_checks']] == [20, 80, 120, 250, 1050]
    assert all(v['ezpp_ms'] > 0 and abs(v['ezpp_ms'] - v['steady_ms']) <= 5 for v in d['clock_checks'])


def main():
    d = json.loads((H / (STEM + '.json')).read_text(encoding='utf-8'))
    assert d['checker_sha256'] == sha(Path(__file__))
    assert d['report_sha256'] == sha(H / (STEM + '.md'))
    check(d)
    if '--self-check' in sys.argv:
        bad = copy.deepcopy(d)
        bad['runs'][1]['rows'][2]['scopes']['verify.submit.post.experts']['calls'] -= 1
        try:
            check(bad)
        except AssertionError:
            pass
        else:
            raise AssertionError('missing layer accounting accepted')
        bad = copy.deepcopy(d)
        bad['runs'][0]['rows'][2]['decode_ms'] = float('nan')
        try:
            check(bad)
        except AssertionError:
            pass
        else:
            raise AssertionError('non-finite timing accepted')
    print('PASS12 actual2K/full128 output fingerprints,48-layer scope counts,clock checks and medians; no speed acceptance')


if __name__ == '__main__':
    main()
