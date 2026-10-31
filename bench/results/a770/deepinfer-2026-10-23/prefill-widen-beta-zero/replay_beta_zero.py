"""Test one omitted beta=0 copy against the frozen FP16 stride fix; no production writes."""
import json
import re
import runpy
import shutil
import subprocess
import sys
from datetime import datetime
from pathlib import Path

H = Path(__file__).resolve().parent
R = H.parent
sys.path.insert(0, str(R / 'tools'))
import bench_deepinfer as b
import build

F = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else H / 'prefill-widen-beta-zero'
P = H / 'prefill-widen-f16-stride/build'
F.mkdir(exist_ok=False)
(F / 'check_beta_zero.py').write_bytes(Path(__file__).read_bytes())
sources = ['include/strata/prefill/gemm.hpp', 'sycl/src/prefill/gemm.dp.cpp',
           'sycl/src/prefill/gemm_parity.cpp', 'sycl/src/prefill/prefill.cpp',
           'sycl/src/kernels/cuda/sampler.dp.cpp', 'src/spec/draft_policy.cpp',
           'src/spec/draft_policy_test.cpp', 'include/strata/spec/draft_policy.hpp']
for path in sources:
    target = F / 'before' / path
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(R / path, target)
for name, args in [('workspace-before.diff', ['git', 'diff', '--binary']),
                   ('status-before.txt', ['git', 'status', '--short'])]:
    (F / name).write_bytes(subprocess.check_output(args, cwd=R))
cfg = json.loads((H / 'config-original.json').read_text(encoding='utf-8-sig'))
protected = [Path(cfg['exe']), R / 'strata-iq2_xs.json']
prior = json.loads((H / 'f16-stride-before/manifest.json').read_text())
deps = {p: b.sha(H / 'baseline' / p) for p in prior['dependencies']}
assert deps == prior['dependencies']
state = {'complete': False, 'date': datetime.now().isoformat(timespec='seconds'),
         'purpose': 'Screen omitting the initial output staging copy only for beta=0; FP32 math unchanged',
         'baseline_build_sha256': b.sha(P / 'build.json'),
         'source_sha256': {p: b.sha(R / p) for p in sources},
         'production_sha256': {str(p): b.sha(p) for p in protected},
         'dependencies': deps}
b.save(F / 'results.json', state)

cpu = F / 'cpu'
cpu.mkdir()
compiler_env = build.environment()
cmd = [shutil.which('cl.exe', path=compiler_env['PATH']), '/nologo', '/O2', '/EHsc', '/std:c++17', '/W4',
       '/I' + str(F / 'before/include'), str(F / 'before/src/spec/draft_policy.cpp'),
       str(F / 'before/src/spec/draft_policy_test.cpp'), '/Fe:' + str(cpu / 'draft_policy_test.exe')]
p = subprocess.run(cmd, cwd=cpu, env=compiler_env, capture_output=True, text=True)
(cpu / 'build.log').write_text(p.stdout + p.stderr, encoding='utf-8')
p.check_returncode()
p = subprocess.run([str(cpu / 'draft_policy_test.exe')], capture_output=True, text=True)
(cpu / 'checks.log').write_text(p.stdout + p.stderr, encoding='utf-8')
p.check_returncode()
state['draft_policy'] = {'command': cmd, 'exe_sha256': b.sha(cpu / 'draft_policy_test.exe'),
                         'checks': len(re.findall(r' ok$', p.stdout, re.M)), 'returncode': p.returncode}
b.save(F / 'results.json', state)
print('CPU', state['draft_policy']['checks'], 'PASS', flush=True)

frozen = F / 'source'
for relative in ('include/strata/prefill/gemm.hpp', 'gemm.dp.cpp', 'gemm_parity.cpp', 'prefill.cpp'):
    target = frozen / relative
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(P / relative, target)
gemm = frozen / 'gemm.dp.cpp'
old = b'q->memcpy(output.get(), Y, (size_t) T * ldy * sizeof(float));'
assert gemm.read_bytes().count(old) == 1
gemm.write_bytes(gemm.read_bytes().replace(old, b'if (beta != 0.0f) ' + old))

sys.argv = [str(H / 'build_prefill_widen.py'), '--source-build', str(frozen), '--reuse', str(P),
            '--checks', '1015', '--out', str(F / 'build')]
runpy.run_path(sys.argv[0], run_name='__main__')
state['candidate_build_sha256'] = b.sha(F / 'build/build.json')
state['operators'] = []
with b.gpu_lock():
    env = b.environment(cfg)
    env.update(STRATA_PREFILL_GPU_WIDEN='1', STRATA_PREFILL_BF16_GPU_WIDEN='1')
    state['environment'] = {k: v for k, v in env.items() if k.startswith(('STRATA_', 'SYCL_', 'UR_', 'ONEAPI_', 'ZES_'))}
    for i, candidate in enumerate((False, True, False, True, False)):
        b.busy()
        exe = (F / 'build' if candidate else P) / 'parity/strata.exe'
        print('BENCH', i, 'candidate' if candidate else 'baseline', flush=True)
        p = subprocess.run([str(exe), '--bench'], cwd=R, env=env, capture_output=True, text=True, timeout=180)
        (F / f'operator-{i}.log').write_text(p.stdout + p.stderr, encoding='utf-8')
        row = {'candidate': candidate, 'exe_sha256': b.sha(exe), 'returncode': p.returncode,
               'batches': [json.loads(s) for s in re.findall(r'^BATCH (.+)$', p.stdout, re.M)],
               'benches': [json.loads(s) for s in re.findall(r'^BENCH (.+)$', p.stdout, re.M)],
               'summary': [json.loads(s) for s in re.findall(r'^SUMMARY (.+)$', p.stdout, re.M)]}
        state['operators'].append(row)
        b.save(F / 'results.json', state)
        p.check_returncode()
        assert row['summary'] == [{'checks': 6, 'mismatched_cases': 0}]
        assert len(row['batches']) == 10 and len(row['benches']) == 2
    state['comparisons'] = []
    for i in (1, 3):
        for bf16 in (False, True):
            times = [next(s['total_us'] for s in state['operators'][j]['benches'] if s['bf16'] == bf16)
                     for j in (i-1, i, i+1)]
            pair = b.paired(*times)
            state['comparisons'].append({'candidate_run': i, 'bf16': bf16, **pair,
                                         'meets_screen': abs(pair['baseline_drift_pct']) <= 3 and pair['throughput_change_pct'] >= 3})
    b.busy()
assert state['source_sha256'] == {p: b.sha(R / p) for p in sources}
assert state['production_sha256'] == {str(p): b.sha(p) for p in protected}
assert state['dependencies'] == {p: b.sha(H / 'baseline' / p) for p in deps}
state.update(complete=True, production_unchanged=True, workspace_sources_unchanged=True,
             unchanged_dependencies=len(deps), performance_screen_only=True)
b.save(F / 'results.json', state)
print(json.dumps(state['comparisons'], indent=2), flush=True)
