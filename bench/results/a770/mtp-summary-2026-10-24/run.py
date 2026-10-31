"""Freeze, build one isolated generator, then check CLI summaries under one shared lock."""
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys

H = Path(__file__).resolve().parent
R = H.parent
P = R/'build-sycl-cli-entry-20261024'
sys.path.insert(0, str(R/'tools'))
import bench_deepinfer as b
spec = importlib.util.spec_from_file_location('builder', R/'build-sycl-deepinfer-20261023/build.py')
builder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)
SOURCES = ('sycl/src/program/generate.cpp', 'src/program/generate.cpp')
OLD = 'mtp.ms_draft / (double) mtp.rounds, (long long) mtp.rounds'
NEW = 'mtp.rounds > 0 ? mtp.ms_draft / (double) mtp.rounds : 0.0, (long long) mtp.rounds'


def freeze():
    assert not (H/'frozen.json').exists()
    prior = json.loads((P/'frozen.json').read_text())
    built = json.loads((P/'build.json').read_text())
    deps = prior['dependencies'].copy()
    deps[str((P/'candidate/strata.exe').relative_to(R))] = built['files_sha256']['candidate\\strata.exe']
    for name, digest in deps.items():
        assert b.sha(R/name) == digest, name
    for name in (*SOURCES, 'docs/DEEPINFER_OPTIMIZATIONS.md'):
        target = H/'before'/name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(R/name, target)
    assert b.sha(H/'before'/SOURCES[0]) == built['files_sha256']['candidate.cpp']
    shutil.copyfile(P/'config.json', H/'config.json')
    (H/'worktree-before.patch').write_bytes(subprocess.check_output(['git', 'diff', '--binary'], cwd=R))
    b.save(H/'frozen.json', {'before': {name: b.sha(H/'before'/name) for name in (*SOURCES, 'docs/DEEPINFER_OPTIMIZATIONS.md')},
                           'dependencies': deps, 'config_sha256': b.sha(H/'config.json'),
                           'baseline_exe': str((P/'candidate/strata.exe').relative_to(R)),
                           'toolchain': prior['toolchain'], 'scope': 'Only guard the CLI MTP summary denominator; no inference change.'})
    print('PASS frozen source and', len(deps), 'dependencies', flush=True)


def verify():
    frozen = json.loads((H/'frozen.json').read_text())
    for name, digest in frozen['dependencies'].items():
        assert b.sha(R/name) == digest, name
    assert b.sha(H/'config.json') == frozen['config_sha256']


def main():
    with b.gpu_lock():
        verify()
        for name in SOURCES:
            before = (H/'before'/name).read_text()
            assert before.count(OLD) == 1
            target = H/'after'/name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(R/name, target)
            assert target.read_text() == before.replace(OLD, NEW), name
        shutil.copyfile(H/'after'/SOURCES[0], H/'candidate.cpp')
        output = H/'candidate'
        output.mkdir(exist_ok=False)
        templates = dict(json.loads((P/'build.json').read_text())['commands'])
        build = {'complete': False, 'commands': [], 'files_sha256': {},
                 'method': 'Reuse the clean CLI-entry candidate as baseline; only recompile generator. Same frozen headers and MTP/sampler/verifier objects and archives.'}
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
        verify()
        build['files_sha256'] = {str(p.relative_to(H)): b.sha(p) for p in (H/'candidate.cpp', H/'candidate.obj', output/'strata.exe')}
        build['complete'] = True
        b.save(H/'build.json', build)
        print('PASS isolated candidate build', flush=True)
        spec = importlib.util.spec_from_file_location('summary_checks', H/'check.py')
        checks = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(checks)
        checks.main(H/'checks')
        verify()


if __name__ == '__main__':
    freeze() if '--freeze' in sys.argv else main()
