"""Check the compact diagnostic; --local rebuilds it from the frozen raw evidence."""
import hashlib
import json
import math
from pathlib import Path
import re
import sys

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
OUTPUT = HERE / 'decode-gpu-profile-2026-10-28.json'
EAGER = ROOT / 'build-sycl-a770-decode-gpu-eager-profile-20261028'
GRAPH = ROOT / 'build-sycl-a770-decode-gpu-profile-20261028'
PROBE = ROOT / 'build-sycl-a770-graph-profiler-check-v2-20261028'
FAMILIES = {
    'IQ2_S GU': ('iq2s_gu_pair',),
    'Other GU': ('native_gu_port',),
    'Down': ('native_down_port',),
    'SwiGLU/Q8': ('swiglu_q8_1_entries',),
    'Native MMVQ wide': ('native_mmvq_wide',),
    'GR four kernels': ('gr_up_multi_kernel', 'gr_down_sliced_k', 'gr_down_reduce_k', 'gr_norm_split_k'),
    'GDN step/norm': ('gdn_step_norm_multi_kernel',),
    'Other kernels': (),
}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def build():
    audit = json.loads((EAGER / 'audit-results.json').read_text(encoding='utf8'))
    assert audit['complete'] and not audit['speed_acceptance']
    groups = {name: dict(seconds=0.0, calls=0) for name in FAMILIES}
    for row in audit['eager']['metrics']:
        if not row['kernel']: continue
        matched = [name for name, patterns in FAMILIES.items() if any(p in row['name'] for p in patterns)]
        assert len(matched) <= 1
        group = groups[matched[0] if matched else 'Other kernels']
        group['seconds'] += row['seconds']; group['calls'] += row['calls']
    total = sum(r['seconds'] for r in groups.values())
    for row in groups.values(): row['kernel_time_share_pct'] = 100 * row['seconds'] / total
    text = (EAGER / 'profile.stderr').read_text(encoding='utf8')
    windows = {}
    for request in (4, 5):
        block = text.split(f'DECODE_ITT_BEGIN request={request}\n')[1].split(f'DECODE_ITT_END request={request} ')[0]
        windows[str(request)] = len(re.findall(r'^strata device adapt: \d+ experts swapped$', block, re.M))
    files = [EAGER / 'audit-results.json', EAGER / 'audit.py', EAGER / 'report.csv', EAGER / 'profile.stderr',
             GRAPH / 'report.csv', PROBE / 'finish-results.json',
             ROOT / 'build-sycl-a770-vram-down-sg16/iq.obj',
             ROOT / 'build-sycl-a770-weight-cache-hint-20261025/baseline.obj']
    return dict(complete=True, speed_acceptance=False, production_changed=False,
        device='A770 16GB / PCIe3.0 / Windows / driver32.0.101.9034 / DPC++2025.3.2 / VTune2026.4',
        scope='Only warm Code/Prose2048/full128 decode; eager diagnostic changes scheduling and profiler overhead. Kernel-time sums are not graph or end-to-end time shares. Original graph replay kernel attribution failed coverage.',
        spec_min_p=0.5, production_spec_min_p=0.8, expert_slots=6297,
        probe_coverage=audit['probe_coverage'], full_outputs_checked=10, long_prefill_48layer_scans=8,
        graph_kernel_calls=sum(r['calls'] for r in audit['graph']['metrics'] if r['kernel']),
        eager_kernel_calls=sum(r['calls'] for r in audit['eager']['metrics'] if r['kernel']),
        eager_kernel_seconds=total, eager_decode_windows=windows, groups=groups,
        gu_local_size='256 x 1', baseline_iq_object_sha256=digest(files[-2]),
        hardware_operator_iq_object_sha256=digest(files[-1]),
        workers={mode: audit[mode]['worker'] for mode in ('graph', 'eager')},
        files_sha256={p.relative_to(ROOT).as_posix(): digest(p) for p in files})


def check(d):
    assert d['complete'] and not d['speed_acceptance'] and not d['production_changed']
    assert d['probe_coverage'] == {'report-eager-paused': 20, 'report-graph-paused': 1, 'report-graph-unpaused': 1}
    assert d['full_outputs_checked'] == 10 and d['long_prefill_48layer_scans'] == 8
    assert d['graph_kernel_calls'] == 0 and d['eager_kernel_calls'] == 260498
    assert d['eager_decode_windows'] == {'4': 55, '5': 57} and d['gu_local_size'] == '256 x 1'
    groups = d['groups']
    assert groups['IQ2_S GU']['calls'] + groups['Other GU']['calls'] == groups['Down']['calls'] == groups['SwiGLU/Q8']['calls'] == 48 * 112
    assert sum(r['calls'] for r in groups.values()) == d['eager_kernel_calls']
    assert math.isclose(sum(r['seconds'] for r in groups.values()), d['eager_kernel_seconds'], abs_tol=1e-9)
    assert math.isclose(sum(r['kernel_time_share_pct'] for r in groups.values()), 100, abs_tol=1e-9)
    for row in groups.values():
        assert row['seconds'] > 0 and row['calls'] > 0
        assert math.isclose(row['kernel_time_share_pct'], 100 * row['seconds'] / d['eager_kernel_seconds'])
    assert d['baseline_iq_object_sha256'] != d['hardware_operator_iq_object_sha256']
    assert d['spec_min_p'] == 0.5 and d['production_spec_min_p'] == 0.8
    for row in d['workers'].values():
        assert row['full_outputs'] == 5 and row['free_mib'] >= 1024 and row['long_prefill_scans'] == [[48, 0, 0]] * 4


if __name__ == '__main__':
    if '--write' in sys.argv:
        assert not OUTPUT.exists()
        d = build(); check(d)
        OUTPUT.write_text(json.dumps(d, indent=2), encoding='utf8')
    d = json.loads(OUTPUT.read_text(encoding='utf8')); check(d)
    if '--local' in sys.argv:
        assert d == build()
        for path, sha in d['files_sha256'].items(): assert digest(ROOT / path) == sha, path
    print('PASS exact output/scan coverage,112 windows,GU/Down counts,kernel groups and diagnostic-only scope')
