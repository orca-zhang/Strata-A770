"""Screen one final wait for opt-in GPU widening; frozen inputs, no production writes."""
import contextlib
import json
import re
import runpy
import shutil
import subprocess
import sys
from pathlib import Path
from unittest.mock import patch

H = Path(__file__).resolve().parent
R = H.parent
sys.path.insert(0, str(R / 'tools'))
import bench_deepinfer as b

P = H / 'prefill-widen-workspace-confirm/build'
F = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else H / 'prefill-widen-queue'
F.mkdir(exist_ok=False)
(F / 'check_widen_queue.py').write_bytes(Path(__file__).read_bytes())
cfg = json.loads((H / 'config-original.json').read_text(encoding='utf-8-sig'))
files = {'include/strata/prefill/gemm.hpp': 'include/strata/prefill/gemm.hpp',
         'gemm.dp.cpp': 'sycl/src/prefill/gemm.dp.cpp',
         'prefill.cpp': 'sycl/src/prefill/prefill.cpp',
         'gemm_parity.cpp': 'sycl/src/prefill/gemm_parity.cpp'}
state = {'complete': False, 'purpose': 'Remove the conversion and GEMM host waits; retain final copy wait and in-order queue guard',
         'source_sha256': {live: b.sha(R / live) for live in files.values()},
         'baseline_build_sha256': b.sha(P / 'build.json'),
         'production_sha256': {str(p): b.sha(p) for p in (Path(cfg['exe']), R / 'strata-iq2_xs.json')},
         'dependencies': json.loads((H / 'f16-stride-before/manifest.json').read_text())['dependencies'],
         'runs': [], 'comparisons': [], 'large_checks': []}
baseline_manifest = json.loads((P / 'build.json').read_text())
for name, live in files.items():
    assert b.sha(P / name) == baseline_manifest['sources'][live], live
    dst = F / 'source' / name
    dst.parent.mkdir(parents=True, exist_ok=True)
    dst.write_bytes((P / name).read_bytes())
for path, digest in state['dependencies'].items():
    assert b.sha(H / 'baseline' / path) == digest, path
b.save(F / 'results.json', state)

gemm = F / 'source/gemm.dp.cpp'
source = gemm.read_bytes()
old = b'''    // ponytail: retain the old API's blocking behavior while testing mapped/pool input lifetimes.\r
    // Replace these waits with explicit dependencies only after queued-chain and model parity pass.\r
    q->wait_and_throw();\r
'''
new = b'''    // ponytail: the in-order queue orders conversion, GEMM and copy; the final wait protects all input lifetimes.\r
'''
assert source.count(old) == 1
source = source.replace(old, new)
old = b'oneapi::mkl::blas::compute_mode::unset).wait_and_throw();\r\n    q->memcpy(Y, output.get()'
assert source.count(old) == 1
source = source.replace(old, b'oneapi::mkl::blas::compute_mode::unset);\r\n    q->memcpy(Y, output.get()')
gemm.write_bytes(source)

def env():
    e = b.environment(cfg)
    e.update(STRATA_PREFILL_GPU_WIDEN='1', STRATA_PREFILL_BF16_GPU_WIDEN='1',
             STRATA_PREFILL_HALF_OUTPUT='0', STRATA_PREFILL_HALF_BF16='0', STRATA_PREFILL_HALF_PROFILE='0')
    return e

def parse(p):
    return {'returncode': p.returncode,
            **{key: [json.loads(v) for v in re.findall(r'^' + tag + r' (.+)$', p.stdout, re.M)]
               for key, tag in [('benches', 'BENCH'), ('batches', 'BATCH'), ('summary', 'SUMMARY')]}}

with b.gpu_lock():
    with patch.object(b, 'gpu_lock', contextlib.nullcontext):
        sys.argv = [str(H / 'build_prefill_widen.py'), '--source-build', str(F / 'source'),
                    '--reuse', str(P), '--checks', '1015', '--out', str(F / 'build')]
        runpy.run_path(sys.argv[0], run_name='__main__')
    C = F / 'build'
    state['candidate_build_sha256'] = b.sha(C / 'build.json')
    state['env'] = {k: v for k, v in env().items() if k.startswith(('STRATA_', 'SYCL_', 'UR_', 'ONEAPI_', 'ZES_'))}
    for arena in (None, '1', '4'):
        b.busy()
        e = env()
        if arena: e['STRATA_GEMM_SHARED_ARENA'] = arena
        label = 'large-' + (arena or 'normal')
        print('CHECK', label, flush=True)
        p = subprocess.run([str(C / 'parity/strata.exe'), '--large'], cwd=R, env=e, capture_output=True, text=True, timeout=240)
        (F / ('check-' + label + '.log')).write_text(p.stdout + p.stderr, encoding='utf-8')
        row = {'label': label, 'arena': arena, **parse(p)}
        state['large_checks'].append(row)
        b.save(F / 'results.json', state)
        p.check_returncode()
        assert row['summary'] == [{'checks': 270, 'mismatched_cases': 0}]
    for shape, args, count in [('small', ['--bench'], 6), ('large', ['--bench', '--large'], 12)]:
        runs = []
        for i, candidate in enumerate((False, True, False, True, False)):
            b.busy()
            exe = (C if candidate else P) / 'parity/strata.exe'
            print('BENCH', shape, i, candidate, flush=True)
            p = subprocess.run([str(exe), *args], cwd=R, env=env(), capture_output=True, text=True, timeout=240)
            (F / f'{shape}-{i}.log').write_text(p.stdout + p.stderr, encoding='utf-8')
            row = {'shape': shape, 'candidate': candidate, 'exe_sha256': b.sha(exe), **parse(p)}
            runs.append(row)
            state['runs'].append(row)
            b.save(F / 'results.json', state)
            p.check_returncode()
            assert row['summary'] == [{'checks': count, 'mismatched_cases': 0}]
            assert len(row['benches']) == (2 if shape == 'small' else 4)
        for i in (1, 3):
            for bench in runs[i]['benches']:
                bf, n = bench['bf16'], bench['n']
                times = [next(v['total_us'] for v in runs[j]['benches'] if (v['bf16'], v['n']) == (bf, n)) for j in (i-1, i, i+1)]
                pair = b.paired(*times)
                state['comparisons'].append({'shape': shape, 'candidate_run': i, 'bf16': bf, 'n': n, **pair,
                                            'gate': pair['throughput_change_pct'] >= 3 and abs(pair['baseline_drift_pct']) <= 3})
        b.save(F / 'results.json', state)
    b.busy()
for path, digest in state['source_sha256'].items(): assert b.sha(R / path) == digest, path
for path, digest in state['production_sha256'].items(): assert b.sha(path) == digest, path
for path, digest in state['dependencies'].items(): assert b.sha(H / 'baseline' / path) == digest, path
state.update(complete=True, production_unchanged=True, workspace_sources_unchanged=True, unchanged_dependencies=len(state['dependencies']))
b.save(F / 'results.json', state)
print(json.dumps(state['comparisons'], indent=2), flush=True)
