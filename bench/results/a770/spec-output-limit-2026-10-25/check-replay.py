"""Real-model limits, sampled prefixes, cancellation and session restoration."""
import json
from pathlib import Path
import re
import subprocess
import sys

H = Path(__file__).resolve().parent
R = H.parent
sys.path.insert(0, str(R/'tools'))
import bench_deepinfer as b

SAMPLING = 'temperature=0.7 top_p=0.9 top_k=20 seed=1234 penalty_repeat=1.1 penalty_freq=0.1 penalty_present=0.2 penalty_last_n=128'


def setup():
    cfg = json.loads((H/'config.json').read_text())
    env = b.environment(cfg, reproducible=True)
    env['STRATA_SPEC_COUPLED'] = '1'
    args = b.model_args(cfg, reproducible=True, cache_mib=4000)+['--kv', 'int8', '--stop-eos']
    return cfg, env, args


def executable(side):
    return (R/'build-sycl-cli-prefill-final-20261024' if side == 'baseline' else H)/'candidate/strata.exe'


def main(out=None):
    out = Path(out) if out is not None else H/'checks'
    out.mkdir(exist_ok=False)
    cfg, env, args = setup()
    env['STRATA_TRACE'] = '1'
    env['STRATA_SNAPSHOT_VERIFY'] = '1'
    ids = b.prompts(cfg)['Short']
    args[args.index('--prompt-cache')+1] = '128'
    args += ['--conversation-cache-mib', '2048', '--conversation-cache-slots', '2']
    data = {'complete': False, 'args': args, 'environment': {k:v for k,v in env.items() if k.startswith(('STRATA_', 'SYCL_', 'UR_', 'ONEAPI_', 'ZES_'))},
            'prompt': ids, 'runs': [], 'cli': [], 'reference': {}}
    def save():
        b.save(out/'results.json', data)
    save()
    references = {}
    for side in ('baseline', 'candidate'):
        stem = out/side
        engine = b.Engine(executable(side), args, env, stem)
        run = {'side': side, 'runtime_info': engine.info, 'rows': []}
        data['runs'].append(run)
        assert 'expert_slots=4191' in engine.info and 'expert_cache_mib=5760' in engine.info, engine.info
        def request(name, tokens, cap, keys='', cancel=False):
            offset = stem.with_suffix('.stderr').stat().st_size
            row = engine.gen(tokens, cap, keys, cancel)
            trace = stem.with_suffix('.stderr').read_bytes()[offset:].decode('utf-8')
            windows = [list(map(int, pair)) for pair in re.findall(r'^strata trace: window (\d+) (\d+)\r?$', trace, re.M)]
            assert windows, (name, trace)
            if side == 'candidate':
                for pos, size in windows:
                    assert 1 <= size <= cap - (pos - len(tokens) + 1), (name, windows, row)
            row.update(name=name, limit=cap, keys=keys, prompt=tokens, windows=windows)
            run['rows'].append(row)
            save()
            print('PASS SERVE', side, name, row['generated'], 'windows', windows, flush=True)
            return row
        try:
            for threshold in (0, .5):
                for sampled in (False, True):
                    keys = f'spec_min_p={threshold} '+(SAMPLING if sampled else '')
                    prefix = f'{threshold}-{sampled}'
                    full = request(prefix+'-reference', ids, 16, keys)
                    assert full['generated'] == 16 and full['finish'] == 'length'
                    refs = references.setdefault(prefix, full['tokens'])
                    assert refs == full['tokens']
                    for cap in (1, 2, 3, 4, 6):
                        row = request(prefix+f'-cap{cap}', ids, cap, keys)
                        assert row['tokens'] == full['tokens'][:cap] and row['finish'] == 'length', row
                    if sampled:
                        row = request(prefix+'-cancel', ids, 128, keys, cancel=True)
                        assert row['finish'] == 'cancel', row
                        assert request(prefix+'-recovery', ids, 6, keys)['tokens'] == full['tokens'][:6]
                # Force a different live request, then resume the parked and live truncated session.
                keys = f'spec_min_p={threshold}'
                full = references[f'{threshold}-False']
                assert request(f'{threshold}-head', ids, 5, keys)['tokens'] == full[:5]
                request(f'{threshold}-other', [271, 91], 2, keys)
                parked = request(f'{threshold}-parked', ids+full[:5], 6, keys)
                assert parked['reused'] > 0 and parked['tokens'] == full[5:11], parked
                live = request(f'{threshold}-live', ids+full[:11], 4, keys)
                assert live['reused'] > 0 and live['tokens'] == full[11:15], live
        finally:
            engine.close()
        save()
    data['reference'] = references
    assert references['0-False'] == references['0.5-False']
    assert references['0-True'] == references['0.5-True']
    # EOS uses a fresh process; its stop token is the first known greedy output.
    for side in ('baseline', 'candidate'):
        engine = b.Engine(executable(side), args+['--eos-ids', str(references['0.5-False'][0])], env, out/(side+'-eos'))
        try:
            row = engine.gen(ids, 6)
            assert row['finish'] == 'stop' and row['tokens'] == references['0.5-False'][:1], row
            data.setdefault('eos', []).append({'side': side, **row, 'runtime_info': engine.info})
        finally:
            engine.close()
        save()
    _, cli_env, cli_args = setup()
    cli_env['STRATA_TRACE'] = '1'
    cli_refs = {}
    for side in ('baseline', 'candidate'):
        for cap, threshold, sampled in ((2, 0, False), (4, .5, False), (6, .5, True)):
            name = f'{side}-cli-{cap}'
            options = cli_args.copy()
            options[options.index('--spec-min-p')+1] = str(threshold)
            if sampled:
                options += ['--seed', '1234', '--temperature', '0.7', '--top-k', '20', '--top-p', '0.9']
            command = [str(executable(side)), *options, '--tokens', '271', '--max-new', str(cap)]
            with (out/(name+'.stdout')).open('xb') as stdout, (out/(name+'.stderr')).open('xb') as stderr:
                process = subprocess.run(command, cwd=R, env=cli_env, stdout=stdout, stderr=stderr, timeout=240)
            text = (out/(name+'.stdout')).read_text()
            trace = (out/(name+'.stderr')).read_text()
            tokens = list(map(int, re.search(r'^output\s+:(.*)$', text, re.M)[1].split()))
            row = {'side': side, 'cap': cap, 'threshold': threshold, 'sampled': sampled, 'tokens': tokens,
                   'command': command, 'returncode': process.returncode,
                   'cache': list(re.search(r'expert cache (\d+) slots, ([\d.]+) GiB', trace).groups())}
            data['cli'].append(row)
            save()
            process.check_returncode()
            assert row['cache'] == ['4191', '5.63'] and len(tokens) == cap, row
            assert cli_refs.setdefault(cap, tokens) == tokens, row
            assert not re.search(r'\b(?:nan|inf)\b', text, re.I)
            print('PASS CLI', name, tokens, flush=True)
    # The CLI accepts prompt + generation exactly fitting its context. Extra verify rows must not reject it.
    for side in ('baseline', 'candidate'):
        options = cli_args.copy()
        options[options.index('--max-context')+1] = '7'
        options += ['--seed', '1234', '--temperature', '0.7', '--top-k', '20', '--top-p', '0.9']
        command = [str(executable(side)), *options, '--tokens', '271', '--max-new', '6']
        name = side+'-cli-context'
        with (out/(name+'.stdout')).open('xb') as stdout, (out/(name+'.stderr')).open('xb') as stderr:
            process = subprocess.run(command, cwd=R, env=cli_env, stdout=stdout, stderr=stderr, timeout=240)
        text = (out/(name+'.stdout')).read_text()
        row = {'side': side, 'command': command, 'returncode': process.returncode}
        match = re.search(r'^output\s+:(.*)$', text, re.M)
        row['tokens'] = list(map(int, match[1].split())) if match else []
        data.setdefault('context', []).append(row)
        save()
        if side == 'candidate' or process.returncode == 0:
            process.check_returncode()
            assert row['tokens'] == cli_refs[6], row
        else:
            assert process.returncode == 2 and 'ran out of context' in (out/(name+'.stderr')).read_text(), row
        print('PASS CLI context', name, row, flush=True)
    data['complete'] = True
    data['raw_sha256'] = {p.name: b.sha(p) for p in out.iterdir() if p.suffix in ('.stdout', '.stderr', '.protocol')}
    save()
    print('PASS model output-window regression', flush=True)


if __name__ == '__main__':
    with b.gpu_lock():
        main(sys.argv[1] if len(sys.argv) > 1 else None)
