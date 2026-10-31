"""Recompute the published INT8 layout gates without a GPU or dependencies."""
from pathlib import Path
import json
import math
import statistics
import re
import datetime

def compare(rows):
    gains=[];stable=[]
    for v in rows:
        gain=100*(math.sqrt(v['a0_us']*v['a1_us'])/math.sqrt(v['b0_us']*v['b1_us'])-1)
        drift=max(abs(100*(v['a1_us']/v['a0_us']-1)),abs(100*(v['b1_us']/v['b0_us']-1)))
        gains.append(gain)
        if drift<=3:stable.append(gain)
    return {'stable_cycles':len(stable),'median_pct':statistics.median(stable) if stable else None,
            'all_median_pct':statistics.median(gains)}

def main():
    d=json.loads(Path(__file__).with_name('int8-layout-2026-10-25.json').read_text(encoding='utf-8'))
    assert not d['adopted'];comparisons=0
    datasets=[('unprofiled',d['full_chain'],d['selected_before_confirm'],d['confirmed']),
        ('production',d['production_queue']['cohorts'],d['production_queue']['selected_before_confirm'],d['production_queue']['confirmed'])]
    for policy,cohorts,expected_selected,expected_confirmed in datasets:
        selected=[];confirmed=[]
        for cohort in cohorts:
            for run in cohort['runs']:
                assert {(g['T'],g['mode']) for g in run['groups']}=={(t,m) for t in (1,6,8,16,40,64,128,256) for m in (1,2)}
                for g in run['groups']:
                    assert [p['cycle'] for p in g['pairs']]==list(range(8))
                    assert all(g[key]==value for key,value in compare(g['pairs']).items())
                    assert {e['profile'] for e in g['errors']}=={0,1} and len(g['errors'])==2
                    quality=all(e['nrmse']<=(.1 if e['profile'] else .05) and e['cosine']>=(.98 if e['profile'] else .995) for e in g['errors'])
                    assert quality==g['quality_passed']
                    passed=quality and g['stable_cycles']>=6 and g['median_pct']>=3
                    assert passed==g['passed'];comparisons+=2
                    cell=[run['gu'],g['T'],g['mode']]
                    if cohort['phase']=='screen' and passed:selected.append(cell)
                    if cohort['phase']=='confirm' and passed and cell in selected:confirmed.append(cell)
                if policy=='unprofiled':
                    queue=next(v for v in d['queue_property_comparison'] if (v['phase'],v['gu'])==(cohort['phase'],run['gu']))
                    assert queue['trial_profiling']==0
                    for g in queue['groups']:
                        rows=[v for v in run['queue_pairs'] if v['T']==g['T']]
                        assert len(rows)==8 and all(g[key]==value for key,value in compare(rows).items())
        assert selected==expected_selected and confirmed==expected_confirmed
        assert len(cohorts)==(2 if selected else 1)
        print(policy,'confirmed',confirmed)
    assert d['production_queue']['default_profiling']==d['production_queue']['trial_profiling']==1
    for schedule in (d['wait_scheduling'],d['production_queue']['wait_scheduling']):
        assert schedule['restored_priority']==32
        assert datetime.datetime.fromisoformat(schedule['restored_utc'])<datetime.datetime.fromisoformat(schedule['compile_log_created_utc'])
    for i,diagnosis in enumerate(d['diagnoses']):
        for run in diagnosis['runs']:
            rows=run['rows'];stages={v['stage'] for v in rows}
            assert len(stages)==(3 if i==0 else 8) and len(rows)==3*2*len(stages)*8
            assert {(v['T'],v['role'],v['stage'],v['cycle']) for v in rows}=={
                (t,r,s,c) for t in (8,64,256) for r in ('gu','down') for s in stages for c in range(8)}
    report=Path(__file__).with_name('int8-layout-2026-10-25.md').read_text(encoding='utf-8')
    cells=re.findall(r'^\| (IQ2_XXS|IQ2_S|Q2_0) \| (\d+) \| ([^|]+) \| ([^|]+) \|$',report,re.M)
    assert len(cells)==24
    gt={'IQ2_XXS':16,'IQ2_S':22,'Q2_0':42}
    for name,t,*values in cells:
        run=next(v for v in d['production_queue']['cohorts'][-1]['runs'] if v['gu']==gt[name])
        for mode,value in enumerate(values,1):
            group=next(g for g in run['groups'] if (g['T'],g['mode'])==(int(t),mode))
            text=f"{group['median_pct']:+.3f}%".replace('-','−')
            assert value.strip()==text+(' ✓' if [gt[name],int(t),mode] in d['production_queue']['confirmed'] else '')
    assert f'{comparisons} 组数值比较' in report
    pairs=sum(len(g['pairs']) for _,cohorts,_,_ in datasets for c in cohorts for r in c['runs'] for g in r['groups'])
    assert f'{pairs} 轮完整链 A/B/B/A' in report
    print(f'PASS {comparisons} numerical records, all paired gates, queue comparisons and profile coverage')

if __name__=='__main__':main()
