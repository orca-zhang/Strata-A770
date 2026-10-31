"""Recompute CPU byte-cache measurements with only the standard library."""
import hashlib
import json
import math
import statistics
from pathlib import Path

F = Path(__file__).resolve().parent


def read(name):
    return json.loads((F / name).read_text(encoding='utf-8'))


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def derive():
    prep, runs, pairs = read('preparation.json'), [], []
    for phase in ('screen', 'confirm'):
        phase_runs = []
        for i, arm in enumerate(('baseline', 'candidate', 'baseline', 'candidate', 'baseline')):
            name = f'{phase}-{i}.json'
            run = read(name)
            assert run['cpu'] == 40 and run['arm'] == arm
            assert run['source_sha256'] == prep['source_sha256' if arm == 'baseline' else 'candidate_sha256']
            assert len(run['cases']) == 5 and {c['name'] for c in run['cases']} == set(prep['cases'])
            for case in run['cases']:
                values = case['seconds']
                assert len(values) == 7 and all(math.isfinite(v) and v > 0 for v in values)
                assert case['median_seconds'] == statistics.median(values)
                assert case['calls_per_batch'] == prep['cases'][case['name']]
            phase_runs.append(run)
            runs.append(dict(run, phase=phase, ordinal=i, raw_file=name))
        assert len({r['input_ids_sha256'] for r in phase_runs}) == 1
        for i in (1, 3):
            for case in phase_runs[i]['cases']:
                samples = [next(c for c in phase_runs[j]['cases'] if c['name'] == case['name']) for j in (i-1, i, i+1)]
                assert len({c['output_sha256'] for c in samples}) == 1
                a, b, c = (s['median_seconds'] for s in samples)
                drift, gain = 100*(c/a-1), 100*(math.sqrt(a*c)/b-1)
                pairs.append(dict(phase=phase, candidate_round=i, name=case['name'],
                                  baseline_drift_pct=drift, throughput_gain_pct=gain, eligible=abs(drift) <= 3))
    return runs, pairs


def summarize(pairs):
    phases, cases, rounds = [], [], []
    for phase in ('screen', 'confirm'):
        valid = [p for p in pairs if p['phase'] == phase and p['eligible']]
        phases.append(dict(phase=phase, eligible=len(valid), positive=sum(p['throughput_gain_pct'] > 0 for p in valid),
                           covered_cases=len({p['name'] for p in valid})))
        for i in (1, 3):
            values = [p['throughput_gain_pct'] for p in valid if p['candidate_round'] == i]
            rounds.append(dict(phase=phase, candidate_round=i, eligible=len(values), positive=sum(g > 0 for g in values),
                               median_gain_pct=statistics.median(values) if values else None))
    for name in read('preparation.json')['cases']:
        values = [p['throughput_gain_pct'] for p in pairs if p['eligible'] and p['name'] == name]
        gain = 100*(math.exp(statistics.mean(math.log1p(g/100) for g in values))-1) if values else None
        cases.append(dict(name=name, eligible=len(values), pooled_gain_pct=gain,
                          eligible_gains_pct=values))
    gates = dict(each_phase_at_least_8_eligible_pairs=all(p['eligible'] >= 8 for p in phases),
                 each_phase_covers_all_cases=all(p['covered_cases'] == 5 for p in phases),
                 each_round_positive_median=all(r['median_gain_pct'] is not None and r['median_gain_pct'] > 0 for r in rounds),
                 each_round_at_least_75pct_positive=all(r['eligible'] and 4*r['positive'] >= 3*r['eligible'] for r in rounds),
                 no_case_pooled_regression_over_2pct=all(c['pooled_gain_pct'] is None or c['pooled_gain_pct'] >= -2 for c in cases))
    return dict(phases=phases, rounds=rounds, cases=cases, gates=gates, performance_pass=all(gates.values()))


def main():
    archive, prep, checks = read('archive.json'), read('preparation.json'), read('checks.json')
    for name, digest in archive['sha256'].items():
        assert sha(F / name) == digest, name
    assert set(archive['sha256']) == {p.relative_to(F).as_posix() for p in F.rglob('*')
                                    if p.is_file() and '__pycache__' not in p.parts and p.name != 'archive.json'}
    baseline = (F / 'baseline/strata_tokenizer.py').read_bytes()
    candidate = (F / 'candidate/strata_tokenizer.py').read_bytes()
    old = b'        self.tokens = tokens\r\n'
    hit = b'        cache = self.__dict__.setdefault("_bytes_cache", {})'
    assert baseline.count(old) == baseline.count(hit) == 1
    assert baseline.replace(old, old + b'        self._bytes_cache: dict[int, bytes] = {}\r\n').replace(hit, b'        cache = self._bytes_cache') == candidate
    assert sha(F / 'baseline/strata_tokenizer.py') == prep['source_sha256']
    assert sha(F / 'candidate/strata_tokenizer.py') == prep['candidate_sha256']
    for name, digest in prep['frozen_sha256'].items():
        if name.endswith('.pyd'):
            assert archive['omitted_binary_sha256'][name] == digest
        else:
            assert sha(F / name) == digest
    assert checks['complete'] and checks['vocabulary'] == 248320 and checks['repeated_byte_checks'] == 496640
    assert checks['streaming_prefix_checks'] == 7576 and checks['decode_error_policy_checks'] == 192
    assert checks['regression_tests'] == 9 and checks['failures'] == checks['errors'] == checks['skips'] == 0
    assert 'Ran 9 tests' in (F / 'tests.log').read_text(encoding='utf-8')
    runs, pairs = derive()
    result = read('results.json')
    assert result['complete'] and result['production_unchanged'] and result['live_source_still_baseline']
    assert result['runs'] == runs and result['pairs'] == pairs
    assert len({r['input_ids_sha256'] for r in runs}) == 1
    for name in prep['cases']:
        assert len({c['output_sha256'] for r in runs for c in r['cases'] if c['name'] == name}) == 1
    summary = read('summary.json')
    assert summary['measurement'] == summarize(pairs)
    final = read('final-checks.json')
    assert final['production_sha256'] == prep['production_sha256']
    assert final['live_regression_returncode'] == 0
    assert 'Ran 2 tests' in (F / 'live-tests.log').read_text(encoding='utf-8')
    assert final['live_regression_source_sha256'] == sha(F / 'regression/test_detok.py')
    if summary['decision'] == 'adopted':
        update = read('adoption-update.json')
        assert update['user_instruction'] == '门槛更新成1%，算子的收益只要是正的都接受'
        assert update['general_performance_min_gain_pct'] == 1 and update['operator_gain_must_exceed_pct'] == 0
        assert update['baseline_drift_exclusion_pct'] == 3 and not update['original_coverage_required_for_retention']
        assert update['operator_retention_pass']
        assert all(c['eligible'] and c['pooled_gain_pct'] > 0 for c in summary['measurement']['cases'])
        assert all(p['throughput_gain_pct'] > 0 for p in pairs)
        for name, digest in update['original_measurement_sha256'].items():
            assert sha(F / name) == digest
        before = read('summary.before-adoption.json')
        assert before['decision'] == 'not_adopted' and before['measurement'] == summary['measurement']
        assert read('final-checks.before-adoption.json')['runtime_source_sha256'] == prep['source_sha256']
        assert final['runtime_source_sha256'] == prep['candidate_sha256']
    else:
        assert summary['decision'] == 'not_adopted' and not summary['measurement']['performance_pass']
        assert final['runtime_source_sha256'] == prep['source_sha256']
    print('PASS: 10 processes, 50 medians, 350 batches, 20 pairs; byte/output hashes match.')
    print('Decision:', summary['decision'])
    for phase in summary['measurement']['phases']:
        print(phase['phase'], 'eligible', phase['eligible'], '/10; positive', phase['positive'])


if __name__ == '__main__':
    main()
