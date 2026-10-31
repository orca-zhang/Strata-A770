"""Real-model CLI prompt-window parity and bounded fallback checks; new --out for each run."""
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import sys

H = Path(__file__).resolve().parent
R = H.parent
P = R/'build-sycl-mtp-summary-20261024'
sys.path.insert(0, str(R/'tools'))
import bench_deepinfer as b
spec = importlib.util.spec_from_file_location('runner', H/'run.py')
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)


def cpu(out):
    command = [sys.executable, '-X', 'utf8', str(R/'tools/test_spec_decode_entry.py'),
               '--cxx', str(runner.builder.ICX/'bin/icx.exe'), '--out', str(out/'cpu')]
    for name in ('src/program/generate.cpp', 'sycl/src/program/generate.cpp'):
        command += ['--source', str(R/name)]
    with (out/'cpu.stdout').open('x') as stdout, (out/'cpu.stderr').open('x') as stderr:
        subprocess.run(command, env=runner.builder.environment(), stdout=stdout, stderr=stderr, check=True, timeout=120)
    source = (H/'candidate.cpp').read_text()
    statements = [re.findall(r'^    (const (?:int64_t n_batched|bool prompt_windows) = [^\n]+;)$', source, re.M)]
    assert len(statements[0]) == 2
    cpp = out/'route.cpp'
    cpp.write_text('''#include <cassert>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
int main() {
    int checks = 0;
    for (bool native_pack : {false, true})
    for (int n_prompt : {1, 2, 7, 8, 23})
    for (int short_read : {0, 1, 6, 7, 22, 8192})
    for (int until : {0, 1, 3, 999}) {
        struct { int prefill_until, short_read; } o{until, short_read};
        '''+'\n        '.join(statements[0])+'''
        const bool partial = until > 0 && until < n_prompt - 1;
        assert(prompt_windows == (native_pack && n_prompt > 1 && !partial && n_prompt <= short_read + 1));
        ++checks;
    }
    std::printf("PASS %d CLI prompt route checks\\n", checks);
}
''', encoding='utf-8')
    exe = out/'route.exe'
    subprocess.run([str(runner.builder.ICX/'bin/icx.exe'), '-std=c++20', '-O0', str(cpp), '-o', str(exe)],
                   env=runner.builder.environment(), check=True, capture_output=True, timeout=120)
    result = subprocess.check_output([str(exe)], text=True)
    (out/'route.stdout').write_text(result, encoding='utf-8')
    print('PASS 216 entry checks;', result.strip(), flush=True)
    return {'entry_command': command, 'entry_tool_sha256': b.sha(R/'tools/test_spec_decode_entry.py'),
            'route_source_sha256': b.sha(cpp)}


def main(out):
    runner.verify()
    if not (H/'queue-fix').exists():
        queue_spec = importlib.util.spec_from_file_location('queue_fix', H/'queue_fix.py')
        queue_fix = importlib.util.module_from_spec(queue_spec)
        queue_spec.loader.exec_module(queue_fix)
        queue_fix.prepare(runner.builder)
    built = json.loads((H/'build.json').read_text())
    assert built['complete'] and b.sha(H/'candidate/strata.exe') == built['files_sha256']['candidate\\strata.exe']
    assert b.sha(R/'sycl/include/strata/core/mtp.hpp') == built['files_sha256']['headers\\strata\\core\\mtp.hpp']
    out.mkdir(parents=True, exist_ok=False)
    cfg = json.loads((H/'config.json').read_text())
    short = json.loads((R/'bench/results/a770/cli-decode-entry-2026-10-24/results.json').read_text())['short_prompt']
    prompts = {'single': [271], 'two': [271, 91], 'seven': short[:7], 'eight': short[:8], 'short': short}
    data = {'complete': False, 'scope': 'Correctness only; trace enabled, no performance claim.',
            'config': cfg, 'prompts': prompts, 'cli': [], 'serve': [], 'cpu': cpu(out),
            'source_sha256': b.sha(H/'candidate.cpp'),
            'mtp_header_sha256': b.sha(H/'headers/strata/core/mtp.hpp'),
            'exe_sha256': {side: b.sha(path) for side, path in [('baseline', P/'candidate/strata.exe'), ('candidate', H/'candidate/strata.exe')]}}
    env = b.environment(cfg, reproducible=True)
    env.update(STRATA_SPEC_COUPLED='1', STRATA_TRACE='1')
    data['environment'] = {k: v for k, v in env.items() if k.startswith(('STRATA_', 'SYCL_', 'ONEAPI_', 'UR_', 'ZES_'))}
    args = b.model_args(cfg, reproducible=True, cache_mib=5500)+['--kv', 'int8', '--stop-eos']
    args[args.index('--prefill')+1] = '1'
    sampling = 'temperature=0.7 top_p=0.9 top_k=20 seed=1234'
    refs = {}
    def save():
        b.save(out/'results.json', data)
    save()
    for side, exe in [('baseline', P/'candidate/strata.exe'), ('candidate', H/'candidate/strata.exe')]:
        engine = b.Engine(exe, args, env, out/(side+'-serve'))
        run = {'side': side, 'args': args, 'runtime_info': engine.info, 'requests': []}
        data['serve'].append(run)
        try:
            labels = list(prompts) if side == 'baseline' else ['two', 'short']
            for label in labels:
                for sampled in (False, True) if label in ('two', 'short') else (False,):
                    name = label+('-sampled' if sampled else '-greedy')
                    row = engine.gen(prompts[label], 6, sampling if sampled else '')
                    row.update(name=name, prompt=prompts[label], keys=sampling if sampled else '')
                    run['requests'].append(row)
                    save()
                    assert 0 < row['generated'] <= 6 and row['finish'] in ('length', 'stop'), row
                    assert row['finish'] == ('stop' if row['tokens'][-1] in (248044, 248046) else 'length'), row
                    assert refs.setdefault(name, row['tokens']) == row['tokens'], row
                    print('PASS SERVE', side, name, flush=True)
        finally:
            engine.close()
    cases = [('single', False, .5, 6, None, False, False), ('two', False, .5, 6, 1, False, False),
             ('seven', False, .5, 6, 6, False, False), ('eight', False, .5, 6, 7, False, False),
             ('short', False, .5, 6, None, False, False), ('short', True, .5, 6, None, False, False),
             ('short', False, 0, 6, None, False, False), ('short', True, 0, 6, None, False, False),
             ('short', False, .5, 6, None, True, False), ('two', False, .5, 6, 0, False, False),
             ('two', False, .5, 6, None, False, True)]
    for index, (label, sampled, threshold, cap, short_read, eos, no_mtp) in enumerate(cases):
        name = f'candidate-cli-{index}-{label}'
        options = args.copy()
        options[options.index('--spec-min-p')+1] = str(threshold)
        if short_read is not None:
            options[options.index('--short-read')+1] = str(short_read)
        if no_mtp:
            at = options.index('--mtp')
            del options[at:at+2]
        if sampled:
            options += ['--seed', '1234', '--temperature', '0.7', '--top-k', '20', '--top-p', '0.9']
        expected = refs[label+('-sampled' if sampled else '-greedy')]
        if eos:
            options += ['--eos-ids', str(expected[0])]
            expected = expected[:1]
        command = [str(H/'candidate/strata.exe'), *options, '--tokens', ','.join(map(str, prompts[label])), '--max-new', str(cap)]
        print('RUN CLI', name, flush=True)
        with (out/(name+'.stdout')).open('xb') as stdout, (out/(name+'.stderr')).open('xb') as stderr:
            result = subprocess.run(command, cwd=R, env=env, stdout=stdout, stderr=stderr, timeout=240)
        text = (out/(name+'.stdout')).read_text()
        trace = (out/(name+'.stderr')).read_text()
        tokens = list(map(int, re.search(r'^output\s+:(.*)$', text, re.M)[1].split()))
        windows = 'through verify windows' in trace
        row = {'name': name, 'command': command, 'returncode': result.returncode, 'tokens': tokens,
               'expected': expected, 'threshold': threshold, 'sampled': sampled, 'eos': eos, 'no_mtp': no_mtp,
               'prompt_windows': windows, 'cache': re.search(r'expert cache (\d+) slots, ([\d.]+) GiB', trace).groups()}
        data['cli'].append(row)
        save()
        result.check_returncode()
        assert tokens == expected, row
        assert windows == (label != 'single' and short_read != 0), row
        assert ('prompt path allocates' in trace) == (label == 'two' and short_read == 0), row
        assert int(re.search(r'^decode\s+(\d+) tokens', text, re.M)[1]) == len(tokens)
        assert not re.search(r'\b(?:nan|inf)\b', text, re.I)
        print('PASS CLI', name, tokens, flush=True)
    runner.verify()
    data['complete'] = True
    data['raw_sha256'] = {p.relative_to(out).as_posix(): b.sha(p) for p in out.rglob('*')
                          if p.suffix in ('.stdout', '.stderr', '.protocol', '.cpp')}
    save()
    print('PASS CLI short prompt checks', flush=True)


if __name__ == '__main__':
    with b.gpu_lock():
        main(Path(sys.argv[1]) if len(sys.argv) > 1 else H/'checks-repeat')
