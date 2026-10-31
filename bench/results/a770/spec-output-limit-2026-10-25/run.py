"""Build and test a two-statement output-window limit with frozen dependencies."""
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys

H = Path(__file__).resolve().parent
R = H.parent
P = R/'build-sycl-cli-prefill-final-20261024'
sys.path.insert(0, str(R/'tools'))
import bench_deepinfer as b


def module(path):
    spec = importlib.util.spec_from_file_location(path.stem, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def verify():
    data = json.loads((H/'frozen.json').read_text())
    for name, digest in data['dependencies'].items():
        if name == 'strata-iq2_xs.json' and (H/'config-external.json').exists():
            digest = b.sha(H/'config-external.json')
        assert b.sha(R/name) == digest, name
    for name, digest in data['protected'].items():
        if name == 'strata-iq2_xs.json' and (H/'config-external.json').exists():
            digest = b.sha(H/'config-external.json')
        assert b.sha(R/name) == digest, name
    return data


def freeze():
    assert not (H/'frozen.json').exists()
    prior = json.loads((P/'frozen.json').read_text())
    built = json.loads((P/'build.json').read_text())
    source = R/'sycl/src/program/generate.cpp'
    assert b.sha(source) == built['files_sha256']['candidate.cpp']
    before = source.read_text()
    needle = '                const bool timed_round = !first_window;'
    assert before.count(needle) == 1
    after = before.replace(needle,
        '                // ponytail: verify only rows that can still be returned to the caller.\n'
        '                T = (int) std::min<int64_t>(T, max_new - produced_n);\n'+needle)
    needle = '            const bool timed_round = !first_window;'
    # Match the CLI indentation exactly; the service statement has four more spaces.
    assert after.count('\n'+needle) == 1
    after = after.replace('\n'+needle,
        '\n            // ponytail: verify only rows that can still be returned to the caller.\n'
        '            T = (int) std::min<int64_t>(T, o.max_new - (int64_t) produced.size());\n'+needle)
    (H/'before.cpp').write_bytes(source.read_bytes())
    (H/'candidate.cpp').write_text(after)
    cfg = json.loads((R/'strata-iq2_xs.json').read_text())
    b.save(H/'config.json', cfg)
    deps = prior['dependencies'].copy()
    for path in (P/'candidate/strata.exe', P/'headers/strata/core/mtp.hpp'):
        deps[str(path.relative_to(R))] = b.sha(path)
    protected = {str(path.relative_to(R)): b.sha(path) for path in
                 (R/'strata-iq2_xs.json', Path(cfg['exe']), R/'src/program/generate.cpp')}
    data = {'dependencies': deps, 'protected': protected,
            'source_sha256': {'before.cpp': b.sha(H/'before.cpp'), 'candidate.cpp': b.sha(H/'candidate.cpp')},
            'baseline_exe': str(P/'candidate/strata.exe'), 'toolchain': prior['toolchain'],
            'scope': 'Only clamp SYCL CLI/service verifier windows to remaining output count; no draft, kernel, sampler or experimental-default change.'}
    b.save(H/'frozen.json', data)
    (H/'worktree-before.patch').write_bytes(subprocess.check_output(['git', 'diff', '--binary'], cwd=R))
    verify()
    print('PASS frozen source and', len(deps), 'dependencies', flush=True)


def main():
    with b.gpu_lock():
        verify()
        builder = module(R/'build-sycl-deepinfer-20261023/build.py')
        env = builder.environment()
        output = H/'candidate'
        output.mkdir(exist_ok=False)
        templates = dict(json.loads((P/'build.json').read_text())['commands'])
        build = {'complete': False, 'commands': []}
        for stage in ('compile-queue-fix', 'link-queue-fix'):
            command = templates[stage]
            for old, new in [(P/'candidate.cpp', H/'candidate.cpp'), (P/'final.obj', H/'candidate.obj'),
                             (P/'final.pdb', H/'candidate.pdb'), (P/'candidate', output)]:
                command = command.replace(str(old), str(new))
            build['commands'].append([stage, command])
            b.save(H/'build.json', build)
            print('BUILD', stage, flush=True)
            with (H/(stage+'.txt')).open('x', encoding='utf-8') as log:
                result = subprocess.run(command, cwd=R/'build-sycl-deepinfer-20261023/candidate',
                    env=env, stdout=log, stderr=subprocess.STDOUT, timeout=1200)
            if result.returncode:
                print((H/(stage+'.txt')).read_text(errors='replace')[-5000:], flush=True)
            result.check_returncode()
        build['files_sha256'] = {str(path.relative_to(H)): b.sha(path) for path in
                                 (H/'candidate.cpp', H/'candidate.obj', output/'strata.exe')}
        build['complete'] = True
        b.save(H/'build.json', build)
        verify()
        with (H/'cpu.txt').open('x') as log:
            subprocess.run([sys.executable, '-X', 'utf8', str(R/'tools/test_spec_window_limit.py'),
                '--source', str(H/'candidate.cpp'), '--cxx', str(builder.ICX/'bin/icx.exe'),
                '--out', str(H/'cpu')], env=env, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=120)
        module(H/'check.py').main()
        module(H/'measure.py').main()
        verify()
        print('PASS output-window experiment', flush=True)


if __name__ == '__main__':
    if '--resume' in sys.argv:
        with b.gpu_lock():
            verify()
            module(H/'check.py').main()
            module(H/'measure.py').main()
            verify()
    else:
        freeze() if '--freeze' in sys.argv else main()
