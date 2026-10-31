"""Audit the independent INT8 benefit replication; --local also checks frozen files/logs."""
from pathlib import Path
import datetime
import hashlib
import importlib.util
import json
import math
import re
import sys

H = Path(__file__).resolve().parent
R = H.parents[2]
spec = importlib.util.spec_from_file_location('layout_audit', H/'check_int8_layout_20261025.py')
f = importlib.util.module_from_spec(spec)
spec.loader.exec_module(f)
LABELS = {16: 'IQ2_XXS', 22: 'IQ2_S', 42: 'Q2_0'}
TRIAL = R/'build-sycl-a770-int8-benefit-verify-20261025'


def cells(cohort):
    return {(r['gu'], g['T'], g['mode']): g for r in cohort['runs'] for g in r['groups']}


def table_rows(d, old):
    previous = cells(old['production_queue']['cohorts'][-1])
    repeats = [cells(c) for c in d['cohorts']]
    rows = []
    for key in d['original_confirmed']:
        gt, t, mode = key
        a, b = [c[tuple(key)] for c in repeats]
        gains = [previous[tuple(key)]['median_pct'], a['median_pct'], b['median_pct']]
        values = [f'{v:+.3f}%' if v is not None else 'NA' for v in gains]
        rows.append(f"| {LABELS[gt]} | {t} | {'col' if mode == 1 else 'VNNI'} | "
                    + ' | '.join(values)
                    + f" | {a['stable_cycles']}/8, {b['stable_cycles']}/8 | "
                    + ('PASS' if key in d['reconfirmed'] else 'FAIL') + ' |')
    return rows


def audit(local=False):
    d = json.loads((H/'int8-benefit-verify-2026-10-25.json').read_text(encoding='utf-8'))
    old_path = H/'int8-layout-2026-10-25.json'
    old = json.loads(old_path.read_text(encoding='utf-8'))
    assert d['complete'] and not d['adopted']
    assert d['original_confirmed'] == old['production_queue']['confirmed']
    assert len(d['original_confirmed']) == 21
    assert d['orders'] == [[22,16,42], [42,16,22]] and len(d['cohorts']) == 2
    assert hashlib.sha256(old_path.read_bytes()).hexdigest() == d['files_sha256'][old_path.relative_to(R).as_posix()]
    expected = {(t,m) for t in (1,6,8,16,40,64,128,256) for m in (1,2)}
    comparisons = cycles = checks = 0
    started = datetime.datetime.fromisoformat(d['created_utc'])
    for order, cohort in zip(d['orders'], d['cohorts']):
        assert [r['gu'] for r in cohort['runs']] == order
        for run in cohort['runs']:
            assert run['default_profiling'] == run['trial_profiling'] == 1 and run['priority_class'] == 32
            assert started < datetime.datetime.fromisoformat(run['started_utc']) < datetime.datetime.fromisoformat(run['finished_utc'])
            assert len(run['groups']) == 16 and {(g['T'],g['mode']) for g in run['groups']} == expected
            assert run['summary']['checks'] > 1000
            checks += run['summary']['checks']
            for g in run['groups']:
                assert [v['cycle'] for v in g['pairs']] == list(range(8))
                assert len(g['errors']) == 2 and {e['profile'] for e in g['errors']} == {0,1}
                for row in g['pairs'] + g['errors']:
                    assert (row['gu'],row['T'],row['mode']) == (run['gu'],g['T'],g['mode'])
                for v in g['pairs']:
                    assert all(math.isfinite(v[k]) and v[k] > 0 for k in ('a0_us','a1_us','b0_us','b1_us'))
                    assert v['gain_pct'] == 100*(math.sqrt(v['a0_us']*v['a1_us'])/math.sqrt(v['b0_us']*v['b1_us'])-1)
                    assert v['a_drift_pct'] == 100*(v['a1_us']/v['a0_us']-1)
                    assert v['b_drift_pct'] == 100*(v['b1_us']/v['b0_us']-1)
                assert all(g[k] == v for k,v in f.compare(g['pairs']).items())
                quality = all(e['nrmse'] <= (.1 if e['profile'] else .05) and
                              e['cosine'] >= (.98 if e['profile'] else .995) for e in g['errors'])
                assert quality == g['quality_passed']
                assert g['passed'] == (quality and g['stable_cycles'] >= 6 and g['median_pct'] >= 3)
                comparisons += 2
                cycles += 8
            if local:
                raw = (TRIAL/cohort['phase']/run['case']).with_suffix('.stdout').read_text()
                assert 'QUEUE default_profiling=1 trial_profiling=1' in raw and 'A770' in raw
                pairs = [json.loads(v[5:]) for v in raw.splitlines() if v.startswith('PAIR ')]
                errors = [json.loads(v[6:]) for v in raw.splitlines() if v.startswith('ERROR ')]
                actual = [v for g in run['groups'] for v in g['pairs']]
                assert len(pairs) == 128 and all(p == {k:v[k] for k in p} for p,v in zip(pairs,actual))
                assert errors == sorted([v for g in run['groups'] for v in g['errors']],
                                        key=lambda v:(v['profile'],v['T'],v['mode']))
                assert [json.loads(v[8:]) for v in raw.splitlines() if v.startswith('SUMMARY ')] == [run['summary']]
    passed = [{k for k,g in cells(c).items() if g['passed']} for c in d['cohorts']]
    assert d['reconfirmed'] == [v for v in d['original_confirmed'] if all(tuple(v) in s for s in passed)]
    assert d['dropped'] == [v for v in d['original_confirmed'] if v not in d['reconfirmed']]
    report = (H/'int8-benefit-verify-2026-10-25.md').read_text(encoding='utf-8')
    assert re.findall(r'^\| (?:IQ2_XXS|IQ2_S|Q2_0) .*$', report, re.M) == table_rows(d, old)
    if local:
        for name, digest in d['files_sha256'].items():
            assert hashlib.sha256((R/name).read_bytes()).hexdigest() == digest, name
        protocol = json.loads((TRIAL/'protocol.json').read_text())
        assert all(protocol[k] == d[k] for k in ('rule_before_measurement','original_confirmed','orders','created_utc'))
    print(f"PASS {len(d['reconfirmed'])}/21 reconfirmed; {comparisons} numerical records, {cycles} ABBA cycles, {checks} internal checks")
    print('DROPPED', d['dropped'])
    return d


if __name__ == '__main__':
    audit('--local' in sys.argv)
