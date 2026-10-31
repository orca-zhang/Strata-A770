"""Recompute the memory screens; --local also verifies retained artifacts."""
import hashlib
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
    assert d['complete'] and not d['adopted'] and d['scope'] == 'operator-screen'
    assert all(code == 0 for code in d['exit_codes'].values())
    timed = d['name'] in ('iq2s-align84', 'host-readonly')
    if d['name'] == 'host-wc':
        timed = d['wc_mapping_observed']
    elif d['name'] == 'weight-uncached':
        assert d['baseline_native_uncached_load_messages'] == 0
        timed = d['native_uncached_load_messages'] > 0
    if not timed:
        assert not d['runs'] and not d['passes_screen'] and 'comparison' not in d
        return
    assert [r['label'] for r in d['runs']] == ['A1', 'B1', 'A2', 'B2', 'A3']
    for run in d['runs']:
        assert run['exit_code'] == 0 and [r['T'] for r in run['rows']] == [1, 4, 6]
        assert [r['checks'] for r in run['rows']] == [24, 48, 72]
        assert len(run['samples']) == 3
        for row, samples in zip(run['rows'], run['samples']):
            assert row['T'] == samples['T'] and len(samples['us']) == 11
            assert all(math.isfinite(v) and v > 0 for v in samples['us'])
            assert abs(row['us'] - statistics.median(samples['us'])) <= 0.000000501
    comparison = {}
    for index in (1, 3):
        a, b, c = d['runs'][index - 1:index + 2]
        comparison[b['label']] = {}
        for ra, rb, rc in zip(a['rows'], b['rows'], c['rows']):
            baseline = math.sqrt(ra['us'] * rc['us'])
            gain = 100 * (baseline / rb['us'] - 1)
            drift = 100 * (rc['us'] / ra['us'] - 1)
            eligible = abs(drift) <= 3
            comparison[b['label']][str(rb['T'])] = dict(
                baseline=baseline, candidate=rb['us'], throughput_change_pct=gain,
                baseline_drift_pct=drift, drift_eligible=eligible,
                screen_pass=eligible and gain >= 3)
    assert d['comparison'] == comparison
    assert d['passes_screen'] == all(p['screen_pass'] for row in comparison.values() for p in row.values())


def main():
    data = json.loads((H / 'weight-memory-2026-10-25.json').read_text(encoding='utf-8'))
    assert data['complete'] and not data['production_changed'] and not data['whole_model_gain_claimed']
    assert sha(Path(__file__)) == data['checker_sha256']
    assert sha(H / 'weight-memory-2026-10-25.md') == data['report_sha256']
    assert [d['name'] for d in data['experiments']] == ['iq2s-align84', 'host-wc', 'weight-uncached', 'host-readonly']
    for d in data['experiments']:
        check(d)
        if '--local' in sys.argv:
            assert sha(R / d['source_results']) == d['source_results_sha256']
            for name, digest in d['files_sha256'].items():
                assert sha(R / name) == digest, name
    print('PASS all memory-screen gates, retained batches, paired gains/drift and publication hashes')


if __name__ == '__main__':
    main()
