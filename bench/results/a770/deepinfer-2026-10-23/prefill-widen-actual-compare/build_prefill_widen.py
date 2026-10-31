"""Build/check GPU widening in isolation; reuse frozen engine objects and the FP32 sampler."""
import argparse
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

H = Path(__file__).resolve().parent
R = H.parent
sys.path.insert(0, str(R/'tools'))
import bench_deepinfer as b
import build

ap = argparse.ArgumentParser()
ap.add_argument('--out', required=True, type=Path)
ap.add_argument('--reuse', type=Path)
ap.add_argument('--gemm-source', type=Path)
a = ap.parse_args()
F = a.out.resolve()
B = H/'baseline'
with b.gpu_lock():
    F.mkdir(exist_ok=False)
    (F/'include/strata/prefill').mkdir(parents=True)
    (F/'build_prefill_widen.py').write_bytes(Path(__file__).read_bytes())
    files = {'include/strata/prefill/gemm.hpp': 'include/strata/prefill/gemm.hpp',
             'gemm.dp.cpp': 'sycl/src/prefill/gemm.dp.cpp',
             'gemm_parity.cpp': 'sycl/src/prefill/gemm_parity.cpp',
             'prefill.cpp': 'sycl/src/prefill/prefill.cpp'}
    for dst, src in files.items():
        (F/dst).write_bytes((a.gemm_source if dst == 'gemm.dp.cpp' and a.gemm_source else R/src).read_bytes())
    rows = json.loads(subprocess.check_output(['ninja','-C',str(B),'-t','compdb',
                       'CXX_COMPILER__strata_prefill_unscanned_Release'],text=True))
    commands = []
    reused_units = {}
    for label, source, template in [('gemm','gemm.dp.cpp','gemm.dp.cpp'),
                                    ('parity','gemm_parity.cpp','gemm.dp.cpp'),
                                    ('prefill','prefill.cpp','prefill.cpp')]:
        if a.reuse and (a.reuse/source).read_bytes() == (F/source).read_bytes() and (a.reuse/'include/strata/prefill/gemm.hpp').read_bytes() == (F/'include/strata/prefill/gemm.hpp').read_bytes():
            shutil.copyfile(a.reuse/(label+'.obj'), F/(label+'.obj'))
            reused_units[label] = {'from':str(a.reuse),'obj_sha256':b.sha(F/(label+'.obj'))}
            continue
        row = next(r for r in rows if r['file'].replace('\\','/').endswith('/prefill/'+template))
        cmd = row['command'].replace(str(H/'source/sycl/src/prefill'/template),str(F/source))
        cmd = cmd.replace(row['output'].replace('/','\\'),str(F/(label+'.obj')))
        cmd = cmd.replace('/nologo','/nologo -I'+str(F/'include'),1)
        cmd = cmd.replace('/FdCMakeFiles\\strata_prefill.dir\\strata_prefill.pdb','/Fd'+str(F/(label+'.pdb')))
        commands.append((label,cmd))
    prior = json.loads((H/'sampler-fp32/baseline/build.json').read_text())
    for name in ('parity','model'):
        (F/name).mkdir()
        if name == 'model' and 'gemm' in reused_units and 'prefill' in reused_units:
            shutil.copyfile(a.reuse/'model/strata.exe',F/'model/strata.exe')
            continue
        cmd = prior['command'].replace(str(H/'sampler-fp32/baseline'),str(F/name))
        cmd = cmd.replace(str(H/'sampler-fp32/sampler.obj'),
                          str(H/'sampler-fp32/sampler.obj')+' '+str(F/'gemm.obj')+' '+str(F/'prefill.obj'))
        # Same installed oneMKL version, DLL import avoids reprocessing its entire static device-code archive.
        cmd = cmd.replace('\\mkl_sycl.lib','\\mkl_sycl_blas_dll.lib')
        if name == 'parity':
            cmd = cmd.replace('CMakeFiles\\strata.dir\\src\\program\\generate.cpp.obj',str(F/'parity.obj'))
        commands.append(('link-'+name,cmd))
    libraries = ['strata_engine.lib','strata_prefill.lib','strata_spec.lib','strata_kernels.lib','strata_core.lib']
    manifest = {'complete':False, 'purpose':'Opt-in GPU widening before the same FP32 GEMM; no added device allocations',
                'compiler':'oneAPI 2025.3.2', 'oneMKL':'2025.3, DLL import on both test arms',
                'sources':{src:b.sha(F/dst) for dst,src in files.items()},
                'original_sources':{src:b.sha(H/'source'/src) for src in files.values() if (H/'source'/src).exists()},
                'sampler_obj_sha256':b.sha(H/'sampler-fp32/sampler.obj'),
                'reused_libraries':{s:b.sha(B/s) for s in libraries},'reused_units':reused_units,'commands':commands}
    b.save(F/'build.json',manifest)
    for label,cmd in commands:
        print(label,flush=True)
        with (F/(label+'.log')).open('x') as log:
            p = subprocess.run(cmd,cwd=B,env=build.environment(),stdout=log,stderr=subprocess.STDOUT)
        p.check_returncode()
    assert manifest['reused_libraries'] == {s:b.sha(B/s) for s in libraries}
    manifest.update(complete=True,exe_sha256={name:b.sha(F/name/'strata.exe') for name in ('parity','model')})
    b.save(F/'build.json',manifest)
    cfg = json.loads((H/'config-original.json').read_text(encoding='utf-8-sig'))
    results = {'complete':False,'runs':[]}
    b.save(F/'results.json',results)
    for i,enabled in enumerate((False,True)):
        env = b.environment(cfg)
        env['STRATA_PREFILL_GPU_WIDEN'] = str(int(enabled))
        argv = [str(F/'parity/strata.exe')]
        print('CHECK',i,enabled,flush=True)
        p = subprocess.run(argv,cwd=R,env=env,capture_output=True,text=True,timeout=180)
        (F/f'{i}-check.log').write_text(p.stdout+p.stderr)
        summary = [json.loads(s) for s in re.findall(r'^SUMMARY (.+)$',p.stdout,re.M)]
        row = {'widen':enabled,'argv':argv,'returncode':p.returncode,'summary':summary}
        results['runs'].append(row)
        b.save(F/'results.json',results)
        p.check_returncode()
        assert summary == [{'checks':219,'mismatched_cases':0}]
    results['complete'] = True
    b.save(F/'results.json',results)
    print('PASS',flush=True)
