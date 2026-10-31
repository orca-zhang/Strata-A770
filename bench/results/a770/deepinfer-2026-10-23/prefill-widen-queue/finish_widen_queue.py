"""Verify and publish the rejected queue-wait experiment without running GPU work."""
import datetime
import hashlib
import json
import math
import re
import shutil
import statistics
import subprocess
from pathlib import Path

H = Path(__file__).resolve().parent
R = H.parent
F = H / 'prefill-widen-queue'
P = H / 'prefill-widen-workspace-confirm/build'
A = R / 'bench/results/a770/deepinfer-2026-10-23/prefill-widen-queue'

def sha(path):
    with Path(path).open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()

def read(path):
    return json.loads(Path(path).read_text(encoding='utf-8'))

def write(path, data):
    Path(path).write_text(json.dumps(data, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')

def records(path, tag):
    return [json.loads(v) for v in re.findall(r'^' + tag + r' (.+)$', Path(path).read_text(encoding='utf-8'), re.M)]

d = read(F / 'results.json')
b = read(F / 'build/build.json')
normal = read(F / 'build/results.json')
repair = read(F / 'logging-repair.json')
assert d['complete'] and b['complete'] and normal['complete'] and repair['complete']
assert sha(F / 'results.json') == repair['original_results_sha256']
assert sha(P / 'build.json') == d['baseline_build_sha256']
assert sha(F / 'build/build.json') == d['candidate_build_sha256']
assert b['reused_libraries'] == read(P / 'build.json')['reused_libraries']
assert b['sampler_obj_sha256'] == sha(H / 'sampler-fp32/sampler.obj')
for kind, digest in b['exe_sha256'].items():
    assert sha(F / 'build' / kind / 'strata.exe') == digest
for path, digest in d['source_sha256'].items():
    assert sha(R / path) == digest, path
for path, digest in d['production_sha256'].items():
    assert sha(path) == digest, path
for path, digest in d['dependencies'].items():
    assert sha(H / 'baseline' / path) == digest, path
assert len(d['dependencies']) == 96
for label in ('parity', 'prefill'):
    assert sha(F / 'build' / (label + '.obj')) == sha(P / (label + '.obj'))

checks = 0
for row in normal['runs']:
    assert row['returncode'] == 0 and row['summary'] == [{'checks': 1015, 'mismatched_cases': 0}]
    log = F / 'build' / f"{int(row['widen'])}-check.log"
    assert records(log, 'SUMMARY') == row['summary']
    assert all(v['different'] == 0 and v['padding_different'] == 0 for v in records(log, 'PARITY'))
    checks += 1015
for row in d['large_checks']:
    # The original structured results are immutable; use repeated raw logs for the two overwritten files.
    log = F / ('check-' + row['label'] + '.log')
    assert row['returncode'] == 0 and row['summary'] == [{'checks': 270, 'mismatched_cases': 0}]
    assert records(log, 'SUMMARY') == row['summary']
    values = records(log, 'PARITY')
    assert len(values) == 270 and all(v['different'] == 0 and v['padding_different'] == 0 for v in values)
    checks += 270
assert checks == 2840
assert [r['arena'] for r in repair['runs']] == ['1', '4']
for row in repair['runs']:
    assert row['returncode'] == 0 and records(F / (row['label'] + '.log'), 'SUMMARY') == row['summary']
assert repair['candidate_exe_sha256'] == b['exe_sha256']['parity']

for shape in ('small', 'large'):
    rows = [r for r in d['runs'] if r['shape'] == shape]
    assert [r['candidate'] for r in rows] == [False, True, False, True, False]
    for i, row in enumerate(rows):
        assert row['returncode'] == 0 and row['summary'] == [{'checks': 6 if shape == 'small' else 12, 'mismatched_cases': 0}]
        assert sha((F / 'build' if row['candidate'] else P) / 'parity/strata.exe') == row['exe_sha256']
        log = F / f'{shape}-{i}.log'
        for key, tag in [('benches', 'BENCH'), ('batches', 'BATCH'), ('summary', 'SUMMARY')]:
            assert records(log, tag) == row[key]
        for bench in row['benches']:
            batch = [v for v in row['batches'] if (v['bf16'], v['n'], v['k']) == (bench['bf16'], bench['n'], bench['k'])]
            assert len(batch) == 5
            for metric in ('total_us', 'submit_us'):
                assert math.isclose(statistics.median(v[metric] for v in batch), bench[metric], abs_tol=1e-6)
    for pair in [v for v in d['comparisons'] if v['shape'] == shape]:
        i = pair['candidate_run']
        a, candidate, c = [next(v['total_us'] for v in rows[j]['benches'] if (v['bf16'], v['n']) == (pair['bf16'], pair['n'])) for j in (i-1, i, i+1)]
        calc = {'baseline': math.sqrt(a*c), 'candidate': candidate,
                'throughput_change_pct': 100*(math.sqrt(a*c)/candidate-1), 'baseline_drift_pct': 100*(c/a-1)}
        for key, value in calc.items():
            assert math.isclose(value, pair[key], rel_tol=1e-12, abs_tol=1e-9)
        assert not pair['gate'] and pair['throughput_change_pct'] < 3 and abs(pair['baseline_drift_pct']) <= 3
assert len(d['comparisons']) == 12
assert (H / 'check_widen_queue.py').read_bytes() == (F / 'replay_check_widen_queue.py').read_bytes()
for path in [F / 'check_widen_queue.py', F / 'replay_check_widen_queue.py', F / 'repair_widen_queue_logs.py']:
    compile(path.read_text(encoding='utf-8'), str(path), 'exec')

decision = {'date_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
            'status': 'rejected_after_operator_screen', 'retained_runtime_source': False,
            'reason': 'All 12 pairs below 3% throughput threshold; all baseline drift within 3%. Preserve existing waits.',
            'correctness_checks_with_raw_logs': checks, 'mismatched_cases': 0,
            'additional_repeated_checks_after_log_collision': 540, 'logging_repair_sha256': sha(F / 'logging-repair.json'),
            'benchmark_checks_excluded_from_total': 90, 'independent_benchmark_processes': 10,
            'throughput_change_pct_range': [min(v['throughput_change_pct'] for v in d['comparisons']), max(v['throughput_change_pct'] for v in d['comparisons'])],
            'model_checks_run': False, 'model_speed_claim': False, 'default_behavior_changed': False,
            'source_sha256': d['source_sha256'], 'baseline_build_sha256': d['baseline_build_sha256'],
            'candidate_build_sha256': d['candidate_build_sha256'], 'baseline_exe_sha256': read(P / 'build.json')['exe_sha256'],
            'candidate_exe_sha256': b['exe_sha256'], 'hardware': 'Intel Arc A770 16GB / Xeon E5-2696 v4',
            'compiler': b['compiler'], 'oneMKL': b['oneMKL'], 'extra_workspace_mib_over_baseline': 0,
            'actual_expert_cache_capacity': None, 'cache_note': 'Operator-only screen; no model/expert cache allocated.',
            'production_unchanged': True, 'unchanged_dependencies': 96, 'warmups': 4, 'batches': 5, 'iterations_per_batch': 16}
write(F / 'decision.json', decision)
shutil.copyfile(Path(__file__), F / 'finish_widen_queue.py')
A.mkdir(exist_ok=True)
allowed = {'.json', '.py', '.cpp', '.hpp', '.log', '.d'}
files = [p for p in F.rglob('*') if p.is_file() and p.suffix in allowed and p.name not in ('final-check.json', 'archive.json') and '__pycache__' not in p.parts]
for source in files:
    target = A / source.relative_to(F)
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, target)
    assert sha(target) == sha(source)
manifest = {'private_source': str(F.relative_to(R)).replace('\\', '/'),
            'files_sha256': {str(p.relative_to(F)).replace('\\', '/'): sha(p) for p in sorted(files)},
            'binary_artifacts_included': False}
write(A / 'archive.json', manifest)
for path, digest in manifest['files_sha256'].items():
    assert sha(A / path) == digest, path
for path in (A / 'final-check.json', F / 'final-check.json'):
    write(path, {'complete': False})
doc = R / 'docs/DEEPINFER_OPTIMIZATIONS.md'
links = []
for raw in re.findall(r'\[[^\]]*\]\(([^)\n]+)\)', doc.read_text(encoding='utf-8')):
    target = raw.strip('<>').split('#', 1)[0]
    if not target or re.match(r'[a-zA-Z][a-zA-Z0-9+.-]*:', target):
        continue
    assert (doc.parent / target).exists(), target
    links.append(target)
run = subprocess.run(['git', 'diff', '--check', '--', 'sycl/src/prefill/gemm.dp.cpp', 'sycl/src/prefill/prefill.cpp',
                      'sycl/src/prefill/gemm_parity.cpp', 'docs/DEEPINFER_OPTIMIZATIONS.md'], capture_output=True, text=True)
assert run.returncode == 0, run.stdout + run.stderr
final = {'complete': True, 'sources_unchanged': True, 'production_unchanged': True, 'unchanged_dependencies': 96,
         'candidate_not_integrated': True, 'checks_verified_with_raw_logs': checks,
         'logging_repair_verified': True, 'benchmark_processes_verified': 10, 'batch_medians_recomputed': True,
         'paired_results_and_gates_recomputed': True, 'source_and_build_executable_hashes_verified': True,
         'archive_files_verified': len(files), 'archive_sha256': sha(A / 'archive.json'),
         'decision_sha256': sha(A / 'decision.json'), 'raw_results_sha256': sha(A / 'results.json'),
         'documentation_sha256': sha(doc), 'documentation_local_links_verified': len(links), 'git_diff_check_passed': True}
for path in (A / 'final-check.json', F / 'final-check.json'):
    write(path, final)
print(json.dumps(final, indent=2))
