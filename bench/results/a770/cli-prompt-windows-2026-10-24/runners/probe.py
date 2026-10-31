"""Bounded CLI cold-prefill diagnosis; keeps all raw output and changes no production file."""
import json
from pathlib import Path
import shutil
import subprocess
import sys
import time

H = Path(__file__).resolve().parent
R = H.parent
P = R/'build-sycl-mtp-summary-20261024'
sys.path.insert(0, str(R/'tools'))
import bench_deepinfer as b


def main():
    out = H/'probe'
    out.mkdir(exist_ok=False)
    frozen = json.loads((P/'frozen.json').read_text())
    built = json.loads((P/'build.json').read_text())
    deps = frozen['dependencies'].copy()
    exe = P/'candidate/strata.exe'
    deps[str(exe.relative_to(R))] = built['files_sha256']['candidate\\strata.exe']
    for name, digest in deps.items():
        assert b.sha(R/name) == digest, name
    sources = ('sycl/src/program/generate.cpp', 'src/program/generate.cpp',
               'sycl/src/prefill/prefill.cpp', 'docs/DEEPINFER_OPTIMIZATIONS.md')
    for name in sources:
        target = H/'before'/name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(R/name, target)
    shutil.copyfile(P/'config.json', H/'config.json')
    (H/'worktree-before.patch').write_bytes(subprocess.check_output(['git', 'diff', '--binary'], cwd=R))
    b.save(H/'frozen.json', {'before': {name: b.sha(H/'before'/name) for name in sources},
                           'dependencies': deps, 'baseline_exe': str(exe.relative_to(R)),
                           'config_sha256': b.sha(H/'config.json'), 'toolchain': frozen['toolchain']})
    cfg = json.loads((H/'config.json').read_text())
    env = b.environment(cfg, reproducible=True)
    env.update(STRATA_SPEC_COUPLED='1', STRATA_TRACE='1', STRATA_PREFILL_TIMING='1')
    args = b.model_args(cfg, reproducible=True, cache_mib=5500)
    args[args.index('--prefill')+1] = '1'
    command = [str(exe), *args, '--kv', 'int8', '--tokens', '271,91', '--max-new', '1']
    data = {'complete': False, 'command': command, 'timeout_s': 120,
            'scope': 'Diagnosis with tracing; not performance evidence.',
            'environment': {k: v for k, v in env.items() if k.startswith(('STRATA_', 'SYCL_', 'ONEAPI_', 'UR_', 'ZES_'))}}
    with b.gpu_lock():
        b.save(out/'results.json', data)
        print('RUN baseline two-token prefill1', flush=True)
        started = time.perf_counter()
        with (out/'baseline.stdout').open('xb') as stdout, (out/'baseline.stderr').open('xb') as stderr:
            try:
                result = subprocess.run(command, cwd=R, env=env, stdout=stdout, stderr=stderr, timeout=120)
                data['returncode'] = result.returncode
                data['timed_out'] = False
            except subprocess.TimeoutExpired:
                data['returncode'] = None
                data['timed_out'] = True
        data['elapsed_s'] = time.perf_counter()-started
        data['complete'] = True
        data['raw_sha256'] = {p.name: b.sha(p) for p in out.glob('baseline.*')}
        b.save(out/'results.json', data)
        print('RESULT', data['timed_out'], data['returncode'], flush=True)


if __name__ == '__main__':
    main()
