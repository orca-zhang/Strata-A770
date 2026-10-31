"""Replay historical speed protocols and score fixed targets without changing production.

python tools/check_orca_regression.py --self-check
python tools/check_orca_regression.py --phase short --out build-orca-short-replay
python tools/check_orca_regression.py --phase raw --variants old,old-v0,orca,orca-tail-off --out build-orca-ppl-screen
"""
import argparse
import json
import math
import os
from pathlib import Path
import statistics
import time

import bench_deepinfer as b

R = b.ROOT
read = lambda p: json.loads(Path(p).read_text(encoding='utf-8'))


def runtime_env(values, libraries):
    # Historical environment dumps also contain unrelated credentials. Only replay runtime fields.
    selected = {k:v for k,v in values.items() if k.startswith(('STRATA_','SYCL_','UR_','ONEAPI_','ZES_'))
                and not any(word in k for word in ('API_KEY','SECRET','TOKEN'))}
    env = {k:v for k,v in os.environ.items() if not k.startswith(('STRATA_','SYCL_','UR_','ONEAPI_','ZES_'))}
    env.update(selected)
    env['PATH'] = os.pathsep.join(libraries) + os.pathsep + env.get('PATH','')
    return env, selected


def scores(path, ids):
    rows = [line.split('\t') for line in path.read_text(encoding='utf-8').splitlines()]
    assert len(rows) == len(ids)-1
    assert [int(row[0]) for row in rows] == list(range(len(ids)-1))
    assert [int(row[1]) for row in rows] == ids[1:]
    assert all(len(row)>=8 and all(math.isfinite(float(row[k])) for k in (2,4,6,7)) for row in rows)
    raw = statistics.mean(-float(row[2]) for row in rows)
    conditional = statistics.mean(-float(row[7]) for row in rows)
    return dict(targets=len(rows), raw_nll=raw, raw_ppl=math.exp(raw),
                conditional_nll=conditional, conditional_ppl=math.exp(conditional),
                scores_tsv=path.read_bytes().decode('utf-8'), scores_sha256=b.sha(path))


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--self-check',action='store_true')
    parser.add_argument('--phase',choices=('short','2k','raw'))
    parser.add_argument('--variants',default='old,old-v0,orca,orca-tail-off')
    parser.add_argument('--config',type=Path,default=R/'strata-iq2_xs.json')
    parser.add_argument('--score-tokens',type=int,default=128)
    parser.add_argument('--out',type=Path)
    args=parser.parse_args()
    if args.self_check:
        import tempfile
        with tempfile.TemporaryDirectory() as directory:
            p=Path(directory)/'score.tsv'
            p.write_text('0\t2\t-2\t2\t-1\t0\t0.5\t-1.30685281944\n',encoding='utf-8')
            s=scores(p,[1,2]); assert s['targets']==1 and math.isclose(s['raw_ppl'],math.exp(2))
            try: scores(p,[1,3])
            except AssertionError: pass
            else: raise AssertionError('wrong target accepted')
        _, selected=runtime_env({'STRATA_GDN_Q8_TAIL':'1','UNRELATED_API_KEY':'private'},[])
        assert selected=={'STRATA_GDN_Q8_TAIL':'1'}
        print('PASS fixed-target coverage, raw/conditional PPL and historical environment whitelist; no GPU')
        return
    if not args.phase or not args.out or not 2<=args.score_tokens<=512:
        parser.error('--phase, new --out and score-tokens in [2,512] required')
    current=read(args.config)
    old=read(R/'strata-iq2_xs.json.sycl-backup')
    assert current['backend']=='orca' and old['backend']=='sycl'
    inputs=b.prompts(current)
    short_plan=read(R/'bench/results/2026-10-21-a770-memory/latest-39-requests-clean-plan.json')
    short_cases=read(R/'bench/results/2026-10-19-a770-model-refresh/inputs.json')
    hc=read(R/'build-sycl-a770-hc-q8-accept1-recovery-20261026/results.json')
    hc_env=read(R/'build-sycl-a770-decode2k-graph-20261025/frozen-environment.json')
    hist_short=R/'build-sycl-a770-memory-20261021/strata-latest.exe'
    variants=args.variants.split(',')
    if not set(variants)<=set(('old','old-v0','orca','orca-tail-off','orca-exact','orca-bf16')):
        parser.error('unknown raw-score variant')
    out=args.out.resolve(); out.mkdir(parents=True,exist_ok=False)
    (out/Path(__file__).name).write_bytes(Path(__file__).read_bytes())
    images=Path(current['env']['STRATA_ORCA_IMAGES'])
    image_manifest=lambda:{p.relative_to(images).as_posix():b.sha(p) for p in images.rglob('*') if p.is_file()}
    files=[Path(__file__),Path(b.__file__),args.config,R/'strata-iq2_xs.json.sycl-backup',
           Path(current['exe']),Path(old['exe']),hist_short,Path(hc['exe'])]
    hashes={str(p):b.sha(p) for p in files}
    assert hashes[str(hist_short)]==short_plan['exe_sha256']
    assert hashes[hc['exe']]==read(R/'bench/results/a770/hc-q8-model-1pct-confirm-2026-10-26.json')['executable_sha256']
    d=dict(complete=False,production_changed=False,linux_gpu_validated=False,phase=args.phase,
           score_tokens=args.score_tokens,files_sha256=hashes,images_sha256=image_manifest(),runs=[])
    save=lambda:b.save(out/'results.json',d)
    save(); print('OUT',out,flush=True)

    def run(label,exe,model_args,values,requests,score=False):
        env, selected=runtime_env(values,current['lib_dirs'])
        arm=dict(label=label,exe=str(exe),args=model_args,runtime_env=selected,rows=[])
        d['runs'].append(arm); save()
        logpos=out/(label+'.tsv')
        if score: env['STRATA_LOGPOS']=str(logpos)
        print('RUN',label,flush=True)
        engine=b.Engine(exe,model_args,env,out/label)
        arm['info']=engine.info
        try:
            for case,ids,count,warm in requests:
                started=time.perf_counter()
                row=engine.gen(ids,count,keys='temperature=0')
                row.update(case=case,input_ids=ids,retained=not warm,wall_ms=(time.perf_counter()-started)*1000)
                assert row['generated']>0 and any(row['tokens']) and row['resume']==0
                arm['rows'].append(row)
                if score: arm['scores']=scores(logpos,ids)
                save()
                print('DONE',label,case,'warm' if warm else 'kept',row['generated'],
                      round(1000*row['generated']/row['decode_ms'],3),'tok/s',
                      row['accepted'],row['offered'],flush=True)
        finally:
            engine.close(); arm['exit_code']=engine.p.returncode; save()

    try:
        with b.gpu_lock():
            if args.phase=='short':
                ids=short_cases['short']['tokens']
                assert len(ids)==28
                requests=[('Short28',ids,128,i<10) for i in range(15)]
                for label,native in [('A0',False),('B',True),('A1',False)]:
                    values=current['env'] if native else short_plan['env']
                    run(label,Path(current['exe']) if native else hist_short,short_plan['args'],values,requests)
            elif args.phase=='2k':
                requests=[('Short23',inputs['Short'],16,True)]+[
                    (case,hc['input_ids'][case],128,i==0) for i in range(4) for case in ('Code2K','Prose2K')]
                for label,native in [('A0',False),('B',True),('A1',False)]:
                    # ponytail: preserve the historical 30s settle, outside timing; no telemetry dependency.
                    time.sleep(30)
                    run(label,Path(current['exe']) if native else Path(hc['exe']),hc['args']['A1'],
                        current['env'] if native else hc_env,requests)
            else:
                for variant in variants:
                    cfg=old if variant.startswith('old') else current
                    values=cfg['env'].copy(); model_args=cfg['args'].copy()
                    if variant=='old-v0': values['STRATA_VALUE_PACKED_BITS']='0'
                    if variant=='orca-tail-off': values.update(STRATA_GDN_Q8_TAIL='0',STRATA_QSA_Q8_TAIL='0')
                    if variant=='orca-exact': values['STRATA_IQ_MT_MIN']='1'
                    if variant=='orca-bf16': model_args+=['--gr-fp32-activations','--native-bf16','--native-bf16-extra']
                    for case in ('Code2K','Prose2K'):
                        run(variant+'-'+case,Path(cfg['exe']),model_args,values,
                            [(case,inputs[case][:args.score_tokens],1,False)],score=True)
            d['files_unchanged']=all(b.sha(Path(p))==h for p,h in hashes.items())
            d['images_unchanged']=image_manifest()==d['images_sha256']
            assert d['files_unchanged'] and d['images_unchanged']
            d['complete']=True
    except BaseException as error:
        d['error']=f'{type(error).__name__}: {error}'
        raise
    finally: save()
    print('COMPLETE',out,flush=True)


if __name__=='__main__': main()
