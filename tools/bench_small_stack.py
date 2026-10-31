"""Test five previously sub-threshold A770 changes together, using frozen objects.

python -X utf8 tools/bench_small_stack.py --self-check
python -X utf8 tools/bench_small_stack.py
python -X utf8 tools/bench_small_stack.py --check
"""
import argparse
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
from types import SimpleNamespace

R = Path(__file__).resolve().parents[1]
H = R / 'build-sycl-a770-small-stack-v3-20261030'
PREVIOUS = R / 'build-sycl-a770-small-stack-v2-20261030'
SCRIPT = Path(__file__).resolve()


def module(path):
    spec = importlib.util.spec_from_file_location('small_stack_' + path.parent.name + path.stem, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


gr = module(R / 'build-sycl-a770-gr-arg-select-model-20261029/run.py')
gdn = gr.module(R / 'build-sycl-a770-gdn-tail-restore-20261029/run.py')
qsa = gr.module(R / 'build-sycl-a770-qsa-tail-restore-20261029/run.py')
b = gr.b
COMPACT = R / 'build-sycl-a770-iq4xs-compact-model-20261028/native.obj'
SHARED = R / 'build-sycl-a770-xpu-shared-pair-20261028'
EXTRA_OBJECTS = [gdn.H / 'tail.obj', qsa.H / 'tail.obj', SHARED / 'model/shared.obj', SHARED / 'candidate.obj']
FLAGS = ('STRATA_IQ4_XS_COMPACT_SG16', 'STRATA_NATIVE_GU_Q8_CHUNK2',
         'STRATA_XPU_SHARED_PAIR', 'STRATA_GDN_Q8_TAIL', 'STRATA_QSA_Q8_TAIL')


def verifier():
    # ponytail: merge the existing disjoint edits; reuse both parity-tested kernel objects.
    original = gdn.verifier()
    saved = qsa.m.VERIFY
    qsa.m.VERIFY = SimpleNamespace(read_text=lambda encoding: original)
    try:
        return qsa.verifier()
    finally:
        qsa.m.VERIFY = saved


def plan():
    parent = R / 'build-sycl-a770-compact-device-counter-20261029/link-model.cmd'
    old = parent.read_text('utf8')
    pairs = [(f'set "OUT={parent.parent}"', f'set "OUT={H}"'),
             (f'/MAP:"{parent.parent / "model.map"}"', f'/MAP:"{H / "model.map"}"'),
             (f'set "VERIFY={gdn.VERIFY.with_suffix(".obj")}"', f'set "VERIFY={H / "verify.obj"}"'),
             (' %LIBS% ^', ' ' + ' '.join('"' + str(p) + '"' for p in EXTRA_OBJECTS) + ' %LIBS% ^')]
    value = gr.edit(old, pairs)
    assert gr.edit(value, [(new, before) for before, new in reversed(pairs)]) == old
    baseline = gr.inputs(gr.LINK.read_text('utf8'))
    candidate = gr.inputs(value)
    assert baseline - candidate == {gdn.VERIFY.with_suffix('.obj')}
    assert candidate - baseline == {H / 'verify.obj', COMPACT, *EXTRA_OBJECTS}
    return value, baseline | candidate


def arm_environment(env, candidate):
    return env | {flag: str(int(candidate)) for flag in FLAGS} | {'STRATA_PREFILL_CACHE_TAIL64': '1'}


def performance():
    m, guard, owned = gr.performance()
    # Reuse the original full-output, telemetry and independent-confirmation collector.
    source = gr.edit(m.SOURCE, [
        ("        assert run['args'] == expected", "        assert run['args'] == expected\n        assert run['exe'] == arm_exe(run['candidate']).relative_to(R).as_posix()\n        assert run['stack_flags'] == {flag: str(int(run['candidate'])) for flag in FLAGS}"),
        ("                    arm = dict(label=label,candidate=candidate,args=run_args,rows=[]); d['runs'].append(arm)",
         "                    arm = dict(label=label,candidate=candidate,args=run_args,rows=[],exe=arm_exe(candidate).relative_to(R).as_posix(),stack_flags={flag: str(int(candidate)) for flag in FLAGS}); d['runs'].append(arm)"),
        ("b.Engine.__init__(engine,EXE,run_args,env | {'STRATA_PREFILL_CACHE_TAIL64': '0' if candidate else '1'},H/label)",
         "b.Engine.__init__(engine,arm_exe(candidate),run_args,arm_environment(env,candidate),H/label)"),
        ("256 if run['candidate'] else 64", '64'), ('256 if candidate else 64', '64'),
        ('        H.mkdir()', '        files += [arm_exe(True), SCRIPT]\n        H.mkdir()'),
        ("                monitor = subprocess.Popen([str(P/'monitor.exe'),'3600'],cwd=R,env=env,stdout=out,stderr=err)",
         "                monitor = subprocess.Popen([str(P/'monitor.exe'),'3600'],cwd=R,env=env,stdout=out,stderr=err)\n                register_monitor(monitor)"),
        ("        d['complete'] = True;", "        stop_compiler_guard()\n        d['complete'] = True;"),
    ])
    source = '\n'.join('            rule=' + repr(
        'Five-change stack: compact IQ4_XS, GU/Q8 chunk2, shared gate/up, GDN Q8 tail, QSA gate/Q8 tail. '
        'Original device-counter EXE/default runtime baseline; same precision,6297 slots,tail64,minp0.5,original2048/full128 interleaved histories. '
        'All complete outputs,8x48 scans,actual exits,source hashes,compiler guard and sampled free>=1024MiB checked. '
        'Any scene>=1%/drift<=3% triggers independent A3/B2/A4; same-scene confirmation required;abs changes<0.3% noise. '
        'No sum of historical gains,production adoption or general quality claim.') + ','
        if line.startswith('            rule=') else line for line in source.splitlines())
    m.H = m.base.H = m.memory_coverage.__globals__['H'] = H / 'performance'
    m.__file__ = str(SCRIPT)
    m.BUILD = H / 'build.json'
    m.SCRIPT, m.FLAGS = SCRIPT, FLAGS
    m.arm_environment = arm_environment
    m.arm_exe = lambda candidate: H / 'bin/strata-unified-all-operators.exe' if candidate else m.EXE
    m.register_monitor, m.stop_compiler_guard = owned.register_monitor, guard.stop_guard
    m.LAUNCH_SOURCES[str(SCRIPT)] = b.sha(SCRIPT)
    exec(compile(source, str(SCRIPT), 'exec'), m.__dict__)
    original_check = m.check

    def check(data):
        original_check(data)
        for run in data['runs']:
            text = (m.H / (run['label'] + '.stderr')).read_text('utf8')
            markers = ('IQ4_XS_COMPACT ni=', 'MODEL_GU_Q8 cap=10 active=1',
                       'XPU_SHARED_PAIR enabled=1', 'GDN_TAIL_CURRENT enabled=1', 'QSA_TAIL_CURRENT active=1')
            for marker in markers:
                assert (marker in text) == run['candidate'], (run['label'], marker)
    m.check = check
    return m, guard, owned


def build():
    command, inputs = plan()
    assert not (H / 'build.json').exists()
    (H / 'bin').mkdir(parents=True)
    (H / 'verify.cpp').write_text(verifier(), 'utf8')
    (H / 'link-model.cmd').write_text(command, 'utf8')
    prior = [gdn.H / 'build.json', qsa.H / 'build.json', SHARED / 'model/build.json',
             R / 'build-sycl-a770-compact-device-counter-20261029/build.json']
    hashes, historical_source_changes = {}, []
    for path in prior:
        parent = json.loads(path.read_text('utf8'))
        assert parent['complete']
        for name, digest in parent['files_sha256'].items():
            file = R / name
            if file.suffix in ('.obj', '.lib', '.cpp', '.hpp'):
                actual = b.sha(file)
                if actual != digest and file.suffix in ('.cpp', '.hpp') and any(
                        file.is_relative_to(R / root) for root in ('sycl', 'src', 'include')):
                    # Only frozen headers/sources enter compilation; these are old provenance, not inputs.
                    historical_source_changes.append(dict(path=str(file), recorded_sha256=digest, current_sha256=actual))
                else:
                    assert actual == digest, file
                    hashes[str(file)] = digest
    paths = (inputs - {H / 'verify.obj'}) | {SCRIPT, H / 'verify.cpp', H / 'link-model.cmd', *prior}
    hashes.update({str(p): b.sha(p) for p in paths})
    previous = json.loads((PREVIOUS / 'build.json').read_text('utf8'))
    assert not previous['complete'] and previous['compile_exit_code'] == 0
    assert previous['files_sha256'][str(SCRIPT)] == b.sha(PREVIOUS / 'run-before-log-fix.py')
    assert b.sha(PREVIOUS / 'verify.cpp') == previous['files_sha256'][str(PREVIOUS / 'verify.cpp')]
    assert (PREVIOUS / 'verify.cpp').read_bytes() == (H / 'verify.cpp').read_bytes()
    shutil.copyfile(PREVIOUS / 'verify.obj', H / 'verify.obj')
    hashes.update({str(path): b.sha(path) for path in
                   (PREVIOUS / 'build.json', PREVIOUS / 'run-before-log-fix.py', PREVIOUS / 'verify.obj', H / 'verify.obj')})
    commands = [['cmd', '/d', '/c', str(H / 'link-model.cmd')]]
    data = dict(complete=False, production_changed=False, files_sha256=hashes, flags=FLAGS, commands=commands,
                historical_source_changes=historical_source_changes)
    b.save(H / 'build.json', data)
    env = gr.module(R / 'build-sycl-deepinfer-20261023/build.py').environment()
    for name, args in zip(('link',), commands):
        b.busy()
        print('BUILD', name, flush=True)
        with (H / (name + '-wrapper.log')).open('x', encoding='utf8') as log:
            result = subprocess.run(args, cwd=R, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=900)
        data[name + '_exit_code'] = result.returncode
        b.save(H / 'build.json', data)
        result.check_returncode()
    for path in (H / 'verify.obj', H / 'model.map', H / 'bin/strata-unified-all-operators.exe'):
        hashes[str(path)] = b.sha(path)
    data['complete'] = True
    b.save(H / 'build.json', data)


def self_check():
    plan()
    value = verifier()
    assert all(value.count(marker) == 1 for marker in ('GDN_TAIL_CURRENT enabled=', 'QSA_TAIL_CURRENT active='))
    assert 'STRATA_NATIVE_GU_Q8_CHUNK2' in (R / 'build-sycl-a770-gu-q8-export-20261027/fusion.cpp').read_text('utf8')
    env = {'STRATA_VALUE_PACKED_BITS': '4'}
    assert arm_environment(env, True) == arm_environment(env, False) | {flag: '1' for flag in FLAGS}
    m, _, _ = performance()
    assert m.H == m.memory_coverage.__globals__['H'] and m.arm_exe(True) != m.arm_exe(False)
    assert not m.qualified({c: dict(throughput_change_pct=.99, baseline_drift_pct=0) for c in m.CASES})
    assert m.qualified({c: dict(throughput_change_pct=1, baseline_drift_pct=3) for c in m.CASES}) == set(m.CASES)
    print('PASS disjoint reversible tail merge, exact link changes, five simultaneous flags, independent full-output collector; no GPU/compiler')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    modes = parser.add_mutually_exclusive_group()
    modes.add_argument('--self-check', action='store_true')
    modes.add_argument('--check', action='store_true')
    args = parser.parse_args()
    if args.self_check:
        self_check()
        return
    if args.check:
        m, _, _ = performance()
        with b.gpu_lock():
            m.check(m.read(m.H / 'results.json'))
            record = m.read(H / 'guard/compiler-guard-results.json')
            assert record['complete'] and record['passed'] and not record['events']
        return
    self_check()
    with b.gpu_lock():
        build()
    m, guard, owned = performance()
    guard.H, guard.SCRIPT = H / 'guard', SCRIPT
    guard.H.mkdir()
    guard.old_lock, guard.old_busy = m.gpu_lock, owned.admission.busy
    m.gpu_lock, b.busy = guard.guarded_lock, guard.busy
    m.run()


if __name__ == '__main__':
    main()
