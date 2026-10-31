"""Check the published same-scene performance record without private files or a GPU."""
import json
import math
from pathlib import Path
import statistics

d=json.loads(Path(__file__).with_name('down-retry-single-scene-2026-10-29.json').read_text(encoding='utf8'))
assert d['schema']==1 and d['scope']=='whole-model-single-scene-performance' and d['performance_complete']
assert not any(d[k] for k in ('production_changed','adopted','target_achieved')) and d['warm_quality_passed'] is None
assert (d['expert_slots'],d['input_tokens'],d['output_tokens'],d['min_p'],d['production_min_p'])==(6297,2048,128,.5,.8)
assert len(d['source_results_sha256'])==64
runs=d['runs'];assert [r['label'] for r in runs]==['A1','B1','A2','A3','B2','A4']
for run in runs:
    assert run['exit_code']==0 and run['expert_slots']==6297 and run['ready_free_mib']>=1024
    assert run['candidate']==run['label'].startswith('B')
    assert [(r['case'],r['warmup']) for r in run['rows']]==[(case,warm) for warm in (True,False,False,False) for case in ('Code2K','Prose2K')]
    assert len(run['scans'])==8 and all(r==dict(checks=48,experts=0,trigger_tiles=0) for r in run['scans'])
    for row in run['rows']:
        assert row['generated']==128 and row['prompt_tokens']==2048 and row['reused']==0 and row['finish']=='length'
        assert math.isfinite(row['decode_ms']) and row['decode_ms']>0 and 0<=row['accepted']<=row['offered']
        assert len(row['output_sha256'])==64
        if not run['candidate']:
            assert row['output_sha256']==next(r['output_sha256'] for r in runs[0]['rows'] if r['case']==row['case'])
assert all(all(a[k]==b[k] for k in ('case','warmup','output_sha256','accepted','offered')) for a,b in zip(runs[1]['rows'],runs[4]['rows']))
assert runs[1]['initializer_sha256']==runs[4]['initializer_sha256']
assert len({run['initializer_sha256'] for run in runs if not run['candidate']})==1
qualified=[]
for key,start in (('comparison',0),('confirmation',3)):
    qualified.append(set())
    for case in ('Code2K','Prose2K'):
        a,c,z=[statistics.median(r['decode_ms'] for r in run['rows'] if r['case']==case and not r['warmup']) for run in runs[start:start+3]]
        base=math.sqrt(a*z);gain=100*(base/c-1);drift=100*(z/a-1)
        row=d[key][case]
        values=dict(baseline=base,candidate=c,throughput_change_pct=gain,baseline_drift_pct=drift,baseline_tps=128000/base,candidate_tps=128000/c)
        candidates=[128000/r['decode_ms'] for r in runs[start+1]['rows'] if r['case']==case and not r['warmup']]
        values.update(candidate_min_tps=min(candidates),candidate_max_tps=max(candidates))
        for name,value in values.items():assert math.isclose(row[name],value,rel_tol=0,abs_tol=1e-9),(key,case,name)
        assert row['throughput_is_noise']==(abs(gain)<.3)
        if gain>=1 and abs(drift)<=3:qualified[-1].add(case)
assert d['confirmed_scenes']==sorted(qualified[0]&qualified[1])==['Code2K']
stable=[case for case in d['confirmed_scenes'] if all(128000/r['decode_ms']>=35 for run in runs if run['candidate'] for r in run['rows'] if r['case']==case and not r['warmup'])]
assert d['stable35_scenes']==stable==[]
for name in ('memory','telemetry'):
    assert [(r['label'],r['case'],r['warmup']) for r in d[name]]==[(run['label'],row['case'],row['warmup']) for run in runs for row in run['rows']]
    assert all(r['samples']>0 for r in d[name])
assert d['sampled_memory_passed']==all(r['min_free_mib']>=1024 for r in d['memory'])
assert sum(r['samples'] for r in d['memory'])==1223 and min(r['min_free_mib'] for r in d['memory'])==1442.1171875
assert sum(r['samples'] for r in d['telemetry'])==217
assert all(r['min_mhz']==r['max_mhz']==2400 and not r['throttle_observed'] for r in d['telemetry'])
print('PASS 48 full requests,independent same-scene gains,output/history hashes,48 scans,1223 memory and217 frequency samples;quality pending,no stable35')
