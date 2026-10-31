"""Recompute the complete expert-chain staging screen without a GPU."""
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import statistics
import sys

H = Path(__file__).resolve().parent
R = H.parents[2]
STEM = 'expert-stage-2026-10-25'


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def check(d):
    assert d['complete'] and not d['adopted'] and not d['target_achieved'] and not d['production_changed']
    assert [r['label'] for r in d['runs']] == ['A1', 'B1', 'A2', 'B2', 'A3']
    for run in d['runs']:
        assert run['exit_code'] == 0 and [r['T'] for r in run['rows']] == [1, 4, 6]
        assert [r['checks'] for r in run['rows']] == [24, 48, 72] and len(run['samples']) == 3
        for row, sample in zip(run['rows'], run['samples']):
            assert row['T'] == sample['T'] and len(sample['us']) == 11
            assert all(math.isfinite(v) and v > 0 for v in sample['us'])
            assert abs(row['us'] - statistics.median(sample['us'])) <= 0.000000501
    for i in (1, 3):
        a, b, z = d['runs'][i-1:i+2]
        for ra, rb, rz in zip(a['rows'], b['rows'], z['rows']):
            base = math.sqrt(ra['us'] * rz['us'])
            assert d['comparison'][b['label']][str(rb['T'])] == dict(baseline=base, candidate=rb['us'],
                throughput_change_pct=100*(base/rb['us']-1), baseline_drift_pct=100*(rz['us']/ra['us']-1))
    assert d['passes_screen'] == all(p['throughput_change_pct'] >= 3 and abs(p['baseline_drift_pct']) <= 3
        for rows in d['comparison'].values() for p in rows.values())


def main():
    d = json.loads((H / (STEM + '.json')).read_text(encoding='utf-8'))
    assert sha(Path(__file__)) == d['checker_sha256'] and sha(H / (STEM + '.md')) == d['report_sha256']
    check(d)
    if '--local' in sys.argv:
        folder = R / d['source_directory']
        assert sha(folder / 'results.json') == d['source_results_sha256']
        spec = importlib.util.spec_from_file_location('stage_raw', folder / 'run.py')
        m = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(m)
        m.audit(json.loads((folder / 'results.json').read_text(encoding='utf-8')))
    print('PASS360 output/guard checks,15 graph checks,165 timing batches and all6 comparisons; no adopted gain')


if __name__ == '__main__':
    main()
