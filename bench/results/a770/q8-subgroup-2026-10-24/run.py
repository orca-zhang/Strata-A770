import hashlib
import json
import math
from pathlib import Path
import re
import subprocess
import sys

H = Path(__file__).resolve().parent
R = H.parent
sys.path.insert(0, str(R / 'tools'))
import bench_deepinfer as b
sys.path.insert(0, str(R / 'build-sycl-deepinfer-20261023'))
import build

def save(path, value):
    path.write_text(json.dumps(value, indent=2), encoding='utf-8')

def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()

def command(args, name, env):
    print(name, flush=True)
    p = subprocess.run([str(a) for a in args], cwd=H, env=env, capture_output=True,
                       text=True, encoding='utf-8', errors='replace', timeout=600)
    (H / (name + '.log')).write_text(p.stdout + p.stderr, encoding='utf-8')
    p.check_returncode()
    return p.stdout

def main():
    env = build.environment()
    compile_args = ['/nologo', '/TP', '-DSTRATA_SYCL_SERIAL_Q8_1=0', '-D_USE_STD_VECTOR_ALGORITHMS=0',
                    '/DWIN32', '/D_WINDOWS', '/EHsc', '/O2', '/Ob2', '/DNDEBUG', '-Qstd:c++20', '-MD',
                    '-fsycl', '-fsycl-default-sub-group-size=32', '-fsycl-device-code-split=per_kernel',
                    '/fp:precise', '-Wno-unused-parameter', '-Wno-unused-variable', '-Wno-deprecated-declarations']
    compile_args += ['-I' + str(H / 'frozen' / x) for x in ('sycl/include', 'include', 'third_party/ggml')]
    icx = build.ICX / 'bin/icx.exe'
    commands = []
    def compile(src, obj, name):
        args = [icx, *compile_args, '/Fo' + str(obj), '-c', src]
        commands.append(list(map(str, args)))
        command(args, name, env)
    # Preserve the original runnable parity checks; filter only the timing copy to Q8.
    source = (H / 'kv_steps_parity.cpp').read_text()
    source = source.replace('for(int fmt=0;fmt<4;++fmt) for(int n', 'for(int fmt=0;fmt<4;++fmt) for(int n')
    source = source.replace('if(bench && mode>=2 && mode<=3 && fmt!=1)continue;', 'if(bench && fmt!=1)continue;')
    # Independent frozen-reference fingerprints catch a scalar/batched error shared by the candidate.
    source = source.replace('int checks=0;', 'int checks=0; uint64_t fingerprint=14695981039346656037ull;')
    source = source.replace('++checks;', 'for (uint8_t byte : data) { fingerprint ^= byte; fingerprint *= 1099511628211ull; } ++checks;')
    source = source.replace('return 0;\n} catch', 'std::printf("FINGERPRINT %016llx\\n", (unsigned long long)fingerprint); return 0;\n} catch')
    (H / 'kv_steps_check.cpp').write_text(source, encoding='utf-8')
    with b.gpu_lock():
        for test, src in [('steps', 'kv_steps_check.cpp'), ('q8', 'kv_q8_parity.cpp')]:
            compile(H / src, H / (test + '.obj'), 'compile-' + test)
        for side in ('baseline', 'candidate'):
            compile(H / side / 'kv_q8.dp.cpp', H / side / 'q8.obj', 'compile-' + side)
            for test in ('steps', 'q8'):
                args = [icx, '/nologo', H / (test + '.obj'), H / side / 'q8.obj',
                        '-fsycl', '-Xsycl-target-backend=spir64', '-cl-fp32-correctly-rounded-divide-sqrt',
                        '-fsycl-device-code-split=per_kernel', H / 'strata_kernels.lib', H / 'strata_core.lib',
                        '/link', '/out:' + str(H / side / (test + '.exe')), '/INCREMENTAL:NO']
                commands.append(list(map(str, args)))
                command(args, 'link-' + side + '-' + test, env)
        save(H / 'build.json', {'commands': commands, 'toolchain': str(icx),
                               'artifacts': {str(p.relative_to(H)): sha(p) for p in H.rglob('*') if p.suffix in ('.obj', '.exe', '.lib')}})
        # Use the same explicitly filtered A770 environment for both binaries.
        cfg = json.loads((R / 'strata-iq2_xs.json').read_text(encoding='utf-8-sig'))
        run_env = b.environment(cfg)
        run_env['PATH'] = env['PATH']
        checks = {}
        for side in ('baseline', 'candidate'):
            for test in ('steps', 'q8'):
                checks[side + '-' + test] = command([H / side / (test + '.exe')], 'check-' + side + '-' + test, run_env)
        assert re.search(r'FINGERPRINT (\w+)', checks['baseline-steps'])[1] == re.search(r'FINGERPRINT (\w+)', checks['candidate-steps'])[1]
        save(H / 'checks.json', checks)
        arms = []
        # Two independent A/B/A/B/A runs; record all negative and excluded results.
        for phase in ('screen', 'confirm'):
            for mode in (0, 1):
                group = []
                for arm, side in enumerate(('baseline', 'candidate', 'baseline', 'candidate', 'baseline')):
                    b.busy()
                    name = f'{phase}-mode{mode}-{arm}-{side}'
                    stdout = command([H / side / 'steps.exe', '--bench', str(mode)], name, run_env)
                    rows = [json.loads(line[7:]) for line in stdout.splitlines() if line.startswith('RESULT ')]
                    assert len(rows) == 3 and {r['T'] for r in rows} == {1, 4, 6}
                    entry = {'phase': phase, 'mode': mode, 'arm': arm, 'side': side, 'rows': rows, 'log': name + '.log'}
                    group.append(entry)
                    arms.append(entry)
                    save(H / 'raw.json', arms)
                for arm in (1, 3):
                    for row in group[arm]['rows']:
                        n = row['T']
                        a = next(x['us'] for x in group[arm - 1]['rows'] if x['T'] == n)
                        c = next(x['us'] for x in group[arm + 1]['rows'] if x['T'] == n)
                        p = b.paired(a, row['us'], c)
                        print(phase, mode, arm, n, p, flush=True)
    print('PASS checks and serial operator measurements', flush=True)

if __name__ == '__main__':
    main()
