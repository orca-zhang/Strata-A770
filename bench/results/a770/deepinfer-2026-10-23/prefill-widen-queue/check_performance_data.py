"""Recompute archived widening timings; no GPU, compiler or third-party package needed."""
import hashlib
import json
import math
import re
import statistics
from datetime import datetime, timedelta, timezone
from pathlib import Path

R = next(p for p in Path(__file__).resolve().parents if (p / 'AGENTS.md').is_file())
A = R / 'bench/results/a770/deepinfer-2026-10-23'
Q = A / 'prefill-widen-queue'
W = A / 'prefill-widen-workspace'

def read(path):
    return json.loads(path.read_text(encoding='utf-8'))

def sha(path):
    with path.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()

def records(path, tag):
    return [json.loads(v) for v in re.findall(r'^' + tag + r' (.+)$', path.read_text(encoding='utf-8'), re.M)]

archives = {}
for directory in (Q, W / 'screen', W / 'confirm'):
    manifest = read(directory / 'archive.json')
    for path, digest in manifest['files_sha256'].items():
        assert sha(directory / path) == digest, (directory, path)
    archives[str(directory.relative_to(A))] = len(manifest['files_sha256'])

queue = read(Q / 'results.json')
screen = read(W / 'screen/results.json')
confirm = read(W / 'confirm/results.json')
integration = read(Q / 'integration.json')
assert sha(Q / 'results.json') == integration['performance_screen_sha256']
assert sha(Q / 'decision.json') == integration['historical_decision_sha256']
assert sha(Q / 'archive.json') == integration['historical_archive_sha256']
assert sha(Q / 'final-check.json') == integration['historical_final_check_sha256']
assert sha(W / 'screen/results.json') == confirm['screen_sha256']
assert sha(W / 'confirm/build/build.json') == queue['baseline_build_sha256']
assert sha(Q / 'build/build.json') == queue['candidate_build_sha256'] == integration['candidate_build_sha256']
baseline_build = read(W / 'confirm/build/build.json')
candidate_build = read(Q / 'build/build.json')
assert baseline_build['sources'] == queue['source_sha256']
assert candidate_build['sources'] == integration['sources_sha256']
assert baseline_build['sampler_obj_sha256'] == candidate_build['sampler_obj_sha256']
assert baseline_build['reused_libraries'] == candidate_build['reused_libraries']
assert baseline_build['reused_units']['parity']['obj_sha256'] == candidate_build['reused_units']['parity']['obj_sha256']
assert baseline_build['reused_units']['prefill']['obj_sha256'] == candidate_build['reused_units']['prefill']['obj_sha256']
assert queue['env']['STRATA_PREFILL_GPU_WIDEN'] == queue['env']['STRATA_PREFILL_BF16_GPU_WIDEN'] == '1'
assert not any(queue['env'].get(k) not in (None, '0') for k in
               ('STRATA_PREFILL_HALF_OUTPUT', 'STRATA_PREFILL_HALF_BF16', 'STRATA_PREFILL_HALF_PROFILE',
                'STRATA_VERIFY_PROFILE', 'STRATA_DECODE_TIMING', 'STRATA_STATE_HASH', 'STRATA_STATE_HASH_GDN'))

processes = points = batches = 0
recomputed = {}
for name, directory, data in [('queue', Q, queue), ('workspace-screen', W / 'screen', screen),
                              ('workspace-confirm', W / 'confirm', confirm)]:
    assert data['complete']
    groups = {shape: [r for r in data['runs'] if r['shape'] == shape] for shape in ('small', 'large')} if name == 'queue' else {'all': data['runs']}
    comparisons = []
    for shape, runs in groups.items():
        assert [r['candidate'] for r in runs] == [False, True, False, True, False]
        for i, run in enumerate(runs):
            log = directory / (f'{shape}-{i}.log' if name == 'queue' else f'operator-{i}.log')
            assert run['returncode'] == 0
            if name == 'queue':
                expected_exe = (candidate_build if run['candidate'] else baseline_build)['exe_sha256']['parity']
            else:
                expected_exe = read(directory / 'build/build.json')['exe_sha256']['parity'] if run['candidate'] else screen['runs'][0]['exe_sha256']
            assert run['exe_sha256'] == expected_exe
            expected_checks = 6 if name == 'queue' and shape == 'small' else 12
            assert run['summary'] == [{'checks': expected_checks, 'mismatched_cases': 0}]
            for key, tag in [('benches', 'BENCH'), ('batches', 'BATCH'), ('summary', 'SUMMARY')]:
                assert records(log, tag) == run[key], (name, shape, i, key)
            for bench in run['benches']:
                batch = [v for v in run['batches'] if (v['bf16'], v['n'], v['k']) == (bench['bf16'], bench['n'], bench['k'])]
                assert len(batch) == 5 and sorted(v['batch'] for v in batch) == list(range(5))
                for metric in ('total_us', 'submit_us'):
                    values = [v[metric] for v in batch]
                    assert all(math.isfinite(v) and v > 0 for v in values)
                    assert math.isclose(statistics.median(values), bench[metric], abs_tol=1e-6)
                points += 1
                batches += len(batch)
            processes += 1
        selected = [v for v in data['comparisons'] if name != 'queue' or v['shape'] == shape]
        for pair in selected:
            i = pair['candidate_run']
            left, candidate, right = [next(v['total_us'] for v in runs[j]['benches'] if (v['bf16'], v['n']) == (pair['bf16'], pair['n'])) for j in (i-1, i, i+1)]
            baseline = math.sqrt(left * right)
            result = {'baseline': baseline, 'candidate': candidate,
                      'throughput_change_pct': 100*(baseline/candidate-1), 'baseline_drift_pct': 100*(right/left-1)}
            for key, value in result.items():
                assert math.isclose(value, pair[key], rel_tol=1e-12, abs_tol=1e-9), (name, pair, key)
            assert pair['gate'] == (result['throughput_change_pct'] >= 3 and abs(result['baseline_drift_pct']) <= 3)
            comparisons.append({**pair, 'latency_reduction_pct': 100*(1-candidate/baseline),
                                'baseline_ms': baseline/1000, 'candidate_ms': candidate/1000})
    recomputed[name] = comparisons

queue_shapes = []
for n in (1280, 4096, 12288):
    for bf16 in (False, True):
        pairs = [v for v in recomputed['queue'] if (v['bf16'], v['n']) == (bf16, n)]
        assert len(pairs) == 2
        ratio = math.prod(1+v['throughput_change_pct']/100 for v in pairs)**(1/len(pairs))
        queue_shapes.append({'type': 'BF16' if bf16 else 'FP16', 'n': n, 'pairs': pairs,
                             'descriptive_geomean_throughput_change_pct': 100*(ratio-1)})

exclusions = [{'experiment': name, **pair} for name, pairs in recomputed.items()
              for pair in pairs if abs(pair['baseline_drift_pct']) > 3]
assert len(exclusions) == 2 and not any(v['gate'] for v in recomputed['queue'])
current_sources = {path: sha(R / path) == digest for path, digest in integration['sources_sha256'].items()}
current_production = {path: sha(Path(path)) == digest if Path(path).exists() else None
                      for path, digest in integration['production_sha256'].items()}
local_executables = {}
for directory, expected in [(Q, candidate_build['exe_sha256']['parity']),
                            (W / 'screen', read(W / 'screen/build/build.json')['exe_sha256']['parity']),
                            (W / 'confirm', baseline_build['exe_sha256']['parity'])]:
    origin = read(directory / 'archive.json')['private_source']
    local_executables[origin + '/build/parity/strata.exe'] = expected
origin = read(W / 'screen/archive.json')['private_source']
local_executables[origin + '/baseline/parity/strata.exe'] = screen['runs'][0]['exe_sha256']
local_executable_checks = {path: sha(R / path) == digest if (R / path).exists() else None
                           for path, digest in local_executables.items()}
assert all(value is not False for value in local_executable_checks.values())
result = {'date': datetime.now(timezone(timedelta(hours=8))).isoformat(), 'status': 'archived_measurement_audit_passed',
          'new_gpu_measurements': False, 'archived_files_verified': sum(archives.values()), 'archives': archives,
          'processes_verified': processes, 'median_points_verified': points, 'batch_rows_verified': batches,
          'paired_comparisons_verified': sum(map(len, recomputed.values())), 'drift_exclusions': exclusions,
          'queue_shapes': queue_shapes, 'queue_meets_3pct_gate': False,
          'current_source_matches_tested_candidate': all(current_sources.values()), 'current_source_details': current_sources,
          'production_matches_protected_hashes': all(current_production.values()) if None not in current_production.values() else None,
          'local_executable_hash_checks': local_executable_checks,
          'input_results_sha256': {str(p.relative_to(A)): sha(p) for p in (Q / 'results.json', W / 'screen/results.json', W / 'confirm/results.json')},
          'limitations': ['Two candidate processes per shape; the two pairs share the middle baseline. Descriptive means are not independent confirmation or confidence intervals.',
                          'Missing local executables are recorded as null, since binaries are intentionally absent from the public archive.',
                          'total_us measures the synchronous GEMM API call, including conversion, output allocation/copies, waits and free. It is not pure GPU kernel time.',
                          'submit_us also includes internal waits. It is not a separate host-only submission cost.',
                          'Workspace 32/128 MiB comparison and wait-merging comparison have different baselines; do not add or multiply their gains.',
                          'No paired full-model performance measurements for the integrated wait-merging candidate. Model prefill, decode and TTFT gains remain unverified.']}
(Q / 'performance-audit.json').write_text(json.dumps(result, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
print(f'PASS {processes} processes, {points} medians, {batches} batches, {result["paired_comparisons_verified"]} pairs, {len(exclusions)} drift exclusions; {sum(archives.values())} archived hashes')
for row in queue_shapes:
    print(row['type'], row['n'], 'pairs', [round(v['throughput_change_pct'], 4) for v in row['pairs']],
          'descriptive_geomean_pct', round(row['descriptive_geomean_throughput_change_pct'], 4))
print('CURRENT_SOURCE_MATCH', result['current_source_matches_tested_candidate'], 'PRODUCTION_UNCHANGED', result['production_matches_protected_hashes'])
