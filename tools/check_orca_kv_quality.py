"""Raw fixed-continuation PPL for native FP16, K8/V8 and K8/V4; includes a K-only arithmetic probe.

python tools/check_orca_kv_quality.py --self-check
python tools/check_orca_kv_quality.py --out build-orca-kv-quality
"""
import argparse
import json
import math
from pathlib import Path
import random
import statistics
import struct

import bench_deepinfer as b
from check_orca_regression import read,runtime_env


def half(x):
    return struct.unpack('<e',struct.pack('<e',x))[0]


def q8(row):
    result=[]
    for start in range(0,len(row),64):
        group=row[start:start+64]
        scale=half(max(map(abs,group))/127)
        result.extend(half(max(-127,min(127,round(x/scale)))*scale) if scale else 0. for x in group)
    return result


def k_only():
    # ponytail: deterministic synthetic attention, not model tensors or a GPU measurement.
    # Real-model PPL below measures the actual combined storage formats.
    rng=random.Random(4132)
    answer=[]
    for outlier in (False,True):
        keys=[[rng.gauss(0,1) for _ in range(256)] for _ in range(128)]
        if outlier:
            for row in keys: row[0]*=20
        for row in keys:
            scale=math.sqrt(statistics.mean(x*x for x in row))
            row[:]=[x/scale for x in row]
        k16=[[half(x) for x in row] for row in keys]
        k8=[q8(row) for row in keys]
        values=[[half(rng.gauss(0,1)) for _ in range(32)] for _ in keys]
        logits_error=[]; kl=[]; output_error=[]; matches=0
        for _ in range(16):
            query=[half(rng.gauss(0,1)) for _ in range(256)]
            logits=[[sum(x*y for x,y in zip(query,key))/16 for key in table] for table in (k16,k8)]
            weights=[]
            for scores in logits:
                unnormalized=[math.exp(x-max(scores)) for x in scores]
                weights.append([x/sum(unnormalized) for x in unnormalized])
            logits_error.extend((x-y)**2 for x,y in zip(*logits))
            kl.append(sum(p*math.log(p/q) for p,q in zip(*weights)))
            matches+=max(range(128),key=weights[0].__getitem__)==max(range(128),key=weights[1].__getitem__)
            outputs=[[sum(w*row[d] for w,row in zip(ws,values)) for d in range(32)] for ws in weights]
            output_error.append(math.sqrt(sum((a-z)**2 for a,z in zip(*outputs))/sum(x*x for x in outputs[0])))
        answer.append(dict(case='normalized_with_outliers' if outlier else 'normalized_normal',
                           qk_rmse=math.sqrt(statistics.mean(logits_error)),
                           attention_kl_mean=statistics.mean(kl),top_attention_agreement=matches/16,
                           output_relative_l2_mean=statistics.mean(output_error),
                           values_identical=True,source='Synthetic CPU arithmetic; V16 unchanged, D256, 128 keys, 16 queries.'))
    return answer


def parse_score(path,cases):
    lines=[line.split('\t') for line in path.read_text(encoding='utf-8').splitlines()]
    offset=0; answer={}
    for name,case in cases:
        ids=case['tokens']; cut=max(i for i in range(1,len(ids)-1) if ids[i]==248045)
        rows=[]
        for start,end in ((0,cut),(cut,len(ids)-1)):
            if end-start>1024: continue
            for position in range(start,end):
                assert offset<len(lines),'missing scores'
                row=lines[offset]; offset+=1
                assert len(row)>=8 and int(row[0])==position and int(row[1])==ids[position+1]
                lp=float(row[2]); assert math.isfinite(lp)
                if position>=case['prompt_tokens']-1:
                    rows.append(dict(position=position,target=int(row[1]),logprob=lp,top=int(row[3])))
        assert len(rows)==len(ids)-case['prompt_tokens']>0
        nll=-sum(r['logprob'] for r in rows)
        answer[name]=dict(targets=len(rows),nll=nll,ppl=math.exp(nll/len(rows)),rows=rows)
    assert offset==len(lines),'extra scores'
    return answer


def compare(a,z):
    total=sum(c['targets'] for c in a.values()); per_case={}
    for name,reference in a.items():
        candidate=z[name]
        assert [(r['position'],r['target']) for r in reference['rows']]==[(r['position'],r['target']) for r in candidate['rows']]
        delta=(candidate['nll']-reference['nll'])/reference['targets']
        per_case[name]=dict(raw_ppl_change_pct=100*math.expm1(delta),
                            top1_agreement=sum(x['top']==y['top'] for x,y in zip(reference['rows'],candidate['rows']))/reference['targets'])
    return dict(targets=total,cases=per_case,
                aggregate_raw_ppl_change_pct=100*math.expm1(sum(z[n]['nll']-a[n]['nll'] for n in a)/total))


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--self-check',action='store_true')
    parser.add_argument('--out',type=Path)
    args=parser.parse_args()
    corpus_path=b.ROOT/'bench/results/2026-10-15-a770-expert-prune-model-v3/corpus.json'
    corpus=read(corpus_path)['cases']
    cases=[(name,corpus[name]) for name in ('short','code','gcd','memory','prose')]
    if args.self_check:
        import tempfile
        assert q8([0.]*256)==[0.]*256
        assert q8([1.]*64)==[half(127*half(1/127))]*64
        lines=[f"{pos}\t{c['tokens'][pos+1]}\t-1\t{c['tokens'][pos+1]}\t0\t0\t0\t-1"
               for _,c in cases for a,z in ((0,max(i for i in range(1,len(c['tokens'])-1) if c['tokens'][i]==248045)),
                                            (max(i for i in range(1,len(c['tokens'])-1) if c['tokens'][i]==248045),len(c['tokens'])-1))
               if z-a<=1024 for pos in range(a,z)]
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'score.tsv'; path.write_text('\n'.join(lines)+'\n',encoding='utf-8')
            scores=parse_score(path,cases)
            assert sum(c['targets'] for c in scores.values())==368
            assert compare(scores,scores)['aggregate_raw_ppl_change_pct']==0
            path.write_text('\n'.join(lines[1:])+'\n',encoding='utf-8')
            try: parse_score(path,cases)
            except AssertionError: pass
            else: raise AssertionError('missing scores accepted')
        print('PASS Q8 stored-scale arithmetic, full gold-target coverage and raw PPL; no GPU')
        print(json.dumps(k_only()))
        return
    if not args.out: parser.error('new --out required')
    cfg=read(b.ROOT/'strata-iq2_xs.json'); assert cfg['backend']=='orca'
    out=args.out.resolve(); out.mkdir(parents=True,exist_ok=False)
    (out/Path(__file__).name).write_bytes(Path(__file__).read_bytes())
    env,selected=runtime_env(cfg['env'],cfg['lib_dirs'])
    model_args=cfg['args'].copy()
    for key,value in {'--expert-cache':'6000','--adapt-swaps':'0','--pcie-frac':'0','--prompt-cache':'6','--short-read':'1024'}.items():
        model_args[model_args.index(key)+1]=value
    model_args+=['--prompt-cache-root','0','--prompt-cache-every','0','--turn-token','248045',
                 '--tail-role-token','-1','--conversation-cache-mib','0']
    images=Path(env['STRATA_ORCA_IMAGES'])
    manifest=lambda:{p.relative_to(images).as_posix():b.sha(p) for p in images.rglob('*') if p.is_file()}
    files=[Path(__file__),Path(b.__file__),b.ROOT/'tools/check_orca_regression.py',Path(cfg['exe']),corpus_path]
    hashes={str(p.resolve()):b.sha(p) for p in files}
    d=dict(complete=False,production_changed=False,linux_gpu_validated=False,
           protocol='Native same EXE FP16/K8V8/K8V4/FP16. Fixed cache6297, no adaptation/PCIe share. '
                    'Five fixed model-generated continuations,368 gold targets. Code/Prose actual2041-token '
                    'batched prefill then logged continuation windows; raw full-distribution scores only. '
                    'K8V8 changes both K and V vs FP16; K8V4 vs K8V8 changes V with K8 fixed. '
                    'Separate K-only check is synthetic CPU attention with V16 unchanged. Limited quality screen.',
           config=cfg,args=model_args,runtime_env=selected,k_only=k_only(),corpus_sha256=b.sha(corpus_path),
           files_sha256=hashes,image_sha256=manifest(),runs=[])
    save=lambda:b.save(out/'results.json',d)
    save(); print('OUT',out,flush=True)
    try:
        with b.gpu_lock():
            for index,kv in enumerate(('fp16','int8','k8v4','fp16')):
                label=str(index)+'-'+kv
                kvargs=model_args+['--kv',kv]
                arm=dict(label=label,kv=kv,rows=[]); d['runs'].append(arm); save()
                logpos=out/(label+'.tsv'); runenv=env|{'STRATA_LOGPOS':str(logpos)}
                print('RUN',label,flush=True)
                engine=b.Engine(Path(cfg['exe']),kvargs,runenv,out/label); arm['info']=engine.info
                try:
                    for name,case in cases:
                        row=engine.gen(case['tokens'],1,keys='temperature=0'); row['case']=name
                        assert row['generated']==1 and row['resume']==row['reused']==0
                        arm['rows'].append(row); save(); print('SCORED',label,name,flush=True)
                finally:
                    engine.close(); arm['exit_code']=engine.p.returncode; save()
                arm['scores']=parse_score(logpos,cases)
                arm['scores_tsv']=logpos.read_bytes().decode('utf-8'); arm['scores_sha256']=b.sha(logpos); save()
            a,z=d['runs'][0]['scores'],d['runs'][-1]['scores']
            d['baseline_scores_repeated']=a==z
            d['comparisons']=[dict(baseline='fp16',candidate=r['kv'],**compare(a,r['scores'])) for r in d['runs'][1:3]]
            d['v4_only_at_fixed_k8']=compare(d['runs'][1]['scores'],d['runs'][2]['scores'])
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
