"""Check the preserved cache-only baseline failure without accepting its timings."""
import hashlib
import json
from pathlib import Path
import re
import sys

R = Path(__file__).resolve().parents[3]
H = R / 'build-sycl-a770-hc-cache-capacity-20261027'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def check():
    d = json.loads((H / 'partial.json').read_text(encoding='utf-8'))
    terminal = json.loads((H / 'runner-exit.json').read_text(encoding='utf-8'))
    assert terminal['runner_exit_code'] == terminal['engine_cleanup_exit_code'] == 1
    assert not terminal['engine_crash_cause_established'] and not terminal['candidate_started']
    assert not terminal['production_changed'] and not terminal['external_processes_terminated']
    assert (terminal['large_requests'], terminal['matching_large_requests'], terminal['all_zero_large_requests']) == (2, 1, 1)
    assert sha(H / 'partial.json') == terminal['partial_sha256']
    assert sha(H / 'run.py') == terminal['source_sha256'] == d['files_sha256'][(H / 'run.py').relative_to(R).as_posix()]
    assert not d['complete'] and not d['runs'] and len(d['smoke']) == 1 and not (H / 'results.json').exists()
    assert not d['smoke_dispatch_passed'] and not d['production_changed'] and not d['adopted']
    assert d['args'][d['args'].index('--expert-cache') + 1] == '5500'
    run = d['smoke'][0]
    assert run['label'] == 'A' and run['enabled'] is False and run['large_cache'] is False
    assert run['expert_slots'] == terminal['expert_slots'] == 5771 and run['free_mib'] == terminal['free_mib'] >= 1500
    assert run['exit_code'] == 1 and run['args'] == d['args']
    assert [row['case'] for row in run['rows']] == ['Code2K', 'Prose2K']
    assert run['initializer']['tokens'] == d['reference']['Short']
    assert run['rows'][0]['tokens'] == d['reference']['Code2K']
    assert not any(run['rows'][1]['tokens']) and run['rows'][1]['tokens'] != d['reference']['Prose2K']
    for row in run['rows']:
        assert row['generated'] == len(row['tokens']) == 128 and row['prompt_tokens'] == 2048
        assert row['reused'] == 0 and row['finish'] == 'length'
    outputs, tokens = [], []
    for line in (H / 'smoke-A.protocol').read_text(encoding='utf-8').splitlines():
        if line.startswith(('T ', 'TB ')):
            tokens.extend(map(int, line.split()[1:] if line.startswith('T ') else line.split()[2:]))
        elif line.startswith('DONE '):
            assert len(tokens) == int(line.split()[1])
            outputs.append((line, tokens))
            tokens = []
    assert not tokens and outputs == [(row['done'], row['tokens']) for row in [run['initializer'], *run['rows']]]
    dispatch = re.findall(r'^MODEL_GU_Q8 cap=(\d+) active=([01])$', (H / 'smoke-A.stderr').read_text(encoding='utf-8'), re.M)
    assert dispatch and all(active == '0' for _, active in dispatch)
    if '--local' in sys.argv:
        sys.path.insert(0, str(R / 'tools'))
        import bench_deepinfer as b
        with b.gpu_lock():
            for name, digest in d['files_sha256'].items():
                assert sha(R / name) == digest, name
    print('PASS preserved baseline failure, full protocol, disabled fusion, 1 match/1 zero; no cache gain or regression result')


if __name__ == '__main__':
    check()
