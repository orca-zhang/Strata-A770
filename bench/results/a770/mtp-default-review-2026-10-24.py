"""Audit recorded arithmetic and output hashes offline; does not rerun the GPU."""
import json
import math
from pathlib import Path
import statistics

data = json.loads(Path(__file__).with_suffix('.json').read_text(encoding='utf-8'))
rows = data['functional']['rows']
assert len(rows)==12 and sum(r['output_tokens'] for r in rows)==1144
for name in {r['name'] for r in rows}:
    pair = [r for r in rows if r['name']==name]
    assert len(pair)==2 and {r['floor'] for r in pair}=={0, .5}
    assert len({r['output_sha256'] for r in pair})==1
screen = data['short_confirmation']
assert [a['floor'] for a in screen['arms']]==[0, .5, 0]
assert all(len(a['rows'])==5 for a in screen['arms'])
assert len({r['output_sha256'] for a in screen['arms'] for r in a['rows']})==1
for comparison in screen['comparisons']:
    key = comparison['metric']
    a, b, c = [statistics.median(r[key]/(r['generated'] if key=='decode_ms' else 1) for r in arm['rows']) for arm in screen['arms']]
    assert math.isclose(comparison['throughput_change_pct'], 100*(math.sqrt(a*c)/b-1))
    assert math.isclose(comparison['baseline_drift_pct'], 100*(c/a-1))
decode = next(c for c in screen['comparisons'] if c['metric']=='decode_ms')
assert abs(decode['baseline_drift_pct'])>screen['max_abs_baseline_drift_pct']
assert screen['excluded'] and not screen['adoption_passed']
assert not data['decision']['production_default_enabled'] and data['decision']['source_default']==0
print('PASS 12 functional hashes, 15 retained hashes, arithmetic and excluded confirmation')
