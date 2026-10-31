"""Portable phase-accounting audit; profiling is excluded from performance acceptance."""
from pathlib import Path
import hashlib
import json
import math
import re
import sys

R = Path(__file__).resolve().parents[3]
H = R / 'build-sycl-a770-u4s4-hc-host-profile-20261027'
REPORT = Path(__file__).with_name('hc-u4-attribution-2026-10-27.json')


def sha(path):
    with path.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def totals(d):
    rows = []
    for i, name in enumerate(('Code2K', 'Prose2K')):
        a, c, z = [r['rows'][i] for r in d['runs']]
        baseline = math.sqrt(a['prompt_ms'] * z['prompt_ms'])
        combined = math.sqrt((a['prompt_ms'] + a['decode_ms']) * (z['prompt_ms'] + z['decode_ms']))
        rows.append(dict(case=name, baseline_prefill_ms=baseline, candidate_prefill_ms=c['prompt_ms'],
                         prefill_throughput_change_pct=100 * (baseline / c['prompt_ms'] - 1),
                         baseline_prefill_decode_ms=combined, candidate_prefill_decode_ms=c['prompt_ms'] + c['decode_ms'],
                         combined_throughput_change_pct=100 * (combined / (c['prompt_ms'] + c['decode_ms']) - 1)))
    return rows


def audit(d):
    model = d['host_model']
    assert model['complete'] and not model['speed_acceptance'] and not model['production_changed'] and not model['adopted']
    assert [r['label'] for r in model['runs']] == ['A', 'B']
    for run in model['runs']:
        assert run['exit_code'] == 0 and run['expert_slots'] == 5771 and run['dispatched'] == bool(run['active'])
        assert run['initializer']['tokens'] == model['reference']['Short']
        assert len(run['rows']) == len(run['timing']) == 2
        for row, t in zip(run['rows'], run['timing']):
            assert row['generated'] == len(row['tokens']) == 128 and row['prompt_tokens'] == 2048 and row['reused'] == 0 and any(row['tokens'])
            if not run['active']:
                assert row['tokens'] == model['reference'][row['case']]
            assert t['tokens'] == 2047 and 0 < t['timeline_ms'] <= t['wall_ms'] + 2
            raw = t['raw']
            assert 'synchronized host timeline' in raw
            parsed = {n: dict(ms=int(ms), pct=float(pct)) for n, ms, pct in re.findall(r' ([a-z][a-z +/]*?) (\d+) \(([\d.]+)%\)', raw)}
            assert parsed == t['phases'] and 'hc up' in parsed
            assert abs(sum(v['ms'] for v in parsed.values()) - t['timeline_ms']) <= len(parsed)
            assert abs(sum(v['pct'] for v in parsed.values()) - 100) < 2
    event = d['rejected_native_event_model']
    bad = [(r['label'], i) for r in event['runs'] for i, t in enumerate(r['timing']) if not 0 < t['timeline_ms'] <= t['wall_ms'] * 1.1]
    assert bad and [list(p) for p in bad] == model['rejected_event_intervals']
    assert d['unprofiled_summary'] == totals(d['unprofiled_model'])
    assert not d['unprofiled_model']['speed_acceptance']


def main():
    if '--freeze' in sys.argv:
        assert not REPORT.exists()
        read = lambda path: json.loads(path.read_text(encoding='utf8'))
        host = read(H / 'model/results.json')
        event_path = R / 'build-sycl-a770-u4s4-hc-profile-20261027/model/results.json'
        original_path = R / 'build-sycl-a770-u4s4-hc-prefill-20261027/model-v2/results.json'
        original = read(original_path)
        files = [Path(__file__), H / 'model/results.json', event_path, original_path]
        d = dict(scope='Synchronized host phase attribution only; per-mark waits change overlap. Native-event timestamps rejected. Unprofiled timings retained as single-request diagnostics, not accepted speed or model quality.',
                 host_model=host, rejected_native_event_model=read(event_path), unprofiled_model=original,
                 unprofiled_summary=totals(original),
                 files_sha256={**host['files_sha256'], **{p.relative_to(R).as_posix(): sha(p) for p in files}})
        audit(d)
        REPORT.write_text(json.dumps(d, ensure_ascii=False, indent=2) + '\n', encoding='utf8')
    d = json.loads(REPORT.read_text(encoding='utf8'))
    audit(d)
    if '--local' in sys.argv:
        for relative, digest in d['files_sha256'].items():
            assert sha(R / relative) == digest, relative
    print('PASS four full2K host profiles, phase conservation, invalid-event exclusion and unprofiled totals; no speed acceptance')


if __name__ == '__main__':
    main()
