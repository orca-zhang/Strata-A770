"""Recompute full-HC fused U4xS4 quality, drift and timing gates from raw logs."""
from pathlib import Path
import hashlib,json,math,statistics

def analyze(stdout, scope='three_pairs'):
    assert scope in ('three_pairs', 'all97')
    count, ts, injected = (97, tuple(range(1,9)), 96) if scope=='all97' else (3, (1,4,6,8), 2)
    errors=[json.loads(s[6:]) for s in stdout.splitlines() if s.startswith('ERROR ')]
    pairs=[json.loads(s[5:]) for s in stdout.splitlines() if s.startswith('PAIR ')]
    expected={(c,t,a,p,f) for c in range(count) for t in ts for a in (0,1) for p in range(3)
              for f in ([0,1]+([2] if c<injected else [])+([3] if a else []))}
    assert len(errors)==len(expected) and {(v['case'],v['T'],v['apply'],v['profile'],v['field']) for v in errors}==expected
    for v in errors:
        assert all(math.isfinite(v[k]) for k in ('nrmse','cosine','max_abs'))
        limit=1e-6 if v['field']==3 else .1 if v['profile']==2 else .05
        cosine=1-1e-9 if v['field']==3 else .98 if v['profile']==2 else .995
        assert v['passed']==(v['nrmse']<=limit and v['cosine']>=cosine) and v['passed']
    assert len(pairs)==8*len(ts) and stdout.rstrip().endswith('PASS HC split-K')
    cells=[]
    for t in ts:
        group=[v for v in pairs if v['T']==t]
        assert len(group)==8 and {v['cycle'] for v in group}==set(range(8))
        gains=[];drifts=[]
        for v in group:
            assert all(math.isfinite(v[k]) and v[k]>0 for k in ('a0_us','b0_us','b1_us','a1_us'))
            drift=max(abs(100*(v['a1_us']/v['a0_us']-1)),abs(100*(v['b1_us']/v['b0_us']-1)))
            drifts.append(drift)
            if drift<=3:gains.append(100*(math.sqrt(v['a0_us']*v['a1_us'])/math.sqrt(v['b0_us']*v['b1_us'])-1))
        gain=statistics.median(gains) if gains else None
        cells.append(dict(T=t,stable=len(gains),gain_pct=gain,max_drift_pct=max(drifts),
                          a_us=statistics.median(math.sqrt(v['a0_us']*v['a1_us']) for v in group),
                          b_us=statistics.median(math.sqrt(v['b0_us']*v['b1_us']) for v in group),
                          screen_passed=len(gains)>=6 and gain>=1))
    return dict(quality_cells=len(errors),max_nrmse=max(v['nrmse'] for v in errors),cells=cells)

def check_models(d):
    assert not d['production_changed'] and not d['adopted']
    assert d['model_speed_measured']==any('performance' in v['stages'] for v in d['variants'])
    for variant in d['variants']:
        if 'storage_validation' in variant:
            s=variant['storage_validation'];parent=next(v for v in d['variants'] if v['name']==s['parent'])
            assert s['parent_exe_sha256']==parent['exe_sha256']
            runs=variant['stages']['smoke']['runs'];old=parent['stages']['smoke']['runs']
            tokens=lambda run:{x['case']:x['tokens'] for x in run['rows']}
            assert s['candidate_token_arrays_equal']==(tokens(runs[1])==tokens(old[1]))
            assert s['baseline_token_arrays_equal']==(tokens(runs[0])==tokens(runs[2])==tokens(old[0])==tokens(old[2]))
            assert s['ready_free_gain_mib']==runs[1]['free_mib']-(runs[0]['free_mib']+runs[2]['free_mib'])/2
            if 'candidate_fixed_target_scores_equal' in s:
                assert s['candidate_fixed_target_scores_equal']==(variant['stages']['quality']['runs'][1]['quality']==parent['stages']['quality']['runs'][1]['quality'])
        for name,stage in variant['stages'].items():
            runs=stage['runs']
            assert stage['complete'] and [r['label'] for r in runs]==['A1','B1','A2']
            assert all(r['exit_code']==0 and r['expert_slots']==5771 and r['free_mib']>=1536 for r in runs)
            assert not runs[0]['dispatch'] and not runs[2]['dispatch'] and len(runs[1]['dispatch'])==1
            selected=95 if variant['name']=='build-sycl-a770-u4s4-model-20261026' or '95-20261027' in variant['name'] else 96
            assert variant.get('selected_up',selected)==selected
            assert f'count={selected} weight_gate_fallback={96-selected}' in runs[1]['dispatch'][0]
            if name=='quality':
                a,b,z=[r['quality'] for r in runs];assert a==z and len(a)==9
                total=0;delta=0
                for case in a:
                    for arm in (a,b,z):
                        rows=arm[case]['rows'];count=arm[case]['tokens'];assert len(rows)==count>0
                        assert all(math.isfinite(r['logprob']) for r in rows)
                        assert arm[case]['nll']==-sum(r['logprob'] for r in rows)
                        assert arm[case]['ppl']==math.exp(arm[case]['nll']/count)
                    assert [(r['position'],r['target']) for r in a[case]['rows']]==[(r['position'],r['target']) for r in b[case]['rows']]
                    change=100*math.expm1((b[case]['nll']-a[case]['nll'])/a[case]['tokens'])
                    assert stage['comparison']['cases'][case]['ppl_change_pct']==change
                    total+=a[case]['tokens'];delta+=b[case]['nll']-a[case]['nll']
                assert total==stage['comparison']['targets']==758
                aggregate=100*math.expm1(delta/total)
                assert aggregate==stage['comparison']['aggregate_ppl_change_pct']
                assert stage['passes_quality_screen']==(aggregate<=1 and all(c['ppl_change_pct']<=3 for c in stage['comparison']['cases'].values()))
            else:
                for r in runs:
                    rows=r['rows'];assert len(rows)==(8 if name=='performance' else 4 if name=='prefix-diagnostic' else 2)
                    length=64 if name=='prefix-diagnostic' else 128
                    assert all(x['generated']==len(x['tokens'])==length and x['prompt_tokens']==2048 and x['reused']==0 and x['finish']=='length' for x in rows)
                    assert all(math.isfinite(x[k]) and x[k]>0 for x in rows for k in ('decode_ms','prompt_ms'))
                    for case in ('Code2K','Prose2K'):
                        group=[x for x in rows if x['case']==case];assert group
                        assert all(x['tokens']==group[0]['tokens'] for x in group)
                        loops=4 if name=='performance' else 2 if name=='prefix-diagnostic' else 1
                        assert len(group)==loops and {x['cycle'] for x in group}==set(range(loops))
                        assert all(x['warmup']==(x['cycle']==0) for x in group)
                for case in ('Code2K','Prose2K'):
                    assert next(r['tokens'] for r in runs[0]['rows'] if r['case']==case)==next(r['tokens'] for r in runs[2]['rows'] if r['case']==case)
                if name=='performance':
                    assert variant['stages']['quality']['passes_quality_screen']
                    for case,c in stage['comparison'].items():
                        times=[statistics.median(x['decode_ms'] for x in r['rows'] if x['case']==case and not x['warmup']) for r in runs]
                        assert c['throughput_change_pct']==100*(math.sqrt(times[0]*times[2])/times[1]-1)
                        assert c['baseline_drift_pct']==100*(times[2]/times[0]-1)
                        assert c['baseline']==math.sqrt(times[0]*times[2]) and c['candidate']==times[1]
                        assert c['baseline_tps']==128000/math.sqrt(times[0]*times[2]) and c['candidate_tps']==128000/times[1]
                    assert stage['passes_screen']==all(c['throughput_change_pct']>=1 and abs(c['baseline_drift_pct'])<=3 for c in stage['comparison'].values())
                elif name=='prefix-diagnostic':
                    assert stage['diagnostic_only'] and not variant['stages']['performance']['passes_screen']
                    assert stage['quiet_depth_note']
                    assert all(len(r['timing'])==len(r['windows'])==4 and not r['depth'] for r in runs)
                    for r in runs:
                        raw=[]
                        for row,windows,timing in zip(r['rows'],r['windows'],r['timing']):
                            assert windows and windows[0]['pos']==2047
                            assert timing.startswith(f"strata decode timing: {len(windows)} windows,")
                            assert all(w['T'] in range(1,7) and 0<=w['hits']<=w['entries']==480*w['T'] for w in windows)
                            assert all(math.isfinite(w[k]) and w[k]>=0 for w in windows for k in ('submit_ms','wait_ms'))
                            assert all(a['pos']<b['pos']<=a['pos']+a['T'] for a,b in zip(windows,windows[1:]))
                            for w in windows:
                                raw.extend([f"verify host: pos={w['pos']} T={w['T']} submit={w['submit_ms']:.3f} wait={w['wait_ms']:.3f} ms",
                                            f"strata device routing: {w['hits']}/{w['entries']} entries in VRAM"])
                        assert raw==r['routing']
                    for case in ('Code2K','Prose2K'):
                        tokens=[next(x['tokens'] for x in r['rows'] if x['case']==case) for r in runs]
                        assert stage['matching_output'][case]==(tokens[0]==tokens[1]==tokens[2])

def main():
    # Two U4 planes already occupy eight bits. An eventual U8xS8 control can keep
    # the exact quantized dot: shift affine q[-136,119] by 8, then correct both biases.
    # This checks integer equivalence only; it is not a GPU speed/quality result.
    for activation in range(-136,120):
        for weight in range(-119,120):
            assert (weight+128)*(activation+8)-128*(activation+8)-8*weight==activation*weight
    assert 64*247*128<2**31   # worst raw group64 DPAS accumulation
    d=json.loads(Path(__file__).with_name('u4s4-hc-fused-2026-10-26.json').read_text(encoding='utf-8'))
    assert not d['model_speed_measured'] and not d['production_changed']
    if 'followup_signed_scale_cpu' in d:
        cpu=d['followup_signed_scale_cpu'];rows=cpu['matrices']
        assert cpu['cpu_only'] and not cpu['gpu_native_packer_used'] and not cpu['model_quality_measured']
        assert len(rows)==len({v['prefix'] for v in rows})==97 and cpu['passed']==97
        assert cpu['maximum_nrmse']==max(v['nrmse'] for v in rows)
        assert all(math.isfinite(v['nrmse']) and 0<=v['nrmse']<=.01 and v['passed'] for v in rows)
        for code in range(-136,120):
            high=(code+8)//16;low=code-16*high
            assert -8<=high<=7 and -8<=low<=7 and high*16+low==code
    if 'followup_native_v2_conversion' in d:
        converted=d['followup_native_v2_conversion'];rows=converted['matrices']
        assert converted['complete'] and not converted['engine_loaded'] and not converted['model_speed_measured']
        assert len(rows)==len({v['name'] for v in rows})==converted['count']==97
        assert converted['source_bytes']==sum(v['source_bytes'] for v in rows)
        assert converted['native_bytes']==sum(v['native_bytes'] for v in rows)
        for row in rows:
            assert row['source_type']=='BF16' and (row['rows'],row['cols'])==(10240,320)
            assert row['source_bytes']==6553600 and row['native_bytes']==32+10240*320+10240*5*4
            assert math.isfinite(row['nrmse']) and 0<=row['nrmse']<=.01 and row['passed']
    if 'followup_native_v2_gpu' in d:
        native=d['followup_native_v2_gpu']
        assert native['complete'] and native['exit_code']==0 and native['matrices']==97
        lines=native['stdout'].splitlines()
        assert [int(s.split()[1]) for s in lines if s.startswith('PASS_NATIVE ')]==list(range(97))
        assert lines[-1]=='PASS native v002 all97 bitwise GPU pack comparison'
    if 'followup_native_positive_cpu' in d:
        converted=d['followup_native_positive_cpu'];rows=converted['matrices']
        assert converted['complete'] and converted['cpu_only'] and not converted['engine_loaded'] and not converted['model_speed_measured']
        assert len(rows)==len({v['name'] for v in rows})==converted['count']==95
        assert not {'blk.44.hc_attn_up.weight','output_hc_up.weight'} & {v['name'] for v in rows}
        assert converted['source_bytes']==sum(v['source_bytes'] for v in rows)
        assert converted['native_bytes']==sum(v['native_bytes'] for v in rows)
        for row in rows:
            assert row['source_type']=='BF16' and (row['rows'],row['cols'])==(10240,320)
            assert row['source_bytes']==6553600 and row['native_bytes']==32+10240*320+10240*5*4
            assert math.isfinite(row['nrmse']) and 0<=row['nrmse']<=.01 and row['passed']
    for run in d['runs']:
        assert run['exit_code']==0 and not run['stderr'].strip()
        assert hashlib.sha256(run['stdout'].encode()).hexdigest()==run['stdout_sha256']
        assert analyze(run['stdout'],run.get('scope','three_pairs'))==run['analysis']
        if run.get('bitwise_u4_output_equal'):
            assert run['variant']==6 and run['products']==2 and run['baseline']=='affine U4xS4 four-DPAS hybrid GRF'
            assert run['analysis']['quality_cells']==16248 and run['analysis']['max_nrmse']==0
            assert all(json.loads(s[6:])['max_abs']==0 for s in run['stdout'].splitlines() if s.startswith('ERROR '))
    affine=[v for v in d['runs'] if v['name'] in ('up4-affine-all97','up4-affine-hybrid-grf-all97','up4-affine-native-hybrid-all97')]
    if len(affine)==3:
        # Equality is of the recorded error metrics, not a claim of bitwise hidden-state parity.
        metrics=lambda v:[json.loads(s[6:]) for s in v['stdout'].splitlines() if s.startswith('ERROR ')]
        assert metrics(affine[0])==metrics(affine[1])==metrics(affine[2])
    controls=[v for v in d['runs'] if v.get('bitwise_u4_output_equal')]
    if len(controls)==2:
        assert controls[0]['exe_sha256']==controls[1]['exe_sha256'] and controls[0]['source_sha256']==controls[1]['source_sha256']
        assert all(c['stable']==8 and c['screen_passed'] for v in controls for c in v['analysis']['cells'])
    for run in d.get('rejected_runs',[]):
        assert run['exit_code']!=0 and not any(s.startswith('PAIR ') for s in run['stdout'].splitlines())
        errors=[json.loads(s[6:]) for s in run['stdout'].splitlines() if s.startswith('ERROR ')]
        assert len(errors)==228 and any(not v['passed'] for v in errors)
        for v in errors:
            limit=1e-6 if v['field']==3 else .1 if v['profile']==2 else .05
            cosine=1-1e-9 if v['field']==3 else .98 if v['profile']==2 else .995
            assert v['passed']==(v['nrmse']<=limit and v['cosine']>=cosine)
    total=sum(run['analysis']['quality_cells'] for run in d['runs'])
    model=Path(__file__).with_name('u4s4-hc-model-gpu-2026-10-26.json')
    if model.exists():check_models(json.loads(model.read_text(encoding='utf-8')))
    print(f"PASS {len(d['runs'])} frozen full-HC runs, {total} quality cells; all raw gates recomputed; no adoption implied")

if __name__=='__main__':main()
