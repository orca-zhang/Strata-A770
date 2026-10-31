"""Check archived raw widening measurements without a GPU or build tree."""
import hashlib
import json
import math
from pathlib import Path
import re
import statistics

F = Path(__file__).resolve().parent

def read(path):
    return json.loads(path.read_text(encoding='utf-8'))

def tagged(path, tag):
    return [json.loads(s) for s in re.findall(r'^' + tag + r' (.+)$', path.read_text(encoding='utf-8'), re.M)]

def close(a, b):
    assert math.isfinite(a) and math.isfinite(b)
    assert math.isclose(a, b, rel_tol=1e-9, abs_tol=2e-6), (a, b)

archive = read(F / 'archive.json')
for name, digest in archive.items():
    assert hashlib.sha256((F / name).read_bytes()).hexdigest() == digest, name
points = batches = pairs = 0
for phase in ('screen', 'confirm', 'repeat'):
    root = F / phase
    if not root.exists():
        continue
    d = read(root / 'results.json')
    assert d['complete'] and d['production_unchanged'] and d['workspace_sources_unchanged']
    assert d['unchanged_dependencies'] == 96
    for shape in ('small', 'large'):
        runs = [r for r in d['runs'] if r['shape'] == shape]
        assert [r['candidate'] for r in runs] == [False, True, False, True, False]
        for i, run in enumerate(runs):
            log = root / f'{shape}-{i}.log'
            assert tagged(log, 'BENCH') == run['benches']
            assert tagged(log, 'BATCH') == run['batches']
            assert tagged(log, 'SUMMARY') == run['summary']
            assert run['returncode'] == 0
            assert run['summary'] == [{'checks': 6 if shape == 'small' else 12, 'mismatched_cases': 0}]
            for bench in run['benches']:
                samples = [s for s in run['batches'] if (s['bf16'], s['n'], s['k']) == (bench['bf16'], bench['n'], bench['k'])]
                assert sorted(s['batch'] for s in samples) == list(range(5))
                for metric in ('total_us', 'submit_us'):
                    close(statistics.median(s[metric] for s in samples), bench[metric])
                points += 1
                batches += len(samples)
        for row in (r for r in d['comparisons'] if r['shape'] == shape):
            i = row['candidate_run']
            times = [next(r['total_us'] for r in runs[j]['benches'] if (r['bf16'], r['n']) == (row['bf16'], row['n'])) for j in (i-1, i, i+1)]
            assert all(t > 0 for t in times)
            a, b, c = times
            close(row['baseline'], math.sqrt(a*c))
            close(row['candidate'], b)
            close(row['throughput_change_pct'], 100*(math.sqrt(a*c)/b-1))
            close(row['baseline_drift_pct'], 100*(c/a-1))
            assert row['eligible'] == (abs(row['baseline_drift_pct']) <= 3)
            pairs += 1
    if (root / 'build/results.json').exists():
        checks = read(root / 'build/results.json')
        assert checks['complete']
        assert [r['widen'] for r in checks['runs']] == [False, True]
        for i, row in enumerate(checks['runs']):
            assert row['returncode'] == 0
            assert row['summary'] == [{'checks': 1015, 'mismatched_cases': 0}]
            assert tagged(root / f'build/{i}-check.log', 'SUMMARY') == row['summary']
    for row in d['large_checks']:
        assert row['returncode'] == 0 and row['summary'] == [{'checks': 270, 'mismatched_cases': 0}]
        assert tagged(root / ('check-' + row['label'] + '.log'), 'SUMMARY') == row['summary']

merged = F / 'merged/build'
if merged.exists():
    d = read(merged / 'results.json')
    assert d['complete']
    for i, row in enumerate(d['runs']):
        assert row['returncode'] == 0 and row['summary'] == [{'checks': 1015, 'mismatched_cases': 0}]
        assert tagged(merged / f'{i}-check.log', 'SUMMARY') == row['summary']
model = F / 'model'
if model.exists():
    d = read(model / 'results.json')
    assert d['complete'] and d['outputs_equal'] and d['replay_equal'] and d['production_unchanged']
    assert d['large_check'] == {'returncode': 0, 'summary': [{'checks': 270, 'mismatched_cases': 0}]}
    assert tagged(model / 'large4.log', 'SUMMARY') == d['large_check']['summary']
    assert [r['label'] for r in d['runs']] == ['baseline', 'candidate']
    for row in d['runs']:
        assert 'expert_slots=4191 ' in row['info'] and 'expert_cache_mib=5760 ' in row['info']
        assert row['env']['STRATA_PREFILL_NATIVE_XMX'] == '0'
        assert [r['generated'] for r in row['requests']] == [8, 48, 48]
        assert row['requests'][0]['reused'] == 0
        assert all(r['reused'] == r['resume'] == 16 for r in row['requests'][1:])
        protocols = []
        tokens = []
        for line in (model / (row['label'] + '.protocol')).read_text().splitlines():
            assert not line.startswith('ERR ')
            if line.startswith(('T ', 'TB ')):
                tokens.extend(map(int, line.split()[1:] if line.startswith('T ') else line.split()[2:]))
            elif line.startswith('DONE '):
                protocols.append((tokens, line))
                tokens = []
        assert protocols == [(r['tokens'], r['done']) for r in row['requests']]
        assert row['requests'][1]['tokens'] == row['requests'][2]['tokens']
    for old, new in zip(d['runs'][0]['requests'], d['runs'][1]['requests']):
        for key in ('tokens', 'generated', 'finish', 'accepted', 'offered'):
            assert old[key] == new[key]
    print('PASS merged parity, 4GiB arena, Short greedy/seeded penalties/replay protocols, fixed cache')

summary = read(F / 'summary.json')
all_rows = []
for phase in summary['phases']:
    rows = read(F / phase['label'] / 'results.json')['comparisons']
    all_rows.extend(rows)
    assert phase['excluded'] == [r for r in rows if not r['eligible']]
    for index in (1, 3):
        values = [r['throughput_change_pct'] for r in rows if r['candidate_run'] == index and r['eligible']]
        close(phase['candidate_round_medians_pct'][str(index)], statistics.median(values))
    for case in phase['cases']:
        values = [r for r in rows if (r['bf16'], r['n']) == (case['bf16'], case['n']) and r['eligible']]
        assert case['eligible_pairs'] == len(values)
        close(case['descriptive_gain_pct'], 100*(math.prod(1+r['throughput_change_pct']/100 for r in values)**(1/len(values))-1))
for case in summary['combined_cases']:
    values = [r for r in all_rows if (r['bf16'], r['n']) == (case['bf16'], case['n']) and r['eligible']]
    assert case['eligible_pairs'] == len(values)
    close(case['descriptive_gain_pct'], 100*(math.prod(1+r['throughput_change_pct']/100 for r in values)**(1/len(values))-1))
assert summary['operator_scope']['eligible_pairs'] == sum(r['eligible'] for r in all_rows) == 34
assert summary['operator_scope']['positive_eligible_pairs'] == sum(r['eligible'] and r['throughput_change_pct'] > 0 for r in all_rows) == 30
manifest = read(F / 'merged/build/build.json')
sources = {'gemm.dp.cpp': 'sycl/src/prefill/gemm.dp.cpp', 'prefill.cpp': 'sycl/src/prefill/prefill.cpp',
           'gemm_parity.cpp': 'sycl/src/prefill/gemm_parity.cpp', 'include/strata/prefill/gemm.hpp': 'include/strata/prefill/gemm.hpp'}
for name, live in sources.items():
    assert hashlib.sha256((F / 'merged/source' / name).read_bytes()).hexdigest() == manifest['sources'][live] == summary['sources_sha256'][live]
integration = read(F / 'integration.json')
assert hashlib.sha256((F / 'merged/before-gemm.dp.cpp').read_bytes()).hexdigest() == integration['source_before_sha256']
assert integration['source_after_sha256'] == manifest['sources']['sycl/src/prefill/gemm.dp.cpp']
assert integration['merged_exe_sha256'] == manifest['exe_sha256']
assert not summary['default_enabled'] and not summary['model_performance_acceptance']
print(json.dumps({'pass': True, 'archived_files': len(archive), 'median_points': points, 'batch_rows': batches, 'paired_comparisons': pairs}))
