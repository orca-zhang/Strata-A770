"""Audit archived CPU measurements; needs only Python's standard library."""
import hashlib
import json
import math
import statistics
from pathlib import Path

F = Path(__file__).resolve().parent
CASES = {(scope, name) for scope in ('Tokenizer.encode', 'Service.encode_prompt')
         for name in ('short', 'prose8k', 'code8k', 'unicode8k', 'prose32k')}


def read(path):
    return json.loads((F / path).read_text(encoding='utf-8'))


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def key(case):
    return case['scope'], case['name']


def derive(experiment):
    preparation = read(f'{experiment}/preparation.json')
    runs, pairs = [], []
    for phase in ('screen', 'confirm'):
        phase_runs = []
        for i, arm in enumerate(('baseline', 'candidate', 'baseline', 'candidate', 'baseline')):
            name = f'{phase}-{i}.json'
            run = read(f'{experiment}/{name}')
            assert run['arm'] == arm and run['cpu'] == 40
            assert run['vocab_size'] == 248320 and run['merges'] == 247587
            assert run['source_sha256'] == preparation['source_sha256' if arm == 'baseline' else 'candidate_sha256']
            assert len(run['cases']) == 10 and {key(c) for c in run['cases']} == CASES
            for case in run['cases']:
                values = case['seconds']
                assert len(values) == 7 and all(math.isfinite(v) and v > 0 for v in values)
                assert statistics.median(values) == case['median_seconds']
                assert case['calls_per_batch'] == (100 if case['name'] == 'short' else 1 if case['name'] == 'prose32k' else 3)
            phase_runs.append(run)
            runs.append(dict(run, phase=phase, ordinal=i, raw_file=name))
        for i in (1, 3):
            for case in phase_runs[i]['cases']:
                samples = [next(c for c in phase_runs[j]['cases'] if key(c) == key(case)) for j in (i-1, i, i+1)]
                assert len({(c['tokens'], c['token_ids_sha256']) for c in samples}) == 1
                a, b, c = (s['median_seconds'] for s in samples)
                drift, gain = 100 * (c / a - 1), 100 * (math.sqrt(a * c) / b - 1)
                pairs.append(dict(phase=phase, candidate_round=i, scope=case['scope'], name=case['name'],
                                  baseline_seconds=math.sqrt(a*c), candidate_seconds=b,
                                  baseline_drift_pct=drift, throughput_gain_pct=gain, eligible=abs(drift) <= 3))
    return runs, pairs


def summarize(pairs):
    eligible = [p for p in pairs if p['eligible']]
    rounds = []
    for phase in ('screen', 'confirm'):
        for i in (1, 3):
            values = [p['throughput_gain_pct'] for p in eligible if p['phase'] == phase and p['candidate_round'] == i]
            rounds.append(dict(phase=phase, candidate_round=i, eligible=len(values),
                               positive=sum(g > 0 for g in values),
                               median_gain_pct=statistics.median(values) if values else None))
    cases = []
    for scope, name in sorted(CASES):
        values = [p['throughput_gain_pct'] for p in eligible if key(p) == (scope, name)]
        gain = 100 * (math.exp(statistics.mean(math.log1p(g / 100) for g in values)) - 1) if values else None
        cases.append(dict(scope=scope, name=name, eligible=len(values), pooled_gain_pct=gain))
    gates = dict(each_round_positive_median=all(r['median_gain_pct'] is not None and r['median_gain_pct'] > 0 for r in rounds),
                 each_round_positive_fraction_at_least_75pct=all(r['eligible'] and 4*r['positive'] >= 3*r['eligible'] for r in rounds),
                 no_case_pooled_regression_over_2pct=all(c['pooled_gain_pct'] is None or c['pooled_gain_pct'] >= -2 for c in cases),
                 at_least_30_eligible_pairs=len(eligible) >= 30,
                 every_case_at_least_two_eligible_pairs=all(c['eligible'] >= 2 for c in cases))
    return dict(pairs=len(pairs), eligible=len(eligible), positive=sum(p['throughput_gain_pct'] > 0 for p in eligible),
                max_abs_baseline_drift_pct=max(abs(p['baseline_drift_pct']) for p in pairs),
                rounds=rounds, cases=cases, gates=gates)


def main():
    archive = read('archive.json')
    for name, digest in archive['sha256'].items():
        assert sha(F / name) == digest, name
    assert set(archive['sha256']) == {p.relative_to(F).as_posix() for p in F.rglob('*')
                                    if p.is_file() and '__pycache__' not in p.parts and p.name != 'archive.json'}
    baseline = (F / 'sources/baseline/strata_tokenizer.py').read_bytes()
    candidate = (F / 'sources/candidate/strata_tokenizer.py').read_bytes()
    old = b'"".join(BYTE_TO_UNICODE[b] for b in piece.encode("utf-8"))'
    new = b'piece.encode("utf-8").decode("latin1").translate(BYTE_TO_UNICODE)'
    assert baseline.count(old) == 1 and baseline.replace(old, new) == candidate
    summary, outputs = read('summary.json'), {}
    for experiment in ('initial', 'repeat'):
        preparation = read(f'{experiment}/preparation.json')
        for arm, field in (('baseline', 'source_sha256'), ('candidate', 'candidate_sha256')):
            assert sha(F / 'sources' / arm / 'strata_tokenizer.py') == preparation[field]
        for name, digest in preparation['frozen_serving_sha256'].items():
            if name.endswith('.pyd'):
                assert archive['omitted_binary_sha256'][name] == digest
            else:
                assert sha(F / 'sources' / name) == digest, name
        differential = read(f'{experiment}/differential.json')
        assert differential['pass'] is True
        assert [differential[k] for k in ('seed', 'raw_alphabet_bytes', 'byte_only_utf8_comparisons',
                                         'surrogate_errors', 'real_vocab_token_id_comparisons')] == [20261024, 256, 10000, 4, 1600]
        runs, pairs = derive(experiment)
        saved = read(f'{experiment}/results.json')
        assert saved['runs'] == runs
        assert saved['pairs'] == (pairs[:20] if experiment == 'initial' else pairs)
        assert summary[experiment] == summarize(pairs)
        if experiment == 'initial':
            assert not saved['complete'] and len(saved['runs']) == 10
            assert (F / 'initial/results.json').read_bytes() == (F / 'initial/interrupted-results.json').read_bytes()
            recovered = read('initial/recovered-pairs.json')
            assert recovered['pairs'] == pairs
            assert recovered['interrupted_results_sha256'] == sha(F / 'initial/interrupted-results.json')
        else:
            assert saved['complete'] and saved['production_unchanged'] and saved['workspace_source_unchanged']
        for run in runs:
            for case in run['cases']:
                value = case['tokens'], case['token_ids_sha256']
                assert outputs.setdefault(key(case), value) == value
    assert summary['decision'] == 'not_adopted'
    assert (summary['initial']['eligible'], summary['initial']['positive']) == (15, 15)
    assert (summary['repeat']['eligible'], summary['repeat']['positive']) == (14, 13)
    assert not all(summary['repeat']['gates'].values())
    checks = read('final-checks.json')
    assert checks['production_before_sha256'] == checks['production_after_sha256'] == read('repeat/preparation.json')['production_sha256']
    assert checks['runtime_source_sha256'] == read('repeat/preparation.json')['source_sha256']
    assert checks['live_regression_returncode'] == 0
    assert 'CHECK_SUMMARY 23 0 0 0' in (F / 'initial/candidate-tests.log').read_text(encoding='utf-8')
    assert 'Ran 1 test' in (F / 'regression/live-test.log').read_text(encoding='utf-8')
    assert 'test_byte_mapping_is_utf8' in (F / 'regression/test_detok.py').read_text(encoding='utf-8')
    print('PASS: 20 processes, 200 medians, 1400 batches, 80 pairs; token hashes agree.')
    print('Eligible: initial 15/40 (15 positive); repeat 14/40 (13 positive). Candidate not adopted.')


if __name__ == '__main__':
    main()
