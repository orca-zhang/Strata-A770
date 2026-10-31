"""Offline audit: python <this file>; no GPU, compiler or private builds needed."""
import hashlib
import json
import math
import re
import statistics
from pathlib import Path

F = Path(__file__).resolve().parent
def read(path):
    return json.loads((F / path).read_text(encoding='utf-8'))

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def rows(path, tag):
    return [json.loads(x) for x in re.findall(r'^' + tag + r' (.+)$', (F / path).read_text(encoding='utf-8'), re.M)]

def close(a, b):
    assert math.isfinite(a) and math.isclose(a, b, rel_tol=1e-9, abs_tol=1e-7), (a, b)

archive, result, summary = read('archive.json'), read('results.json'), read('summary.json')
for path, digest in archive['sha256'].items():
    assert sha(F / path) == digest, path
assert result['production_unchanged'] and result['unchanged_dependencies'] == 96
assert result['env']['STRATA_PREFILL_NATIVE_XMX'] == '0'
assert result['env']['STRATA_PREFILL_HALF_OUTPUT'] == '0'
assert result['env']['STRATA_PREFILL_HALF_BF16'] == '0'
assert result['env']['STRATA_PREFILL_XMX_PROFILE'] == '0'
baseline = (F / 'baseline-source/gemm.dp.cpp').read_bytes()
candidate = (F / 'candidate-source/gemm.dp.cpp').read_bytes()
old = b'''    q->parallel_for(sycl::range<1>((size_t) T * K), [=](sycl::id<1> i) {
        const size_t offset = (size_t(i) / K) * ldx + size_t(i) % K;'''.replace(b'\n', b'\r\n')
new = b'''    q->parallel_for(sycl::range<2>(size_t(T), size_t(K)), [=](sycl::id<2> i) {
        const size_t offset = i[0] * ldx + i[1];'''.replace(b'\n', b'\r\n')
assert baseline.count(old) == 1 and baseline.replace(old, new) == candidate
for name in ('prefill.cpp', 'gemm_parity.cpp', 'include/strata/prefill/gemm.hpp'):
    assert (F / 'baseline-source' / name).read_bytes() == (F / 'candidate-source' / name).read_bytes(), name

checks = 0
for arm in ('baseline', 'candidate'):
    build, check = read(arm + '/build.json'), read(arm + '/results.json')
    assert build['complete'] and check['complete']
    for name, live in [('gemm.dp.cpp', 'sycl/src/prefill/gemm.dp.cpp'),
                       ('gemm_parity.cpp', 'sycl/src/prefill/gemm_parity.cpp'),
                       ('prefill.cpp', 'sycl/src/prefill/prefill.cpp'),
                       ('include/strata/prefill/gemm.hpp', 'include/strata/prefill/gemm.hpp')]:
        assert sha(F / (arm + '-source') / name) == build['sources'][live]
    for i in (0, 1):
        path = f'{arm}/{i}-check.log'
        assert rows(path, 'SUMMARY') == [{'checks': 1015, 'mismatched_cases': 0}]
        checks += 1015
    assert build['reused_libraries'] == read('baseline/build.json')['reused_libraries']

for path in archive['sha256']:
    if path.endswith('.log'):
        for item in rows(path, 'PARITY'):
            assert item['different'] == item['padding_different'] == 0 and item['max_abs'] == 0, path
        for item in rows(path, 'CHAIN'):
            assert item['different'] == 0, path
for item in result['large_checks']:
    assert rows(item['log'], 'SUMMARY') == [{'checks': 270, 'mismatched_cases': 0}]
    checks += 270

medians, batches, pairs = 0, 0, []
key = lambda r: (r['bf16'], r['n'], r['k'], r['t'])
for run in result['runs']:
    assert run['returncode'] == 0 and sha(F / run['log']) == run['log_sha256']
    assert rows(run['log'], 'SUMMARY') == run['summary']
    assert rows(run['log'], 'BENCH') == run['benches'] and rows(run['log'], 'BATCH') == run['batches']
    checks += run['summary'][0]['checks']
    medians += len(run['benches'])
    batches += len(run['batches'])
    for item in run['benches']:
        values = [r['total_us'] for r in run['batches'] if key(r) == key(item)]
        assert len(values) == 5 and all(math.isfinite(v) and v > 0 for v in values)
        close(item['total_us'], statistics.median(values))
for phase in summary['phases']:
    for shape in ('small', 'large'):
        runs = sorted((r for r in result['runs'] if r['phase'] == phase and r['shape'] == shape), key=lambda r: r['ordinal'])
        assert [r['arm'] for r in runs] == ['baseline', 'candidate', 'baseline', 'candidate', 'baseline']
        for i in (1, 3):
            for item in runs[i]['benches']:
                a, b, c = [next(r['total_us'] for r in runs[j]['benches'] if key(r) == key(item)) for j in (i-1, i, i+1)]
                base, drift = math.sqrt(a*c), 100*(c/a-1)
                stored = next(r for r in result['comparisons'] if r['phase'] == phase and r['shape'] == shape
                              and r['candidate_run'] == i and key(r) == key(item))
                close(stored['baseline'], base)
                close(stored['candidate'], b)
                close(stored['baseline_drift_pct'], drift)
                close(stored['throughput_change_pct'], 100*(base/b-1))
                assert stored['eligible'] == (abs(drift) <= 3)
                pairs.append(stored)
assert len(pairs) == len(result['comparisons']) == summary['paired_comparisons']
eligible = [p for p in pairs if p['eligible']]
assert len(eligible) == summary['eligible_pairs']
assert len(pairs) - len(eligible) == summary['excluded_pairs'] and len(result['runs']) == summary['independent_processes']
assert sum(p['throughput_change_pct'] > 0 for p in eligible) == summary['positive_pairs']
for item in summary['rounds']:
    selected = [p for p in eligible if p['phase'] == item['phase'] and p['candidate_run'] == item['candidate_run']]
    close(item['median_gain_pct'], statistics.median(p['throughput_change_pct'] for p in selected))
for item in summary['cases']:
    selected = [p for p in eligible if (p['bf16'], p['n'], p['t']) == (item['bf16'], item['n'], item['t'])]
    close(item['descriptive_gain_pct'], 100*(math.exp(statistics.mean(math.log1p(p['throughput_change_pct']/100) for p in selected))-1))
assert checks == summary['correctness_checks'] and medians == summary['median_points'] and batches == summary['batch_rows']
cpu = read('cpu-checks/results.json')
assert cpu['pass']
for path, digest in cpu['sources'].items():
    relative = path.replace('\\', '/').split('/Strata/', 1)[1]
    assert sha(F / 'cpu-checks/source' / relative) == digest
assert len(re.findall(r' ok$', (F / 'cpu-checks/test.log').read_text(), re.M)) == summary['cpu_draft_policy_checks'] == 56
assert summary['status'] == 'rejected_at_screen_source_not_integrated'
assert all(r['median_gain_pct'] < 0 for r in summary['rounds'])
assert summary['positive_pairs'] / summary['eligible_pairs'] < .75
interrupted = read('interrupted-results.json')
assert not interrupted['complete']
for field in ('runs', 'comparisons', 'large_checks'):
    assert interrupted[field] == result[field], field
issue = summary['workflow_issue']
assert issue['raw_data_complete_before_error'] and not issue['measurements_rerun']
executed, corrected = F / 'scripts/check_widen_activation.executed.py', F / 'scripts/check_widen_activation.py'
assert sha(executed) == issue['executed_script_sha256'] and sha(corrected) == issue['corrected_script_sha256']
assert executed.read_bytes().replace(b'throughput_gain_pct', b'throughput_change_pct') == corrected.read_bytes()
print(json.dumps({'pass': True, 'archived_files': len(archive['sha256']), 'checks': checks,
                  'processes': len(result['runs']), 'median_points': medians, 'batches': batches, 'pairs': len(pairs)}))
