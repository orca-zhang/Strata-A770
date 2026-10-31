"""Copy completed measurements without editing any original samples."""
import hashlib
import importlib.util
import json
import platform
import shutil
import subprocess
import sys
from pathlib import Path

R = Path(__file__).resolve().parent.parent
A = R / 'bench/results/a770/tokenizer-translate-2026-10-24'
assert not (A / 'archive.json').exists()
spec = importlib.util.spec_from_file_location('audit', A / 'check_results.py')
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)


def copy(source, target):
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, target)


def save(path, data):
    path.write_text(json.dumps(data, indent=2) + '\n', encoding='utf-8')


folders = {'initial': R / 'build-sycl-a770-tokenizer-translate-20261024',
           'repeat': R / 'build-sycl-a770-tokenizer-translate-repeat-20261024'}
for name, source in folders.items():
    for p in source.iterdir():
        if p.is_file() and (p.suffix in ('.json', '.log') or p.name in ('run.py', 'run.initial.py', 'check_candidate.py')):
            copy(p, A / name / p.name)
    prep = json.loads((source / 'preparation.json').read_text(encoding='utf-8'))
    for arm, field in (('baseline', 'source_sha256'), ('candidate', 'candidate_sha256')):
        p = source / arm / 'strata_tokenizer.py'
        assert audit.sha(p) == prep[field]
        dest = A / 'sources' / arm / p.name
        if dest.exists():
            assert dest.read_bytes() == p.read_bytes()
        else:
            copy(p, dest)
    for relative, digest in prep['frozen_serving_sha256'].items():
        p = source / relative
        assert audit.sha(p) == digest
        if p.suffix == '.pyd':
            continue
        dest = A / 'sources' / relative
        if dest.exists():
            assert dest.read_bytes() == p.read_bytes()
        else:
            copy(p, dest)

summaries = {}
for name in folders:
    runs, pairs = audit.derive(name)
    summaries[name] = audit.summarize(pairs)
    if name == 'initial':
        save(A / name / 'recovered-pairs.json', dict(
            explanation='All ten child processes completed. Final reporting failed on an empty eligible round; '
                        'derive the missing confirm pairs from unchanged raw JSON, with no new samples.',
            interrupted_results_sha256=audit.sha(A / name / 'interrupted-results.json'), pairs=pairs))
summaries['decision'] = 'not_adopted'
summaries['reason'] = ('Repeat eligibility is 14/40, below 30/40; only 4/10 cases have two eligible pairs. '
                       'Short Tokenizer.encode pooled eligible throughput is -5.580509%; '
                       'confirm round 1 has 2/3 positive eligible pairs. No further repeats.')
save(A / 'summary.json', summaries)

copy(R / 'serve/test_detok.py', A / 'regression/test_detok.py')
p = subprocess.run([sys.executable, '-m', 'unittest', 'serve.test_detok.HeapBpe.test_byte_mapping_is_utf8', '-v'],
                   cwd=R, capture_output=True, text=True)
(A / 'regression/live-test.log').write_text(p.stdout + p.stderr, encoding='utf-8')
p.check_returncode()
for path, digest in prep['production_sha256'].items():
    assert audit.sha(Path(path)) == digest, path
assert audit.sha(R / 'tools/strata_tokenizer.py') == prep['source_sha256']
save(A / 'final-checks.json', dict(
    production_before_sha256=prep['production_sha256'],
    production_after_sha256={path: audit.sha(Path(path)) for path in prep['production_sha256']},
    runtime_source_sha256=audit.sha(R / 'tools/strata_tokenizer.py'),
    live_regression_returncode=p.returncode,
    live_regression_source_sha256=audit.sha(R / 'serve/test_detok.py'),
    candidate_tests_scope='Frozen candidate tokenizer, live serving tests (not the frozen timing modules).',
    gpu_or_model_measurement=False, expert_cache_capacity=None,
    cpu='Intel Xeon E5-2696 v4 @ 2.20GHz', logical_cpus=44,
    platform=platform.platform(), python=sys.version))
for name in ('tools/bench_deepinfer.py', 'build-sycl-deepinfer-20261023/check_bf16_arena.py'):
    copy(R / name, A / 'tooling' / Path(name).name)
copy(Path(__file__), A / 'tooling/archive_results.py')
save(A / 'archive.json', dict(
    sha256={p.relative_to(A).as_posix(): audit.sha(p) for p in sorted(A.rglob('*'))
            if p.is_file() and '__pycache__' not in p.parts and p.name != 'archive.json'},
    omitted_binary_sha256={name: digest for name, digest in prep['frozen_serving_sha256'].items() if name.endswith('.pyd')},
    note='Source and raw CPU measurements only; no EXE, extension binary, model weights or tokenizer assets.'))
audit.main()
