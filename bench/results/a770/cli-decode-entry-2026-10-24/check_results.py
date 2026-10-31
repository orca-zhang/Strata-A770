"""CPU-only audit of archived source, raw CLI/protocol output and build evidence."""
import difflib
import hashlib
import json
from pathlib import Path
import re

H = Path(__file__).resolve().parent


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def protocol(path):
    requests, tokens = [], []
    for line in path.read_text(encoding='utf-8').splitlines():
        fields = line.split()
        if fields[:1] == ['T']:
            tokens.append(int(fields[1]))
        elif fields[:1] == ['TB']:
            block = list(map(int, fields[2:]))
            assert len(block) == int(fields[1])
            tokens.extend(block)
        elif fields[:1] == ['DONE']:
            assert len(tokens) == int(fields[1])
            requests.append((tokens, line))
            tokens = []
    assert not tokens
    return requests


def main():
    manifest = json.loads((H/'manifest.json').read_text())
    for name, digest in manifest['files_sha256'].items():
        assert sha(H/name) == digest, name
    data = json.loads((H/'results.json').read_text())
    build = json.loads((H/'build.json').read_text())
    frozen = json.loads((H/'frozen.json').read_text())
    assert data['complete'] and build['complete']
    assert len(data['cli']) == 11 and len(data['serve']) == 4
    assert data['single_prompt'] == [271] and len(data['short_prompt']) > 1
    assert json.loads((H/'config-original.json').read_text(encoding='utf-8-sig')) == data['config']
    assert sha(H/'config-original.json') == frozen['config_sha256']
    assert data['exe_sha256']['candidate'] == build['files_sha256']['candidate\\strata.exe']
    assert data['exe_sha256']['baseline'] == frozen['dependencies'][frozen['baseline_exe']]
    for name, digest in manifest['protected_sha256_after'].items():
        assert digest == frozen['dependencies'][name]
    for name, digest in data['raw_sha256'].items():
        assert sha(H/'raw'/name) == digest, name
    patch = ''
    for name, tag in [('sycl/src/program/generate.cpp', 'sycl'), ('src/program/generate.cpp', 'cuda')]:
        before = H/'source'/(tag+'.before.cpp')
        after = H/'source'/(tag+'.after.cpp')
        assert sha(before) == frozen['before'][name]
        assert sha(after) == data['source_sha256'][name]
        expected = before.read_text().replace('int64_t spec_pos = 0;', 'int64_t spec_pos = -1;').replace(
            '(0 = not used)', '(-1 = not used; position 0 is valid)').replace('if (spec_pos > 0 &&', 'if (spec_pos >= 0 &&')
        assert after.read_text() == expected
        patch += ''.join(difflib.unified_diff(before.read_text().splitlines(True), after.read_text().splitlines(True),
                                             fromfile=name+' (before)', tofile=name+' (after)'))
    assert patch == (H/'cli-entry.patch').read_text()
    assert sha(H/'source/sycl.after.cpp') == build['files_sha256']['candidate.cpp']
    for side in ('before', 'after'):
        stdout = (H/'raw'/('cpu-'+side+'.stdout')).read_text()
        assert stdout.count('PASS 108 CLI entry checks') == 2
        for i, tag in enumerate(('cuda', 'sycl')):
            source = (H/'source'/(tag+'.'+side+'.cpp')).read_text()
            harness = (H/'raw'/('cpu-'+side)/f'entry-{i}.cpp').read_text()
            for pattern in (r'^    (int64_t spec_pos = [^;]+;)', r'^    (if \(spec_pos [^\n]+\{)$',
                            r'^        (if \(native_pack\) \{ spec_pos = pos; break; \})$',
                            r'^        (if \(o.spec > 0 && pos >= n_prompt - 1\) \{ spec_pos = pos \+ 1; break; \})$'):
                assert re.search(pattern, source, re.M)[1] in harness
            assert (H/'raw'/('cpu-'+side)/f'entry-{i}.stdout').read_text().strip() == 'PASS 108 CLI entry checks'
    refs, args_by_threshold, slots = {}, {}, set()
    for run in data['serve']:
        raw = protocol(H/'raw'/(run['name']+'.protocol'))
        assert len(raw) == len(run['requests']) == 4
        info = dict(field.split('=', 1) for field in run['runtime_info'].split() if '=' in field)
        slots.add(info['expert_slots'])
        assert info['expert_cache_mib'] == '7922' and info['kv'] == 'int8'
        args_by_threshold[run['threshold']] = run['args']
        for (tokens, done), row in zip(raw, run['requests']):
            assert tokens == row['tokens'] and done == row['done']
            assert row['generated'] == len(tokens) == 6 and row['finish'] == 'length'
            assert row['prompt'] == (data['single_prompt'] if row['name'].startswith('single-') else data['short_prompt'])
            key = (run['threshold'], row['name'])
            assert refs.setdefault(key, tokens) == tokens
    assert slots == {'5771'}
    for label in ('single-greedy', 'single-sampled', 'short-greedy', 'short-penalties'):
        assert refs[(0, label)] == refs[(.5, label)]
    for row in data['cli']:
        text = (H/'raw'/(row['name']+'.stdout')).read_text()
        stderr = (H/'raw'/(row['name']+'.stderr')).read_text()
        tokens = list(map(int, re.search(r'^output\s+:(.*)$', text, re.M)[1].split()))
        assert row['returncode'] == 0 and tokens == row['tokens'] == row['expected']
        assert list(re.search(r'expert cache (\d+) slots, ([\d.]+) GiB', stderr).groups()) == row['cache'] == ['5771', '7.74']
        command = row['command']
        threshold = float(command[command.index('--spec-min-p')+1])
        cap = int(command[command.index('--max-new')+1])
        assert command[1:1+len(args_by_threshold[threshold])] == args_by_threshold[threshold]
        assert command[-2:] == ['--tokens', '271']
        assert int(re.search(r'^decode\s+(\d+) tokens', text, re.M)[1]) == len(tokens)
        if row['name'].startswith('baseline-'):
            assert tokens == [] and 'speculation' not in text
        else:
            assert 'speculation' in text
            sampled = '--seed' in command
            ref = refs[(threshold, 'single-sampled' if sampled else 'single-greedy')]
            if '--eos-ids' in command:
                assert int(command[command.index('--eos-ids')+1]) == ref[0]
                assert tokens == ref[:1]
            else:
                assert tokens == ref[:cap] and len(tokens) == cap
    print('PASS 432 CPU checks; 11 CLI runs; 16 serving requests; source/build/raw/protected hashes')


if __name__ == '__main__':
    main()
