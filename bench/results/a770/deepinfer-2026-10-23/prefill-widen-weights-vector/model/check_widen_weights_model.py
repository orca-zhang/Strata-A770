"""Functional Short checks for scalar/vector widening, fixed cache, no production writes."""
import json
import re
import subprocess
from pathlib import Path
import sys

H = Path(__file__).resolve().parent
R = H.parent
sys.path.insert(0, str(R / 'tools'))
import bench_deepinfer as b

C = Path(sys.argv[2]).resolve() if len(sys.argv) > 2 else H / 'prefill-widen-weights-vector/build'
F = Path(sys.argv[1]).resolve()
F.mkdir(exist_ok=False)
(F / Path(__file__).name).write_bytes(Path(__file__).read_bytes())
cfg = json.loads((H / 'config-original.json').read_text(encoding='utf-8-sig'))
ids = b.prompts(cfg)['Short']
assert len(ids) == 23
argv = b.model_args(cfg, True, .5, 4000)
for key, value in (('--prefill', '256'), ('--short-read', '0'), ('--prompt-cache', '128')):
    argv[argv.index(key)+1] = value
argv += ['--kv', 'int8']
keys = 'temperature=0.7 top_p=0.9 top_k=20 seed=1234 penalty_repeat=1.1 penalty_freq=0.1 penalty_present=0.2 penalty_last_n=128'
state = {'complete': False, 'functional_only': True, 'performance_acceptance': False,
         'input_tokens': ids, 'argv': argv, 'sampling': keys, 'runs': [],
         'production_sha256': {str(p): b.sha(p) for p in (Path(cfg['exe']), R / 'strata-iq2_xs.json')}}
b.save(F / 'results.json', state)
env = b.environment(cfg, reproducible=True)
env.update(STRATA_PREFILL_GPU_WIDEN='1', STRATA_PREFILL_BF16_GPU_WIDEN='1',
           STRATA_PREFILL_HALF_OUTPUT='0', STRATA_PREFILL_HALF_BF16='0',
           STRATA_PREFILL_HALF_PROFILE='0', STRATA_TRACE='1',
           STRATA_PREFILL_NATIVE_XMX='0', STRATA_PREFILL_XMX_PROFILE='0',
           UR_L0_USM_ALLOCATOR='1;512M;host:128M,8,64K;device:128M,8,64K;shared:128M,8,64K')
with b.gpu_lock():
    manifest = json.loads((C / 'build.json').read_text())
    assert manifest['complete'] and b.sha(C / 'parity/strata.exe') == manifest['exe_sha256']['parity']
    print('CHECK merged large 4GiB arena', flush=True)
    check_env = dict(env, STRATA_GEMM_SHARED_ARENA='4')
    proc = subprocess.run([str(C / 'parity/strata.exe'), '--large'], cwd=R, env=check_env,
                          capture_output=True, text=True, timeout=240)
    (F / 'large4.log').write_text(proc.stdout + proc.stderr, encoding='utf-8')
    state['large_check'] = {'returncode': proc.returncode,
        'summary': [json.loads(v) for v in re.findall(r'^SUMMARY (.+)$', proc.stdout, re.M)]}
    b.save(F / 'results.json', state)
    proc.check_returncode()
    assert state['large_check']['summary'] == [{'checks': 270, 'mismatched_cases': 0}]
    for label, root in (('baseline', H / 'prefill-widen-queue/build'),
                        ('candidate', C)):
        exe = root / 'model/strata.exe'
        manifest = json.loads((root / 'build.json').read_text())
        assert manifest['complete'] and b.sha(exe) == manifest['exe_sha256']['model']
        print('MODEL', label, flush=True)
        engine = b.Engine(exe, argv, env, F / label)
        row = {'label': label, 'info': engine.info, 'exe_sha256': b.sha(exe),
               'build_sha256': b.sha(root / 'build.json'), 'requests': [],
               'env': {k:v for k,v in env.items() if k.startswith(('STRATA_', 'SYCL_', 'UR_', 'ONEAPI_', 'ZES_'))}}
        state['runs'].append(row)
        b.save(F / 'results.json', state)
        try:
            assert 'expert_slots=4191 ' in engine.info and 'expert_cache_mib=5760 ' in engine.info
            for name, n, parameters in (('greedy', 8, ''), ('sample', 48, keys), ('sample-replay', 48, keys)):
                print(label, name, flush=True)
                result = engine.gen(ids, n, parameters)
                row['requests'].append({'name': name, **result})
                b.save(F / 'results.json', state)
                assert result['generated'] == n and any(t != 0 for t in result['tokens'])
        finally:
            engine.close()
        b.busy()
    for j in range(3):
        old, new = (r['requests'][j] for r in state['runs'])
        for key in ('tokens', 'generated', 'finish', 'accepted', 'offered'):
            assert old[key] == new[key], (j, key)
    for row in state['runs']:
        assert row['requests'][1]['tokens'] == row['requests'][2]['tokens']
for name, digest in state['production_sha256'].items():
    assert b.sha(name) == digest, name
state.update(complete=True, outputs_equal=True, replay_equal=True, production_unchanged=True)
b.save(F / 'results.json', state)
print('PASS Short greedy, seeded penalties and prompt-cache replay', flush=True)
