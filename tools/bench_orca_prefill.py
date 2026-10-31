"""Screen existing prefill submission policies; profiling is a separate diagnostic.

python tools/bench_orca_prefill.py --self-check
python tools/bench_orca_prefill.py --config profile.json --out new-results-directory
python tools/bench_orca_prefill.py --config profile.json --profile --out new-profile-directory
"""
import argparse
import json
import math
from pathlib import Path
import re
import statistics
import time

import bench_deepinfer as b
from check_orca_regression import read,runtime_env


def phase_waits(text):
    # Marks precede their named phase: the next mark's drain belongs to the previous phase.
    totals={};previous=None;count=0
    for phase,elapsed in re.findall(r'strata prefill sync: mark \d+ phase (.*?) done \(waited ([\d.]+) ms\)',text):
        if previous is not None:totals[previous]=totals.get(previous,0.)+float(elapsed)
        previous=phase;count+=1
    return dict(marks=count,previous_phase_drain_ms=totals)


def comparisons(runs):
    result=[]
    for case in ('Code2K','Long8K'):
        for metric in ('prompt_ms','ttft_ms','wall_ms','decode_ms'):
            # The first batched cycle warms graph shapes; do not call its decode steady state.
            values=[statistics.median(r[metric] for r in arm['rows'] if r['case']==case and
                                      (metric!='decode_ms' or r.get('cycle',1)>0)) for arm in runs]
            paired=b.paired(*values)
            paired.update(case=case,metric=metric,medians_A0_B_A1=values,
                          latency_change_pct=100*(values[1]/paired['baseline']-1),
                          first_cycle_excluded=metric=='decode_ms')
            paired['speed_pass']=paired['throughput_change_pct']>=1 and abs(paired['baseline_drift_pct'])<=3
            result.append(paired)
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config',type=Path,default=b.ROOT/'strata-iq2_xs.json')
    parser.add_argument('--out',type=Path)
    parser.add_argument('--profile',action='store_true')
    parser.add_argument('--pair',action='store_true',help='Same-binary FP16 pair OFF/ON/OFF; supply its frozen config')
    parser.add_argument('--repeats',type=int,default=1,help='Retained Code2K/Long8K cycles after the Short primer')
    parser.add_argument('--tokens',type=int,default=16)
    parser.add_argument('--self-check',action='store_true')
    args=parser.parse_args()
    if args.self_check:
        s=phase_waits('strata prefill sync: mark 1 phase gdn done (waited 3.0 ms)\n'
                      'strata prefill sync: mark 2 phase qsa proj done (waited 7.5 ms)\n')
        assert s==dict(marks=2,previous_phase_drain_ms={'gdn':7.5})
        runs=[dict(rows=[dict(case=case,prompt_ms=v,ttft_ms=v,wall_ms=v,decode_ms=v,cycle=1) for case in ('Code2K','Long8K')])
              for v in (100,90,100)]
        assert all(r['speed_pass'] and math.isclose(r['latency_change_pct'],-10) for r in comparisons(runs))
        runs[-1]['rows'][0]['ttft_ms']=105
        assert not comparisons(runs)[1]['speed_pass']
        for arm in runs:
            arm['rows'] += [r|{'cycle':0,'decode_ms':9999} for r in arm['rows']]
        assert comparisons(runs)[3]['medians_A0_B_A1']==[100,90,100]
        print('PASS phase-boundary attribution and gain/drift gates; no GPU');return
    if not args.out:parser.error('new --out required')
    if args.repeats<1 or args.tokens<1 or (args.pair and (args.profile or args.repeats<2)):
        parser.error('positive repeats/tokens required; --pair needs >=2 repeats and excludes --profile')
    cfg=read(args.config);assert cfg['backend']=='orca'
    inputs=b.prompts(cfg);env,selected=runtime_env(cfg['env'],cfg['lib_dirs'])
    model_args=cfg['args'].copy()
    for key,value in {'--expert-cache':'5500','--adapt-swaps':'0','--pcie-frac':'0'}.items():
        model_args[model_args.index(key)+1]=value
    assert model_args[model_args.index('--spec-min-p')+1]=='0.5'
    out=args.out.resolve();out.mkdir(parents=True,exist_ok=False)
    (out/Path(__file__).name).write_bytes(Path(__file__).read_bytes())
    files=[Path(__file__),Path(b.__file__),b.ROOT/'tools/check_orca_regression.py',args.config,Path(cfg['exe'])]
    hashes={str(p.resolve()):b.sha(p) for p in files}
    images=Path(env['STRATA_ORCA_IMAGES'])
    manifest=lambda:{p.relative_to(images).as_posix():b.sha(p) for p in images.rglob('*') if p.is_file()}
    d=dict(complete=False,production_changed=False,performance_confirmed=False,
           diagnostic_only=args.profile,linux_gpu_validated=False,exe=cfg['exe'],args=model_args,
           protocol='Same native binary, fixed5771 expert slots/adapt0/pcie0/minp0.5. '
                    +('FP16 pair OFF/ON/OFF; ' if args.pair else 'Screen default/batch0/chain0/default; ')
                    +f'Short16 primer, then {args.repeats} retained Code2K/Long8K cycles, {args.tokens} output tokens. '
                    'No discarded batched warmup; first cycle retained. Pair: >=1% gain and <=3% baseline drift, '
                    'exact output and draft workload required; small sample, no confidence interval. '
                    'Decode uses the engine timer after prompt preparation and excludes cycle0; primer is not a timing sample. '
                    'Separate --profile drains each phase and is not a speed measurement.',
           runtime_env=selected,files_sha256=hashes,images_sha256=manifest(),runs=[])
    save=lambda:b.save(out/'results.json',d)
    save();print('OUT',out,flush=True)
    arms=[('A0',{}),('sync-each-expert',{'STRATA_PREFILL_XMX_EXPERT_BATCH':'0'}),
          ('generic-gemm',{'STRATA_PREFILL_XMX_EXPERT_CHAIN':'0','STRATA_PREFILL_XMX_EXPERT_BATCH':'0'}),('A1',{})]
    if args.profile:arms=[('profile',{'STRATA_PREFILL_SYNC':'1'})]
    if args.pair:arms=[('A0',{'STRATA_PREFILL_FP16_PAIR':'0'}),('B',{'STRATA_PREFILL_FP16_PAIR':'1'}),
                       ('A1',{'STRATA_PREFILL_FP16_PAIR':'0'})]
    try:
        with b.gpu_lock():
            for label,overrides in arms:
                runenv=env|overrides;arm=dict(label=label,overrides=overrides,rows=[])
                d['runs'].append(arm);save();print('RUN',label,flush=True)
                engine=b.Engine(Path(cfg['exe']),model_args,runenv,out/label);arm['info']=engine.info
                try:
                    assert re.search(r'\bexpert_slots=5771\b',engine.info)
                    arm['primer']=engine.gen(inputs['Short'],16,keys='temperature=0')
                    cases=['Code2K'] if args.profile else ['Code2K','Long8K']*args.repeats
                    for i,case in enumerate(cases):
                        start=time.perf_counter();row=engine.gen(inputs[case],args.tokens,keys='temperature=0')
                        row.update(case=case,cycle=i//2,input_ids=inputs[case],wall_ms=(time.perf_counter()-start)*1000)
                        assert row['generated']==args.tokens and row['resume']==row['reused']==0
                        assert row['prompt_tokens']==len(inputs[case])
                        assert all(math.isfinite(row[k]) and row[k]>0 for k in ('prompt_ms','ttft_ms','wall_ms'))
                        arm['rows'].append(row);save();print('DONE',label,case,row['prompt_ms'],row['ttft_ms'],flush=True)
                finally:engine.close();arm['exit_code']=engine.p.returncode;save()
                assert arm['exit_code']==0
                if args.profile:arm['phase_waits']=phase_waits((out/(label+'.stderr')).read_text(encoding='utf-8'));save()
                if args.pair:
                    arm['pair_stats']=re.findall(r'^PREFILL_FP16_PAIR pairs=(\d+) leftover=(\d+) scratch_added=0 workspace=gemm$',
                                                (out/(label+'.stderr')).read_text(encoding='utf-8'),re.M)
                    assert len(arm['pair_stats'])==(2*args.repeats if label=='B' else 0)
                    assert all(int(p)>0 for p,_ in arm['pair_stats'])
            d['files_unchanged']=all(b.sha(Path(p))==h for p,h in hashes.items())
            d['images_unchanged']=manifest()==d['images_sha256']
            assert d['files_unchanged'] and d['images_unchanged']
            if not args.profile:
                reference={r['case']:r for r in d['runs'][0]['rows'][:2]}
                d['exact_output_and_drafts']=all((r['tokens'],r['accepted'],r['offered'])==
                    (reference[r['case']]['tokens'],reference[r['case']]['accepted'],reference[r['case']]['offered'])
                    for arm in d['runs'] for r in arm['rows'])
            if args.pair:
                d['comparisons']=comparisons(d['runs'])
                d['performance_confirmed']=d['exact_output_and_drafts'] and all(r['speed_pass'] for r in d['comparisons'] if r['metric']!='decode_ms')
                d['decode_speed_confirmed']=d['exact_output_and_drafts'] and all(r['speed_pass'] for r in d['comparisons'] if r['metric']=='decode_ms')
            d['complete']=True
    except BaseException as error:d['error']=f'{type(error).__name__}: {error}';raise
    finally:save()
    print('COMPLETE',out,flush=True)


if __name__=='__main__':main()
