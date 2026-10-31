"""Real-model terminal-round regression. Run through run.py for the shared lock."""
import json
from pathlib import Path
import re
import subprocess
import sys
import threading

H = Path(__file__).resolve().parent
R = H.parent
sys.path.insert(0, str(R/'tools'))
import bench_deepinfer as b


def rounds(text):
    result = []
    for line in text.splitlines():
        if line.startswith('TC ROUND '):
            fields = line.split()
            values = dict(zip(('round', 'T', 'accepted', 'timed', 'produced', 'limit', 'eos', 'cancel'), map(int, fields[3:])))
            result.append(dict(path=fields[2], **values, drafts=0, observations=[]))
        elif line == 'TC DRAFT' and result:
            result[-1]['drafts'] += 1
        elif line.startswith('TC OBSERVE ') and result:
            result[-1]['observations'].append(list(map(int, line.split()[2:])))
    return result


def check_rounds(rows, fixed):
    assert rows
    for row in rows:
        terminal = row['eos'] or row['produced'] >= row['limit'] or (fixed and row['cancel'])
        assert row['drafts'] == (0 if terminal else 1), row
        observed = row['timed'] and not row['eos'] and (not terminal if fixed else True)
        assert row['observations'] == ([[0, row['T'], row['accepted']]] if observed else []), row


def main(out=None, resume=False):
    out = Path(out) if out else H/'checks'
    out.mkdir(exist_ok=resume)
    cfg = json.loads((H/'config.json').read_text(encoding='utf-8-sig'))
    ids = b.prompts(cfg)['Short']
    data = {'complete': False, 'config': cfg, 'prompt_ids': ids, 'runs': [],
            'scope': 'Functional and invocation regression, not a performance measurement. Trace/private diagnostics enabled. Actual model, Q8 KV, reproducible CPU experts and coupled sampling, no production replacement.'}
    target = out/'results.json'
    reference = {}
    if resume:
        data = json.loads(target.read_text())
        diagnostic = [run for run in data['runs'] if not run.get('clean') and not run.get('cli')]
        assert len(diagnostic) == 4 and all(len(run['requests']) == 11 for run in diagnostic)
        ids = data['prompt_ids']
        for run in diagnostic:
            for row in run['requests']:
                if row['name'] in ('greedy', 'sampled'):
                    key = (run['threshold'], row['name'])
                    assert reference.setdefault(key, row['tokens']) == row['tokens']

    def request(engine, stem, name, n, keys='', cancel=False):
        stderr = stem.with_suffix('.stderr')
        offset = stderr.stat().st_size
        quit_monitor = threading.Event()
        stop_sent = []
        if cancel:
            def monitor():
                with stderr.open(encoding='utf-8') as log:
                    log.seek(offset)
                    count = 0
                    while not quit_monitor.is_set():
                        line = log.readline()
                        if not line:
                            quit_monitor.wait(.002)
                        elif line.startswith('strata trace: window '):
                            count += 1
                            if count == 2:
                                engine.send('STOP')
                                stop_sent.append(line.strip())
                                return
            thread = threading.Thread(target=monitor, daemon=True)
            thread.start()
        try:
            row = engine.gen(ids, n, keys)
        finally:
            quit_monitor.set()
            if cancel:
                thread.join(5)
        row.update(name=name, limit=n, keys=keys, trace=stderr.read_bytes()[offset:].decode('utf-8'))
        row['rounds'] = rounds(row['trace'])
        if cancel:
            assert stop_sent and row['finish'] == 'cancel', row
            assert row['rounds'][-1]['cancel'] == 1, row
            row['stop_sent_at'] = stop_sent
        return row

    for threshold in (() if resume else (0, .5)):
        for side in ('baseline', 'candidate'):
            fixed = side == 'candidate'
            argv = b.model_args(cfg, reproducible=True, threshold=threshold, cache_mib=5500)+['--kv', 'int8']
            env = b.environment(cfg, 'cost', reproducible=True)
            env.update(STRATA_TRACE='1', STRATA_SPEC_COUPLED='1')
            tag = str(threshold).replace('.', 'p')
            stem = out/f'{side}-{tag}'
            engine = b.Engine(H/(side+'-debug')/'strata.exe', argv, env, stem)
            run = {'side': side, 'threshold': threshold, 'args': argv, 'runtime_info': engine.info, 'requests': []}
            data['runs'].append(run)

            def retain(name, n, keys='', cancel=False):
                row = request(engine, stem, name, n, keys, cancel)
                run['requests'].append(row)
                b.save(target, data)
                check_rounds(row['rounds'], fixed)
                print('PASS', side, threshold, name, row['generated'], row['finish'], flush=True)
                return row
            try:
                full = retain('greedy', 32)
                assert full['finish'] == 'length'
                for n in (1, 4, 6):
                    limited = retain('cap-'+str(n), n)
                    assert limited['tokens'] == full['tokens'][:n] and limited['finish'] == 'length'
                sampling = 'temperature=0.7 top_p=0.9 top_k=20 seed=1234 penalty_repeat=1.1 penalty_freq=0.1 penalty_present=0.2 penalty_last_n=128'
                sampled = retain('sampled', 32, sampling)
                assert retain('sampled-repeat', 32, sampling)['tokens'] == sampled['tokens']
                retain('cancel-during-verify', 128, sampling, cancel=True)
                assert retain('recovery', 32)['tokens'] == full['tokens']
                assert retain('sampled-recovery', 32, sampling)['tokens'] == sampled['tokens']
                # Per-request probability floor zero also exercises a full chain in the 0.50 process.
                row = retain('threshold-request-0', 6, 'spec_min_p=0')
                assert row['tokens'] == full['tokens'][:6]
                key = (threshold, 'greedy')
                assert reference.setdefault(key, full['tokens']) == full['tokens']
                key = (threshold, 'sampled')
                assert reference.setdefault(key, sampled['tokens']) == sampled['tokens']
            finally:
                engine.close()
            stop_id = full['tokens'][3]
            stem = out/f'{side}-{tag}-eos'
            engine = b.Engine(H/(side+'-debug')/'strata.exe', argv+['--eos-ids', str(stop_id)], env, stem)
            try:
                eos = request(engine, stem, 'forced-eos', 16)
                run['requests'].append(eos)
                b.save(target, data)
                check_rounds(eos['rounds'], fixed)
                assert eos['finish'] == 'stop' and eos['tokens'] == full['tokens'][:full['tokens'].index(stop_id)+1]
            finally:
                engine.close()

    # Clean independent binaries: output equality without private instrumentation or cost selection.
    for side in ('baseline', 'candidate'):
        env = b.environment(cfg, 'baseline', reproducible=True)
        env['STRATA_SPEC_COUPLED'] = '1'
        argv = b.model_args(cfg, reproducible=True, threshold=.5, cache_mib=5500)+['--kv', 'int8']
        stem = out/(side+('-clean-final' if resume else '-clean'))
        if not any(run.get('clean') and run['side'] == side for run in data['runs']):
            engine = b.Engine(H/side/'strata.exe', argv, env, stem)
            try:
                row = engine.gen(ids, 32)
                assert row['tokens'] == reference[(.5, 'greedy')]
                data['runs'].append({'side': side, 'clean': True, 'stem': stem.name, 'args': argv, 'runtime_info': engine.info, 'requests': [dict(row, name='greedy')]})
            finally:
                engine.close()
        b.save(target, data)
        if any(run.get('cli') and run['side'] == side for run in data['runs']):
            continue
        # Exercise the actual CLI cap branch too.
        cli_argv = argv.copy()
        cli_argv[cli_argv.index('--prefill')+1] = '1'
        command = [str(H/(side+'-debug')/'strata.exe'), *cli_argv, '--tokens', ','.join(map(str, ids)), '--max-new', '6']
        with (out/(side+'-cli.stdout')).open('x') as stdout, (out/(side+'-cli.stderr')).open('x') as stderr:
            subprocess.run(command, cwd=R, env=env, stdout=stdout, stderr=stderr, check=True, timeout=600)
        text = (out/(side+'-cli.stdout')).read_text()
        tokens = list(map(int, re.search(r'^output  :(.*)$', text, re.M)[1].split()))
        assert tokens == reference[(.5, 'greedy')][:6]
        trace = (out/(side+'-cli.stderr')).read_text()
        rows = rounds(trace)
        check_rounds(rows, side == 'candidate')
        data['runs'].append({'side': side, 'cli': True, 'command': command, 'tokens': tokens, 'rounds': rows})
        b.save(target, data)
    assert reference[(0, 'greedy')] == reference[(.5, 'greedy')]
    assert reference[(0, 'sampled')] == reference[(.5, 'sampled')]
    data['complete'] = True
    data['raw_sha256'] = {p.name: b.sha(p) for p in out.iterdir() if p.suffix in ('.stderr', '.stdout', '.protocol')}
    b.save(target, data)
    print('PASS terminal-round functional regression', flush=True)


if __name__ == '__main__':
    with b.gpu_lock():
        main(sys.argv[1] if len(sys.argv) > 1 else None, '--tail' in sys.argv)
