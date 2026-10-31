import argparse
import difflib
import json
import subprocess
import sys
import time
from pathlib import Path

H = Path(__file__).resolve().parent
R = H.parent
sys.path.insert(0, str(R / 'tools'))
import bench_deepinfer as b
import build

ap = argparse.ArgumentParser()
ap.add_argument('--out', type=Path, default=H/'prefill-trace')
ap.add_argument('--seconds', type=int, default=180)
a = ap.parse_args()
F = a.out.resolve()
B = H / 'baseline'
with b.gpu_lock():
    F.mkdir(exist_ok=False)
    (F / 'build_prefill_trace.py').write_bytes(Path(__file__).read_bytes())
    source = R / 'sycl/src/prefill/prefill.cpp'
    frozen = H / 'source/sycl/src/prefill/prefill.cpp'
    (F / 'prefill.cpp').write_bytes(source.read_bytes())
    (F / 'source.diff').write_text(''.join(difflib.unified_diff(
        frozen.read_text().splitlines(True), source.read_text().splitlines(True))), encoding='utf-8')
    rows = json.loads(subprocess.check_output(['ninja', '-C', str(B), '-t', 'compdb',
                      'CXX_COMPILER__strata_prefill_unscanned_Release'], text=True))
    row = next(r for r in rows if r['file'].replace('\\', '/').endswith('/prefill/prefill.cpp'))
    command = row['command'].replace(str(frozen), str(F / 'prefill.cpp'))
    command = command.replace('CMakeFiles\\strata_prefill.dir\\src\\prefill\\prefill.cpp.obj', str(F / 'prefill.obj'))
    command = command.replace('/FdCMakeFiles\\strata_prefill.dir\\strata_prefill.pdb', '/Fd' + str(F / 'prefill.pdb'))
    prior = json.loads((H / 'sampler-fp32/baseline/build.json').read_text())
    link = prior['command'].replace(str(H / 'sampler-fp32/sampler.obj'),
                                  str(H / 'sampler-fp32/sampler.obj') + ' ' + str(F / 'prefill.obj'))
    link = link.replace(str(H / 'sampler-fp32/baseline'), str(F))
    data = {'complete': False, 'purpose': 'Add submit traces only, without extra GPU waits or math changes',
            'compiler': 'Intel oneAPI 2025.3.2', 'source_sha256': b.sha(F / 'prefill.cpp'),
            'original_source_sha256': b.sha(frozen), 'original_exe_sha256': prior['exe_sha256'],
            'sampler_obj_sha256': b.sha(H / 'sampler-fp32/sampler.obj'), 'commands': [command, link]}
    b.save(F / 'build.json', data)
    for name, cmd in [('compile', command), ('link', link)]:
        print(name, flush=True)
        with (F / (name + '.log')).open('x') as log:
            p = subprocess.run(cmd, cwd=B, env=build.environment(), stdout=log, stderr=subprocess.STDOUT)
        p.check_returncode()
    data.update(complete=True, exe_sha256=b.sha(F / 'strata.exe'), obj_sha256=b.sha(F / 'prefill.obj'))
    b.save(F / 'build.json', data)
    cfg = json.loads((H / 'config-original.json').read_text(encoding='utf-8-sig'))
    argv = b.model_args(cfg, False, .5, 5500)
    argv[argv.index('--prefill') + 1] = '256'
    argv[argv.index('--short-read') + 1] = '0'
    argv += ['--kv', 'int8']
    env = b.environment(cfg)
    env.update(STRATA_TRACE='1', STRATA_WATCHDOG_S='35')
    ids = b.prompts(cfg)['Short']
    request = 'GEN 16 ' + ','.join(map(str, ids)) + '\nQUIT\n'
    (F / 'requests.in').write_text(request)
    result = {'complete': False, 'diagnostic_only': True, 'argv': argv,
              'env': {k: v for k, v in env.items() if k.startswith(('STRATA_', 'UR_', 'SYCL_', 'ONEAPI_', 'ZES_'))},
              'exe_sha256': data['exe_sha256'], 'prompt_tokens': len(ids), 'timeout_seconds': a.seconds}
    b.save(F / 'results.json', result)
    t0 = time.perf_counter()
    print('trace Short forced prefill', flush=True)
    with (F / 'baseline.protocol').open('x') as out, (F / 'baseline.stderr').open('x') as err:
        try:
            p = subprocess.run([str(F / 'strata.exe'), '--serve', *argv], cwd=R, env=env,
                               input=request, text=True, stdout=out, stderr=err, timeout=a.seconds)
            result.update(returncode=p.returncode, timeout=False)
        except subprocess.TimeoutExpired:
            result['timeout'] = True
    result.update(complete=True, wall_seconds=time.perf_counter()-t0,
                  protocol_sha256=b.sha(F/'baseline.protocol'), stderr_sha256=b.sha(F/'baseline.stderr'))
    result['last_log'] = (F / 'baseline.stderr').read_text(errors='replace').splitlines()[-25:]
    result['done'] = [s for s in (F / 'baseline.protocol').read_text().splitlines() if s.startswith('DONE ')]
    b.save(F / 'results.json', result)
    print(json.dumps(result, indent=2), flush=True)
