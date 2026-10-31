"""One isolated candidate, reusing the exact preceding clean baseline and dependencies."""
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys

H = Path(__file__).resolve().parent
R = H.parent
P = R/'build-sycl-mtp-terminal-20261024'
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


def main():
    with b.gpu_lock():
        verify()
        for name in ('sycl/src/program/generate.cpp', 'src/program/generate.cpp'):
            target = H/'after'/name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(R/name, target)
            before = (H/'before'/name).read_text()
            expected = before.replace('int64_t spec_pos = 0;', 'int64_t spec_pos = -1;').replace(
                '(0 = not used)', '(-1 = not used; position 0 is valid)').replace(
                'if (spec_pos > 0 &&', 'if (spec_pos >= 0 &&')
            assert target.read_text() == expected, name
        shutil.copyfile(H/'after/sycl/src/program/generate.cpp', H/'candidate.cpp')
        output = H/'candidate'
        output.mkdir(exist_ok=False)
        previous = json.loads((P/'build.json').read_text())
        records = {'complete': False, 'commands': [], 'files_sha256': {},
                   'baseline_exe': str(P/'candidate/strata.exe'),
                   'method': 'Previous clean candidate is the baseline. Recompile only the generator; identical frozen headers, MTP/sampler/verifier objects and archives.'}
        templates = dict(previous['commands'])
        for stage in ('compile', 'link'):
            cmd = templates[stage+'-candidate']
            for old, new in [(P/'candidate.cpp', H/'candidate.cpp'), (P/'candidate.obj', H/'candidate.obj'),
                             (P/'candidate.pdb', H/'candidate.pdb'), (P/'candidate', output)]:
                cmd = cmd.replace(str(old), str(new))
            records['commands'].append([stage, cmd])
            b.save(H/'build.json', records)
            print('BUILD', stage, flush=True)
            with (H/(stage+'.log')).open('x', encoding='utf-8') as log:
                p = subprocess.run(cmd, cwd=R/'build-sycl-deepinfer-20261023/candidate',
                                   env=builder.environment(), stdout=log, stderr=subprocess.STDOUT, timeout=1200)
            if p.returncode:
                print((H/(stage+'.log')).read_text(errors='replace')[-6000:], flush=True)
            p.check_returncode()
        verify()
        for path in (H/'candidate.cpp', H/'candidate.obj', output/'strata.exe'):
            records['files_sha256'][str(path.relative_to(H))] = b.sha(path)
        records['complete'] = True
        b.save(H/'build.json', records)
        print('PASS isolated candidate build', flush=True)


if __name__ == '__main__':
    main()
