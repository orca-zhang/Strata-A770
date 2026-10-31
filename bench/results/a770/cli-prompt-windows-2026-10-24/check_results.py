"""Offline audit of CLI prompt parity, raw measurements, source diff and frozen build hashes."""
import difflib
import hashlib
import json
import math
from pathlib import Path
import re

H = Path(__file__).resolve().parent


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    manifest = json.loads((H/'manifest.json').read_text())
    for name, digest in manifest['files_sha256'].items():
        assert sha(H/name) == digest, name
    data = json.loads((H/'results.json').read_text())
    measured = json.loads((H/'measurements.json').read_text())
    frozen = json.loads((H/'frozen.json').read_text())
    build = json.loads((H/'build.json').read_text())
    probe = json.loads((H/'diagnosis/results.json').read_text())
    assert data['complete'] and measured['complete'] and build['complete'] and probe['complete']
    assert data['exe_sha256'] == measured['exe_sha256']
    assert data['exe_sha256']['baseline'] == frozen['dependencies'][frozen['baseline_exe']]
    assert data['exe_sha256']['candidate'] == build['files_sha256']['candidate\\strata.exe']
    assert sha(H/'source/generate.before.cpp') == frozen['before']['sycl/src/program/generate.cpp']
    assert sha(H/'source/generate.after.cpp') == data['source_sha256'] == build['files_sha256']['candidate.cpp']
    before, after = [(H/'source'/name).read_text() for name in ('generate.before.cpp', 'generate.after.cpp')]
    patch = ''.join(difflib.unified_diff(before.splitlines(True), after.splitlines(True),
                                        fromfile='sycl/src/program/generate.cpp (before)', tofile='sycl/src/program/generate.cpp (after)'))
    header_before, header_after = [(H/'source'/name).read_text() for name in ('mtp.before.hpp', 'mtp.after.hpp')]
    assert header_after == header_before.replace('dpct::queue_ptr cs_ = &dpct::get_in_order_queue();', 'dpct::queue_ptr cs_ = nullptr;')
    assert sha(H/'source/mtp.before.hpp') == frozen['before']['sycl/include/strata/core/mtp.hpp']
    assert sha(H/'source/mtp.after.hpp') == build['files_sha256']['headers\\strata\\core\\mtp.hpp'] == data['mtp_header_sha256']
    patch += ''.join(difflib.unified_diff(header_before.splitlines(True), header_after.splitlines(True),
                                        fromfile='sycl/include/strata/core/mtp.hpp (before)', tofile='sycl/include/strata/core/mtp.hpp (after)'))
    assert patch == (H/'cli-prefill.patch').read_text()
    queue = json.loads((H/'queue-fix/reproducer.json').read_text())
    assert queue['returncode'] == 0xC0000409 and 'output  : 91' in (H/'queue-fix/baseline.stdout').read_text()
    assert queue['before_sha256'] == sha(H/'source/mtp.before.hpp') and queue['after_sha256'] == sha(H/'source/mtp.after.hpp')
    assert '&& !prompt_windows)' in after and 'n_batched == n_prompt - 1' in after
    assert 'ver.set_head_sampling(false);' in after and 'ver.wait_commit(err)' in after
    assert data['config'] == measured['config'] == json.loads((H/'config-original.json').read_text())
    assert sha(H/'config-original.json') == frozen['config_sha256']
    for name, digest in manifest['protected_sha256_after'].items():
        assert digest == frozen['dependencies'][name], name
    for name, digest in data['raw_sha256'].items():
        assert sha(H/'raw'/name) == digest, name
    for name, digest in measured['raw_sha256'].items():
        assert sha(H/'timing'/name) == digest, name
    for name, digest in probe['raw_sha256'].items():
        assert sha(H/'diagnosis'/name) == digest, name
    assert not probe['timed_out'] and probe['returncode'] == 0
    assert 'output  : 42' in (H/'diagnosis/baseline.stdout').read_text()
    assert (H/'raw/cpu.stdout').read_text().count('PASS 108 CLI entry checks') == 2
    assert (H/'raw/route.stdout').read_text().strip() == 'PASS 240 CLI prompt route checks'
    assert len(data['cli']) == 11 and len(data['serve']) == 2
    refs = {}
    for run in data['serve']:
        assert '5771' in run['runtime_info'] and '7922' in run['runtime_info']
        for row in run['requests']:
            assert 0 < row['generated'] <= 6 and row['finish'] in ('length', 'stop')
            assert refs.setdefault(row['name'], row['tokens']) == row['tokens']
    assert sum(len(run['requests']) for run in data['serve']) == 11
    for row in data['cli']:
        text = (H/'raw'/(row['name']+'.stdout')).read_text()
        stderr = (H/'raw'/(row['name']+'.stderr')).read_text()
        tokens = list(map(int, re.search(r'^output\s+:(.*)$', text, re.M)[1].split()))
        label = row['name'].rsplit('-', 1)[1]+('-sampled' if row['sampled'] else '-greedy')
        expected = refs[label][:1 if row['eos'] else 6]
        assert tokens == row['tokens'] == row['expected'] == expected and row['returncode'] == 0
        assert row['cache'] == ['5771', '7.74']
        assert row['prompt_windows'] == ('through verify windows' in stderr)
        command = row['command']
        prompt = command[command.index('--tokens')+1].split(',')
        short_read = int(command[command.index('--short-read')+1])
        assert row['prompt_windows'] == (len(prompt) > 1 and len(prompt)-1 <= short_read)
        assert int(re.search(r'^decode\s+(\d+) tokens', text, re.M)[1]) == len(tokens)
        assert not re.search(r'\b(?:nan|inf)\b', text, re.I)
    assert [run['side'] for run in measured['runs']] == ['baseline', 'candidate', 'baseline', 'candidate', 'baseline']
    assert len(measured['comparisons']) == 8
    assert not any(key in measured['environment'] for key in ('STRATA_TRACE', 'STRATA_PREFILL_TIMING', 'STRATA_PREFILL_SYNC', 'STRATA_DECODE_TIMING', 'STRATA_VERIFY_PROFILE'))
    for row in measured['runs']:
        text = (H/'timing'/(row['name']+'.stdout')).read_text()
        assert row['returncode'] == 0 and row['tokens'] == measured['expected'] == refs['two-greedy']
        assert row['cache'] == ['5771', '7.74']
        for metric in ('prompt_ms', 'ttft_ms', 'decode_ms', 'wall_s'):
            assert math.isfinite(row[metric]) and row[metric] > 0
        prompt_ms, ttft_ms = map(float, re.search(r'^prefill\s+1 tokens in ([\d.]+) ms.*time to first token ([\d.]+) ms', text, re.M).groups())
        assert prompt_ms == row['prompt_ms'] and ttft_ms == row['ttft_ms']
        assert tuple(map(int, re.search(r'^speculation\s+(\d+) rounds of (\d+), drafts accepted (\d+) of (\d+)', text, re.M).groups())) == tuple(row[key] for key in ('rounds', 'window', 'accepted', 'offered'))
    for row in measured['comparisons']:
        i, metric = row['candidate_run'], row['metric']
        a, v, c = [measured['runs'][k][metric] for k in (i-1, i, i+1)]
        base = math.sqrt(a*c)
        assert math.isclose(row['baseline'], base)
        assert math.isclose(row['throughput_change_pct'], 100*(base/v-1))
        assert math.isclose(row['baseline_drift_pct'], 100*(c/a-1))
        assert math.isclose(row['latency_reduction_pct'], 100*(1-v/base))
        assert row['stable'] == (abs(100*(c/a-1)) <= 3)
        assert row['passes_1pct'] == (row['stable'] and 100*(base/v-1) >= 1)
    print('PASS 456 CPU checks; 11 CLI calls; 11 service requests; 5 serial timing processes; source/build/raw hashes')


if __name__ == '__main__':
    main()
