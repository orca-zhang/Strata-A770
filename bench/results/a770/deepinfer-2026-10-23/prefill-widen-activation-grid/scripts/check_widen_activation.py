"""Check a 2D activation-widening grid against the frozen weight-vector baseline."""
import contextlib
import json
import math
import re
import runpy
import statistics
import subprocess
import sys
from pathlib import Path
from unittest.mock import patch

H = Path(__file__).resolve().parent
R = H.parent
sys.path.insert(0, str(R / 'tools'))
import bench_deepinfer as b

P = H / 'prefill-widen-weights-vector-merged/build'
F = Path(sys.argv[1]).resolve()
F.mkdir(exist_ok=False)
(F / Path(__file__).name).write_bytes(Path(__file__).read_bytes())
cfg = json.loads((H / 'config-original.json').read_text(encoding='utf-8-sig'))
files = {'include/strata/prefill/gemm.hpp': 'include/strata/prefill/gemm.hpp',
         'gemm.dp.cpp': 'sycl/src/prefill/gemm.dp.cpp',
         'prefill.cpp': 'sycl/src/prefill/prefill.cpp',
         'gemm_parity.cpp': 'sycl/src/prefill/gemm_parity.cpp'}
manifest = json.loads((P / 'build.json').read_text())
state = {'complete': False, 'purpose': __doc__,
         'workspace_entry_sha256': {live: b.sha(R / live) for live in files.values()},
         'frozen_baseline_sha256': b.sha(P / 'build.json'),
         'production_sha256': {str(p): b.sha(p) for p in (Path(cfg['exe']), R / 'strata-iq2_xs.json')},
         'dependencies': json.loads((H / 'f16-stride-before/manifest.json').read_text())['dependencies'],
         'support_sha256': {str(p): b.sha(p) for p in (Path(__file__), H / 'build_prefill_widen.py',
             H / 'build.py', H / 'environment.cmd', R / 'tools/bench_deepinfer.py')},
         'retention_rule': 'Exact parity. Screen and independent confirmation: positive median gain in each candidate round, at least 75% drift-eligible pairs positive, no per-shape geometric regression beyond 1%. Adjacent baseline drift <=3%. No 3% gain requirement.',
         'runs': [], 'comparisons': [], 'large_checks': []}
for name, live in files.items():
    assert b.sha(P / name) == manifest['sources'][live], live
    for arm in ('baseline', 'candidate'):
        dst = F / (arm + '-source') / name
        dst.parent.mkdir(parents=True, exist_ok=True)
        dst.write_bytes((P / name).read_bytes())

# Benchmark-only changes: expose both window-sized and prefill-sized activation grids.
for arm in ('baseline', 'candidate'):
    path = F / (arm + '-source/gemm_parity.cpp')
    source = path.read_bytes()
    old = b'for (int t : boundary ?'
    new = b'for (int t : bench_only ? std::vector<int>{4,40,256} : boundary ?'
    assert source.count(old) == 1
    source = source.replace(old, new).replace(b'            if (bench_only && t!=4) continue;\r\n', b'')
    old = b'\\"n\\":%d,\\"k\\":%d}\\n"'
    new = b'\\"n\\":%d,\\"k\\":%d,\\"t\\":%d}\\n"'
    assert source.count(old) == 2
    source = source.replace(old, new)
    source = source.replace(b'total.back(),n,k);', b'total.back(),n,k,t);')
    source = source.replace(b'total[2],n,k);', b'total[2],n,k,t);')
    path.write_bytes(source)
path = F / 'candidate-source/gemm.dp.cpp'
source = path.read_bytes()
old = b'''    q->parallel_for(sycl::range<1>((size_t) T * K), [=](sycl::id<1> i) {
        const size_t offset = (size_t(i) / K) * ldx + size_t(i) % K;'''.replace(b'\n', b'\r\n')
new = b'''    q->parallel_for(sycl::range<2>(size_t(T), size_t(K)), [=](sycl::id<2> i) {
        const size_t offset = i[0] * ldx + i[1];'''.replace(b'\n', b'\r\n')
assert source.count(old) == 1
path.write_bytes(source.replace(old, new))
state['source_sha256'] = {arm: {name: b.sha(F / (arm + '-source') / name) for name in files}
                          for arm in ('baseline', 'candidate')}

def env():
    e = b.environment(cfg)
    e.update(STRATA_PREFILL_GPU_WIDEN='1', STRATA_PREFILL_BF16_GPU_WIDEN='1',
             STRATA_PREFILL_NATIVE_XMX='0', STRATA_PREFILL_XMX_PROFILE='0',
             STRATA_PREFILL_HALF_OUTPUT='0', STRATA_PREFILL_HALF_BF16='0', STRATA_PREFILL_HALF_PROFILE='0')
    e.pop('STRATA_GEMM_SHARED_ARENA', None)
    e['UR_L0_USM_ALLOCATOR'] = '1;512M;host:128M,8,64K;device:128M,8,64K;shared:128M,8,64K'
    return e

def execute(exe, args, log, expected, arena=None):
    b.busy()
    e = env()
    if arena: e['STRATA_GEMM_SHARED_ARENA'] = arena
    p = subprocess.run([str(exe), *args], cwd=R, env=e, capture_output=True, text=True, timeout=300)
    (F / log).write_text(p.stdout + p.stderr, encoding='utf-8')
    row = {'exe_sha256': b.sha(exe), 'args': args, 'log': log, 'log_sha256': b.sha(F / log),
           'returncode': p.returncode, 'arena': arena,
           **{key: [json.loads(v) for v in re.findall(r'^' + tag + r' (.+)$', p.stdout, re.M)]
              for key, tag in [('benches', 'BENCH'), ('batches', 'BATCH'), ('summary', 'SUMMARY')]}}
    p.check_returncode()
    assert row['summary'] == [{'checks': expected, 'mismatched_cases': 0}], row['summary']
    return row

def verify():
    for path, digest in state['support_sha256'].items(): assert b.sha(path) == digest, path
    for path, digest in state['production_sha256'].items(): assert b.sha(path) == digest, path
    for path, digest in state['dependencies'].items(): assert b.sha(H / 'baseline' / path) == digest, path

state['env'] = {k: v for k, v in env().items() if k.startswith(('STRATA_', 'SYCL_', 'UR_', 'ONEAPI_', 'ZES_'))}
b.save(F / 'results.json', state)
with b.gpu_lock():
    verify()
    for arm in ('baseline', 'candidate'):
        with patch.object(b, 'gpu_lock', contextlib.nullcontext):
            sys.argv = [str(H / 'build_prefill_widen.py'), '--source-build', str(F / (arm + '-source')),
                        '--reuse', str(P if arm == 'baseline' else F / 'baseline'),
                        '--checks', '1015', '--out', str(F / arm)]
            runpy.run_path(sys.argv[0], run_name='__main__')
    for arena in (None, '1', '4'):
        label = 'large-' + (arena or 'normal')
        print('CHECK', label, flush=True)
        state['large_checks'].append(execute(F / 'candidate/parity/strata.exe', ['--large'], label + '.log', 270, arena))
        b.save(F / 'results.json', state)
    for phase in ('screen', 'confirm'):
        for shape, args, count, points in [('small', ['--bench'], 18, 6), ('large', ['--bench', '--large'], 36, 12)]:
            runs = []
            for i, arm in enumerate(('baseline', 'candidate', 'baseline', 'candidate', 'baseline')):
                print('BENCH', phase, shape, i, arm, flush=True)
                row = {'phase': phase, 'shape': shape, 'arm': arm, 'ordinal': i,
                       **execute(F / arm / 'parity/strata.exe', args, f'{phase}-{shape}-{i}.log', count)}
                assert len(row['benches']) == points and len(row['batches']) == points * 5
                runs.append(row)
                state['runs'].append(row)
                b.save(F / 'results.json', state)
            for i in (1, 3):
                for bench in runs[i]['benches']:
                    key = (bench['bf16'], bench['n'], bench['k'], bench['t'])
                    times = [next(v['total_us'] for v in runs[j]['benches']
                                  if (v['bf16'], v['n'], v['k'], v['t']) == key) for j in (i-1, i, i+1)]
                    pair = b.paired(*times)
                    state['comparisons'].append({'phase': phase, 'shape': shape, 'candidate_run': i,
                        'bf16': key[0], 'n': key[1], 'k': key[2], 't': key[3], **pair,
                        'eligible': abs(pair['baseline_drift_pct']) <= 3})
            b.save(F / 'results.json', state)
        groups = [p for p in state['comparisons'] if p['phase'] == phase and p['eligible']]
        print('RESULT', phase, 'positive', sum(p['throughput_change_pct'] > 0 for p in groups), '/', len(groups),
              'round_medians', {i: statistics.median(p['throughput_change_pct'] for p in groups if p['candidate_run'] == i)
                                for i in (1, 3)}, flush=True)
    b.busy()
    verify()
state.update(complete=True, production_unchanged=True, unchanged_dependencies=len(state['dependencies']),
             workspace_exit_sha256={live: b.sha(R / live) for live in files.values()})
state['workspace_sources_unchanged'] = state['workspace_entry_sha256'] == state['workspace_exit_sha256']
b.save(F / 'results.json', state)
print('PASS frozen activation-grid experiment', flush=True)
