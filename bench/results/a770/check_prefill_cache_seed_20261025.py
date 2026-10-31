"""Offline pre-decode LFU seed-cap functional accounting; no throughput acceptance."""
import hashlib
import importlib.util
import json
from pathlib import Path
import sys

H = Path(__file__).resolve().parent
R = H.parents[2]
STEM = 'prefill-cache-seed-2026-10-25'


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def check(d):
    assert d['complete'] and not d['adopted'] and not d['target_achieved'] and not d['production_changed']
    assert not d['performance_measured']
    assert [r['label'] for r in d['runs']] == ['A', 'B']
    for run in d['runs']:
        assert run['exit_code'] == 0 and run['expert_slots'] == 6297
        assert len(run['rows']) == 4 and run['free_mib'] >= 1500
        assert [r['case'] for r in run['rows']] == ['Code2K', 'Prose2K'] * 2
        for row in run['rows']:
            assert row['generated'] == 128 and row['prompt_tokens'] == 2048 and row['reused'] == 0
            assert row['finish'] == 'length' and row['decode_ms'] > 0 and 0 <= row['accepted'] <= row['offered']
            assert row['output_sha256'] == d['reference_output_sha256'][row['case']]
            seed = row['cache_seed']
            assert seed['cap'] == (4 if run['label'] == 'A' else 64)
            assert 0 <= seed['batches'] <= seed['cap'] and 0 <= seed['swaps'] <= 32 * seed['batches']
            assert seed['tail_total'] == 122880
    assert d['candidate_did_extra_batches'] == any(row['cache_seed']['batches'] > 4 for row in d['runs'][1]['rows'])


def main():
    d = json.loads((H / (STEM + '.json')).read_text(encoding='utf-8'))
    assert sha(Path(__file__)) == d['checker_sha256'] and sha(H / (STEM + '.md')) == d['report_sha256']
    check(d)
    if '--local' in sys.argv:
        folder = R / d['source_directory']
        path = folder / 'checks/results.json'
        assert sha(path) == d['source_results_sha256']
        spec = importlib.util.spec_from_file_location('seed_cap_raw', folder / 'check.py')
        m = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(m)
        m.audit(json.loads(path.read_text(encoding='utf-8')))
    print('PASS cache seed cap4/64:8 full2K/128 unchanged outputs and all actual preparation counts; no speed claim')


if __name__ == '__main__':
    main()
