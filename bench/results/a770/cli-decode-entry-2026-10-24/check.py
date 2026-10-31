"""Serial source-condition and real-model regressions; never writes production files."""
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import sys
import time

H = Path(__file__).resolve().parent
R = H.parent
sys.path.insert(0, str(R/'tools'))
import bench_deepinfer as b
spec = importlib.util.spec_from_file_location('entry_builder', H/'build.py')
builder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)


def main(out):
    builder.verify()
    build = json.loads((H/'build.json').read_text())
    assert build['complete']
    for name, digest in build['files_sha256'].items():
        assert b.sha(H/name) == digest, name
    out.mkdir(parents=True, exist_ok=False)
    cfg = json.loads((H/'config.json').read_text(encoding='utf-8-sig'))
    previous = R/'bench/results/a770/mtp-terminal-round-2026-10-24/results.json'
    short = json.loads(previous.read_text())['prompt_ids']
    baseline = Path(build['baseline_exe'])
    candidate = H/'candidate/strata.exe'
    data = {'complete': False, 'scope': 'Functional regression only; no timing/performance acceptance.',
            'config': cfg, 'single_prompt': [271], 'short_prompt': short, 'cli': [], 'serve': [],
            'cpu': [], 'source_sha256': {name: b.sha(H/'after'/name) for name in
                    ('src/program/generate.cpp', 'sycl/src/program/generate.cpp')},
            'exe_sha256': {'baseline': b.sha(baseline), 'candidate': b.sha(candidate)}}

    def save():
        b.save(out/'results.json', data)

    for side in ('before', 'after'):
        command = [sys.executable, '-X', 'utf8', str(R/'tools/test_spec_decode_entry.py'),
                   '--cxx', str(builder.builder.ICX/'bin/icx.exe'), '--out', str(out/('cpu-'+side))]
        for name in ('src/program/generate.cpp', 'sycl/src/program/generate.cpp'):
            command += ['--source', str(H/side/name)]
        if side == 'before':
            command += ['--expect-zero-bug']
        with (out/('cpu-'+side+'.stdout')).open('x') as stdout, (out/('cpu-'+side+'.stderr')).open('x') as stderr:
            subprocess.run(command, env=builder.builder.environment(), stdout=stdout, stderr=stderr, check=True, timeout=120)
        data['cpu'].append({'side': side, 'command': command})
        save()
        print('PASS CPU', side, flush=True)

    refs = {}
    sampling = 'temperature=0.7 top_p=0.9 top_k=20 seed=1234'

    def cli(exe, name, args, env, expect):
        command = [str(exe), *args, '--tokens', '271']
        print('RUN CLI', name, flush=True)
        start = time.perf_counter()
        with (out/(name+'.stdout')).open('xb') as stdout, (out/(name+'.stderr')).open('xb') as stderr:
            result = subprocess.run(command, cwd=R, env=env, stdout=stdout, stderr=stderr, timeout=300)
        text = (out/(name+'.stdout')).read_text(encoding='utf-8')
        trace = (out/(name+'.stderr')).read_text(encoding='utf-8')
        match = re.search(r'^output\s+:(.*)$', text, re.M)
        tokens = list(map(int, match[1].split())) if match else None
        row = {'name': name, 'command': command, 'tokens': tokens, 'expected': expect,
               'returncode': result.returncode, 'wall_s': time.perf_counter()-start,
               'cache': re.search(r'expert cache (\d+) slots, ([\d.]+) GiB', trace).groups()}
        data['cli'].append(row)
        save()
        result.check_returncode()
        assert tokens == expect, row
        assert ('speculation' in text) == bool(expect), row
        assert int(re.search(r'^decode\s+(\d+) tokens', text, re.M)[1]) == len(tokens)
        print('PASS CLI', name, tokens, flush=True)

    for threshold in (0, .5):
        tag = str(threshold).replace('.', 'p')
        args = b.model_args(cfg, reproducible=True, threshold=threshold, cache_mib=5500)+['--kv', 'int8']
        args[args.index('--prefill')+1] = '1'
        env = b.environment(cfg, 'baseline', reproducible=True)
        env.update(STRATA_SPEC_COUPLED='1', STRATA_TRACE='1')
        data.setdefault('environments', {})[tag] = {k: v for k, v in env.items() if k.startswith(('STRATA_', 'SYCL_', 'ONEAPI_', 'UR_', 'ZES_'))}
        for side, exe in (('baseline', baseline), ('candidate', candidate)):
            name = side+'-serve-'+tag
            engine = b.Engine(exe, args, env, out/name)
            run = {'name': name, 'threshold': threshold, 'side': side, 'args': args,
                   'runtime_info': engine.info, 'requests': []}
            data['serve'].append(run)
            try:
                for label, prompt, keys in (('single-greedy', [271], ''), ('single-sampled', [271], sampling),
                                           ('short-greedy', short, ''), ('short-penalties', short, sampling+' penalty_repeat=1.1 penalty_freq=0.1 penalty_present=0.2 penalty_last_n=128')):
                    row = engine.gen(prompt, 6, keys)
                    row.update(name=label, prompt=prompt, keys=keys)
                    run['requests'].append(row)
                    save()
                    assert row['generated'] == 6 and row['finish'] == 'length', row
                    key = (threshold, label)
                    assert refs.setdefault(key, row['tokens']) == row['tokens'], row
                    print('PASS SERVE', side, tag, label, flush=True)
            finally:
                engine.close()
        if threshold == .5:
            cli(baseline, 'baseline-cli-'+tag, args+['--max-new', '6'], env, [])
        full = refs[(threshold, 'single-greedy')]
        for cap in (1, 4, 6):
            cli(candidate, 'candidate-cli-'+tag+'-cap'+str(cap), args+['--max-new', str(cap)], env, full[:cap])
        sample_args = ['--seed', '1234', '--temperature', '0.7', '--top-k', '20', '--top-p', '0.9']
        cli(candidate, 'candidate-cli-'+tag+'-sampled', args+sample_args+['--max-new', '6'], env,
            refs[(threshold, 'single-sampled')])
        cli(candidate, 'candidate-cli-'+tag+'-eos', args+['--max-new', '6', '--eos-ids', str(full[0])], env, full[:1])
    for label in ('single-greedy', 'single-sampled', 'short-greedy', 'short-penalties'):
        assert refs[(0, label)] == refs[(.5, label)], label
    builder.verify()
    data['complete'] = True
    data['raw_sha256'] = {p.relative_to(out).as_posix(): b.sha(p) for p in out.rglob('*')
                          if p.suffix in ('.stdout', '.stderr', '.protocol', '.cpp')}
    save()
    print('PASS CLI decode entry and serving smoke', flush=True)


if __name__ == '__main__':
    with b.gpu_lock():
        main(Path(sys.argv[1]) if len(sys.argv) > 1 else H/'checks')
