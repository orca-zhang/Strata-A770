"""Two retained-result audits in one CPU batch; no GPU lease, model or compiler."""
import hashlib
import importlib.util
import json
from pathlib import Path

R = Path(__file__).resolve().parents[3]


def module(path):
    spec = importlib.util.spec_from_file_location('offline_'+path.parent.name.replace('-', '_'), path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


# ponytail: call the original pure checks directly; their CLI wrappers unnecessarily acquire a GPU lease.
record = json.loads((Path(__file__).parent/'offline-audit-batch-2026-10-29.json').read_text('utf-8-sig'))
assert record['complete'] and record['gpu_requests'] == 0 and not record['gpu_lease_acquired']
proofs = {'window-reuse': R/'build-sycl-a770-window-reuse-20261029/results.json',
          'scoped-MMVQ-cold-quality': R/'build-sycl-a770-igc-scoped-mmvq-device-counter-20261029/quality/results.json'}
assert [job['task'] for job in record['jobs']] == list(proofs)
for job in record['jobs']:
    path = Path(job['script'])
    with path.open('rb') as stream:
        assert hashlib.file_digest(stream, 'sha256').hexdigest() == job['script_sha256']
    with proofs[job['task']].open('rb') as stream:
        assert hashlib.file_digest(stream, 'sha256').hexdigest() == job['result_sha256']
w = module(R/'build-sycl-a770-window-reuse-20261029/run.py')
d = w.read(w.H/'results.json')
assert d['complete'] and d['summary'] == w.analyze(d)
for path, digest in d['files_sha256'].items():
    assert w.b.sha(R/path) == digest, path
q = module(R/'build-sycl-a770-igc-scoped-mmvq-device-counter-20261029/quality.py')
d = json.loads((q.SCRIPT.parent/'quality/results.json').read_text('utf8'))
q.audit(d)
assert d['summary']['fixed_targets_passed']
assert record['queued_duplicate_transition']['stopped'] and not record['queued_duplicate_not_yet_stopped']
print('PASS both original offline audits and recorded queue consolidation; no GPU lease/model/compile')
