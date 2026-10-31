"""Run retained-result audits in one CPU batch without a GPU lease."""
from pathlib import Path
import runpy
import sys

ROOT = Path(__file__).resolve().parents[3]
JOBS = (
    ('build-sycl-a770-gr-source-review-20261029/historical.py', ['--check']),
    ('build-sycl-a770-gr-source-review-20261029/current-stages.py', ['--check']),
    ('build-sycl-a770-window-reuse-20261029/remainder3.py', ['--check']),
    ('build-sycl-a770-gu-fixed-three-batch-20261029/check-screen-review.py', []),
    ('build-sycl-a770-gr-private-isa-20261029/private-witness.py', ['--check']),
    ('build-sycl-a770-gr-graph-replay-wait-20261029/audit.py', ['--check']),
    ('build-sycl-a770-gr-graph-replay-wait-20261029/trace.py', ['--check']),
    ('build-sycl-a770-gr-native-20261029/run.py', ['--check']),
    ('build-sycl-a770-gr-arg-select-20261029/run.py', ['--check']),
    ('build-sycl-a770-gr-arg-select-isa-20261029/run.py', ['--check']),
    ('build-sycl-a770-gr-arg-select-speed-20261029/run.py', ['--check']),
    ('build-sycl-a770-gr-graph-diagnostic-20261029/run.py', ['--check']),
)


def check_gr_model():
    # ponytail: the completed model's CLI check takes a GPU lease; reuse its pure assertions here.
    task = runpy.run_path(str(ROOT/'build-sycl-a770-gr-arg-select-model-20261029/run.py'))
    model, _, _ = task['performance']()
    data = model.read(model.H/'results.json')
    model.check(data)
    compiled = model.read(task['H']/'guard/compiler-guard-results.json')
    assert compiled['complete'] and compiled['passed'] and not compiled['events']
    for path, digest in compiled['files_sha256'].items():
        assert task['b'].sha(ROOT/path) == digest, path
    assert task['linkage']((task['H']/'model.map').read_text('utf8', errors='replace')) == model.read(task['H']/'build.json')['linkage']
    print('GR model speed audit passed; sampled_memory_passed=', data['sampled_memory_passed'],
          'quality_history_pending=', data['quality_history_pending'], 'adopted=', data['adopted'])


if __name__ == '__main__':
    saved = sys.argv
    saved_path = sys.path[:]
    try:
        # ponytail: fixed CPU-only audits; their own checks retain source and sample validation.
        for relative, args in JOBS:
            path = ROOT / relative
            sys.argv = [str(path), *args]
            sys.path.insert(0, str(path.parent))
            try:
                runpy.run_path(str(path), run_name='__main__')
            finally:
                sys.path[:] = saved_path
        check_gr_model()
    finally:
        sys.argv = saved
        sys.path[:] = saved_path
    print(f'PASS all {len(JOBS)+1} audit entries; no GPU lease, model launch or compilation; adoption gates remain separate')
