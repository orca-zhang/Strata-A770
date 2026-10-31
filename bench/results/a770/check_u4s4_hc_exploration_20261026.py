"""Audit exploratory full-HC logs; no exploratory positive is promoted to a confirmed gain."""
from pathlib import Path
import json
import math
import statistics


def calculate(text):
    errors=[json.loads(line[6:]) for line in text.splitlines() if line.startswith('ERROR ')]
    pairs=[json.loads(line[5:]) for line in text.splitlines() if line.startswith('PAIR ')]
    assert len(errors)==192 and len(pairs)==32 and text.rstrip().endswith('PASS HC split-K')
    expected={(c,t,a,p,f) for c in range(3) for t in (1,4,6,8) for a in (0,1)
              for p in (0,1,2) for f in ((0,1,2) if c<2 else (0,1))}
    assert {(v['case'],v['T'],v['apply'],v['profile'],v['field']) for v in errors}==expected
    for v in errors:
        assert math.isfinite(v['nrmse']) and math.isfinite(v['cosine']) and math.isfinite(v['max_abs'])
        assert v['passed']==(v['nrmse']<=(.1 if v['profile']==2 else .05) and v['cosine']>=(.98 if v['profile']==2 else .995))
        assert v['passed']
    cells=[]
    for t in (1,4,6,8):
        group=[v for v in pairs if v['T']==t];assert len(group)==8
        assert {v['cycle'] for v in group}==set(range(8))
        stable=[]
        for v in group:
            assert all(v[k]>0 and math.isfinite(v[k]) for k in ('a0_us','b0_us','b1_us','a1_us'))
            da=100*(v['a1_us']/v['a0_us']-1);db=100*(v['b1_us']/v['b0_us']-1)
            if max(abs(da),abs(db))<=3:
                stable.append(100*(math.sqrt(v['a0_us']*v['a1_us'])/math.sqrt(v['b0_us']*v['b1_us'])-1))
        median=statistics.median(stable) if stable else None
        cells.append(dict(T=t,stable_cycles=len(stable),gain_pct=median,
            a_us=statistics.median(math.sqrt(v['a0_us']*v['a1_us']) for v in group),
            b_us=statistics.median(math.sqrt(v['b0_us']*v['b1_us']) for v in group),
            screen_passed=len(stable)>=6 and median>=1))
    return dict(max_nrmse=max(v['nrmse'] for v in errors),errors=errors,pairs=pairs,cells=cells)


def main():
    path=Path(__file__).with_name('u4s4-hc-exploration-2026-10-26.json')
    d=json.loads(path.read_text(encoding='utf-8'))
    assert d['exploratory_only'] and not d['model_speed_measured'] and not d['production_changed']
    assert len(d['runs'])==7
    for run in d['runs']:
        assert calculate(run['stdout'])==run['analysis']
        assert not any(v['screen_passed'] for v in run['analysis']['cells'])
    assert d['excluded_noop_runs']==['fused3-split16','fused4-split16']
    assert d['slm_probe']['valid_measurement'] is False
    print('PASS 7 exploratory runs, 1,344 quality cells, 224 raw ABBA cycles; no full-HC confirmed speedup; two misrouted runs excluded')


if __name__=='__main__':main()
