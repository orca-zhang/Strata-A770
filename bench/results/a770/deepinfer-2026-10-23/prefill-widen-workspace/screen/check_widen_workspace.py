"""Isolated 32/128 MiB widening workspace checks and serial A/B/A/B/A screening."""
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
import build

F = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else H / 'prefill-widen-workspace-v2'
P = H / 'prefill-widen-f16-stride/build'
F.mkdir(exist_ok=False)
(F / 'check_widen_workspace.py').write_bytes(Path(__file__).read_bytes())
paths = {'include/strata/prefill/gemm.hpp': 'include/strata/prefill/gemm.hpp',
         'gemm.dp.cpp': 'sycl/src/prefill/gemm.dp.cpp',
         'gemm_parity.cpp': 'sycl/src/prefill/gemm_parity.cpp', 'prefill.cpp': 'sycl/src/prefill/prefill.cpp'}
for dst, src in paths.items():
    target = F / 'before' / dst
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes((R / src).read_bytes())
    if dst != 'gemm.dp.cpp':
        assert target.read_bytes() == (P / dst).read_bytes(), src
(F / 'workspace-before.diff').write_bytes(subprocess.check_output(['git', 'diff', '--binary'], cwd=R, stderr=subprocess.DEVNULL))
cfg = json.loads((H / 'config-original.json').read_text(encoding='utf-8-sig'))
protected = [Path(cfg['exe']), R / 'strata-iq2_xs.json']
deps = json.loads((H / 'f16-stride-before/manifest.json').read_text())['dependencies']
assert all(b.sha(H / 'baseline' / p) == v for p, v in deps.items())
state = {'complete': False, 'purpose': 'Expand only opted-in GPU widening workspace from 32 to 128 MiB',
         'source_sha256': {src: b.sha(R / src) for src in paths.values()},
         'dependencies': deps, 'production_sha256': {str(p): b.sha(p) for p in protected},
         'baseline_build_sha256': b.sha(P / 'build.json'), 'runs': [], 'comparisons': []}
b.save(F / 'results.json', state)
source = F / 'source'
shutil.copytree(F / 'before', source)

def replace(path, old, new):
    data = path.read_text(encoding='utf-8')
    assert data.count(old) == 1, (path, old)
    path.write_text(data.replace(old, new), encoding='utf-8')

gate = '''[] {
        const char* f16 = std::getenv("STRATA_PREFILL_GPU_WIDEN");
        const char* bf16 = std::getenv("STRATA_PREFILL_BF16_GPU_WIDEN");
        return ((f16 && std::strcmp(f16, "1") == 0) || (bf16 && std::strcmp(bf16, "1") == 0))
                   ? 128u << 20 : 32u << 20;
    }()'''
replace(source / 'gemm.dp.cpp', 'const size_t ws = 32u << 20;',
        '// ponytail: keep the old budget unless GPU widening is explicitly enabled.\n    const size_t ws = ' + gate + ';')
replace(source / 'prefill.cpp', 'constexpr size_t GEMM_WS = 32u << 20;               // cuBLAS workspace',
        '// ponytail: bytes_needed, init and relayout must count the same widening workspace.\nconst size_t GEMM_WS = ' + gate + ';')
test = source / 'gemm_parity.cpp'
replace(test, '    auto& q = dpct::get_in_order_queue();',
        '    const bool large = argc > 1 && std::strcmp(argv[argc - 1], "--large") == 0;\n    auto& q = dpct::get_in_order_queue();')
replace(test, '(256u<<20),q)', '(512u<<20),q)')
replace(test, '32u<<20,error)', '(large ? 128u : 32u)<<20,error)')
replace(test, '        if (bench_only && output_padding) continue;', '        if ((bench_only || large) && output_padding) continue;')
replace(test, '''            for (auto dims : {std::pair{1280,2560}, std::pair{640,2560}, std::pair{2560,640}, std::pair{17,65}, std::pair{4096,2560},
                              std::pair{320,10240}, std::pair{10240,320}, std::pair{1,2560}}) {''',
'''            const auto shapes = large
                ? std::vector<std::pair<int,int>>{{4096,2560},{6144,2560},{10240,2560},{12288,2560},{16384,2560}}
                : std::vector<std::pair<int,int>>{{1280,2560},{640,2560},{2560,640},{17,65},{4096,2560},
                                                  {320,10240},{10240,320},{1,2560}};
            for (auto dims : shapes) {''')
replace(test, 'if (bench_only && n!=1280) continue;', 'if (bench_only && (large ? n!=4096 && n!=12288 : n!=1280)) continue;')
replace(test, 'size_t(n)*k*2 > (48u<<20)', 'size_t(n)*k*2 > (96u<<20)')
for old, new in [('176u','352u'),('208u','416u')]:
    replace(test, 'arena+arena_offset+(' + old + '<<20)', 'arena+arena_offset+(' + new + '<<20)')
# X/W each appear once; avoid replacing the new workspace-size constant.
replace(test, 'arena+arena_offset+(112u<<20)', 'arena+arena_offset+(224u<<20)')
replace(test, 'arena+arena_offset+(128u<<20)', 'arena+arena_offset+(240u<<20)')
replace(test, '''batch,submit.back(),total.back());''', '''batch,submit.back(),total.back(),n,k);''')
replace(test, '''\\"total_us\\":%.6f}\\n",
                                        bf16?"true":"false",batch''', '''\\"total_us\\":%.6f,\\"n\\":%d,\\"k\\":%d}\\n",
                                        bf16?"true":"false",batch''')
replace(test, '''widen?"true":"false",submit[2],total[2]);''', '''widen?"true":"false",submit[2],total[2],n,k);''')
replace(test, '''\\"total_us\\":%.6f}\\n",
                                    bf16?"true":"false",widen''', '''\\"total_us\\":%.6f,\\"n\\":%d,\\"k\\":%d}\\n",
                                    bf16?"true":"false",widen''')

def check(exe, args, env, label, expected):
    b.busy()
    print('CHECK', label, flush=True)
    p = subprocess.run([str(exe), *args], cwd=R, env=env, capture_output=True, text=True, timeout=240)
    (F / (label + '.log')).write_text(p.stdout + p.stderr, encoding='utf-8')
    summary = [json.loads(v) for v in re.findall(r'^SUMMARY (.+)$', p.stdout, re.M)]
    state.setdefault('large_checks', []).append({'label': label, 'args': args, 'returncode': p.returncode, 'summary': summary})
    b.save(F / 'results.json', state)
    p.check_returncode()
    assert summary == [{'checks': expected, 'mismatched_cases': 0}]

with b.gpu_lock():
    with patch.object(b, 'gpu_lock', contextlib.nullcontext):
        sys.argv = [str(H / 'build_prefill_widen.py'), '--source-build', str(source), '--reuse', str(P),
                    '--checks', '1015', '--out', str(F / 'build')]
        runpy.run_path(sys.argv[0], run_name='__main__')
    candidate = F / 'build'
    manifest = json.loads((candidate / 'build.json').read_text())
    baseline = F / 'baseline'
    (baseline / 'parity').mkdir(parents=True)
    compile_cmd = dict(manifest['commands'])['gemm'].replace(str(candidate / 'gemm.dp.cpp'), str(F / 'before/gemm.dp.cpp'))
    compile_cmd = compile_cmd.replace(str(candidate / 'gemm.obj'), str(baseline / 'gemm.obj'))
    compile_cmd = compile_cmd.replace(str(candidate / 'gemm.pdb'), str(baseline / 'gemm.pdb'))
    state['baseline_compile_command'] = compile_cmd
    print('COMPILE current baseline without the workspace change', flush=True)
    with (baseline / 'compile.log').open('x') as log:
        subprocess.run(compile_cmd, cwd=H / 'baseline', env=build.environment(), stdout=log, stderr=subprocess.STDOUT, check=True)
    command = dict(manifest['commands'])['link-parity'].replace(str(candidate / 'gemm.obj'), str(baseline / 'gemm.obj'))
    command = command.replace(str(candidate / 'prefill.obj'), str(P / 'prefill.obj'))
    command = command.replace(str(candidate / 'parity') + '\\', str(baseline / 'parity') + '\\')
    command = command.replace('--intdir=' + str(candidate / 'parity'), '--intdir=' + str(baseline / 'parity'))
    state['baseline_link_command'] = command
    b.save(F / 'results.json', state)
    print('LINK baseline with the same test object', flush=True)
    with (baseline / 'link.log').open('x') as log:
        subprocess.run(command, cwd=H / 'baseline', env=build.environment(), stdout=log, stderr=subprocess.STDOUT, check=True)
    env = b.environment(cfg)
    env.update(STRATA_PREFILL_GPU_WIDEN='1', STRATA_PREFILL_BF16_GPU_WIDEN='1',
               STRATA_PREFILL_HALF_OUTPUT='0', STRATA_PREFILL_HALF_BF16='0')
    state['env'] = {k:v for k,v in env.items() if k.startswith(('STRATA_','SYCL_','UR_','ONEAPI_','ZES_'))}
    for arena in (None, '1', '4'):
        selected = env.copy()
        if arena: selected['STRATA_GEMM_SHARED_ARENA'] = arena
        check(candidate / 'parity/strata.exe', ['--large'], selected, 'large-' + (arena or 'normal'), 270)
    for i, cand in enumerate((False, True, False, True, False)):
        b.busy()
        exe = (candidate if cand else baseline) / 'parity/strata.exe'
        print('BENCH', i, cand, flush=True)
        p = subprocess.run([str(exe), '--bench', '--large'], cwd=R, env=env, capture_output=True, text=True, timeout=240)
        (F / f'operator-{i}.log').write_text(p.stdout + p.stderr, encoding='utf-8')
        row = {'candidate': cand, 'exe_sha256': b.sha(exe), 'returncode': p.returncode,
               'benches': [json.loads(v) for v in re.findall(r'^BENCH (.+)$', p.stdout, re.M)],
               'batches': [json.loads(v) for v in re.findall(r'^BATCH (.+)$', p.stdout, re.M)],
               'summary': [json.loads(v) for v in re.findall(r'^SUMMARY (.+)$', p.stdout, re.M)]}
        state['runs'].append(row)
        b.save(F / 'results.json', state)
        p.check_returncode()
        assert row['summary'] == [{'checks': 12, 'mismatched_cases': 0}]
        assert len(row['benches']) == 4 and len(row['batches']) == 20
    for i in (1, 3):
        for bf16 in (False, True):
            for n in (4096, 12288):
                times = [next(v['total_us'] for v in state['runs'][j]['benches'] if v['bf16']==bf16 and v['n']==n) for j in (i-1,i,i+1)]
                pair = b.paired(*times)
                state['comparisons'].append({'candidate_run': i, 'bf16': bf16, 'n': n, **pair,
                                             'gate': pair['throughput_change_pct']>=3 and abs(pair['baseline_drift_pct'])<=3})
    b.busy()
assert state['source_sha256'] == {src:b.sha(R/src) for src in paths.values()}
assert all(b.sha(Path(p)) == v for p,v in state['production_sha256'].items())
assert all(b.sha(H/'baseline'/p) == v for p,v in deps.items())
state.update(complete=True, production_unchanged=True, workspace_sources_unchanged=True,
             candidate_build_sha256=b.sha(candidate/'build.json'), unchanged_dependencies=len(deps))
b.save(F/'results.json', state)
print(json.dumps(state['comparisons'], indent=2), flush=True)
