"""Repeat the unchanged small stack after the original baseline's nonfinite failure."""
import argparse
import importlib.util
from pathlib import Path

SCRIPT = Path(__file__).resolve()
ROOT = SCRIPT.parents[1]
OUT = ROOT / 'build-sycl-a770-small-stack-retry-20261030'
spec = importlib.util.spec_from_file_location('small_stack', SCRIPT.with_name('bench_small_stack.py'))
stack = importlib.util.module_from_spec(spec)
spec.loader.exec_module(stack)


def collector():
    m, guard, owned = stack.performance()
    m.H = m.base.H = m.memory_coverage.__globals__['H'] = OUT / 'performance'
    m.SCRIPT = SCRIPT
    m.LAUNCH_SOURCES[str(SCRIPT)] = stack.b.sha(SCRIPT)
    assert m.BUILD == stack.H / 'build.json'
    assert m.arm_exe(True) == stack.H / 'bin/strata-unified-all-operators.exe'
    return m, guard, owned


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--self-check', action='store_true')
    mode.add_argument('--check', action='store_true')
    args = parser.parse_args()
    m, guard, owned = collector()
    if args.self_check:
        stack.self_check()
        failed = m.read(stack.H / 'performance/results.json')
        assert not failed['complete'] and 'layer=35' in failed['runs'][-1]['primary_error']
        assert m.H != stack.H / 'performance'
        print('PASS unchanged binaries/settings and complete fresh cohort; previous failure retained; no GPU')
    elif args.check:
        with stack.b.gpu_lock():
            m.check(m.read(m.H / 'results.json'))
            record = m.read(OUT / 'guard/compiler-guard-results.json')
            assert record['complete'] and record['passed'] and not record['events']
    else:
        OUT.mkdir(exist_ok=False)
        guard.H, guard.SCRIPT = OUT / 'guard', SCRIPT
        guard.H.mkdir()
        guard.old_lock, guard.old_busy = m.gpu_lock, owned.admission.busy
        m.gpu_lock, stack.b.busy = guard.guarded_lock, guard.busy
        m.run()


if __name__ == '__main__':
    main()
