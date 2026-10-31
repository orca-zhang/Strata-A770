"""Offline audit of the archived output-window experiment; no model or GPU."""
import hashlib
import json
import math
from pathlib import Path
import re

H = Path(__file__).resolve().parent


def read(path):
    return json.loads(path.read_text(encoding='utf-8'))


def sha(path):
    with path.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def main():
    for name, expected in read(H/'manifest.json').items():
        assert sha(H/name) == expected, name
    checks = read(H/'checks/results.json')
    assert checks['complete']
    for name, expected in checks['raw_sha256'].items():
        assert sha(H/'checks'/name) == expected, name
    assert len(checks['runs']) == 2 and len(checks['cli']) == 6 and len(checks['eos']) == 2
    for run in checks['runs']:
        assert 'expert_slots=4191' in run['runtime_info'] and 'expert_cache_mib=5760' in run['runtime_info']
        assert len(run['rows']) == 36
        for row in run['rows']:
            assert row['generated'] == len(row['tokens']) <= row['limit']
            if run['side'] == 'candidate':
                for pos, size in row['windows']:
                    assert 1 <= size <= row['limit'] - (pos - len(row['prompt']) + 1)
            prefix = row['name'].split('-cap')[0]
            if '-cap' in row['name']:
                assert row['tokens'] == checks['reference'][prefix][:row['limit']]
            if row['name'].endswith(('-head', '-parked', '-live')):
                full = checks['reference'][row['name'].split('-')[0]+'-False']
                start = 5 if row['name'].endswith('-parked') else 11 if row['name'].endswith('-live') else 0
                assert row['tokens'] == full[start:start+row['limit']]
                if start:
                    assert row['reused'] > 0
            if row['name'].endswith('-cancel'):
                assert row['finish'] == 'cancel'
            if row['name'].endswith('-recovery'):
                assert row['tokens'] == checks['reference'][row['name'].removesuffix('-recovery')][:6]
    for row in checks['cli']:
        assert row['returncode'] == 0 and len(row['tokens']) == row['cap']
        assert row['tokens'] == next(other['tokens'] for other in checks['cli'] if other['cap'] == row['cap'])
    for row in checks['eos']:
        assert row['finish'] == 'stop' and row['tokens'] == checks['reference']['0.5-False'][:1]
    assert len(checks['context']) == 2
    candidate = next(row for row in checks['context'] if row['side'] == 'candidate')
    assert candidate['returncode'] == 0
    assert candidate['tokens'] == next(row['tokens'] for row in checks['cli'] if row['cap'] == 6)
    measured = read(H/'measure/results.json')
    assert measured['complete']
    for name, expected in measured['raw_sha256'].items():
        assert sha(H/'measure'/name) == expected, name
    for phase in ('screen', 'confirm'):
        runs = measured[phase]
        if not runs:
            assert phase == 'confirm' and not measured['screen_passed']
            continue
        assert [row['side'] for row in runs] == ['baseline', 'candidate', 'baseline', 'candidate', 'baseline']
        for run in runs:
            assert len(run['warmup']) == 2
            for cap, rows in run['samples'].items():
                assert len(rows) == 3
                assert all(row['tokens'] == checks['reference']['0.5-False'][:int(cap)] and row['reused'] == 0 for row in rows)
                for metric, median in run['medians'][cap].items():
                    assert median == sorted(row[metric] for row in rows)[1]
        for pair in measured[phase+'_summary']['comparisons']:
            i, cap, metric = pair['candidate_run'], str(pair['cap']), pair['metric']
            a, b, c = [runs[k]['medians'][cap][metric] for k in (i-1, i, i+1)]
            assert math.isclose(pair['throughput_change_pct'], 100*(math.sqrt(a*c)/b-1), abs_tol=1e-10)
            assert math.isclose(pair['baseline_drift_pct'], 100*(c/a-1), abs_tol=1e-10)
            assert pair['eligible'] == (abs(pair['baseline_drift_pct']) <= 3)
    cpu = (H/'cpu-final.txt').read_text()
    assert re.search(r'PASS \d+ acceptance cases; \d+ oversized windows trimmed', cpu)
    frozen = read(H/'frozen.json')
    for name, expected in frozen['source_sha256'].items():
        assert sha(H/'source'/name) == expected
    assert read(H/'build.json')['complete']
    print('PASS source/log hashes, clipped outputs, restoration, sampling, CLI/EOS and all timing comparisons')


if __name__ == '__main__':
    main()
