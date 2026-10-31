"""Reproduce zero-round NaN and verify finite CLI summaries and unchanged output."""
import importlib.util
import json
import math
from pathlib import Path
import re
import subprocess
import sys

H = Path(__file__).resolve().parent
R = H.parent
P = R/'build-sycl-cli-entry-20261024'
sys.path.insert(0, str(R/'tools'))
import bench_deepinfer as b
spec = importlib.util.spec_from_file_location('summary_runner', H/'run.py')
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)


def main(out):
    runner.verify()
    out.mkdir(parents=True, exist_ok=False)
    previous = R/'bench/results/a770/cli-decode-entry-2026-10-24/results.json'
    reference = json.loads(previous.read_text())
    cfg = json.loads((H/'config.json').read_text(encoding='utf-8-sig'))
    assert cfg == reference['config']
    assert b.sha(P/'candidate/strata.exe') == reference['exe_sha256']['candidate']
    build = json.loads((H/'build.json').read_text())
    assert build['complete']
    assert b.sha(H/'candidate/strata.exe') == build['files_sha256']['candidate\\strata.exe']
    data = {'complete': False, 'scope': 'Summary correctness and token parity only; trace enabled, no performance measurement.',
            'config': cfg, 'reference_sha256': b.sha(previous), 'cli': [],
            'source_sha256': {name: b.sha(H/'after'/name) for name in runner.SOURCES},
            'exe_sha256': {'baseline': b.sha(P/'candidate/strata.exe'), 'candidate': b.sha(H/'candidate/strata.exe')}}
    (out/'reference.json').write_bytes(previous.read_bytes())
    tool = R/'tools/test_spec_decode_entry.py'
    command = [sys.executable, '-X', 'utf8', str(tool), '--cxx', str(runner.builder.ICX/'bin/icx.exe'), '--out', str(out/'cpu')]
    for name in ('src/program/generate.cpp', 'sycl/src/program/generate.cpp'):
        command += ['--source', str(H/'after'/name)]
    with (out/'cpu.stdout').open('x') as stdout, (out/'cpu.stderr').open('x') as stderr:
        subprocess.run(command, env=runner.builder.environment(), stdout=stdout, stderr=stderr, check=True, timeout=120)
    data['cpu'] = {'command': command, 'tool_sha256': b.sha(tool)}
    print('PASS existing 216 CLI entry checks', flush=True)
    env = b.environment(cfg, 'baseline', reproducible=True)
    env.update(STRATA_SPEC_COUPLED='1', STRATA_TRACE='1')
    data['environment'] = {k: v for k, v in env.items() if k.startswith(('STRATA_', 'SYCL_', 'ONEAPI_', 'UR_', 'ZES_'))}
    cases = [('baseline', '0p5', 'cap1'), ('baseline', '0p5', 'cap6')]
    cases += [('candidate', tag, case) for tag in ('0', '0p5') for case in ('cap1', 'cap6', 'eos')]
    cases += [('candidate', '0p5', 'sampled')]
    for side, tag, case in cases:
        old = next(row for row in reference['cli'] if row['name'] == 'candidate-cli-'+tag+'-'+case)
        exe = P/'candidate/strata.exe' if side == 'baseline' else H/'candidate/strata.exe'
        command = [str(exe), *old['command'][1:]]
        name = side+'-'+tag+'-'+case
        print('RUN', name, flush=True)
        with (out/(name+'.stdout')).open('xb') as stdout, (out/(name+'.stderr')).open('xb') as stderr:
            result = subprocess.run(command, cwd=R, env=env, stdout=stdout, stderr=stderr, timeout=300)
        text = (out/(name+'.stdout')).read_text(encoding='utf-8')
        trace = (out/(name+'.stderr')).read_text(encoding='utf-8')
        tokens = list(map(int, re.search(r'^output\s+:(.*)$', text, re.M)[1].split()))
        mean, count = re.search(r'^mtp\s+(\S+) ms/round drafting \((\d+) rounds\)', text, re.M).groups()
        try:
            value = float(mean)
        except ValueError:
            value = math.nan  # MSVC spells the baseline's 0/0 as -nan(ind).
        row = {'name': name, 'side': side, 'threshold': float(tag.replace('p', '.')), 'case': case,
               'command': command, 'returncode': result.returncode, 'tokens': tokens, 'reference_name': old['name'],
               'mean_text': mean, 'draft_rounds': int(count), 'finite': math.isfinite(value),
               'cache': re.search(r'expert cache (\d+) slots, ([\d.]+) GiB', trace).groups()}
        data['cli'].append(row)
        b.save(out/'results.json', data)
        result.check_returncode()
        assert tokens == old['tokens'], row
        assert int(re.search(r'^decode\s+(\d+) tokens', text, re.M)[1]) == len(tokens)
        assert (int(count) == 0) == (case in ('cap1', 'eos')), row
        if side == 'baseline' and case == 'cap1':
            assert math.isnan(value), row
        else:
            assert math.isfinite(value) and value >= 0, row
            if int(count) == 0:
                assert mean == '0.000', row
            assert not re.search(r'\b(?:nan|inf)\b', text, re.I), row
        print('PASS', name, 'mean='+mean, 'rounds='+count, flush=True)
    runner.verify()
    data['complete'] = True
    data['raw_sha256'] = {p.relative_to(out).as_posix(): b.sha(p) for p in out.rglob('*')
                          if p.suffix in ('.stdout', '.stderr', '.cpp')}
    b.save(out/'results.json', data)
    print('PASS MTP summary regression', flush=True)


if __name__ == '__main__':
    with b.gpu_lock():
        main(Path(sys.argv[1]) if len(sys.argv) > 1 else H/'checks-repeat')
