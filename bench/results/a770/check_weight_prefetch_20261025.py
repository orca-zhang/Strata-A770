"""Check the published native-prefetch screen; --local also checks retained build artifacts."""
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
    data = json.loads((H / 'weight-prefetch-2026-10-25.json').read_text(encoding='utf-8'))
    assert sha(Path(__file__)) == data['checker_sha256']
    assert sha(H / 'weight-prefetch-2026-10-25.md') == data['report_sha256']
    d = data['experiment']
    assert d['complete'] and not d['adopted'] and d['scope'] == 'operator-screen'
    assert d['native_prefetch_load_messages'] == 12 and d['baseline_native_prefetch_load_messages'] == 0
    assert all(code == 0 for code in d['exit_codes'].values())
    assert [r['label'] for r in d['runs']] == ['A1', 'B1', 'A2', 'B2', 'A3']
    for run in d['runs']:
        assert run['exit_code'] == 0 and [r['T'] for r in run['rows']] == [1, 4, 6]
        assert [r['checks'] for r in run['rows']] == [24, 48, 72]
        assert len(run['samples']) == 3
        for row, samples in zip(run['rows'], run['samples']):
            assert row['T'] == samples['T'] and len(samples['us']) == 11
            assert all(math.isfinite(v) and v > 0 for v in samples['us'])
            assert abs(row['us'] - statistics.median(samples['us'])) <= 0.000000501
    for index in (1, 3):
        a, b, c = d['runs'][index - 1:index + 2]
        for ra, rb, rc in zip(a['rows'], b['rows'], c['rows']):
            baseline = math.sqrt(ra['us'] * rc['us'])
            gain = 100 * (baseline / rb['us'] - 1)
            drift = 100 * (rc['us'] / ra['us'] - 1)
            eligible = abs(drift) <= 3
            assert d['comparison'][b['label']][str(rb['T'])] == dict(
                baseline=baseline, candidate=rb['us'], throughput_change_pct=gain,
                baseline_drift_pct=drift, drift_eligible=eligible,
                screen_pass=eligible and gain >= 3)
    assert not any(p['screen_pass'] for row in d['comparison'].values() for p in row.values())
    if '--local' in sys.argv:
        assert sha(R / data['source_results']) == data['source_results_sha256']
        for name, digest in d['files_sha256'].items():
            assert sha(R / name) == digest, name
        folder = (R / data['source_results']).parent
        files = list((folder / 'disasm').glob('*gu_pair_wg128*.asm'))
        assert len(files) == 1
        asm = files[0].read_text(encoding='utf-8')
        ends = [m.end() for m in re.finditer(r'^.*\{EOT.*$', asm, re.M)]
        assert len(ends) == 1
        loads = [line for line in asm[:ends[0]].splitlines()
                 if re.search(r'\bsend\.ugm\s+\([^)]*\)\s+null\b', line) and 'load.' in line]
        assert len(loads) == d['native_prefetch_load_messages']
        for run in d['runs']:
            text = (folder / (run['label'] + '.stdout')).read_text(encoding='utf-8')
            assert [json.loads(x) for x in re.findall(r'^RESULT (.*)$', text, re.M)] == run['rows']
            assert [json.loads(x) for x in re.findall(r'^SAMPLES (.*)$', text, re.M)] == run['samples']
            assert text.rstrip().endswith('PASS')
    print('PASS 360 full-chain checks, all 165 batches, 12 native prefetch messages, paired drift/gains; no adopted gain')


if __name__ == '__main__':
    main()
