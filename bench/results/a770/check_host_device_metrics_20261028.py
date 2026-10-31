"""Check the public host/device hardware diagnostic without private files or a GPU."""
import json
import math
from pathlib import Path

d = json.loads(Path(__file__).with_name('host-device-metrics-2026-10-28.json').read_text(encoding='utf-8'))
assert d['schema'] == 1 and d['scope'] == 'operator-hardware-diagnostic'
assert not any(d[k] for k in ('production_changed', 'speed_acceptance', 'target_achieved'))
assert d['metric_catalog'] == dict(group_count=2128, metric_entries=46580, unique_names=5082, tlb_named_metric_found=False)
assert d['first_custom_collection_refused']
assert d['system_read_metric'] == 'SYSMEM_BYTE_READ'
assert d['host_gpu_read_metric'] == 'HOST_TO_GPUMEM_BYTE_READ'
assert [x['name'] for x in d['cohorts']] == ['default_global_memory', 'explicit_system_memory']
checks = 0
for cohort in d['cohorts']:
    assert cohort['complete'] and cohort['terminal_audit_passed']
    assert len(cohort['source_results_sha256']) == 64
    assert [r['placement'] for r in cohort['runs']] == ['host', 'device']
    for run in cohort['runs']:
        assert run['full_chain_checks'] == 72 and run['weight_bytes_per_expert'] == 1510400
        checks += run['full_chain_checks']
        assert len(run['rows']) == 11
        for stage in ('GU', 'Down', 'SwiGLU'):
            rows = [r for r in run['rows'] if r['stage'] == stage]
            assert len(rows) == (1 if stage == 'SwiGLU' else 5)
            assert sum(int(r['Computing Task:Instance Count']) for r in rows) == 2088
            assert sorted(int(r['Computing Task:Instance Count']) for r in rows) == ([2088] if stage == 'SwiGLU' else [24, 24, 680, 680, 680])
        for row in run['rows']:
            for key, value in row.items():
                if key not in ('stage', 'Work Size:Global', 'Work Size:Local'):
                    assert value and math.isfinite(float(value)) and float(value) >= 0, key
            assert float(row['Computing Task:Total Time']) > 0
            assert abs(sum(float(row['XVE Array:' + k + '(%)']) for k in ('Active', 'Stalled', 'Idle')) - 100) <= .2
            assert 0 <= float(row['XVE Threads Occupancy(%)']) <= 100
            assert row['Computing Task:Spill Memory Size'] == '0'
            if row['stage'] in ('GU', 'Down'):
                assert row['Computing Task:General Register File Size'] == '128'
                assert row['Computing Task:SIMD Width'] == '16'
            assert ('System Memory Bandwidth, GB/sec:Read' in row) == (cohort['name'] == 'explicit_system_memory')
assert checks == 288
latest = d['cohorts'][1]['runs']
for stage in ('GU', 'Down'):
    for groups in (10, 20, 30):
        rows = [next(r for r in run['rows'] if r['stage'] == stage and r['Work Size:Global'] == f'10240 x {groups}') for run in latest]
        host, device = rows
        assert host['Computing Task:Instance Count'] == device['Computing Task:Instance Count'] == '680'
        assert 8.69 <= float(host['System Memory Bandwidth, GB/sec:Read']) <= 9.38
        assert float(device['System Memory Bandwidth, GB/sec:Read']) < .01
        assert float(host['XVE Array:Stalled(%)']) > float(device['XVE Array:Stalled(%)'])
print('PASS 44 kernel groups,288 full-chain checks,exact stage coverage,metric directions and finite counters; no model gain claim')
