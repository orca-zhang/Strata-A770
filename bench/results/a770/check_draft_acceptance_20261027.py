"""Recompute the recorded acceptance ratios and paired decode timings without a GPU."""
import json
import hashlib
import math
from pathlib import Path
import statistics
import sys


def rate(rows, prefix=''):
    accepted = sum(r[prefix + 'accepted'] for r in rows)
    offered = sum(r[prefix + 'offered'] for r in rows)
    return accepted / offered if offered else None


def check(d):
    assert d['complete'] and d['selected_floor'] > .5
    assert {c['name']: len(c['rows']) for c in d['cohorts']} == dict(screen=24, holdout=24, confirmation=63, production=10)
    assert d['request_count'] == sum(len(c['rows']) for c in d['cohorts']) == 121
    assert d['output_token_count'] == sum(r['generated'] for c in d['cohorts'] for r in c['rows'])
    references = {}
    for cohort in d['cohorts']:
        rows = cohort['rows']
        assert rows and cohort['complete']
        for row in rows:
            assert 0 <= row['accepted'] <= row['offered']
            assert row['generated'] > 1 and row['reused'] == 0
            assert all(math.isfinite(row[k]) and row[k] > 0 for k in ('decode_ms', 'prompt_ms'))
            references.setdefault(row['case'], row['output_sha256'])
            assert row['matches_reference'] == (row['output_sha256'] == references[row['case']])
            assert row['matches_reference']
        for arm in cohort['arms']:
            arm_rows = [r for r in rows if r['arm'] == arm['arm']]
            assert arm['accepted'] == sum(r['accepted'] for r in arm_rows)
            assert arm['offered'] == sum(r['offered'] for r in arm_rows)
            assert math.isclose(arm['acceptance'], rate(arm_rows))
    confirmation = next(c for c in d['cohorts'] if c['name'] == 'confirmation')
    rows = confirmation['rows']
    assert {r['arm'] for r in rows} == {'A1', 'B', 'A2'}
    for row in rows:
        assert row['accepted'] == row['mtp_accepted'] + row['suffix_accepted']
        assert row['offered'] == row['mtp_offered'] + row['suffix_offered']
        assert 0 <= row['mtp_accepted'] <= row['mtp_offered']
    candidate = [r for r in rows if r['arm'] == 'B' and not r['warmup']]
    assert candidate and all(r['floor'] == d['selected_floor'] for r in candidate)
    assert rate(candidate, 'mtp_') > .8
    production = next(c for c in d['cohorts'] if c['name'] == 'production')['rows']
    references = {r['case']: r['output_sha256'] for c in d['cohorts'][:2] for r in c['rows']}
    assert len(production) == len(references) == 10
    assert d['deployment']['config_applied']
    assert math.isclose(d['deployment']['acceptance'], rate(production, 'mtp_'))
    assert d['deployment']['accepted'] == sum(r['mtp_accepted'] for r in production)
    assert d['deployment']['offered'] == sum(r['mtp_offered'] for r in production)
    for row in production:
        assert row['output_sha256'] == references[row['case']]
        assert row['accepted'] == row['mtp_accepted'] + row['suffix_accepted']
        assert row['offered'] == row['mtp_offered'] + row['suffix_offered']
        assert row['mtp_offered'] > 0 and row['mtp_accepted'] / row['mtp_offered'] > .8
    for comparison in d['comparisons']:
        case = comparison['case']
        samples = [[r for r in rows if r['arm'] == arm and r['case'] == case and not r['warmup']]
                   for arm in ('A1', 'B', 'A2')]
        assert all(len(s) == 2 for s in samples)
        a, b, c = [statistics.median(r['decode_ms'] / r['generated'] for r in s) for s in samples]
        assert math.isclose(comparison['throughput_change_pct'], 100 * (math.sqrt(a*c) / b - 1))
        assert math.isclose(comparison['baseline_drift_pct'], 100 * (c/a - 1))
    print(f"PASS recorded outputs, MTP/lookup counters, weighted acceptance {rate(candidate, 'mtp_'):.2%}, paired timings")


if __name__ == '__main__':
    data = json.loads(Path(__file__).with_name('draft-acceptance-2026-10-27.json').read_text(encoding='utf-8'))
    check(data)
    if '--local' in sys.argv:
        root = Path(__file__).resolve().parents[3]
        for cohort in data['cohorts']:
            for name, digest in cohort['files_sha256'].items():
                assert hashlib.sha256((root / name).read_bytes()).hexdigest() == digest, name
            raw = json.loads((root / cohort['local_results']).read_text(encoding='utf-8'))
            assert len(raw['rows']) == len(cohort['rows'])
            for row, public in zip(raw['rows'], cohort['rows']):
                encoded = json.dumps(row['tokens'], separators=(',', ':')).encode()
                assert hashlib.sha256(encoded).hexdigest() == public['output_sha256']
                for key in ('case', 'floor', 'generated', 'accepted', 'offered', 'decode_ms', 'prompt_ms'):
                    assert row[key] == public[key], (cohort['name'], key)
        print('PASS local raw file hashes, token hashes and all published counts/timings')
