"""Recompute the Windows NEO/IGC hotspot shortlist from frozen decode CSV; no GPU."""
from collections import defaultdict
import csv
import hashlib
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
SUMMARY = ROOT/'bench/results/a770/decode-gpu-profile-2026-10-28.json'
CSV = ROOT/'build-sycl-a770-decode-gpu-eager-profile-20261028/report.csv'


def family(name):
    if any(s in name for s in ('iq2s_gu_pair', 'native_gu_port', 'native_down_port', 'swiglu_q8_1_entries')):
        return 'Expert chain'
    if 'mmvq' in name: return 'Quantized MMVQ, all variants'
    if 'fused_gr_read_multi' in name: return 'GR'
    if 'gdn_' in name: return 'GDN, all variants'
    if 'bf16_gemv' in name: return 'BF16 GEMV'
    if 'qsa_block_topk' in name: return 'QSA block top-k'
    if 'router' in name: return 'Router'
    return 'Remaining kernels'


def build():
    prior = json.loads(SUMMARY.read_text(encoding='utf8'))
    assert prior['complete'] and not prior['speed_acceptance']
    assert prior['graph_kernel_calls'] == 0
    assert hashlib.sha256(CSV.read_bytes()).hexdigest() == prior['files_sha256'][CSV.relative_to(ROOT).as_posix()]
    with CSV.open(encoding='utf-8-sig', newline='') as f:
        rows = [r for r in csv.DictReader(f) if not r['Computing Task'].startswith(('ze', '['))]
    groups = defaultdict(lambda: dict(seconds=0., calls=0, reported_spill_bytes=set(),
        time_in_kernels_reporting_spill_s=0.))
    spilled = defaultdict(lambda: dict(seconds=0., calls=0, reported_spill_bytes=set()))
    for r in rows:
        name = r['Computing Task']; seconds = float(r['Computing Task:Total Time'])
        calls = int(r['Computing Task:Instance Count']); spill = int(r['Computing Task:Spill Memory Size'])
        assert math.isfinite(seconds) and seconds >= 0 and calls > 0 and spill >= 0
        g = groups[family(name)]; g['seconds'] += seconds; g['calls'] += calls
        g['reported_spill_bytes'].add(spill)
        if spill:
            g['time_in_kernels_reporting_spill_s'] += seconds
            s = spilled[name]; s['seconds'] += seconds; s['calls'] += calls
            s['reported_spill_bytes'].add(spill)
        if 'iq2s_gu_pair' in name or 'native_down_port' in name:
            assert spill == 0 and int(r['Computing Task:General Register File Size']) == 128
    total = sum(g['seconds'] for g in groups.values())
    assert math.isclose(total, prior['eager_kernel_seconds'], abs_tol=1e-9)
    assert sum(g['calls'] for g in groups.values()) == prior['eager_kernel_calls'] == 260498
    for values in (groups, spilled):
        for g in values.values():
            g['reported_spill_bytes'] = sorted(g['reported_spill_bytes'])
            g['kernel_time_share_pct'] = 100*g['seconds']/total
    assert groups['Expert chain']['calls'] == 3*48*112
    assert math.isclose(groups['Expert chain']['seconds'],
        sum(prior['groups'][s]['seconds'] for s in ('IQ2_S GU','Other GU','Down','SwiGLU/Q8')), abs_tol=1e-9)
    assert family('native_mmvq_wide') == 'Quantized MMVQ, all variants'
    assert family('gdn_step_norm_multi_kernel') == 'GDN, all variants'
    assert family('unclassified_kernel') == 'Remaining kernels'
    return dict(scope='Eager diagnostic cumulative kernel time only; graph/critical-path shares unavailable',
        speed_acceptance=False, seconds=total, calls=260498,
        groups=dict(sorted(groups.items(), key=lambda kv: -kv[1]['seconds'])),
        top_spilling_kernels=[dict(name=k, **v) for k,v in sorted(spilled.items(),key=lambda kv:-kv[1]['seconds'])[:9]],
        time_in_kernels_reporting_spill_pct=100*sum(g['time_in_kernels_reporting_spill_s'] for g in groups.values())/total,
        spill_interpretation='Kernel metadata reports spill allocation; entire kernel duration is not spill overhead.')


if __name__ == '__main__':
    print(json.dumps(build(), indent=2))
