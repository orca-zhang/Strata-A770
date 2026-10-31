"""Recompute every retained XMX/HC pair; --local also verifies frozen evidence hashes."""
from pathlib import Path
import hashlib
import json
import math
import statistics
import sys

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[2]

def pairs(runs,stored,keys):
    shapes={tuple(r[k] for k in keys) for r in runs}
    passed={shape:True for shape in shapes}
    assert len(stored)==len(shapes)*4
    for cohort in [1,2]:
        for shape in shapes:
            rows=[r for r in runs if r['cohort']==cohort and tuple(r[k] for k in keys)==shape]
            assert [r['arm'] for r in rows]==['A1','B1','A2','B2','A3']
            event='median_ms' in rows[0]['row']
            if event:
                assert len({r['row']['operations'] for r in rows})==1
                for r in rows:
                    v=r['row'];samples=v['samples_ms']
                    assert len(samples)==9 and all(math.isfinite(x) and x>0 for x in samples)
                    assert math.isclose(v['median_ms'],statistics.median(samples),rel_tol=1e-8)
                    assert math.isclose(v['tops'],v['operations']/v['median_ms']/1e9,rel_tol=1e-8)
                    assert v['checked_outputs']==2*v['items']*8*64
            times=[r['row']['median_ms' if event else 'us'] for r in rows]
            assert all(math.isfinite(t) and t>0 for t in times)
            for i in [1,3]:
                actual=[v for v in stored if v['cohort']==cohort and v['arm']==rows[i]['arm'] and tuple(v[k] for k in keys)==shape]
                assert len(actual)==1
                base=math.sqrt(times[i-1]*times[i+1]);gain=100*(base/times[i]-1);drift=100*(times[i+1]/times[i-1]-1)
                for name,expected in [('baseline',base),('candidate',times[i]),('throughput_change_pct',gain),('baseline_drift_pct',drift)]:
                    assert math.isclose(actual[0][name],expected,rel_tol=1e-10,abs_tol=1e-10)
                ok=gain>=1 and abs(drift)<=3
                assert actual[0]['passed']==ok
                passed[shape]&=ok
    return {shape for shape,ok in passed.items() if ok}

def main():
    assert sys.argv[1:] in [[],['--local']]
    d=json.loads((HERE/'xmx-issue-2026-10-26.json').read_text(encoding='utf-8'))
    assert d['minimum_gain_pct']==1 and d['maximum_baseline_drift_pct']==3
    issue,low,hc,final=(d[k] for k in ['issue','lowbit','hc','final'])
    assert all(v['complete'] for v in [issue,low,hc,final])
    assert len(issue['runs'])==60 and len(low['runs'])==40 and len(hc['runs'])==10
    assert pairs(issue['runs'],issue['pairs'],['type','local'])==set()==set(tuple(v[k] for k in ['type','local']) for v in issue['qualified'])
    assert pairs(low['runs'],low['pairs'],['precision'])=={(p,) for p in low['qualified']}=={('u4s4',),('u2s2',)}
    flat=[dict(cohort=r['cohort'],arm=r['arm'],T=v['T'],row=v) for r in hc['runs'] for v in r['rows']]
    assert pairs(flat,hc['pairs'],['T'])=={(t,) for t in hc['qualified_T']}=={(1024,),(2069,)}
    assert hc['adopted_T']==[1024,2069]
    for r in hc['runs']:
        assert r['summary']['checks']==256 and r['summary']['worst_random_abs_vs_cpu64']<=.005
        assert [(v['T'],v['hash']) for v in r['rows']]==[(v['T'],v['hash']) for v in hc['runs'][0]['rows']]
    for v in [hc['boundary'],final['boundary']]:assert v['checks']==1728 and v['mismatched_cases']==0
    assert final['hc']['all_hashes_equal_baseline'] and final['hc']['summary']['checks']==256 and final['xmx_selftest_cases']==131
    assert [(v['T'],v['hash']) for v in final['hc']['rows']]==[(v['T'],v['hash']) for v in hc['runs'][0]['rows']]
    assert len(d['native'])==19 and all(v['grf_count']==128 and not v['declares_spill_buffer'] for v in d['native'])
    for v in d['native']:
        if v['scope']!='production':assert len(v['loops'])==1 and v['loops'][0]['memory_sends']==0
    if sys.argv[1:]:
        for name,digest in d['evidence_sha256'].items()|final['source_sha256'].items():
            assert hashlib.sha256((ROOT/name).read_bytes()).hexdigest()==digest,name
    print('PASS 100 instruction processes,10 HC processes,all samples/pairs,19 native audits,final1728/256/131 checks')

if __name__=='__main__':main()
