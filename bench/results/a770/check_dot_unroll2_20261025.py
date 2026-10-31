"""Recompute the numeric unroll screen; --local verifies retained raw artifacts."""
import hashlib
import json
import math
from pathlib import Path
import re
import statistics
import sys

H = Path(__file__).resolve().parent
R = H.parents[2]


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    d = json.loads((H / 'dot-unroll2-2026-10-25.json').read_text(encoding='utf-8'))
    assert d['complete'] and not d['adopted'] and not d['whole_model_gain_claimed']
    assert sha(Path(__file__)) == d['checker_sha256']
    assert sha(H / 'dot-unroll2-2026-10-25.md') == d['report_sha256']
    assert [r['label'] for r in d['runs']] == ['A1', 'B1', 'A2', 'B2', 'A3']
    assert d['full_output_guard_checks'] == 360 and d['reference_capture_checks'] == 72
    assert d['bitwise_output_comparisons'] == 288 and d['graph_output_checks'] == 15
    assert d['retained_batches'] == 165
    for run in d['runs']:
        assert run['exit_code'] == 0 and len(run['samples']) == 3
        assert [r['T'] for r in run['rows']] == [1, 4, 6]
        assert [r['checks'] for r in run['rows']] == [24, 48, 72]
        for row, samples in zip(run['rows'], run['samples']):
            assert row['T'] == samples['T'] and len(samples['us']) == 11
            assert all(math.isfinite(v) and v > 0 for v in samples['us'])
            assert abs(statistics.median(samples['us']) - row['us']) <= 0.000000501
    comparison = {}
    for i in (1, 3):
        before, candidate, after = d['runs'][i-1:i+2]
        comparison[candidate['label']] = {}
        for a, b, c in zip(before['rows'], candidate['rows'], after['rows']):
            baseline = math.sqrt(a['us'] * c['us'])
            gain, drift = 100 * (baseline / b['us'] - 1), 100 * (c['us'] / a['us'] - 1)
            comparison[candidate['label']][str(b['T'])] = dict(baseline=baseline, candidate=b['us'],
                throughput_change_pct=gain, baseline_drift_pct=drift,
                eligible=abs(drift) <= 3, **{'pass': abs(drift) <= 3 and gain >= 3})
    assert d['comparison'] == comparison
    advance = any(all(comparison[label][str(t)]['pass'] for label in ('B1', 'B2')) for t in (1, 4, 6))
    advance = advance and all(row['eligible'] and row['throughput_change_pct'] >= -1
                              for group in comparison.values() for row in group.values())
    assert d['advance_eligible'] == advance
    if '--local' in sys.argv:
        path = R / d['source_results']
        assert sha(path) == d['source_results_sha256']
        assert sha(path.parent / 'upstream-snapshot-fixed.json') == d['upstream_snapshot_sha256']
        raw = json.loads(path.read_text(encoding='utf-8'))
        assert raw['runs'] == d['runs'] and raw['comparison'] == comparison
        for name, digest in raw['files_sha256'].items():
            assert sha(R / name) == digest, name
        for run in d['runs']:
            text = (path.parent / (run['label'] + '.stdout')).read_text(encoding='utf-8')
            assert text.rstrip().endswith('PASS')
            assert [json.loads(x) for x in re.findall(r'^RESULT (.*)$', text, re.M)] == run['rows']
            assert [json.loads(x) for x in re.findall(r'^SAMPLES (.*)$', text, re.M)] == run['samples']
    print('PASS 360 full-chain checks, all 165 batches and paired controls; advance eligible:', advance)


if __name__ == '__main__':
    main()
