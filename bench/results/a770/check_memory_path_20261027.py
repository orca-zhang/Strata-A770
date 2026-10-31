"""Audit the public A770 A/B/C numeric record without a GPU or private files."""
import json
import math
from pathlib import Path
import statistics

d = json.loads(Path(__file__).with_name('memory-path-2026-10-27.json').read_text(encoding='utf-8'))
assert d['schema'] == 1 and not d['production_changed'] and not d['target_achieved']
assert [c['mode'] for c in d['cohorts']] == ['eager', 'graph']
count = 0
for cohort in d['cohorts']:
    assert cohort['timed_outputs_checked_before_diagnostics'] == (cohort['mode'] == 'graph')
    assert [(r['type'], r['order']) for r in cohort['runs']] == [(t, order) for t in cohort['types'] for order in ('forward', 'reverse')]
    for run in cohort['runs']:
        candidates = ['B', 'C1', 'C5', 'C10', 'C30']
        if run['order'] == 'reverse': candidates.reverse()
        labels = ['A1', *[x for i, label in enumerate(candidates, 2) for x in (label, 'A'+str(i))]]
        expected = [(t, label) for t in (1, 2, 4, 6) for label in labels]
        assert run['exit_code'] == 0
        assert [(r['T'], r['label']) for r in run['timings']] == expected
        assert [(r['T'], r['label']) for r in run['metrics']] == expected
        assert [(r['T'], r['label']) for r in run['checks']] == expected
        times = {}
        for row, check in zip(run['timings'], run['checks']):
            assert len(row['total_us']) == 11 and all(math.isfinite(x) and x > 0 for x in row['total_us'])
            count += 11
            times[row['T'], row['label']] = statistics.median(row['total_us'])
            groups = 10 if row['T'] == 1 else row['T']*5
            chunks = math.ceil(groups/int(row['label'][1:])) if row['label'].startswith('C') else 1
            assert row['compute_chains'] == 8*chunks
            assert row['copy_commands'] == (8*chunks if row['label'].startswith('C') else 0)
            assert row['weight_bytes'] == groups*run['path']['weight_bytes_per_expert']
            assert check['eager_checks'] == check['pipeline_output_sets'] == 8 and check['guards'] and check['pointer_tables']
            if cohort['mode'] == 'graph':
                assert row['transfer_us'] is None and check['timed_graph_batches'] == 11 and check['timed_graph_output_sets'] == 88
            else:
                assert len(row['transfer_us']) == 11
                assert all(0 <= c <= t for c, t in zip(row['transfer_us'], row['total_us']))
        assert len(run['summary']) == 20
        for row in run['summary']:
            i = labels.index(row['label']); t = row['T']
            a, candidate, z = [times[t, label] for label in labels[i-1:i+2]]
            base = math.sqrt(a*z)
            gain, drift = 100*(base/candidate-1), 100*(z/a-1)
            for key, value in [('baseline', base), ('candidate', candidate), ('throughput_change_pct', gain), ('baseline_drift_pct', drift)]:
                assert math.isclose(row[key], value, rel_tol=0, abs_tol=1e-9)
            assert row['passes_screen'] == (gain >= 1 and abs(drift) <= 3)
            assert row['classification'] == ('noise' if abs(gain) < .3 else 'gain' if gain > 0 else 'regression')
            assert row['kernel_us'] is row['host_read_gbps'] is row['tokens_per_sec'] is None
        for row in run['metrics']:
            total, transfer, span, overlap = [row[k] for k in ('diagnostic_total_us', 'transfer_us', 'compute_queue_span_us', 'copy_compute_span_overlap_us')]
            assert all(math.isfinite(x) and x >= 0 for x in (total, transfer, span, overlap))
            assert 0 < span <= total and transfer <= total and overlap <= min(transfer, span)+1e-5
            assert row['kernel_us'] is row['down_kernel_us'] is None
    for ty, methods in cohort['operator_confirmed'].items():
        for label, shapes in methods.items():
            for t, passed in shapes.items():
                rows = [s for r in cohort['runs'] if r['type'] == int(ty) for s in r['summary'] if s['label'] == label and s['T'] == int(t)]
                assert len(rows) == 2 and passed == all(r['passes_screen'] for r in rows)
assert count == 5808
print('PASS 5808 retained batches, numerical-check coverage, paired medians, noise/drift gates and independent repeats; no model acceptance')
