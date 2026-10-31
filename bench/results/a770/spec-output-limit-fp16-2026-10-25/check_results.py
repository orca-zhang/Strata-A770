"""Offline audit of the archived FP16 output-window experiment; no model or GPU."""
import hashlib
import json
import math
from pathlib import Path
import re

H = Path(__file__).resolve().parent


def read(path):
    return json.loads(path.read_text(encoding='utf-8'))


def sha(path):
    with path.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def audit_timing(folder, checks):
    measured = read(folder/'results.json')
    assert measured['complete']
    for name, expected in measured['raw_sha256'].items():
        assert sha(folder/name) == expected, name
    for phase in ('screen', 'confirm'):
        runs = measured[phase]
        if not runs:
            assert (phase=='screen' and measured.get('retry_reason')) or (phase=='confirm' and not measured['screen_passed'])
            continue
        assert [row['side'] for row in runs] == ['baseline', 'candidate', 'baseline', 'candidate', 'baseline']
        for run in runs:
            assert len(run['warmup']) == 2
            for cap, rows in run['samples'].items():
                assert len(rows) == 3
                assert all(row['tokens'] == checks['reference']['0.5-False'][:int(cap)] and row['reused'] == 0 for row in rows)
                for metric, median in run['medians'][cap].items():
                    assert median == sorted(row[metric] for row in rows)[1]
        for pair in measured[phase+'_summary']['comparisons']:
            i, cap, metric = pair['candidate_run'], str(pair['cap']), pair['metric']
            a, b, c = [runs[k]['medians'][cap][metric] for k in (i-1, i, i+1)]
            assert math.isclose(pair['throughput_change_pct'], 100*(math.sqrt(a*c)/b-1), abs_tol=1e-10)
            assert math.isclose(pair['baseline_drift_pct'], 100*(c/a-1), abs_tol=1e-10)
            assert pair['eligible'] == (abs(pair['baseline_drift_pct']) <= 3)
    assert measured['args'][measured['args'].index('--kv')+1] == 'fp16'
    assert measured['primary_cap'] == 6
    for key in ('STRATA_TRACE','STRATA_VERIFY_PROFILE','STRATA_DECODE_TIMING','STRATA_STATE_HASH','STRATA_SNAPSHOT_VERIFY'):
        assert key not in measured['environment']
    assert measured['environment']['STRATA_NO_BATCH_KV_STEP'] == '1'
    for key in ('STRATA_MTP_COST_WINDOW','STRATA_MTP_CHAIN_SUBMIT','STRATA_QSA_FUSED_KV_STORE'):
        assert measured['environment'].get(key,'0') == '0'
    for phase in ('screen','confirm'):
        if not measured[phase]:
            continue
        for run in measured[phase]:
            assert 'kv=fp16' in run['runtime_info']
            assert 'expert_slots=4191' in run['runtime_info'] and 'expert_cache_mib=5760' in run['runtime_info']
            for row in run['warmup']:
                assert row['tokens'] == checks['reference']['0.5-False'] and row['reused'] == 0
            for rows in run['samples'].values():
                for row in rows:
                    assert all(math.isfinite(row[k]) and row[k] > 0 for k in ('decode_ms','prompt_ms','ttft_ms'))
                    assert row['generated'] == len(row['tokens']) and row['finish'] == 'length'
        summary = measured[phase+'_summary']
        primary = [p for p in summary['comparisons'] if p['eligible'] and p['metric']=='decode_ms' and p['cap']==6]
        passed = bool(primary) and all(p['throughput_change_pct'] >= 1 for p in primary)
        regressions = [p for p in summary['comparisons'] if p['eligible'] and p['metric']=='decode_ms' and p['throughput_change_pct'] < -2]
        assert summary['primary_passed'] == passed
        assert summary['decode_regressions_over_2pct'] == regressions
        assert measured[phase+'_passed'] == (passed and not regressions)
    return measured


def main():
    for name, expected in read(H/'manifest.json').items():
        assert sha(H/name) == expected, name
    checks = read(H/'checks/results.json')
    assert checks['complete']
    for name, expected in checks['raw_sha256'].items():
        assert sha(H/'checks'/name) == expected, name
    assert [r['side'] for r in checks['runs']] == ['baseline','candidate']
    assert len(checks['cli']) == 6 and len(checks['eos']) == 2
    for run in checks['runs']:
        assert 'kv=fp16' in run['runtime_info']
        assert 'expert_slots=4191' in run['runtime_info'] and 'expert_cache_mib=5760' in run['runtime_info']
        assert len(run['rows']) == 36
        for row in run['rows']:
            assert row['generated'] == len(row['tokens']) <= row['limit']
            if run['side'] == 'candidate':
                for pos, size in row['windows']:
                    assert 1 <= size <= row['limit'] - (pos - len(row['prompt']) + 1)
            if row['name'].endswith('-reference'):
                assert row['tokens'] == checks['reference'][row['name'].removesuffix('-reference')]
            prefix = row['name'].split('-cap')[0]
            if '-cap' in row['name']:
                assert row['tokens'] == checks['reference'][prefix][:row['limit']]
            if row['name'].endswith(('-head', '-parked', '-live')):
                full = checks['reference'][row['name'].split('-')[0]+'-False']
                start = 5 if row['name'].endswith('-parked') else 11 if row['name'].endswith('-live') else 0
                assert row['tokens'] == full[start:start+row['limit']]
                if start:
                    assert row['reused'] > 0
            if row['name'].endswith('-cancel'):
                assert row['finish'] == 'cancel'
            if row['name'].endswith('-recovery'):
                assert row['tokens'] == checks['reference'][row['name'].removesuffix('-recovery')][:6]
    for row in checks['cli']:
        assert row['returncode'] == 0 and len(row['tokens']) == row['cap']
        assert row['tokens'] == next(other['tokens'] for other in checks['cli'] if other['cap'] == row['cap'])
    for row in checks['eos']:
        assert row['finish'] == 'stop' and row['tokens'] == checks['reference']['0.5-False'][:1]
    assert len(checks['context']) == 2
    for row in checks['context']:
        assert row['returncode'] == 0
        assert row['tokens'] == next(r['tokens'] for r in checks['cli'] if r['cap']==6)
    candidate = next(row for row in checks['context'] if row['side'] == 'candidate')
    assert candidate['returncode'] == 0
    assert candidate['tokens'] == next(row['tokens'] for row in checks['cli'] if row['cap'] == 6)
    frozen = read(H/'frozen.json')
    assert sha(H/'source/candidate.cpp') == frozen['files_sha256']['sycl\\src\\program\\generate.cpp']
    assert read(H/'delivery.json')['production_unchanged']
    assert read(H/'reused-build.json')['complete']
    assert checks['args'][checks['args'].index('--kv')+1] == 'fp16'
    assert checks['reference']['0-False'] == checks['reference']['0.5-False']
    assert checks['reference']['0-True'] == checks['reference']['0.5-True']
    original = audit_timing(H/'measure', checks)
    assert original['screen_passed'] and not original['confirm_passed']
    primary = [p for p in original['confirm_summary']['comparisons'] if p['eligible'] and p['metric']=='decode_ms' and p['cap']==6]
    assert not primary
    prereg = read(H/'retry-preregistered.json')
    assert sha(H/'measure/results.json') == prereg['prior_results_sha256']
    assert sha(H/'confirm_retry.py') == prereg['script_sha256']
    assert prereg['gain_threshold_pct'] == 1 and prereg['max_baseline_drift_pct'] == 3
    retry = audit_timing(H/'confirm-retry', checks)
    assert retry['retry_reason']
    assert retry['args'] == original['args'] and retry['environment'] == original['environment']
    assert retry['warmups_per_process'] == original['warmups_per_process'] == 2
    assert retry['retained_per_cap'] == original['retained_per_cap'] == 3
    assert read(H/'retry-delivery.json')['production_unchanged']
    summary = read(H/'summary.json')
    assert summary['functional_calls'] == 82
    assert summary['timing_processes'] == 15 and summary['warmups'] == 30 and summary['retained'] == 135
    stages = {'screen': (original, 'screen'), 'confirm': (original, 'confirm'), 'confirm_retry': (retry, 'confirm')}
    pairs = []
    for label, (data, phase) in stages.items():
        decode = [p for p in data[phase+'_summary']['comparisons'] if p['metric'] == 'decode_ms']
        pairs.extend(decode)
        for cap in (4, 6, 16):
            gains = [p['throughput_change_pct'] for p in decode if p['eligible'] and p['cap'] == cap]
            entry = summary['gains'][label][str(cap)]
            assert entry == {'eligible': len(gains), 'min_pct': min(gains, default=None), 'max_pct': max(gains, default=None)}
    assert summary['total_decode_pairs'] == len(pairs) == 18
    assert summary['eligible_decode_pairs'] == sum(p['eligible'] for p in pairs)
    assert summary['maximum_abs_decode_baseline_drift_pct'] == max(abs(p['baseline_drift_pct']) for p in pairs)
    assert summary['confirm_retry_passed'] == retry['confirm_passed']
    print('PASS source/log hashes, clipped outputs, restoration, sampling, CLI/EOS and all timing comparisons')


if __name__ == '__main__':
    main()
