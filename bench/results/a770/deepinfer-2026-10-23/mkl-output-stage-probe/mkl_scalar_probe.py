import argparse
import json
import re
import subprocess
import sys
from pathlib import Path
H = Path(__file__).resolve().parent
R = H.parent
sys.path.insert(0, str(R/'tools'))
import bench_deepinfer as b
import build
ap = argparse.ArgumentParser()
ap.add_argument('--out', type=Path, default=H/'mkl-scalar-probe')
ap.add_argument('--source', type=Path, default=H/'mkl_scalar_probe.cpp')
ap.add_argument('--dynamic', action='store_true')
a = ap.parse_args()
F = a.out.resolve()
compiler = Path('C:/Program Files (x86)/Intel/oneAPI/compiler/2025.3/bin/icx.exe')
mkl = Path('C:/Program Files (x86)/Intel/oneAPI/mkl/2025.3')
with b.gpu_lock():
    F.mkdir(exist_ok=False)
    (F/'probe.cpp').write_bytes(a.source.read_bytes())
    (F/'mkl_scalar_probe.py').write_bytes(Path(__file__).read_bytes())
    cmd = [str(compiler), '/nologo', '/EHsc', '/O2', '/MD', '-Qstd:c++20', '-fsycl',
           '-fsycl-device-code-split=per_kernel', '/fp:precise', '-Wno-deprecated-declarations',
           '-I'+str(H/'source/sycl/include'), '-I'+str(mkl/'include'), '-c', str(F/'probe.cpp'),
           '/Fo'+str(F/'probe.obj')]
    link = [str(compiler), '/nologo', str(F/'probe.obj'), '-fsycl',
            '-fsycl-device-code-split=per_kernel', *[str(mkl/'lib'/name) for name in
           (('mkl_sycl_blas_dll.lib' if a.dynamic else 'mkl_sycl.lib'),'mkl_intel_lp64.lib','mkl_sequential.lib','mkl_core.lib')],
           '/link', '/out:'+str(F/'strata-mkl-scalar-probe.exe')]
    data = {'complete': False, 'purpose': 'Independent oneMKL probe',
            'source_sha256': b.sha(F/'probe.cpp'), 'commands': [cmd, link], 'runs': []}
    b.save(F/'results.json', data)
    print('compile/link scalar probe', flush=True)
    for label, command in [('compile', cmd), ('link', link)]:
        with (F/(label+'.log')).open('x') as log:
            p = subprocess.run(command, cwd=F, env=build.environment(), stdout=log, stderr=subprocess.STDOUT)
        p.check_returncode()
    cfg = json.loads((H/'config-original.json').read_text(encoding='utf-8-sig'))
    data['exe_sha256'] = b.sha(F/'strata-mkl-scalar-probe.exe')
    print('run scalar probe', flush=True)
    p = subprocess.run([str(F/'strata-mkl-scalar-probe.exe')], cwd=F, env=b.environment(cfg),
                       capture_output=True, text=True, timeout=180)
    (F/'probe.log').write_text(p.stdout+p.stderr)
    data.update(complete=True, returncode=p.returncode,
                queries=[json.loads(s) for s in re.findall(r'^QUERY (.+)$', p.stdout, re.M)],
                parity=[json.loads(s) for s in re.findall(r'^PARITY (.+)$', p.stdout, re.M)],
                benches=[json.loads(s) for s in re.findall(r'^BENCH (.+)$', p.stdout, re.M)],
                summary=[json.loads(s) for s in re.findall(r'^SUMMARY (.+)$', p.stdout, re.M)],
                runs=[json.loads(s) for s in re.findall(r'^GEMM (.+)$', p.stdout, re.M)])
    b.save(F/'results.json', data)
    print(p.stdout+p.stderr, flush=True)
    p.check_returncode()
