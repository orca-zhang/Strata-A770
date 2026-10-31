"""Recompute both formal cache cohorts; --local also verifies the frozen private records."""
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import re
import statistics
import sys

H = Path(__file__).resolve().parent
R = H.parents[2]


def sha(path):
    with path.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def compact(row):
    keys = ('case', 'warmup', 'generated', 'prompt_tokens', 'reused', 'finish',
            'prompt_ms', 'decode_ms', 'ttft_ms', 'accepted', 'offered')
    return {k: row[k] for k in keys} | {
        'output_sha256': hashlib.sha256(json.dumps(row['tokens'], separators=(',', ':')).encode()).hexdigest()}


def check_cohort(d, reference):
    assert d['complete'] and [r['label'] for r in d['runs']] == ['A1', 'B1', 'A2']
    cases = ('Code2K', 'Prose2K')
    for run in d['runs']:
        assert run['exit_code'] == 0 and len(run['rows']) == 8
        assert run['expert_slots'] == (6297 if run['label'] == 'B1' else 5771)
        assert run['startup_free_mib'] > 0
        assert [(r['case'], r['warmup']) for r in run['rows']] == (
            [(n, True) for n in cases] + [(n, False) for _ in range(3) for n in cases])
        for row in run['rows']:
            assert row['generated'] == 128 and row['prompt_tokens'] == 2048 and row['reused'] == 0
            assert row['finish'] == 'length' and math.isfinite(row['decode_ms']) and row['decode_ms'] > 0
            assert 0 <= row['accepted'] <= row['offered']
            assert row['output_sha256'] == reference[row['case']]
        for name in cases:
            rows = [r for r in run['rows'] if r['case'] == name and not r['warmup']]
            tps = [128000 / r['decode_ms'] for r in rows]
            summary = run['summary'][name]
            assert summary['decode_tps'] == statistics.median(tps)
            assert summary['decode_tps_range'] == [min(tps), max(tps)]
            assert summary['accepted'] == [r['accepted'] for r in rows]
            assert summary['offered'] == [r['offered'] for r in rows]
    for name in cases:
        a, x, z = [1000 / r['summary'][name]['decode_tps'] for r in d['runs']]
        baseline = math.sqrt(a * z)
        assert d['comparison'][name] == dict(baseline=baseline, candidate=x,
            throughput_change_pct=100 * (baseline / x - 1), baseline_drift_pct=100 * (z / a - 1))
    assert d['passes_screen'] == all(abs(p['baseline_drift_pct']) <= 3 and p['throughput_change_pct'] >= 3
                                     for p in d['comparison'].values())
    assert len(d['telemetry']) == 24 and all(t['samples'] >= 1 for t in d['telemetry'])
    assert [(t['label'], t['case'], t['warmup']) for t in d['telemetry']] == [
        (run['label'], row['case'], row['warmup']) for run in d['runs'] for row in run['rows']]


def main():
    d = json.loads((H / 'decode2k-cache-2026-10-25.json').read_text(encoding='utf-8'))
    assert d['complete'] and d['production_changed'] is False
    assert d['settings'] == dict(expert_count_A=5500, expert_count_B=6000, reserve_mib=1536, kv='fp16',
        context=8192, prefill=8192, cli_spec=4, spec_min_p=0.5, mtp_window=1024,
        vocabulary_rows=248320, prompt_cache=0, prompt_tokens=2048, output_tokens=128)
    assert sha(Path(__file__)) == d['checker_sha256']
    assert sha(H / 'decode2k-cache-2026-10-25.md') == d['report_sha256']
    assert len(d['cohorts']) == 2 and d['qualified_initial_screen']['counts_as_confirmation'] is False
    for cohort in d['cohorts']:
        check_cohort(cohort, d['reference_output_sha256'])
        if '--local' in sys.argv:
            folder = R / cohort['source_directory']
            assert sha(folder / 'results.json') == cohort['raw_results_sha256']
            raw = json.loads((folder / 'results.json').read_text(encoding='utf-8'))
            assert sha(Path(raw['exe'])) == d['executable_sha256']
            assert sha(R / 'build-sycl-a770-decode2k-graph-20261025/frozen-environment.json') == d['environment_sha256']
            spec = importlib.util.spec_from_file_location('cache_audit', folder / 'run.py')
            m = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(m)
            m.audit(raw)
            assert [r['rows'] for r in cohort['runs']] == [[compact(row) for row in run['rows']] for run in raw['runs']]
            assert cohort['comparison'] == raw['comparison'] and cohort['telemetry'] == raw['telemetry']
            for run in cohort['runs']:
                text = (folder / (run['label'] + '.stderr')).read_text(encoding='utf-8')
                free = re.findall(r'(\d+) MiB of VRAM free with everything loaded', text)
                assert len(free) == 1 and run['startup_free_mib'] == int(free[0])
    assert d['passes_confirmation'] == all(c['passes_screen'] for c in d['cohorts'])
    target = d['passes_confirmation'] and all(128000 / row['decode_ms'] >= 40
        for c in d['cohorts'] for run in c['runs'] if run['label'] == 'B1'
        for row in run['rows'] if not row['warmup'])
    assert d['target_achieved'] == target
    print('PASS two formal cohorts,48 complete2K/128 outputs, all samples, cache isolation, drift/gains and telemetry coverage')


if __name__ == '__main__':
    main()
