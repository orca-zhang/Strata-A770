"""Build only the isolated CLI generator, with frozen dependencies, then validate under one lock."""
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys

H = Path(__file__).resolve().parent
R = H.parent
P = R/'build-sycl-mtp-summary-20261024'
sys.path.insert(0, str(R/'tools'))
import bench_deepinfer as b
spec = importlib.util.spec_from_file_location('builder', R/'build-sycl-deepinfer-20261023/build.py')
builder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)


def verify():
    frozen = json.loads((H/'frozen.json').read_text())
    for name, digest in frozen['dependencies'].items():
        assert b.sha(R/name) == digest, name
    assert b.sha(H/'config.json') == frozen['config_sha256']
    assert b.sha(R/'src/program/generate.cpp') == frozen['before']['src/program/generate.cpp']
    assert b.sha(R/'sycl/src/prefill/prefill.cpp') == frozen['before']['sycl/src/prefill/prefill.cpp']


def main():
    with b.gpu_lock():
        verify()
        target = H/'after/sycl/src/program/generate.cpp'
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(R/'sycl/src/program/generate.cpp', target)
        shutil.copyfile(target, H/'candidate.cpp')
        output = H/'candidate'
        output.mkdir(exist_ok=False)
        templates = dict(json.loads((P/'build.json').read_text())['commands'])
        build = {'complete': False, 'commands': [], 'files_sha256': {},
                 'method': 'Only recompile the SYCL generator. Reuse all frozen headers, MTP/sampler/verifier objects, and archives from the preceding clean build.'}
        for stage in ('compile', 'link'):
            command = templates[stage]
            for old, new in [(P/'candidate.cpp', H/'candidate.cpp'), (P/'candidate.obj', H/'candidate.obj'),
                             (P/'candidate.pdb', H/'candidate.pdb'), (P/'candidate', output)]:
                command = command.replace(str(old), str(new))
            build['commands'].append([stage, command])
            b.save(H/'build.json', build)
            print('BUILD', stage, flush=True)
            with (H/(stage+'.log')).open('x', encoding='utf-8') as log:
                result = subprocess.run(command, cwd=R/'build-sycl-deepinfer-20261023/candidate',
                                        env=builder.environment(), stdout=log, stderr=subprocess.STDOUT, timeout=1200)
            if result.returncode:
                print((H/(stage+'.log')).read_text(errors='replace')[-5000:], flush=True)
            result.check_returncode()
        build['files_sha256'] = {str(p.relative_to(H)): b.sha(p) for p in (H/'candidate.cpp', H/'candidate.obj', output/'strata.exe')}
        build['complete'] = True
        b.save(H/'build.json', build)
        verify()
        assert b.sha(R/'sycl/src/program/generate.cpp') == b.sha(target)
        print('PASS isolated CLI prefill build', flush=True)
        spec = importlib.util.spec_from_file_location('checks', H/'check.py')
        checks = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(checks)
        checks.main(H/'checks-final')
        verify()


if __name__ == '__main__':
    main()
