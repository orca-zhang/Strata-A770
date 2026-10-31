"""Measure existing native FP16, K8/V8 and K8/V4 cache paths; production stays unchanged.

python tools/bench_orca_kv.py --self-check
python tools/bench_orca_kv.py --out build-orca-kv-screen
The initial A/B/C/A is a screen, not an independently confirmed speedup.
"""
import argparse
import json
import math
from pathlib import Path
import statistics
import time

import bench_deepinfer as b
from check_orca_regression import read, runtime_env


def summary(runs):
    answer=[]
    for case in ('Code2K','Prose2K'):
        groups=[[r for r in arm['rows'] if r['case']==case and r['retained']] for arm in runs]
        assert all(len(g)==3 and all(r['generated']==128 for r in g) for g in groups)
        for candidate in (1,2):
            metrics={}
            for key in ('prompt_ms','decode_ms','ttft_ms','wall_ms'):
                values=[statistics.median(r[key] for r in group) for group in groups]
                assert all(math.isfinite(v) and v>0 for v in values)
                metrics[key]=b.paired(values[0],values[candidate],values[3])
                metrics[key]['latency_change_pct']=100*(values[candidate]/metrics[key]['baseline']-1)
            answer.append(dict(case=case,candidate=runs[candidate]['kv'],metrics=metrics,
                               same_output=all(r['tokens']==groups[0][0]['tokens'] for r in groups[candidate]),
                               baseline_output_repeated=all(r['tokens']==groups[0][0]['tokens'] for i in (0,3) for r in groups[i])))
    return answer


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--self-check',action='store_true')
    parser.add_argument('--config',type=Path,default=b.ROOT/'strata-iq2_xs.json')
    parser.add_argument('--out',type=Path)
    args=parser.parse_args()
    if args.self_check:
        runs=[dict(kv=kv,rows=[dict(case=case,retained=True,generated=128,tokens=[1]*128,
                    prompt_ms=v,decode_ms=v,ttft_ms=v,wall_ms=v) for case in ('Code2K','Prose2K') for _ in range(3)])
              for kv,v in [('fp16',100),('int8',90),('k8v4',80),('fp16',100)]]
        result=summary(runs)
        assert len(result)==4 and math.isclose(result[1]['metrics']['decode_ms']['throughput_change_pct'],25)
        assert all(r['baseline_output_repeated'] and r['same_output'] for r in result)
        print('PASS native KV A/B/C/A metrics, per-case samples and output tracking; no GPU')
        return
    if not args.out: parser.error('new --out required')
    cfg=read(args.config); assert cfg['backend']=='orca'
    inputs=b.prompts(cfg)
    out=args.out.resolve(); out.mkdir(parents=True,exist_ok=False)
    (out/Path(__file__).name).write_bytes(Path(__file__).read_bytes())
    env,selected=runtime_env(cfg['env'],cfg['lib_dirs'])
    images=Path(env['STRATA_ORCA_IMAGES'])
    manifest=lambda:{p.relative_to(images).as_posix():b.sha(p) for p in images.rglob('*') if p.is_file()}
    files=[Path(__file__),args.config,Path(cfg['exe']),Path(b.__file__),b.ROOT/'tools/check_orca_regression.py']
    hashes={str(p.resolve()):b.sha(p) for p in files}
    d=dict(complete=False,production_changed=False,linux_gpu_validated=False,performance_confirmed=False,
           protocol='One native-EXE FP16 / K8V8 / K8V4 / FP16 screen; actual automatic-cache profile, '
                    'Short16 then one warmup and three retained interleaved Code/Prose2K/128; '
                    'output changes retained and scored separately. Not the historical K16/V4 implementation.',
           config=cfg,runtime_env=selected,files_sha256=hashes,image_sha256=manifest(),runs=[])
    save=lambda:b.save(out/'results.json',d)
    save(); print('OUT',out,flush=True)
    try:
        with b.gpu_lock():
            for index,kv in enumerate(('fp16','int8','k8v4','fp16')):
                model_args=cfg['args'].copy()
                if '--kv' in model_args: model_args[model_args.index('--kv')+1]=kv
                else: model_args+=['--kv',kv]
                arm=dict(label=str(index),kv=kv,args=model_args,rows=[])
                d['runs'].append(arm); save(); print('RUN',index,kv,flush=True)
                engine=b.Engine(Path(cfg['exe']),model_args,env,out/str(index))
                arm['info']=engine.info
                try:
                    arm['warmup']=engine.gen(inputs['Short'],16,keys='temperature=0')
                    for i in range(4):
                        for case in ('Code2K','Prose2K'):
                            started=time.perf_counter()
                            row=engine.gen(inputs[case],128,keys='temperature=0')
                            row.update(case=case,input_ids=inputs[case],retained=i>0,wall_ms=(time.perf_counter()-started)*1000)
                            arm['rows'].append(row); save()
                            assert row['generated']==128 and any(row['tokens']) and row['resume']==0
                            print('DONE',index,kv,case,i,round(128000/row['decode_ms'],3),flush=True)
                finally:
                    engine.close(); arm['exit_code']=engine.p.returncode; save()
            d['comparisons']=summary(d['runs'])
            d['files_unchanged']=all(b.sha(Path(p))==h for p,h in hashes.items())
            d['images_unchanged']=manifest()==d['image_sha256']
            assert d['files_unchanged'] and d['images_unchanged']
            d['complete']=True
    except BaseException as error:
        d['error']=f'{type(error).__name__}: {error}'
        raise
    finally: save()
    print('COMPLETE',json.dumps(d['comparisons']),flush=True)


if __name__=='__main__': main()
