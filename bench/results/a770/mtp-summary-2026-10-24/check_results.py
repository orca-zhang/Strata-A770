"""Offline audit of the actual CLI summaries, token output and source/build hashes."""
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
    build = json.loads((H/'build.json').read_text())
    frozen = json.loads((H/'frozen.json').read_text())
    reference = json.loads((H/'reference.json').read_text())
    assert data['complete'] and build['complete'] and len(data['cli']) == 9
    assert sha(H/'reference.json') == data['reference_sha256']
    assert data['exe_sha256']['baseline'] == reference['exe_sha256']['candidate'] == frozen['dependencies'][frozen['baseline_exe']]
    assert data['exe_sha256']['candidate'] == build['files_sha256']['candidate\\strata.exe']
    assert sha(H/'config-original.json') == frozen['config_sha256']
    assert json.loads((H/'config-original.json').read_text(encoding='utf-8-sig')) == data['config'] == reference['config']
    for name, digest in data['raw_sha256'].items():
        assert sha(H/'raw'/name) == digest, name
    for name, digest in manifest['protected_sha256_after'].items():
        assert digest == frozen['dependencies'][name]
    assert sha(H/'test_spec_decode_entry.py') == data['cpu']['tool_sha256']
    assert (H/'raw/cpu.stdout').read_text().count('PASS 108 CLI entry checks') == 2
    patch = ''
    for name, tag in [('sycl/src/program/generate.cpp', 'sycl'), ('src/program/generate.cpp', 'cuda')]:
        before, after = H/'source'/(tag+'.before.cpp'), H/'source'/(tag+'.after.cpp')
        assert sha(before) == frozen['before'][name]
        assert sha(after) == data['source_sha256'][name]
        old = 'mtp.ms_draft / (double) mtp.rounds, (long long) mtp.rounds'
        new = 'mtp.rounds > 0 ? mtp.ms_draft / (double) mtp.rounds : 0.0, (long long) mtp.rounds'
        assert before.read_text().count(old) == 1 and after.read_text() == before.read_text().replace(old, new)
        patch += ''.join(difflib.unified_diff(before.read_text().splitlines(True), after.read_text().splitlines(True),
                                             fromfile=name+' (before)', tofile=name+' (after)'))
    assert patch == (H/'mtp-summary.patch').read_text()
    assert sha(H/'source/sycl.after.cpp') == build['files_sha256']['candidate.cpp']
    old_cases = {row['name']: row for row in reference['cli']}
    zeros = 0
    for row in data['cli']:
        text = (H/'raw'/(row['name']+'.stdout')).read_text()
        stderr = (H/'raw'/(row['name']+'.stderr')).read_text()
        tokens = list(map(int, re.search(r'^output\s+:(.*)$', text, re.M)[1].split()))
        mean, count = re.search(r'^mtp\s+(\S+) ms/round drafting \((\d+) rounds\)', text, re.M).groups()
        try:
            value = float(mean)
        except ValueError:
            value = math.nan
        assert row['returncode'] == 0 and tokens == row['tokens'] == old_cases[row['reference_name']]['tokens']
        assert row['command'][1:] == old_cases[row['reference_name']]['command'][1:]
        assert mean == row['mean_text'] and int(count) == row['draft_rounds'] and math.isfinite(value) == row['finite']
        assert (int(count) == 0) == (row['case'] in ('cap1', 'eos'))
        assert int(re.search(r'^decode\s+(\d+) tokens', text, re.M)[1]) == len(tokens)
        assert list(re.search(r'expert cache (\d+) slots, ([\d.]+) GiB', stderr).groups()) == row['cache'] == ['5771', '7.74']
        if row['side'] == 'baseline' and row['case'] == 'cap1':
            assert math.isnan(value)
        else:
            assert math.isfinite(value) and value >= 0 and not re.search(r'\b(?:nan|inf)\b', text, re.I)
            if int(count) == 0:
                assert mean == '0.000'
                zeros += 1
    assert zeros == 4 and sum(row['side'] == 'candidate' for row in data['cli']) == 7
    print('PASS 216 CPU entry checks; 9 CLI runs; 4 finite zero-round summaries; token/source/build/raw hashes')


if __name__ == '__main__':
    main()
