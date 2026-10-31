"""Whole deployed-version comparison, with a separate fixed-target quality gate.

Unlike the same-source backend experiment, generated token IDs and speculative
work may differ. Keep that distinction in every published interpretation.
"""
from pathlib import Path
import argparse
import json
import math
import os
import statistics
import sys
import time

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT/'tools'))
import bench_deepinfer as b
from check_orca_http import audit

read = lambda p: json.loads(p.read_text(encoding='utf-8'))


def fixed_scores(path, case, priming=0):
    """Require every logged position; score only the fixed assistant continuation."""
    ids = case['tokens']
    first = case['prompt_tokens'] - 1
    cut = max(i for i in range(1, first) if ids[i] == 248045)
    assert ids[cut:cut+3] == [248045, 74455, 198], 'missing assistant header'
    assert cut < first < len(ids)-1
    # ponytail: reuse the engine's existing turn split; long prompt chunks emit no scores.
    # Reject any coverage mismatch; a future score_from protocol can remove this 1024-token limit.
    assert 0 <= priming < first
    positions = list(range(priming))+[p for a,z in ((0,cut),(cut,len(ids)-1)) if z-a <= 1024 for p in range(a,z)]
    rows = [line.split('\t') for line in path.read_text(encoding='utf-8').splitlines()]
    assert len(rows) == len(positions), 'missing or extra quality scores'
    selected = []
    for index,(position,row) in enumerate(zip(positions, rows)):
        assert len(row) >= 8 and int(row[0]) == position, 'wrong quality position'
        assert int(row[1]) == ids[position+1], 'wrong quality target'
        raw,top,end = (float(row[k]) for k in (2,4,6))
        assert all(math.isfinite(v) and v <= 0 for v in (raw,top,end)), 'invalid quality probability'
        # The excluded token has no conditional probability. It is valid in the unscored prompt.
        if position < first and int(row[1]) == 248046: continue
        conditional = float(row[7])
        assert math.isfinite(conditional) and conditional <= 0, 'invalid conditional probability'
        assert raw <= conditional + 2e-9, 'invalid conditional probability'
        if index >= priming and position >= first:
            selected.append(dict(position=position, target=int(row[1]), raw_nll=-raw,
                                 conditional_nll=-conditional, end_probability=math.exp(end),
                                 end_penalty_nll=conditional-raw))
    assert [r['position'] for r in selected] == list(range(first,len(ids)-1)), 'missing assistant targets'
    return dict(targets=len(selected),priming_rows=priming,
                raw_mean_nll=statistics.mean(r['raw_nll'] for r in selected),
                conditional_mean_nll=statistics.mean(r['conditional_nll'] for r in selected),
                end_penalty_mean_nll=statistics.mean(r['end_penalty_nll'] for r in selected),
                mean_end_probability=statistics.mean(r['end_probability'] for r in selected),
                rows=selected, scores_tsv=path.read_bytes().decode('utf-8'), scores_sha256=b.sha(path))


def compare_quality(old, new):
    assert old and len(old) == len(new)
    assert len({a['case'] for a in old}) == len(old), 'duplicate quality case'
    assert [a['case'] for a in old] == [c['case'] for c in new], 'different quality cases'
    invalid = [dict(arm=label,case=c['case'],error=c['error'])
               for label,group in (('baseline',old),('candidate',new)) for c in group if not c.get('valid',True)]
    if invalid:
        return dict(passed=False,primary_metric='raw_ppl',targets=0,
                    expected_targets=sum(c.get('expected_targets',c['targets']) for c in old),
                    valid_targets_by_arm=[sum(c['targets'] for c in group if c.get('valid',True)) for group in (old,new)],
                    invalid_cases=invalid,interpretation='Invalid required scores: no aggregate PPL or quality pass. '
                    'Valid cases are retained for diagnosis; performance comparison is blocked.')
    checks = []
    for a,c in zip(old,new):
        assert a['targets'] == c['targets'] > 0
        if 'rows' in a or 'rows' in c:
            assert [(r['position'],r['target']) for r in a['rows']] == [(r['position'],r['target']) for r in c['rows']]
        raw_delta = c['raw_mean_nll']-a['raw_mean_nll']
        # Historical archives used mean_nll for conditional scores; new records name both metrics.
        conditional_delta = c.get('conditional_mean_nll',c.get('mean_nll'))-a.get('conditional_mean_nll',a.get('mean_nll'))
        assert all(math.isfinite(v) for v in (raw_delta,conditional_delta))
        checks.append(dict(case=a['case'],targets=a['targets'],raw_nll_delta=raw_delta,
                           conditional_nll_delta=conditional_delta,end_penalty_nll_delta=raw_delta-conditional_delta,
                           raw_ppl_change_pct=100*math.expm1(raw_delta),
                           conditional_ppl_change_pct=100*math.expm1(conditional_delta),
                           metric_direction_conflict=raw_delta*conditional_delta < 0))
    total = sum(r['targets'] for r in checks)
    raw = math.fsum(r['targets']*r['raw_nll_delta'] for r in checks)/total
    conditional = math.fsum(r['targets']*r['conditional_nll_delta'] for r in checks)/total
    return dict(cases=checks,targets=total,aggregate_raw_ppl_change_pct=100*math.expm1(raw),
                aggregate_conditional_ppl_change_pct=100*math.expm1(conditional),
                aggregate_raw_nll_delta=raw,aggregate_conditional_nll_delta=conditional,
                aggregate_end_penalty_nll_delta=raw-conditional,
                metric_direction_conflict=raw*conditional < 0,primary_metric='raw_ppl',
                interpretation='Full-distribution raw PPL is the gate (aggregate <= +1%, each case <= +3%). '
                               'Conditional PPL and end probability are diagnostics. A pass is a limited quality screen, not quality equivalence.',
                passed=raw <= math.log1p(.01) and all(r['raw_nll_delta'] <= math.log1p(.03) for r in checks))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline-config', type=Path, required=True, help='Saved previous SYCL profile, including its executable')
    parser.add_argument('--candidate-config', type=Path, required=True, help='Native Orca profile with the same model arguments')
    parser.add_argument('--out', type=Path, required=True, help='New output directory; existing results are never overwritten')
    parser.add_argument('--quality-only', action='store_true', help='Score fixed assistant continuations; do not run performance requests')
    parser.add_argument('--quality-cache-slots', type=int, default=3000, help='Fixed requested expert slots for quality only (3000 leaves room for 8K prefill on the tested A770)')
    parser.add_argument('--quality-corpus', type=Path, default=ROOT/'bench/results/a770/orca-assistant-corpus.json')
    args = parser.parse_args()
    if args.quality_cache_slots < 1: parser.error('--quality-cache-slots must be positive')
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    (out/Path(__file__).name).write_bytes(Path(__file__).read_bytes())
    paths = [args.baseline_config.resolve(), args.candidate_config.resolve()]
    cfgs = [read(p) for p in paths]
    assert [cfg['backend'] for cfg in cfgs] == ['sycl','orca']
    assert cfgs[0]['args'] == cfgs[1]['args']
    assert cfgs[0]['tokenizer'] == cfgs[1]['tokenizer']
    inputs = {} if args.quality_only else b.prompts(cfgs[0])
    corpus_path = args.quality_corpus.resolve()
    corpus = read(corpus_path)['cases']
    cases = ['Short','Code2K','Prose2K','Long8K']
    d = dict(complete=False, version_comparison_valid=False, same_token_backend_comparison=False,
             quality_equivalence_established=False,quality_evaluation_complete=False,
             linux_gpu_validated=False, scope='Previous deployed SYCL binary/profile versus current deployed Orca binary/profile, preserving actual settings. Generated token trajectories may differ; this is a whole-version comparison, not equal-token execution or isolated runtime overhead.',
             max_new=64, retained=3, warmup_per_case_per_arm=1, profiles=[dict(path=str(p),sha256=b.sha(p),config=cfg) for p,cfg in zip(paths,cfgs)],
             input_ids=inputs, quality=[], runs=[], comparisons=[], quality_only=args.quality_only,
             quality_corpus=dict(path=str(corpus_path),sha256=b.sha(corpus_path),cases=corpus),
             quality_rule='Fresh process per fixed assistant continuation, complete target coverage and finite probabilities; '
                          'full-distribution raw PPL increase <= 1% aggregate and <= 3% per case. Conditional PPL is diagnostic only. '
                          'Fixed expert cache, no adaptation/PCIe share/prefill borrowing; explicit turn split for scoring. '
                          'Diagnostic timings excluded from performance. This is limited quality coverage, not model-wide equivalence.',
             script_sha256=b.sha(Path(__file__)), helper_sha256=b.sha(ROOT/'tools/bench_deepinfer.py'))
    save = lambda: b.save(out/'results.json',d)
    save()
    print('OUT',out,flush=True)


    def environment(cfg):
        env = {k:v for k,v in os.environ.items() if not k.startswith(('STRATA_','SYCL_','UR_','ONEAPI_','ZES_'))}
        env.update(cfg.get('env',{}))
        env['PATH'] = os.pathsep.join(cfg.get('lib_dirs',[]))+os.pathsep+env.get('PATH','')
        return env


    def quality(which):
        cfg=cfgs[which]
        launch=cfg['args'].copy()
        for key,value in {'--expert-cache':str(args.quality_cache_slots),'--adapt-swaps':'0','--pcie-frac':'0','--prompt-cache':'6','--short-read':'1024',
                          '--prompt-cache-root':'0','--prompt-cache-every':'0','--turn-token':'248045',
                          '--tail-role-token':'-1','--conversation-cache-mib':'0'}.items():
            if key in launch: launch[launch.index(key)+1]=value
            else: launch.extend([key,value])
        if '--prefill-borrow' in launch: launch.remove('--prefill-borrow')
        arm=dict(backend=cfg['backend'],args=launch,cases=[])
        d['quality'].append(arm)
        save()
        for case,reference in corpus.items():
            ids=reference['tokens']
            name='quality-'+str(which)+'-'+case
            logpos=out/(name+'.tsv')
            env=environment(cfg)
            env['STRATA_LOGPOS']=str(logpos)
            env['STRATA_LOGPOS_EXTRA']='248046'
            print('QUALITY',which,case,flush=True)
            engine=b.Engine(Path(cfg['exe']),launch,env,out/name)
            try:
                generated=engine.gen(ids,1,keys='temperature=0')
            finally:
                engine.close()
            assert generated['generated']==1 and generated['resume']==generated['reused']==0
            info=dict(field.split('=',1) for field in engine.info.split()[1:])
            log=(out/name).with_suffix('.stderr').read_text(encoding='utf-8')
            priming=log.count('strata serve: priming Windows before the first batched prompt')
            assert priming in (0,1), 'unexpected prefill priming'
            priming=min(int(info['spec']),len(ids)-1) if priming else 0
            cache={key:info[key] for key in ('expert_slots','expert_cache_mib')}
            assert cache==d.setdefault('quality_cache',cache), 'quality cache capacity differs'
            item=dict(case=case,targets=0,expected_targets=len(ids)-reference['prompt_tokens'],info=engine.info,
                      generated=generated,exit_code=engine.p.returncode)
            try:
                item.update(fixed_scores(logpos,reference,priming),valid=True)
            except (AssertionError,ValueError,OSError) as error:
                item.update(valid=False,error=f'{type(error).__name__}: {error}',
                            scores_tsv=logpos.read_bytes().decode('utf-8') if logpos.exists() else None,
                            scores_sha256=b.sha(logpos) if logpos.exists() else None)
            arm['cases'].append(item)
            save()
            print('QUALITY DONE',which,case,'valid',item['valid'],'targets',item['targets'],flush=True)


    def timing(label,which):
        cfg=cfgs[which]
        arm=dict(label=label,backend=cfg['backend'],rows=[])
        d['runs'].append(arm)
        save()
        print('RUN',label,cfg['backend'],flush=True)
        engine=b.Engine(Path(cfg['exe']),cfg['args'],environment(cfg),out/label)
        try:
            arm['info']=engine.info
            if which and os.name == 'nt':arm['modules_startup']=audit(engine.p.pid)
            for iteration in range(4):
                for case in cases:
                    print('GEN',label,iteration,case,flush=True)
                    started=time.perf_counter()
                    row=engine.gen(inputs[case],64,keys='temperature=0')
                    row.update(case=case,iteration=iteration,retained=iteration>0,wall_ms=(time.perf_counter()-started)*1000)
                    arm['rows'].append(row)
                    save()
                    assert row['generated']>=2 and any(row['tokens']),(label,case,'invalid output')
                    if label!='A0':
                        expected=next(r for r in d['runs'][0]['rows'] if r['case']==case and r['iteration']==iteration)
                        row['same_output']=row['tokens']==expected['tokens']
                        row['same_draft_workload']=all(row[k]==expected[k] for k in ('accepted','offered','reused','resume','finish'))
                    print('DONE',label,iteration,case,'n',row['generated'],'decode_ms',row['decode_ms'],'prompt_ms',row['prompt_ms'],flush=True)
                    save()
            if which and os.name == 'nt':arm['modules_workload']=audit(engine.p.pid)
        finally:
            engine.close()
            arm['exit_code']=engine.p.returncode
            save()
        assert arm['exit_code']==0,label


    try:
        with b.gpu_lock():
            d['exe_sha256']=[b.sha(Path(cfg['exe'])) for cfg in cfgs]
            images=Path(cfgs[1]['env']['STRATA_ORCA_IMAGES'])
            image_hashes={p.relative_to(images).as_posix():b.sha(p) for p in images.rglob('*') if p.is_file()}
            d['image_sha256']=image_hashes
            save()
            quality(0)
            quality(1)
            old=d['quality'][0]['cases'];new=d['quality'][1]['cases']
            d['quality_comparison']=compare_quality(old,new)
            d['quality_evaluation_complete']=True
            save()
            print('QUALITY COMPARISON',json.dumps(d['quality_comparison']),flush=True)
            passed=d['quality_comparison']['passed']
            for label,which in (() if args.quality_only or not passed else (('A0',0),('B',1),('A1',0))):
                timing(label,which)
            for case in ([] if args.quality_only or not passed else cases):
                groups=[[r for r in arm['rows'] if r['case']==case and r['retained']] for arm in d['runs']]
                counts=[[r['generated'] for r in group] for group in groups]
                assert all(values==[64]*3 for values in counts),(case,'different output lengths',counts)
                result=dict(case=case,generated=64,metrics={})
                for metric in ('decode_ms','prompt_ms','ttft_ms','wall_ms'):
                    values=[statistics.median(r[metric] for r in group) for group in groups]
                    assert all(math.isfinite(v) and v>0 for v in values),(case,metric)
                    result['metrics'][metric]=dict(median_ms=values,**b.paired(*values))
                result['same_outputs']=all(r.get('same_output',True) for arm in d['runs'] for r in arm['rows'] if r['case']==case)
                result['same_draft_workloads']=all(r.get('same_draft_workload',True) for arm in d['runs'] for r in arm['rows'] if r['case']==case)
                result['drift_within_3pct']=all(abs(result['metrics'][m]['baseline_drift_pct'])<=3 for m in ('decode_ms','prompt_ms','wall_ms'))
                d['comparisons'].append(result)
                print('RESULT',case,json.dumps(result),flush=True)
            d['profiles_unchanged']=all(b.sha(p)==v['sha256'] for p,v in zip(paths,d['profiles']))
            d['exes_unchanged']=all(b.sha(Path(cfg['exe']))==v for cfg,v in zip(cfgs,d['exe_sha256']))
            d['images_unchanged']=image_hashes=={p.relative_to(images).as_posix():b.sha(p) for p in images.rglob('*') if p.is_file()}
            d['script_unchanged']=b.sha(Path(__file__))==d['script_sha256']
            d['helper_unchanged']=b.sha(ROOT/'tools/bench_deepinfer.py')==d['helper_sha256']
            d['corpus_unchanged']=b.sha(corpus_path)==d['quality_corpus']['sha256']
            assert all(v for k,v in d.items() if k.endswith('_unchanged'))
            d.update(complete=passed,version_comparison_valid=bool(d['comparisons']) and all(r['drift_within_3pct'] for r in d['comparisons']))
            assert passed,'raw PPL quality gate failed; no performance comparison'
    except BaseException as error:
        d['error']=f'{type(error).__name__}: {error}'
        raise
    finally:
        save()
    print('COMPLETE',out,flush=True)


if __name__ == '__main__':
    main()
