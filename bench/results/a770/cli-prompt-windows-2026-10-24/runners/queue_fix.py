"""Reproduce the unloaded-drafter exit failure, then rebuild with a one-header queue ownership fix."""
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
NAME = 'sycl/include/strata/core/mtp.hpp'
OLD = 'dpct::queue_ptr cs_ = &dpct::get_in_order_queue();'
NEW = 'dpct::queue_ptr cs_ = nullptr;'


def freeze():
    before = H/'before'/NAME
    before.parent.mkdir(parents=True, exist_ok=True)
    assert not before.exists()
    shutil.copyfile(R/NAME, before)
    frozen = json.loads((H/'frozen.json').read_text())
    frozen['before'][NAME] = b.sha(before)
    b.save(H/'frozen.json', frozen)


def prepare(builder):
    out = H/'queue-fix'
    out.mkdir(exist_ok=True)
    before = (H/'before'/NAME).read_text()
    after = (R/NAME).read_text()
    assert before.count(OLD) == 1 and after == before.replace(OLD, NEW)
    header = H/'headers/strata/core/mtp.hpp'
    header.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(R/NAME, header)
    frozen_header = R/'build-sycl-mtp-terminal-20261024/headers/sycl/include/strata/core/mtp.hpp'
    assert b.sha(H/'before'/NAME) == b.sha(frozen_header)
    cfg = json.loads((H/'config.json').read_text())
    args = b.model_args(cfg, reproducible=True, cache_mib=5500)+['--kv', 'int8', '--tokens', '271', '--max-new', '1']
    at = args.index('--mtp')
    del args[at:at+2]
    env = b.environment(cfg, reproducible=True)
    env.update(STRATA_SPEC_COUPLED='1', STRATA_TRACE='1')
    command = [str(P/'candidate/strata.exe'), *args]
    if (out/'reproducer.json').exists():
        data = json.loads((out/'reproducer.json').read_text())
        assert data['command'] == command and data['after_sha256'] == b.sha(header)
    else:
        print('CHECK baseline without MTP, single token (queue ownership)', flush=True)
        with (out/'baseline.stdout').open('xb') as stdout, (out/'baseline.stderr').open('xb') as stderr:
            result = subprocess.run(command, cwd=R, env=env, stdout=stdout, stderr=stderr, timeout=240)
        data = {'command': command, 'returncode': result.returncode, 'before_sha256': b.sha(H/'before'/NAME),
                'after_sha256': b.sha(header), 'scope': 'An unloaded drafter must not destroy the shared default queue.'}
        b.save(out/'reproducer.json', data)
    assert data['returncode'] in (0xC0000005, 0xC0000409) and 'output  : 91' in (out/'baseline.stdout').read_text(), data
    print('PASS baseline abnormal exit reproduced after correct output', flush=True)
    build = json.loads((H/'build.json').read_text())
    shutil.copyfile(H/'build.json', out/'build-before-queue-fix.json')
    commands = dict(build['commands'])
    compile_command = commands['compile'].replace(str(H/'candidate.obj'), str(H/'final.obj')).replace(str(H/'candidate.pdb'), str(H/'final.pdb'))
    old_include = '-I'+str(R/'build-sycl-mtp-terminal-20261024/headers/sycl/include')
    assert old_include in compile_command
    compile_command = compile_command.replace(old_include, '-I'+str(H/'headers')+' '+old_include)
    link_command = commands['link'].replace(str(H/'candidate.obj'), str(H/'final.obj'))
    build['complete'] = False
    for stage, command in [('compile-queue-fix', compile_command), ('link-queue-fix', link_command)]:
        build['commands'].append([stage, command])
        b.save(H/'build.json', build)
        print('BUILD', stage, flush=True)
        with (H/(stage+'.log')).open('x', encoding='utf-8') as log:
            result = subprocess.run(command, cwd=R/'build-sycl-deepinfer-20261023/candidate', env=builder.environment(),
                                    stdout=log, stderr=subprocess.STDOUT, timeout=1200)
        if result.returncode:
            print((H/(stage+'.log')).read_text(errors='replace')[-5000:], flush=True)
        result.check_returncode()
    build['files_sha256'] = {str(p.relative_to(H)): b.sha(p) for p in (H/'candidate.cpp', H/'final.obj', H/'candidate/strata.exe', header)}
    build['method'] = 'Recompile generator with one MTP header overlay (unloaded queue starts null); all other frozen headers, objects and archives unchanged. No MTP destructor or inference math changed.'
    build['complete'] = True
    b.save(H/'build.json', build)
    print('PASS final build with unloaded-drafter queue ownership fix', flush=True)


if __name__ == '__main__':
    assert sys.argv[1:] == ['--freeze']
    freeze()
