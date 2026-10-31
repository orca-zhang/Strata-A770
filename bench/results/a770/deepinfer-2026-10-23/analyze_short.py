"""Pair the selected Short requests with existing host-stage timers (no GPU profiling)."""
import json
import hashlib
import math
from pathlib import Path
import re
import sys

HERE = Path(__file__).resolve().parent
pattern = re.compile(r'(\d+) windows, avg T ([\d.]+), [\d.]+ tokens/window, [\d.]+ ms/window = '
                     r'verify ([\d.]+) .*?\+ commit/emit ([\d.]+) \+ draft ([\d.]+);')

def pair(a, b, c):
    return {'throughput_change_pct': 100*(math.sqrt(a*c)/b-1),
            'baseline_drift_pct': 100*(c/a-1)} if a > 0 and b > 0 and c > 0 else None

for name in sys.argv[1:]:
    directory = HERE/name
    data = json.loads((directory/'results.json').read_text())
    assert data['complete'] and data['phase'] == 'model'
    assert list(data['prompts']) == ['Short']
    stages = []
    for run in data['runs']:
        samples = run['samples']['Short']
        index = samples.index(run['rows']['Short'])
        lines = run['stage_timings']
        assert len(lines) == 1 + len(run['case_warmups']['Short']) + len(samples)
        line = lines[1 + len(run['case_warmups']['Short']) + index]
        match = pattern.search(line)
        assert match, line
        windows, avg_t, verify, commit, draft = map(float, match.groups())
        row = run['rows']['Short']
        stages.append({'mode': run['mode'], 'selected_retained_index': index,
                       'generated': row['generated'], 'accepted': row['accepted'], 'offered': row['offered'],
                       'windows': int(windows), 'avg_T': avg_t, 'source_line': line,
                       'decode_ms_per_token': row['decode_ms']/row['generated'],
                       'retained_outputs_equal': all(s['tokens'] == row['tokens'] for s in samples),
                       'stage_ms_per_token': {k: v*windows/row['generated'] for k, v in
                                              [('verify', verify), ('commit_emit', commit), ('draft', draft)]},
                       'retained_tokens_per_second': [1000*s['generated']/s['decode_ms'] for s in samples]})
    comparisons = []
    for i in range(1, len(stages)-1):
        a, b, c = stages[i-1:i+2]
        if a['mode'] != 'baseline' or c['mode'] != 'baseline' or b['mode'] == 'baseline':
            continue
        decode = pair(*(r['decode_ms_per_token'] for r in (a, b, c)))
        outputs_equal = all(r['retained_outputs_equal'] for r in (a, b, c)) and len({tuple(r['rows']['Short']['tokens']) for r in data['runs'][i-1:i+2]}) == 1
        comparisons.append({'mode': b['mode'], 'candidate_run_index': i, 'decode': decode,
                            'outputs_equal': outputs_equal,
                            'short_screen_pass': outputs_equal and abs(decode['baseline_drift_pct']) <= 3 and decode['throughput_change_pct'] >= 1,
                            'host_stages': {k: pair(*(r['stage_ms_per_token'][k] for r in (a, b, c))) for k in b['stage_ms_per_token']}})
    result = {'complete': True, 'purpose': 'Short screening only; not a four-scene whole-model speed claim',
              'results_sha256': hashlib.sha256((directory/'results.json').read_bytes()).hexdigest(),
              'script_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              'notes': ['Stage averages are printed to 0.01 ms; normalized values inherit that rounding.',
                        'Stages use the same retained request selected for median decode time.'],
              'runs': stages, 'comparisons': comparisons}
    (directory/'stage-summary.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(name, [(r['mode'], round(r['decode']['throughput_change_pct'], 3), r['short_screen_pass']) for r in comparisons])
