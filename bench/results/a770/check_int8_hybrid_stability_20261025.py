"""Check the hybrid experiment and the preceding full-fusion diagnosis."""
from pathlib import Path
import importlib.util
import json
import hashlib
import math
import re
import sys

H=Path(__file__).resolve().parent;R=H.parents[2]
spec=importlib.util.spec_from_file_location('full_fusion',H/'check_int8_fusion_stability_20261025.py')
f=importlib.util.module_from_spec(spec);spec.loader.exec_module(f)
TRIAL=R/'build-sycl-a770-int8-hybrid-fused-20261025'


def audit(local=False):
    previous=f.audit(local)
    d=json.loads((H/'int8-hybrid-stability-2026-10-25.json').read_text(encoding='utf-8'))
    assert d['complete'] and not d['adopted'] and d['orders']==previous['orders']
    assert d['original_confirmed']==previous['original_confirmed'] and len(d['cohorts'])==4
    reference=f.cells(previous['cohorts'][0])
    for (phase,variant,order),cohort in zip(d['orders'],d['cohorts']):
        assert (cohort['phase'],cohort['variant'],[r['gu'] for r in cohort['runs']])==(phase,variant,order)
        for run in cohort['runs']:
            assert run['default_profiling']==run['trial_profiling']==1
            assert len(run['groups'])==16 and {(g['T'],g['mode']) for g in run['groups']}=={(t,m) for t in (1,6,8,16,40,64,128,256) for m in (1,2)}
            for g in run['groups']:
                assert [v['cycle'] for v in g['pairs']]==list(range(8))
                assert all(g[k]==v for k,v in f.f.compare(g['pairs']).items())
                assert g['errors']==reference[(run['gu'],g['T'],g['mode'])]['errors']
                assert g['quality_passed'] and g['passed']==(g['stable_cycles']>=6 and g['median_pct']>=3)
                assert all((v['gu'],v['T'],v['mode'])==(run['gu'],g['T'],g['mode']) for v in g['pairs']+g['errors'])
                assert all(math.isfinite(v[k]) and v[k]>0 for v in g['pairs'] for k in ('a0_us','a1_us','b0_us','b1_us'))
            if local:
                raw=(TRIAL/phase/run['case']).with_suffix('.stdout').read_text()
                assert 'QUEUE default_profiling=1 trial_profiling=1' in raw and f'VARIANT {variant}' in raw
                pairs=[json.loads(v[5:]) for v in raw.splitlines() if v.startswith('PAIR ')]
                actual=[v for g in run['groups'] for v in g['pairs']]
                assert len(pairs)==128 and all(p=={k:v[k] for k in p} for p,v in zip(pairs,actual))
                assert [json.loads(v[8:]) for v in raw.splitlines() if v.startswith('SUMMARY ')]==[run['summary']]
    for variant in ('split','fused'):
        passed=[{k for k,g in f.cells(c).items() if g['passed']} for c in d['cohorts'] if c['variant']==variant]
        assert d[variant+'_confirmed']==[v for v in d['original_confirmed'] if all(tuple(v) in s for s in passed)]
    assert d['dropped']==[v for v in d['original_confirmed'] if v not in d['fused_confirmed']]
    assert [(v['mode'],v['cycle']) for v in d['weight_profile']]==[(m,c) for m in (1,2) for c in range(8)]
    assert all(math.isfinite(v[k]) and v[k]>0 for v in d['weight_profile'] for k in ('a0_us','a1_us','b0_us','b1_us'))
    report=(H/'int8-hybrid-stability-2026-10-25.md').read_text(encoding='utf-8')
    assert re.findall(r'^\| (?:IQ2_S|IQ2_XXS|Q2_0) .*$',report,re.M)==f.table_rows(d)
    if local:
        assert (TRIAL/'layout.inc').read_bytes()==(R/'build-sycl-a770-int8-vector-production-20261025/layout.inc').read_bytes()
        for name,digest in d['files_sha256'].items():assert hashlib.sha256((R/name).read_bytes()).hexdigest()==digest,name
        raw=(TRIAL/'profile.stdout').read_text()
        assert [json.loads(v[12:]) for v in raw.splitlines() if v.startswith('WEIGHT_PAIR ')]==d['weight_profile']
        protocol=json.loads((TRIAL/'protocol.json').read_text())
        assert all(protocol[k]==d[k] for k in ('orders','rule_before_measurement','original_confirmed','created_utc'))
    print(f"PASS hybrid {len(d['fused_confirmed'])}/21, control {len(d['split_confirmed'])}/21; 1536 ABBA cycles, 384 numerical records, equal-work weight profile")
    print('DROPPED',d['dropped'])
    return d


if __name__=='__main__':audit('--local' in sys.argv)
