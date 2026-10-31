"""CPU-only audit of direct FP16/BF16 -> U4S4 precision and all retained timing pairs."""
from pathlib import Path
import hashlib
import json
import math
import statistics
import sys

HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[2]
def precision(v):
    assert all(math.isfinite(v[k]) for k in ('nrmse','cosine','max_abs'))
    assert v['nrmse']>=0 and v['max_abs']>=0 and -1.00000001<=v['cosine']<=1.00000001
    passed=v['nrmse']<=(.10 if v['profile'] else .05) and v['cosine']>=(.98 if v['profile'] else .995)
    assert passed==v['passed'];return passed

def timing(cell):
    assert len(cell['errors'])==2 and {v['profile'] for v in cell['errors']}=={0,1}
    quality=all([precision(v) for v in cell['errors']]);assert len(cell['pairs'])==(8 if quality else 0)
    gains=[]
    for cycle,v in enumerate(cell['pairs']):
        assert v['cycle']==cycle and (v['T'],v['mode'],v['repack'])==(cell['T'],cell['mode'],cell['repack'])
        assert all(math.isfinite(v[k]) and v[k]>0 for k in ('a0_us','b0_us','b1_us','a1_us'))
        gain=100*(math.sqrt(v['a0_us']*v['a1_us'])/math.sqrt(v['b0_us']*v['b1_us'])-1)
        da=100*(v['a1_us']/v['a0_us']-1);db=100*(v['b1_us']/v['b0_us']-1)
        for k,expected in [('gain_pct',gain),('a_drift_pct',da),('b_drift_pct',db)]:
            assert math.isclose(v[k],expected,rel_tol=1e-10,abs_tol=1e-10)
        stable=max(abs(da),abs(db))<=3;assert v['stable']==stable
        if stable:gains.append(gain)
    median=statistics.median(gains) if gains else None
    assert cell['stable_cycles']==len(gains)
    assert (median is None and cell['median_pct'] is None) or math.isclose(median,cell['median_pct'],rel_tol=1e-10,abs_tol=1e-10)
    passed=quality and len(gains)>=6 and median>=1;assert cell['passed']==passed;return passed

def cohort(d):
    assert d['complete'] and d['selftest']['checks']>10000 and len(d['runs'])==6 and d['minimum_gain_pct']==1
    eligible=set();seen=set()
    for r in d['runs']:
        c=r['case'];label=c['label'];assert label not in seen;seen.add(label)
        assert c['type'] in ('bf16','fp16') and c['N']*c['K']==3276800
        assert c['fp16_from_bf16_relative_l2']<1.6e-8
        if c['type']=='bf16':assert c['sha256']==c['source_bf16_sha256']
        es=r['errors'];assert len(es)==48
        assert {(v['T'],v['mode'],v['profile']) for v in es}=={
            (t,m,p) for t in (1,6,8,16,40,64,128,256) for m in range(1,4) for p in range(2)}
        for v in es:precision(v);assert v['type']==c['type']
        assert len(r['weights'])==2 and [v['digits'] for v in r['weights']]==[1,2]
        assert 0<=r['weights'][1]['relative_l2']<=r['weights'][0]['relative_l2']
        for t in (1,8,64,256):
            for m in range(1,4):
                if all(v['passed'] for v in es if (v['T'],v['mode'])==(t,m)):
                    for repack in (False,True):eligible.add((label,t,m,repack))
        for v in r['raw_pairs']:
            assert (label,v['T'],v['mode'],v['repack']) in eligible
            cells=[cell for cell in d['screen'] if (cell['case'],cell['T'],cell['mode'],cell['repack'])==(label,v['T'],v['mode'],v['repack'])]
            assert len(cells)==1 and all(cells[0]['pairs'][v['cycle']][k]==value for k,value in v.items())
        assert len(r['raw_pairs'])==8*sum(key[0]==label for key in eligible)
    key=lambda v:(v['case'],v['T'],v['mode'],v['repack'])
    assert {key(v) for v in d['screen']}==eligible and len(d['screen'])==len(eligible)
    selected={key(v) for v in d['screen'] if timing(v)}
    assert {key(v) for v in d['confirm']}==selected and len(d['confirm'])==len(selected)
    confirmed={key(v) for v in d['confirm'] if timing(v)}
    assert {tuple(v) for v in d['confirmed']}==confirmed
    return confirmed

def main():
    assert sys.argv[1:] in ([],['--local'])
    d=json.loads((HERE/'u4s4-projection-2026-10-26.json').read_text(encoding='utf-8'))
    confirmed=cohort(d);previous=cohort(d['previous_cohort'])
    assert {tuple(v) for v in d['confirmed_in_both_versions']}==confirmed & previous
    validation=d['validation']
    assert validation['cmake_ctest_passed'] and validation['selftest_checks']==131929
    assert validation['tiny_bf16_finite'] and validation['invalid_cli_cases']==9
    assert all(v['zero_projection_cells']==48 and v['nonfinite_weight_rejected'] for v in validation['zero_checks'])
    if sys.argv[1:]:
        for name,digest in d['evidence_sha256'].items()|d['source_sha256'].items():
            assert hashlib.sha256((ROOT/name).read_bytes()).hexdigest()==digest,name
        old=d['previous_cohort']['source_sha256']
        assert hashlib.sha256((ROOT/'build-sycl-a770-u4s4-projection-v2-20261026/u4s4_projection_check.cpp').read_bytes()).hexdigest()==old['sycl/src/prefill/u4s4_projection_check.cpp']
    print(f"PASS 131929 U4S4 self-checks,576 precision cells,96 screens,28 confirms,CMake/CTest/zero/tiny/invalid checks;both_versions={sorted(confirmed & previous)}")
if __name__=='__main__':main()
