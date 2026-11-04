"""Apply only a validated frozen candidate and archive the complete experiment."""
import importlib.util
import json
import platform
import shutil
import subprocess
import sys
from pathlib import Path

F = Path(__file__).resolve().parent
R = F.parent
A = R / 'bench/results/a770/token-bytes-map-2026-10-24'
assert not (A / 'archive.json').exists()
assert json.loads((F / 'results.json').read_text())['complete']
spec = importlib.util.spec_from_file_location('cache_audit', A / 'check_results.py')
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)


def copy(source, target):
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, target)


def save(path, data):
    path.write_text(json.dumps(data, indent=2) + '\n', encoding='utf-8')


for p in F.iterdir():
    if p.is_file() and (p.suffix in ('.json', '.log') or p.name in ('run.py', 'prepare.py')):
        copy(p, A / p.name)
prep = audit.read('preparation.json')
for arm in ('baseline', 'candidate'):
    copy(F / arm / 'strata_tokenizer.py', A / arm / 'strata_tokenizer.py')
for name in prep['frozen_sha256']:
    if not name.endswith('.pyd'):
        copy(F / name, A / name)
runs, pairs = audit.derive()
assert audit.read('results.json')['runs'] == runs and audit.read('results.json')['pairs'] == pairs
measurement = audit.summarize(pairs)
baseline = (F / 'baseline/strata_tokenizer.py').read_bytes()
candidate = (F / 'candidate/strata_tokenizer.py').read_bytes()
live = R / 'tools/strata_tokenizer.py'
assert live.read_bytes() == baseline
for name, digest in prep['production_sha256'].items():
    assert audit.sha(Path(name)) == digest
decision = 'adopted' if measurement['retention_pass'] else 'not_adopted'
if decision == 'adopted':
    live.write_bytes(candidate)
try:
    result = subprocess.run([sys.executable, '-m', 'unittest',
                             'serve.test_detok.HeapBpe.test_token_bytes_cold_and_cached',
                             'serve.test_detok.HeapBpe.test_byte_mapping_is_utf8', '-v'],
                            cwd=R, capture_output=True, text=True)
    (A / 'live-tests.log').write_text(result.stdout + result.stderr, encoding='utf-8')
    result.check_returncode()
except BaseException:
    if decision == 'adopted' and live.read_bytes() == candidate:
        live.write_bytes(baseline)
    raise
copy(R / 'serve/test_detok.py', A / 'regression/test_detok.py')
save(A / 'summary.json', dict(decision=decision, measurement=measurement,
                             scope='CPU token byte conversion, decode and incremental detokenization only; no model/HTTP timing.'))
for name, digest in prep['production_sha256'].items():
    assert audit.sha(Path(name)) == digest
import regex
save(A / 'final-checks.json', dict(runtime_source_sha256=audit.sha(live),
                                  production_sha256={name: audit.sha(Path(name)) for name in prep['production_sha256']},
                                  live_regression_returncode=result.returncode,
                                  live_regression_source_sha256=audit.sha(R / 'serve/test_detok.py'),
                                  platform=platform.platform(), python=sys.version, regex_version=regex.__version__,
                                  cpu='Intel Xeon E5-2696 v4 @ 2.20GHz', logical_cpus=44,
                                  expert_cache_capacity=None, model_speedup_claim=False))
for name in ('tools/bench_deepinfer.py', 'build-sycl-deepinfer-20261023/check_bf16_arena.py'):
    copy(R / name, A / 'tooling' / Path(name).name)
copy(Path(__file__), A / 'tooling/finalize.py')
save(A / 'archive.json', dict(
    sha256={p.relative_to(A).as_posix(): audit.sha(p) for p in sorted(A.rglob('*'))
            if p.is_file() and '__pycache__' not in p.parts and p.name != 'archive.json'},
    omitted_binary_sha256={name: digest for name, digest in prep['frozen_sha256'].items() if name.endswith('.pyd')},
    note='Frozen source and raw measurements; no weights, tokenizer pack, EXE or extension binary.'))
audit.main()
for case in measurement['cases']:
    print(case)
