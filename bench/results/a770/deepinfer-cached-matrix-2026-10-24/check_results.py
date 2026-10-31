"""Stdlib audit: raw protocol, medians, drift, screen rule and archive hashes."""
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


def pair(a, b, c):
    base = math.sqrt(a*c)
    return dict(baseline=base, candidate=b, throughput_change_pct=100*(base/b-1), baseline_drift_pct=100*(c/a-1))


def derive(data, prep):
    assert len(data['runs']) == 3 and [r['mode'] for r in data['runs']] == prep['modes']
    comparisons = []
    for name, ids in prep['prompts'].items():
        cases = [run['cases'][name] for run in data['runs']]
        rows = [v for c in cases for v in [c['initial'], *c['warmups'], *c['samples']]]
        assert all(len(c['warmups']) == prep['warmups'] and len(c['samples']) == prep['retained'] for c in cases)
        assert all(v['prompt_tokens'] == len(ids) and v['generated'] == len(v['tokens']) > 0
                   and v['generated'] <= prep['max_tokens'] and v['resume'] == v['reused'] for v in rows)
        assert all(v['finish'] in ('length', 'stop') for v in rows)
        outputs_equal = len({tuple(v['tokens']) for v in rows}) == 1
        cached = [v for c in cases for v in [*c['warmups'], *c['samples']]]
        reused = sorted({v['reused'] for v in cached})
        if name != 'Short':
            assert all(len(ids)-16 <= v['reused'] < len(ids) for v in cached)
        metrics = {}
        for key in ('decode_ms', 'prompt_ms', 'ttft_ms'):
            medians = []
            for case in cases:
                values = [row[key]/row['generated'] if key == 'decode_ms' else row[key] for row in case['samples']]
                assert all(math.isfinite(v) and v > 0 for v in values)
                medians.append(statistics.median(values))
            metrics[key] = pair(*medians)
        comparisons.append(dict(case=name, outputs_equal=outputs_equal, reuse_equal=len(reused) == 1,
                                metrics=metrics, decode_eligible=abs(metrics['decode_ms']['baseline_drift_pct']) <= 3,
                                cached_reused_tokens=reused))
    exact = all(c['outputs_equal'] and c['reuse_equal'] for c in comparisons)
    eligible = all(c['decode_eligible'] for c in comparisons)
    gains = [c['metrics']['decode_ms']['throughput_change_pct'] for c in comparisons]
    overall = 100*(math.exp(statistics.mean(math.log1p(g/100) for g in gains))-1)
    return dict(comparisons=comparisons, outputs_and_reuse_equal=exact, all_decode_pairs_eligible=eligible,
                descriptive_decode_geomean_pct=overall,
                screen_pass=exact and eligible and overall >= 1 and min(gains) >= -2,
                confirmed_model_speedup=False)


def protocol_rows(path):
    rows, tokens, resume = [], [], 0
    for line in path.read_text(encoding='utf-8').splitlines():
        if line.startswith(('T ', 'TB ')):
            tokens.extend(map(int, line.split()[1:] if line.startswith('T ') else line.split()[2:]))
        elif line.startswith('RESUME '):
            resume = int(line.split()[1])
        elif line.startswith('DONE '):
            p = line.split()
            rows.append(dict(tokens=tokens, generated=int(p[1]), prompt_tokens=int(p[2]),
                             prompt_ms=float(p[3]), decode_ms=float(p[4]), finish=p[5],
                             accepted=int(p[6]), offered=int(p[7]), reused=int(p[8]), resume=resume, done=line))
            tokens = []
    assert not tokens
    return rows


def main():
    archive, prep, data = read('archive.json'), read('preparation.json'), read('results.json')
    for name, digest in archive['sha256'].items():
        assert sha(F / name) == digest, name
    assert set(archive['sha256']) == {p.relative_to(F).as_posix() for p in F.rglob('*')
                                    if p.is_file() and '__pycache__' not in p.parts and p.name != 'archive.json'}
    assert data['complete'] and data['production_unchanged'] and 'failure' not in data
    assert data['rule_before_measurement'] == prep['rule']
    assert data['preparation_sha256'] == sha(F / 'preparation.json')
    for name, digest in data['tooling_sha256'].items():
        assert sha(F / name) == digest
    assert data['summary'] == derive(data, prep)
    assert data['summary']['outputs_and_reuse_equal']
    for i, run in enumerate(data['runs']):
        assert run['capacity'] == dict(expert_slots=5771, expert_cache_mib=7922)
        expected = [v for c in run['cases'].values() for v in [c['initial'], *c['warmups'], *c['samples']]]
        raw = protocol_rows(F / f'{i}-{run["mode"]}.protocol')
        assert len(raw) == len(expected) == 24
        for a, b in zip(raw, expected):
            assert all(a[key] == b[key] for key in a)
    final = read('final-checks.json')
    assert final['production_unchanged'] and final['runtime_defaults_changed'] is False
    assert final['frozen_sha256'] == prep['frozen_sha256']
    print('PASS: 3 common-EXE processes; 4 cases; 72 protocol-verified requests; 36 retained samples.')
    print('Screen pass:', data['summary']['screen_pass'], '(no independent confirmation)')
    for c in data['summary']['comparisons']:
        print(c['case'], c['metrics']['decode_ms']['throughput_change_pct'], c['decode_eligible'])


if __name__ == '__main__':
    main()
