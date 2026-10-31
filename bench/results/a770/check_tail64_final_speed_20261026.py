"""Audit the final-speed attempt; --local checks raw outputs, timings and provenance."""
import hashlib
import json
import math
from pathlib import Path
import sys

H = Path(__file__).resolve().parent
R = H.parents[2]


def digest(tokens):
    return hashlib.sha256(json.dumps(tokens, separators=(',', ':')).encode()).hexdigest()


def main():
    d = json.loads((H / 'tail64-final-speed-2026-10-26.json').read_text(encoding='utf-8'))
    assert d['status'] == 'correctness_failed' and d['diagnosis_complete']
    assert not d['speed_measurement_complete'] and not d['tail64_gain_confirmed']
    assert d['speed_gain_pct'] is None and not d['target_achieved'] and not d['production_changed']
    assert d['planned_requests_2k'] == 32 and d['completed_requests_2k'] == 7
    assert d['short_requests'] == 4 and d['output_tokens_compared'] == 960
    assert [r['label'] for r in d['runs']] == ['SCREEN_A1', 'AUTO64', 'FIXED256', 'FIXED64']
    assert [r['flag'] for r in d['runs']] == ['0', '1', '0', '1']
    assert [r['expert_slots'] for r in d['runs']] == [6297, 6801, 6297, 6297]
    assert sum(row['generated'] for run in d['runs'] for row in run['rows']) == 960
    failed = []
    for run in d['runs']:
        assert run['exit_code'] == 0
        for row in run['rows']:
            short = row['case'] == 'Short'
            assert row['generated'] == (16 if short else 128) and row['prompt_tokens'] == (23 if short else 2048)
            assert row['finish'] == 'length' and row['reused'] == 0
            assert row['reference_matches'] == (row['output_sha256'] == d['reference_output_sha256'][row['case']])
            assert row['reference_matches'] == (row['valid_decode_tps'] is not None)
            if row['reference_matches']:
                assert math.isclose(row['valid_decode_tps'], 1000 * row['generated'] / row['decode_ms'])
            else:
                assert not short and row['zero_tokens'] == 128 and row['output_sha256'] == digest([0] * 128)
                failed.append((run['label'], row['case']))
    assert failed == [('SCREEN_A1', 'Code2K'), ('AUTO64', 'Prose2K'), ('FIXED256', 'Code2K'), ('FIXED64', 'Prose2K')]
    assert d['failed_requests_2k'] == len(failed) == 4
    if '--local' in sys.argv:
        for path, expected in d['files_sha256'].items():
            assert hashlib.sha256((R / path).read_bytes()).hexdigest() == expected, path
        original = json.loads((R / 'build-sycl-a770-tail64-final-speed-v2-20261026/partial.json').read_text(encoding='utf-8'))
        diag = json.loads((R / 'build-sycl-a770-tail64-speed-diagnostic-20261026/results.json').read_text(encoding='utf-8'))
        raw_runs = [dict(rows=[original['runs'][0]['initializer'], *original['runs'][0]['rows']]), *diag['runs']]
        for run, raw in zip(d['runs'], raw_runs):
            for row, source in zip(run['rows'], raw['rows']):
                assert row['output_sha256'] == digest(source['tokens']) and row['zero_tokens'] == source['tokens'].count(0)
                assert all(row[k] == source[k] for k in ('generated', 'prompt_tokens', 'prompt_ms', 'decode_ms', 'finish', 'accepted', 'offered', 'reused', 'ttft_ms'))
    print('PASS evidence audit: 7 full2K requests, 4 all-zero failures; no speed-gain claim')


if __name__ == '__main__':
    main()
