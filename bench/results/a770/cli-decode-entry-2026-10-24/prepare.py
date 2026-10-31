"""Freeze the current dirty worktree and reuse the previous clean SYCL baseline."""
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

assert not (H/'frozen.json').exists()
previous = json.loads((P/'frozen.json').read_text())
built = json.loads((P/'build.json').read_text())
files = ['sycl/src/program/generate.cpp', 'src/program/generate.cpp', 'docs/DEEPINFER_OPTIMIZATIONS.md']
for name in files:
    target = H/'before'/name
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(R/name, target)
assert b.sha(R/files[0]) == built['files_sha256']['candidate.cpp']
dependencies = previous['dependencies'].copy()
for name in ('candidate/strata.exe', 'mtp.obj'):
    path = P/name
    assert b.sha(path) == built['files_sha256'][name.replace('/', '\\')]
    dependencies[str(path.relative_to(R))] = b.sha(path)
for name, digest in previous['headers'].items():
    dependencies[str((P/name).relative_to(R))] = digest
for name, digest in dependencies.items():
    assert b.sha(R/name) == digest, name
shutil.copyfile(P/'config.json', H/'config.json')
(H/'worktree-before.patch').write_bytes(subprocess.check_output(['git', 'diff', '--binary'], cwd=R))
(H/'status-before.txt').write_bytes(subprocess.check_output(['git', 'status', '--short'], cwd=R))
b.save(H/'frozen.json', {
    'before': {name: b.sha(H/'before'/name) for name in files},
    'dependencies': dependencies,
    'baseline_exe': str((P/'candidate/strata.exe').relative_to(R)),
    'config_sha256': b.sha(H/'config.json'),
    'toolchain': previous['toolchain'],
    'scope': 'CLI decode entry sentinel; two lines per backend. One SYCL candidate, shared frozen headers, MTP/sampler/verifier objects and engine archives. Production files unchanged.'})
print('PASS frozen current sources and', len(dependencies), 'dependencies')
