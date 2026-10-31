"""Recompute the production 2K baseline; --local also checks frozen evidence."""
import hashlib
import json
from pathlib import Path
import statistics
import sys

H = Path(__file__).resolve().parent
R = H.parents[2]


def audit(local=False):
    d = json.loads((H / 'decode2k-baseline-2026-10-25.json').read_text(encoding='utf-8'))
    assert d['complete'] and not d['target_achieved'] and not d['gpu_stage_profile_available']
    assert len(d['requests']) == 8
    for row in d['requests']:
        assert row['input_tokens'] == 2048 and row['output_tokens'] == 128 and row['reused'] == 0
        assert 0 <= row['accepted'] <= row['offered'] and row['decode_ms'] > 0
        assert row['decode_tps'] == 128000 / row['decode_ms']
    for case in ('Code2K', 'Prose2K'):
        rows = [v for v in d['requests'] if v['case'] == case and not v['warmup']]
        assert len(rows) == 3 and len({v['output_sha256'] for v in rows}) == 1
        rates = [v['decode_tps'] for v in rows]
        s = d['summary'][case]
        assert s['median_tps'] == statistics.median(rates)
        assert s['range_tps'] == [min(rates), max(rates)]
        assert s['required_gain_pct'] == 100 * (40 / s['median_tps'] - 1)
    if local:
        for name, digest in d['local_files_sha256'].items():
            assert hashlib.sha256((R / name).read_bytes()).hexdigest() == digest, name
        p = R / 'build-sycl-a770-decode2k-baseline-20261025'
        raw = json.loads((p / 'results.json').read_text(encoding='utf-8'))
        for actual, reported in zip(raw['runs'][0]['rows'], d['requests']):
            assert actual['case'] == reported['case'] and actual['warmup'] == reported['warmup']
            assert actual['decode_ms'] == reported['decode_ms'] and actual['prompt_ms'] == reported['prompt_ms']
            assert hashlib.sha256(json.dumps(actual['tokens'], separators=(',', ':')).encode()).hexdigest() == reported['output_sha256']
        assert 'stage waits disabled on Windows EAGER' in (p / 'profile.stderr').read_text(encoding='utf-8')
    print('PASS production Code2K/Prose2K: six complete retained requests, medians, ranges, output hashes and target gap')


if __name__ == '__main__':
    audit('--local' in sys.argv)
