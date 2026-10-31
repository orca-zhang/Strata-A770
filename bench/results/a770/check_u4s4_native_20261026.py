"""Recompute native U4S4 precision/timing gates; --local also verifies preserved source/input hashes."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import statistics


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--local',action='store_true');args=p.parse_args()
    here=Path(__file__).resolve().parent;root=here.parents[2]
    d=json.loads((here/'u4s4-native-2026-10-26.json').read_text(encoding='utf-8'))
    assert d['complete'] and d['selftest']['checks']>=135000
    assert len(d['cases'])==6 and len(d['quality'])==6
    for q in d['quality']:
        assert q['native']==[dict(digits=2,bytes=3686432,exact_gpu_match=True)]
        assert len(q['errors'])==48
        assert {(v['T'],v['mode'],v['profile']) for v in q['errors']}=={(t,m,profile) for t in (1,6,8,16,40,64,128,256) for m in (1,2,3) for profile in (0,1)}
        for v in q['errors']:
            passed=v['nrmse']<=(.1 if v['profile'] else .05) and v['cosine']>=(.98 if v['profile'] else .995)
            assert passed==v['passed']
            if v['mode']==3:assert passed
    assert len(d['screen'])==52
    keys=set()
    for cohort in ('screen','confirm'):
        for v in d[cohort]:
            key=(v['case'],v['T'],v['pipeline'])
            if cohort=='screen':assert key not in keys;keys.add(key)
            assert len(v['pairs'])==8 and len(v['errors'])==2
            stable=[]
            for pair in v['pairs']:
                assert pair['mode']==3 and pair['T']==v['T'] and pair['repack'] is False
                assert pair['fused']==(v['pipeline']=='fused-vs-cached')
                assert pair['baseline_cached']==(v['pipeline']=='fused-vs-cached')
                assert pair['baseline_repack']==(v['pipeline']=='cached-vs-repack')
                a=math.sqrt(pair['a0_us']*pair['a1_us']);b=math.sqrt(pair['b0_us']*pair['b1_us'])
                gain=100*(a/b-1);da=100*(pair['a1_us']/pair['a0_us']-1);db=100*(pair['b1_us']/pair['b0_us']-1)
                assert math.isclose(gain,pair['gain_pct'],abs_tol=1e-10)
                assert math.isclose(da,pair['a_drift_pct'],abs_tol=1e-10) and math.isclose(db,pair['b_drift_pct'],abs_tol=1e-10)
                ok=max(abs(da),abs(db))<=3;assert ok==pair['stable']
                if ok:stable.append(gain)
            median=statistics.median(stable) if stable else None
            assert len(stable)==v['stable_cycles'] and median==v['median_pct']
            assert v['passed']==(all(e['passed'] for e in v['errors']) and len(stable)>=6 and median>=1)
            assert math.isclose(v['a_us'],statistics.median(math.sqrt(x['a0_us']*x['a1_us']) for x in v['pairs']),abs_tol=1e-10)
            assert math.isclose(v['b_us'],statistics.median(math.sqrt(x['b0_us']*x['b1_us']) for x in v['pairs']),abs_tol=1e-10)
    key=lambda v:(v['case'],v['T'],v['pipeline'])
    selected={key(v) for v in d['screen'] if v['passed']}
    assert selected=={key(v) for v in d['confirm']}
    confirmed={key(v) for v in d['confirm'] if v['passed']}
    assert confirmed=={key(v) for v in d['confirmed']}
    assert len(d['validation']['native_cases'])==14
    for v in d['validation']['native_cases']:assert (v['returncode']==0)==v['expected_success']
    assert d['cmake_ctest']['build_returncode']==d['cmake_ctest']['ctest_returncode']==0
    if args.local:
        for path,sha in d['source_sha256'].items():
            assert hashlib.sha256((root/path).read_bytes()).hexdigest()==sha,path
    print(f"PASS {d['selftest']['checks']} assertions, 288 precision cells, 52 timing cells, {len(d['confirm'])} fresh confirmations, {len(confirmed)} confirmed gains; native parser and CMake/CTest checks")


if __name__=='__main__':main()
