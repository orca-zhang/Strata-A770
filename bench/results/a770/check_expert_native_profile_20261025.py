"""Check the mapped expert-chain diagnostic; --local also verifies the frozen trace evidence."""
import argparse
import csv
import hashlib
import importlib.util
import json
from pathlib import Path

H = Path(__file__).resolve().parent
R = H.parents[2]
FIELDS = {
    'total_time_s': 'Computing Task:Total Time',
    'instances': 'Computing Task:Instance Count',
    'simd_width': 'Computing Task:SIMD Width',
    'spill_bytes': 'Computing Task:Spill Memory Size',
    'grf_size_reported': 'Computing Task:General Register File Size',
    'active_pct': 'XVE Array:Active(%)',
    'stalled_pct': 'XVE Array:Stalled(%)',
    'idle_pct': 'XVE Array:Idle(%)',
    'occupancy_pct': 'XVE Threads Occupancy(%)',
    'peak_occupancy_pct': 'Peak XVE Threads Occupancy(%)',
    'l3_read_gbps': 'GPU L3:Average Bandwidth, GB/s:Read',
    'gpu_memory_read_gbps': 'GPU Memory Bandwidth, GB/sec:Read',
}
ROLES = {'GU': 'gu_pair_wg128', 'down': 'native_down_port', 'SwiGLU_Q8': 'swiglu_q8_1_entries'}


def extract(path):
    with path.open(encoding='utf-8-sig', newline='') as f:
        rows = list(csv.DictReader(f))
    result = []
    for row in rows:
        for role, name in ROLES.items():
            if name in row['Computing Task']:
                result.append(dict(role=role, global_work_size=row['Work Size:Global'],
                    local_work_size=row['Work Size:Local'], **{key: float(row[col]) for key, col in FIELDS.items()}))
    return result


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--local', action='store_true')
    args = ap.parse_args()
    d = json.loads((H / 'expert-native-profile-2026-10-25.json').read_text(encoding='utf-8'))
    assert d['complete'] and d['scope'] == 'operator-hardware-diagnostic'
    assert d['whole_model_gain_claimed'] is False and d['full_chain_output_guard_checks'] == 72
    assert len(d['rows']) == 11
    for role in ROLES:
        selected = [r for r in d['rows'] if r['role'] == role]
        assert sum(r['instances'] for r in selected) == 2088
        if role != 'SwiGLU_Q8':
            counts = {r['global_work_size']: r['instances'] for r in selected}
            assert counts == {'10240': 24, '10240 x 3': 24, '10240 x 10': 680, '10240 x 20': 680, '10240 x 30': 680}
        for row in selected:
            assert row['total_time_s'] > 0 and row['instances'] == int(row['instances'])
            assert row['simd_width'] == (32 if role == 'SwiGLU_Q8' else 16) and row['spill_bytes'] == 0
            assert abs(sum(row[k] for k in ('active_pct', 'stalled_pct', 'idle_pct')) - 100) <= 0.2
            assert all(0 <= row[k] <= 100 for k in FIELDS if k.endswith('_pct'))
    if args.local:
        raw = R / 'build-sycl-a770-expert-native-profile-recover-20261025'
        source = R / 'build-sycl-a770-expert-native-profile-20261025'
        for path, key in ((raw / 'results.json', 'raw_results_sha256'), (source / 'results.json', 'source_results_sha256')):
            assert hashlib.sha256(path.read_bytes()).hexdigest() == d[key]
        spec = importlib.util.spec_from_file_location('native_audit', raw / 'run.py')
        m = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(m)
        m.audit(json.loads((raw / 'results.json').read_text(encoding='utf-8')))
        assert d['rows'] == extract(raw / 'metrics.csv')
    print('PASS mapped expert diagnostic: exact2088 GU/down/SwiGLU calls,72 output/guard checks, work sizes and metrics')


if __name__ == '__main__':
    main()
