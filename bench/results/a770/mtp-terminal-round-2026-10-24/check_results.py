"""CPU-only audit of raw protocol, terminal decisions, build/source hashes."""
import hashlib
import json
import difflib
from pathlib import Path

H = Path(__file__).resolve().parent


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def parse_rounds(text):
    rows = []
    for line in text.splitlines():
        p = line.split()
        if p[:2] == ['TC', 'ROUND']:
            r, t, a, timed, produced, limit, eos, cancel = map(int, p[3:])
            rows.append(dict(path=p[2], round=r, T=t, accepted=a, timed=timed,
                             produced=produced, limit=limit, eos=eos, cancel=cancel,
                             drafts=0, observations=[]))
        elif p == ['TC', 'DRAFT'] and rows:
            rows[-1]['drafts'] += 1
        elif p[:2] == ['TC', 'OBSERVE'] and rows:
            rows[-1]['observations'].append(list(map(int, p[2:])))
    return rows


def protocol(path):
    requests, tokens = [], []
    for line in path.read_text(encoding='utf-8').splitlines():
        p = line.split()
        if p[:1] == ['T']:
            tokens.append(int(p[1]))
        elif p[:1] == ['TB']:
            block = list(map(int, p[2:]))
            assert len(block) == int(p[1])
            tokens.extend(block)
        elif p[:1] == ['DONE']:
            assert len(tokens) == int(p[1])
            requests.append((tokens, line))
            tokens = []
    assert not tokens
    return requests


def main():
    manifest = json.loads((H/'manifest.json').read_text())
    for name, digest in manifest['files_sha256'].items():
        assert sha(H/name) == digest, name
    data = json.loads((H/'results.json').read_text())
    assert data['complete']
    build = json.loads((H/'build.json').read_text())
    frozen = json.loads((H/'frozen.json').read_text())
    summary = json.loads((H/'summary.json').read_text())
    assert build['complete']
    assert len(data['runs']) == 6 and sum(len(run['requests']) for run in data['runs']) == 46
    assert sha(H/'config-original.json') == frozen['config_sha256']
    assert json.loads((H/'config-original.json').read_text(encoding='utf-8-sig')) == data['config']
    assert sha(H/'source/generate.after.cpp') == build['files_sha256']['candidate.cpp'] == summary['runtime_source_sha256']
    before = next(digest for name, digest in frozen['before'].items()
                  if name.replace('\\', '/').endswith('sycl/src/program/generate.cpp'))
    assert sha(H/'source/generate.before.cpp') == before
    patch = ''.join(difflib.unified_diff((H/'source/generate.before.cpp').read_text().splitlines(True),
                                        (H/'source/generate.after.cpp').read_text().splitlines(True),
                                        fromfile='sycl/src/program/generate.cpp (before)',
                                        tofile='sycl/src/program/generate.cpp (after)'))
    assert patch == (H/'terminal-round.patch').read_text()
    for name, digest in manifest['protected_sha256_after'].items():
        assert digest == frozen['dependencies'][name] == summary['protected_sha256_after'][name]
    assert data['cli_model_check']['status'] == 'not_validated'
    caller = json.loads((H/'raw/cli-caller.stdout').read_text().removeprefix('SUMMARY ').strip())
    assert caller == data['cli_caller_check'] == {'pairs': 54, 'checks': 108, 'terminal_differences': 18}
    for name, digest in data['raw_sha256'].items():
        assert sha(H/'raw'/name) == digest, name
    original_cap_observations = original_cancel_drafts = fixed_cancel_skips = total = 0
    references = {}
    cli_reference = None
    slots = set()
    for run in data['runs']:
        side = run['side']
        fixed = side == 'candidate'
        if run.get('cli'):
            text = (H/'raw'/f'{side}-cli.stderr').read_text()
            assert run['rounds'] == parse_rounds(text)
            if cli_reference is None:
                cli_reference = run['tokens']
            assert len(run['tokens']) == 6 and run['tokens'] == cli_reference
            assert run['prompt_ids'] == data['prompt_ids'][-1:]
            requests = [dict(rounds=run['rounds'])]
        else:
            info = dict(word.split('=', 1) for word in run['runtime_info'].split() if '=' in word)
            slots.add(info['expert_slots'])
            tag = str(run.get('threshold', .5)).replace('.', 'p')
            stem = run.get('stem', side+'-clean') if run.get('clean') else f'{side}-{tag}'
            raw = protocol(H/'raw'/(stem+'.protocol'))
            requests = run['requests']
            main_requests = requests if run.get('clean') else requests[:-1]
            assert len(raw) == len(main_requests)
            if not run.get('clean'):
                raw += protocol(H/'raw'/(stem+'-eos.protocol'))
                actual = parse_rounds((H/'raw'/(stem+'.stderr')).read_text(encoding='utf-8'))
                assert actual == [row for request in main_requests for row in request['rounds']]
                actual = parse_rounds((H/'raw'/(stem+'-eos.stderr')).read_text(encoding='utf-8'))
                assert actual == requests[-1]['rounds']
            assert [(r['tokens'], r['done']) for r in requests] == raw
            for request in requests:
                if 'trace' in request:
                    assert request['rounds'] == parse_rounds(request['trace'])
                name = request['name']
                threshold = run.get('threshold', .5)
                if name in ('greedy', 'sampled'):
                    key = (threshold, name)
                    assert references.setdefault(key, request['tokens']) == request['tokens']
                if name in ('recovery', 'sampled-repeat', 'sampled-recovery'):
                    key = (threshold, 'greedy' if name == 'recovery' else 'sampled')
                    assert request['tokens'] == references[key]
                if name == 'cancel-during-verify':
                    assert request['finish'] == 'cancel' and request['stop_sent_at']
                    assert request['tokens'] == references[(threshold, 'sampled')][:request['generated']]
                    last = request['rounds'][-1]
                    assert last['cancel'] and last['timed']
                    if fixed:
                        fixed_cancel_skips += not last['drafts']
                    else:
                        original_cancel_drafts += last['drafts']
                if name.startswith('cap-'):
                    assert request['tokens'] == references[(threshold, 'greedy')][:request['limit']]
        for request in requests:
            for row in request.get('rounds', []):
                total += 1
                truncated = row['eos'] or row['produced'] >= row['limit'] or (fixed and row['cancel'])
                assert row['drafts'] == (0 if truncated else 1), row
                observed = row['timed'] and not row['eos'] and (not truncated if fixed else True)
                assert len(row['observations']) == int(bool(observed)), row
                if row['observations']:
                    assert row['observations'] == [[0, row['T'], row['accepted']]], row
                if not fixed and row['produced'] >= row['limit'] and row['observations']:
                    original_cap_observations += 1
    assert len(slots) == 1
    assert references[(0, 'greedy')] == references[(.5, 'greedy')]
    assert references[(0, 'sampled')] == references[(.5, 'sampled')]
    assert original_cap_observations > 0 and original_cancel_drafts == fixed_cancel_skips == 2
    print(f'PASS raw protocol and {total} rounds; reproduced {original_cap_observations} truncated cost observations; '
          f'{fixed_cancel_skips} cancellations each skip one draft chain; expert_slots={next(iter(slots))}')


if __name__ == '__main__':
    main()
