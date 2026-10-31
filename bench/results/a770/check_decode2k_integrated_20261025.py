"""Recompute the frozen whole-build 2K screen; --local checks raw evidence."""
import hashlib
import json
import math
from pathlib import Path
import re
import statistics
import sys

H = Path(__file__).resolve().parent
R = H.parents[2]
CASES = ('Code2K', 'Prose2K')


def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()


def request(row, label):
    return dict(build=label, case=row['case'], warmup=row['warmup'], input_tokens=row['prompt_tokens'],
                output_tokens=row['generated'], reused=row['reused'], accepted=row['accepted'],
                offered=row['offered'], prompt_ms=row['prompt_ms'], decode_ms=row['decode_ms'],
                output_sha256=hashlib.sha256(json.dumps(row['tokens'], separators=(',', ':')).encode()).hexdigest())


def summarize(rows):
    result = {}
    for case in CASES:
        groups = [[r for r in rows if r['case'] == case and r['build'] == label and not r['warmup']]
                  for label in ('baseline', 'B', 'A2')]
        assert all(len(g) == 3 for g in groups)
        rates = [[128000 / r['decode_ms'] for r in g] for g in groups]
        medians = list(map(statistics.median, rates))
        drift = 100 * (medians[0] / medians[2] - 1)
        result[case] = dict(median_tps=dict(zip(('baseline', 'B', 'A2'), medians)),
            range_tps=dict(zip(('baseline', 'B', 'A2'), ([min(v), max(v)] for v in rates))),
            throughput_change_pct=100 * (medians[1] / math.sqrt(medians[0] * medians[2]) - 1),
            baseline_time_drift_pct=drift, drift_eligible=abs(drift) <= 3,
            outputs_equal=len({r['output_sha256'] for g in groups for r in g}) == 1,
            required_gain_to40_pct=100 * (40 / medians[1] - 1))
    return result


def host_profile(text):
    rows, routes, hosts = [], [], []
    last = None
    for line in text.splitlines():
        m = re.match(r'verify host: pos=(\d+) T=(\d+) submit=([\d.]+) wait=([\d.]+) ms', line)
        if m:
            last = tuple(map(float, m.groups()))
        m = re.match(r'strata device routing: (\d+)/(\d+) entries in VRAM', line)
        if m:
            # ponytail: routing follows decode windows; unpaired Short-prefill host lines are excluded.
            assert last is not None
            routes.append(tuple(map(int, m.groups())))
            hosts.append(last)
            last = None
        if line.startswith('strata decode timing:'):
            windows = int(re.search(r'(\d+) windows', line)[1])
            assert len(hosts) == len(routes) == windows
            hits, entries = map(sum, zip(*routes))
            rows.append(dict(case=('initializer', *CASES)[len(rows)], windows=windows,
                vram_entries=hits, routed_entries=entries, vram_entry_hit_pct=100 * hits / entries,
                submit_ms_mean=statistics.mean(v[2] for v in hosts),
                post_submit_wait_ms_mean=statistics.mean(v[3] for v in hosts)))
            routes, hosts = [], []
    assert len(rows) == 3 and not routes and not hosts
    return rows


def audit(local=False):
    d = json.loads((H / 'decode2k-integrated-2026-10-25.json').read_text(encoding='utf-8'))
    assert d['complete'] and d['scope'] == 'screen' and not d['target_achieved']
    assert len(d['requests']) == 24
    for row in d['requests']:
        assert row['input_tokens'] == 2048 and row['output_tokens'] == 128 and row['reused'] == 0
        assert row['decode_ms'] > 0 and row['prompt_ms'] > 0 and 0 <= row['accepted'] <= row['offered']
    assert d['summary'] == summarize(d['requests'])
    assert len(d['gpu_stage_lines']) == 3
    assert all('sampled windows' in s for s in d['gpu_stage_lines'])
    if local:
        for name, digest in d['local_files_sha256'].items():
            assert sha(R / name) == digest, name
        folder = R / 'build-sycl-a770-decode2k-integrated-20261025'
        raw = json.loads((folder / 'results.json').read_text(encoding='utf-8'))
        assert d['requests'] == [request(v, run['label']) for run in raw['runs'][:3] for v in run['rows']]
        text = (folder / 'profile.stderr').read_text(encoding='utf-8')
        assert d['host_profile'] == host_profile(text)
        assert d['gpu_stage_lines'] == [s for s in text.splitlines() if s.startswith('strata decode GPU stages:')]
    print('PASS complete2K outputs, three-build screen, medians/ranges/drift, diagnostics and hashes')


if __name__ == '__main__':
    audit('--local' in sys.argv)
