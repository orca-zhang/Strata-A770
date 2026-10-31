"""Stdlib-only audit of frozen-reference checks, raw timings, exclusions and archive hashes."""
import hashlib
import json
import math
from pathlib import Path
import statistics

F = Path(__file__).resolve().parent

def read(name):
    return json.loads((F / name).read_text(encoding='utf-8'))

def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()

def derive(arms, execution):
    assert len(arms) == 20
    pairs = []
    for phase in ('screen', 'confirm'):
        for mode in (0, 1):
            group = [a for a in arms if a['phase'] == phase and a['mode'] == mode]
            assert [a['side'] for a in group] == ['baseline', 'candidate', 'baseline', 'candidate', 'baseline']
            assert [a['arm'] for a in group] == list(range(5))
            for i in (1, 3):
                for n in (1, 4, 6):
                    rows = [next(r for r in group[j]['rows'] if r['T'] == n) for j in (i - 1, i, i + 1)]
                    assert all(r['format'] == 1 and r['mode'] == mode and len(r['samples_us']) == 9 for r in rows)
                    for r in rows:
                        assert all(math.isfinite(v) and v > 0 for v in r['samples_us'])
                        assert abs(r['us'] - statistics.median(r['samples_us'])) <= 1e-8
                    a, b, c = [r['us'] for r in rows]
                    gain = 100 * (math.sqrt(a*c) / b - 1)
                    drift = 100 * (c / a - 1)
                    pairs.append(dict(execution=execution, phase=phase, mode=mode, arm=i, T=n,
                                      baseline_us=math.sqrt(a*c), candidate_us=b,
                                      throughput_change_pct=gain, baseline_drift_pct=drift,
                                      eligible=abs(drift) <= 3))
    return pairs

def main():
    archive, report = read('archive.json'), read('results.json')
    for name, digest in archive['sha256'].items():
        assert sha(F / name) == digest, name
    assert set(archive['sha256']) == {p.relative_to(F).as_posix() for p in F.rglob('*')
                                    if p.is_file() and '__pycache__' not in p.parts and p.name != 'archive.json'}
    pairs = []
    for name, execution in (('raw.json', 'prototype-direct'), ('graph-raw.json', 'prototype-graph'),
                            ('integrated-raw.json', 'retained-direct')):
        arms = read(name)
        for arm in arms:
            lines = (F / arm['log']).read_text(encoding='utf-8').splitlines()
            raw = [json.loads(line[7:]) for line in lines if line.startswith('RESULT ')]
            assert raw == arm['rows']
        pairs.extend(derive(arms, execution))
    assert pairs == report['pairs']
    checks = read('checks.json')
    assert checks['baseline-steps'] == checks['candidate-steps']
    assert all('kv_q8_parity: OK' in checks[s + '-q8'] for s in ('baseline', 'candidate'))
    assert all('PASS kv_steps: 198 ' in (F / ('final-check-' + s + '.log')).read_text() for s in ('baseline', 'candidate'))
    for value in (0, 1):
        assert (F / f'integrated-check-{value}-steps.log').read_text() == checks['baseline-steps']
        assert 'kv_q8_parity: OK' in (F / f'integrated-check-{value}-q8.log').read_text()
        assert 'PASS kv_steps: 198 ' in (F / f'integrated-check-{value}-final.log').read_text()
    assert report['production_unchanged'] and not report['whole_model_speedup_claimed']
    assert report['default_enabled'] is False
    print('PASS: frozen KV fingerprints, CPU reference, OFF/ON 198 graph/storage checks, 60 processes / 72 comparisons.')
    for execution in ('prototype-direct', 'prototype-graph', 'retained-direct'):
        rows = [p for p in pairs if p['execution'] == execution]
        valid = [p['throughput_change_pct'] for p in rows if p['eligible']]
        print(execution, 'eligible', len(valid), '/', len(rows), 'median', statistics.median(valid) if valid else None)

if __name__ == '__main__':
    main()
