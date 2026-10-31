"""Offline audit of the frozen BPE rank-reference experiment (stdlib only)."""
import hashlib
import json
import math
import statistics
from pathlib import Path

F = Path(__file__).resolve().parent
OPERATOR = {'n1', 'n2', 'n4', 'n8', 'n16', 'n32', 'n64', 'unmergeable64', 'heap128'}
SCOPES = ('BPE.operator', 'Tokenizer.encode', 'Service.encode_prompt')
CASES = {('BPE.operator', name) for name in OPERATOR} | {
    (scope, name) for scope in SCOPES[1:] for name in ('short', 'prose8k', 'code8k', 'unicode8k', 'prose32k')}


def read(name):
    return json.loads((F / name).read_text(encoding='utf-8'))


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def key(case):
    return case['scope'], case['name']


def derive():
    prep, runs, pairs = read('preparation.json'), [], []
    for phase in ('screen', 'confirm'):
        phase_runs = []
        for i, arm in enumerate(('baseline', 'candidate', 'baseline', 'candidate', 'baseline')):
            name = f'{phase}-{i}.json'
            run = read(name)
            assert run['arm'] == arm and run['cpu'] == 40
            assert run['vocab_size'] == 248320 and run['merges'] == 247587
            assert run['source_sha256'] == prep['source_sha256' if arm == 'baseline' else 'candidate_sha256']
            assert len(run['cases']) == 19 and {key(c) for c in run['cases']} == CASES
            for case in run['cases']:
                values = case['seconds']
                assert len(values) == 7 and all(math.isfinite(v) and v > 0 for v in values)
                assert case['median_seconds'] == statistics.median(values)
                expected = 256 if case['scope'] == SCOPES[0] else 100 if case['name'] == 'short' else 1 if case['name'] == 'prose32k' else 3
                assert case['calls_per_batch'] == expected
            phase_runs.append(run)
            runs.append(dict(run, phase=phase, ordinal=i, raw_file=name))
        for i in (1, 3):
            for case in phase_runs[i]['cases']:
                samples = [next(c for c in phase_runs[j]['cases'] if key(c) == key(case)) for j in (i-1, i, i+1)]
                assert len({(c['tokens'], c['token_ids_sha256']) for c in samples}) == 1
                a, b, c = (s['median_seconds'] for s in samples)
                drift, gain = 100*(c/a-1), 100*(math.sqrt(a*c)/b-1)
                pairs.append(dict(phase=phase, candidate_round=i, scope=case['scope'], name=case['name'],
                                  baseline_seconds=math.sqrt(a*c), candidate_seconds=b,
                                  baseline_drift_pct=drift, throughput_gain_pct=gain, eligible=abs(drift) <= 3))
    return runs, pairs


def pooled(values):
    return 100*(math.exp(statistics.mean(math.log1p(v/100) for v in values))-1) if values else None


def summarize(pairs):
    phases, rounds, cases = [], [], []
    for phase in ('screen', 'confirm'):
        for scope in SCOPES:
            rows = [p for p in pairs if p['phase'] == phase and p['scope'] == scope and p['eligible']]
            phases.append(dict(phase=phase, scope=scope, eligible=len(rows), positive=sum(p['throughput_gain_pct'] > 0 for p in rows),
                               pooled_gain_pct=pooled([p['throughput_gain_pct'] for p in rows])))
        for i in (1, 3):
            rows = [p for p in pairs if p['phase'] == phase and p['candidate_round'] == i and p['eligible']]
            rounds.append(dict(phase=phase, candidate_round=i, eligible=len(rows)))
    for scope, name in sorted(CASES):
        rows = [p for p in pairs if key(p) == (scope, name) and p['eligible']]
        cases.append(dict(scope=scope, name=name, eligible=len(rows),
                          pooled_gain_pct=pooled([p['throughput_gain_pct'] for p in rows])))
    gates = dict(each_scope_positive_in_each_phase=all(p['pooled_gain_pct'] is not None and p['pooled_gain_pct'] > 0 for p in phases),
                 every_round_has_eligible_case=all(r['eligible'] for r in rounds),
                 no_prompt_case_pooled_regression_over_2pct=all(c['pooled_gain_pct'] is None or c['pooled_gain_pct'] >= -2
                                                               for c in cases if c['scope'] != SCOPES[0]))
    return dict(phases=phases, rounds=rounds, cases=cases, gates=gates, retention_pass=all(gates.values()))


def main():
    manifest, prep = read('archive.json'), read('preparation.json')
    for name, digest in manifest['sha256'].items():
        assert sha(F / name) == digest, name
    assert set(manifest['sha256']) == {p.relative_to(F).as_posix() for p in F.rglob('*')
                                     if p.is_file() and '__pycache__' not in p.parts and p.name != 'archive.json'}
    baseline, candidate = (F/'baseline/strata_tokenizer.py').read_bytes(), (F/'candidate/strata_tokenizer.py').read_bytes()
    old = b'            best, best_rank = None, None\r\n            for i in range(len(parts) - 1):'
    new = b'            best, best_rank = None, None\r\n            ranks = self.ranks\r\n            for i in range(len(parts) - 1):'
    lookup = b'                r = self.ranks.get((parts[i], parts[i + 1]))'
    assert baseline.count(old) == baseline.count(lookup) == 1
    assert baseline.replace(old, new).replace(lookup, b'                r = ranks.get((parts[i], parts[i + 1]))') == candidate
    assert sha(F/'baseline/strata_tokenizer.py') == prep['source_sha256']
    assert sha(F/'candidate/strata_tokenizer.py') == prep['candidate_sha256']
    for name, digest in prep['frozen_serving_sha256'].items():
        if name.endswith('.pyd'):
            assert manifest['omitted_binary_sha256'][name] == digest
        else:
            assert sha(F/name) == digest
    assert read('differential.json') == {'pass':True, 'seed':20261024, 'raw_alphabet_bytes':256,
                                       'byte_only_utf8_comparisons':10000, 'surrogate_errors':4,
                                       'real_vocab_token_id_comparisons':1600}
    assert read('regressions.json') == dict(tests=10, failures=0, errors=0, skips=0)
    assert 'Ran 10 tests' in (F/'tests.log').read_text(encoding='utf-8')
    failed = read('precheck-skipped/failure.json')
    assert failed['performance_processes_started'] == 0 and failed['skips'] == 5
    assert 'skipped=5' in (F/'precheck-skipped/tests.log').read_text(encoding='utf-8')
    runs, pairs = derive()
    result, summary, final = read('results.json'), read('summary.json'), read('final-checks.json')
    assert result['complete'] and result['production_unchanged'] and result['workspace_source_unchanged']
    assert result['runs'] == runs and result['pairs'] == pairs
    for case in CASES:
        assert len({c['token_ids_sha256'] for r in runs for c in r['cases'] if key(c) == case}) == 1
    assert summary['measurement'] == summarize(pairs)
    assert summary['decision'] == ('adopted' if summary['measurement']['retention_pass'] else 'not_adopted')
    assert final['production_sha256'] == prep['production_sha256']
    expected = prep['candidate_sha256'] if summary['decision'] == 'adopted' else prep['source_sha256']
    assert final['runtime_source_sha256'] == expected
    assert final['live_regression_returncode'] == 0
    print('PASS: 10 processes, 190 medians, 1330 batches, 76 pairs; outputs agree.')
    print('Decision:', summary['decision'])
    for phase in summary['measurement']['phases']:
        print(phase['phase'], phase['scope'], 'eligible', phase['eligible'], 'pooled gain', phase['pooled_gain_pct'])


if __name__ == '__main__':
    main()
