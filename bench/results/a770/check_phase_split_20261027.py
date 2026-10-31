"""Offline recomputation: python bench/results/a770/check_phase_split_20261027.py [--local]."""
import hashlib
import json
import math
from pathlib import Path
import statistics
import sys


def summarize(raw):
    result = []
    for t, variant in sorted({(x['T'], x['variant']) for x in raw['PAIR']}):
        cycles = [x for x in raw['PAIR'] if (x['T'], x['variant']) == (t, variant)]
        assert sorted(x['cycle'] for x in cycles) == list(range(8))
        stable, baseline, candidate = [], [], []
        for x in cycles:
            a, b = math.sqrt(x['a0_us']*x['a1_us']), math.sqrt(x['b0_us']*x['b1_us'])
            assert min(x[k] for k in ('a0_us', 'b0_us', 'b1_us', 'a1_us')) > 0
            baseline.append(a)
            candidate.append(b)
            if max(abs(x['a1_us']/x['a0_us']-1), abs(x['b1_us']/x['b0_us']-1)) <= .03:
                stable.append(100*(a/b-1))
        gain = statistics.median(stable) if stable else None
        result.append(dict(T=t, variant=variant, stable_cycles=len(stable), gain_pct=gain,
                           baseline_us=statistics.median(baseline), candidate_us=statistics.median(candidate),
                           passed=len(stable) >= 6 and gain >= 3))
    return result


def confirmed(runs):
    selected = []
    for run in runs:
        if run['repeat'] != 'confirm':
            continue
        screen = next(x for x in runs if x['repeat'] == 'screen' and (x['case'], x['phase']) == (run['case'], run['phase']))
        for row in run['summary']:
            previous = next(x for x in screen['summary'] if (x['T'], x['variant']) == (row['T'], row['variant']))
            if row['passed'] and previous['passed']:
                selected.append(dict(phase=run['phase'], case=run['case'], T=row['T'], variant=row['variant']))
    return selected


def main():
    root = Path(__file__).resolve().parents[3]
    data = json.loads(Path(__file__).with_name('phase-split-2026-10-27.json').read_text(encoding='utf8'))
    assert data['model_quality_tested'] is False and data['whole_model_speed_tested'] is False
    errors = pairs = 0
    for cohort in data['cohorts']:
        assert cohort['complete'] and len({r['label'] for r in cohort['runs']}) == len(cohort['runs'])
        for run in cohort['runs']:
            assert len(run['raw']['SUMMARY']) == 1 and run['raw']['SUMMARY'][0]['banks'] == 8
            for error in run['raw']['ERROR']:
                limit = 2e-6 if error['phase'] in ('decode', 'same-quantized') else (.10 if error['profile'] else .05)
                assert error['passed'] and 0 <= error['nrmse'] <= limit
            assert summarize(run['raw']) == run['summary']
            errors += len(run['raw']['ERROR'])
            pairs += len(run['raw']['PAIR'])
        assert confirmed(cohort['runs']) == cohort['confirmed']
        if '--local' in sys.argv:
            frozen = root / cohort['directory']
            assert hashlib.sha256((frozen/'results.json').read_bytes()).hexdigest() == cohort['raw_results_sha256']
            assert hashlib.sha256((frozen/'phase.exe').read_bytes()).hexdigest() == cohort['exe_sha256']
            for path, sha in cohort['source_sha256'].items():
                assert hashlib.sha256((root/path).read_bytes()).hexdigest() == sha, path
            for run in cohort['runs']:
                assert hashlib.sha256((frozen/(run['label']+'.stdout')).read_bytes()).hexdigest() == run['stdout_sha256']
    assert (errors, pairs) == (data['accuracy_rows'], data['paired_cycles'])
    print(f'PASS {len(data["cohorts"])} frozen cohorts, {errors} accuracy rows, {pairs} ABBA cycles; phase gates recomputed')


if __name__ == '__main__':
    main()
