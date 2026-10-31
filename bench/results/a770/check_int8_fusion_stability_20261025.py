"""CPU-only audit of fusion parity, independent benefit gates and scheduling diagnosis."""
from pathlib import Path
import hashlib
import importlib.util
import json
import math
import re
import sys

H=Path(__file__).resolve().parent;R=H.parents[2]
spec=importlib.util.spec_from_file_location('layout',H/'check_int8_layout_20261025.py')
f=importlib.util.module_from_spec(spec);spec.loader.exec_module(f)
LABELS={16:'IQ2_XXS',22:'IQ2_S',42:'Q2_0'}
TRIAL=R/'build-sycl-a770-int8-swiglu-fused-v2-20261025'
DIAG=R/'build-sycl-a770-int8-stability-20261025'


def cells(c):return {(r['gu'],g['T'],g['mode']):g for r in c['runs'] for g in r['groups']}


def table_rows(d):
    cohorts={c['phase']:cells(c) for c in d['cohorts']}
    def gain(g):return f"{g['median_pct']:+.3f}%" if g['median_pct'] is not None else 'NA'
    rows=[]
    for v in d['original_confirmed']:
        key=tuple(v);gt,t,mode=key
        a,b,c,z=[cohorts[k][key] for k in ('control1','candidate1','candidate2','control2')]
        rows.append(f"| {LABELS[gt]} | {t} | {'col' if mode==1 else 'VNNI'} | {gain(a)} / {gain(z)} | "
                    f"{gain(b)} | {gain(c)} | {b['stable_cycles']}/8, {c['stable_cycles']}/8 | "
                    +('PASS' if v in d['fused_confirmed'] else 'FAIL')+' |')
    return rows


def audit(local=False):
    d=json.loads((H/'int8-fusion-stability-2026-10-25.json').read_text(encoding='utf-8'))
    old=json.loads((H/'int8-layout-2026-10-25.json').read_text(encoding='utf-8'))
    assert d['complete'] and not d['adopted']
    assert d['original_confirmed']==old['production_queue']['confirmed'] and len(d['original_confirmed'])==21
    expected={(t,m) for t in (1,6,8,16,40,64,128,256) for m in (1,2)}
    cycles=comparisons=0
    prior=cells(old['production_queue']['cohorts'][-1])
    for (phase,variant,order),cohort in zip(d['orders'],d['cohorts']):
        assert cohort['phase']==phase and cohort['variant']==variant
        assert [r['gu'] for r in cohort['runs']]==order
        for run in cohort['runs']:
            assert run['default_profiling']==run['trial_profiling']==1
            assert len(run['groups'])==16 and {(g['T'],g['mode']) for g in run['groups']}==expected
            for g in run['groups']:
                assert [v['cycle'] for v in g['pairs']]==list(range(8))
                assert len(g['errors'])==2 and {v['profile'] for v in g['errors']}=={0,1}
                assert all((v['gu'],v['T'],v['mode'])==(run['gu'],g['T'],g['mode']) for v in g['pairs']+g['errors'])
                assert all(math.isfinite(v[k]) and v[k]>0 for v in g['pairs'] for k in ('a0_us','a1_us','b0_us','b1_us'))
                assert all(g[k]==v for k,v in f.compare(g['pairs']).items())
                quality=all(v['nrmse']<=(.1 if v['profile'] else .05) and v['cosine']>=(.98 if v['profile'] else .995) for v in g['errors'])
                assert g['quality_passed']==quality
                assert g['passed']==(quality and g['stable_cycles']>=6 and g['median_pct']>=3)
                assert g['errors']==prior[(run['gu'],g['T'],g['mode'])]['errors']
                cycles+=8;comparisons+=2
            if local:
                raw=(TRIAL/phase/run['case']).with_suffix('.stdout').read_text()
                assert f'VARIANT {variant}' in raw and 'QUEUE default_profiling=1 trial_profiling=1' in raw and 'A770' in raw
                pairs=[json.loads(v[5:]) for v in raw.splitlines() if v.startswith('PAIR ')]
                actual=[v for g in run['groups'] for v in g['pairs']]
                assert len(pairs)==128 and all(p=={k:v[k] for k in p} for p,v in zip(pairs,actual))
                errors=[json.loads(v[6:]) for v in raw.splitlines() if v.startswith('ERROR ')]
                assert errors==sorted([v for g in run['groups'] for v in g['errors']],key=lambda v:(v['profile'],v['T'],v['mode']))
                assert [json.loads(v[8:]) for v in raw.splitlines() if v.startswith('SUMMARY ')]==[run['summary']]
    assert len(d['cohorts'])==4 and [c['variant'] for c in d['cohorts']]==['split','fused','fused','split']
    for variant in ('split','fused'):
        passed=[{k for k,g in cells(c).items() if g['passed']} for c in d['cohorts'] if c['variant']==variant]
        assert d[variant+'_confirmed']==[v for v in d['original_confirmed'] if all(tuple(v) in s for s in passed)]
        groups=[g for c in d['cohorts'] if c['variant']==variant for r in c['runs'] for g in r['groups']]
        assert d['stability_counts'][variant]=={'stable':sum(g['stable_cycles'] for g in groups),'total':len(groups)*8}
    assert d['dropped']==[v for v in d['original_confirmed'] if v not in d['fused_confirmed']]
    diag=d['diagnosis'];assert diag['complete'] and [r['affinity'] for r in diag['runs']]==['free','pin','pin','free']
    assert d['affinity_stable_cycles']=={a:sum(g['stable_cycles'] for r in diag['runs'] if r['affinity']==a for g in r['groups']) for a in ('free','pin')}
    for run in diag['runs']:
        assert len(run['arms'])==192 and len(run['groups'])==6 and len(run['errors'])==12
        assert run['host']['priority']==32 and run['host']['trace']==1
        assert run['host']['pinned']==int(run['affinity']=='pin')
        for g in run['groups']:
            assert all(g[k]==v for k,v in f.compare(g['pairs']).items() if k!='all_median_pct')
            for v in g['pairs']:
                arms=sorted([a for a in run['arms'] if (a['T'],a['mode'],a['cycle'])==(g['T'],g['mode'],v['cycle'])],key=lambda a:a['arm'])
                assert [a['arm'] for a in arms]==[0,1,2,3]
                assert [a['wall_us'] for a in arms]==[v[k] for k in ('a0_us','b0_us','b1_us','a1_us')]
                assert all(0<a['device_span_us']<=a['wall_us'] and a['thread_cycles']>0 for a in arms)
                if run['affinity']=='pin':assert all(a['cpu0']==a['cpu1']==run['host']['cpu'] for a in arms)
    report=(H/'int8-fusion-stability-2026-10-25.md').read_text(encoding='utf-8')
    assert re.findall(r'^\| (?:IQ2_S|IQ2_XXS|Q2_0) .*$',report,re.M)==table_rows(d)
    if local:
        for evidence in (d,diag):
            for name,digest in evidence['files_sha256'].items():
                assert hashlib.sha256((R/name).read_bytes()).hexdigest()==digest,name
        protocol=json.loads((TRIAL/'protocol.json').read_text())
        assert all(protocol[k]==d[k] for k in ('rule_before_measurement','orders','original_confirmed','created_utc'))
    print(f"PASS fused {len(d['fused_confirmed'])}/21, split {len(d['split_confirmed'])}/21; {cycles} ABBA cycles, {comparisons} numerical records; diagnostic arms checked")
    print('DROPPED',d['dropped'])
    return d


if __name__=='__main__':audit('--local' in sys.argv)
