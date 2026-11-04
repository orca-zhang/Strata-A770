"""Offline accounting for the actual BF16 GEMV entry-point experiment."""
from pathlib import Path
import hashlib
import json
import math
import statistics
import sys

R=Path(__file__).resolve().parents[3]
H=R/'build-sycl-a770-decode-gemv-exact-20261027'
FAILED=R/'build-sycl-a770-decode-gemv-model-20261027'
REPORT=Path(__file__).with_name('decode-bf16-vec8-2026-10-27.json')


def sha(p):
    with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()


def errors(text):
    rows=[json.loads(line[6:]) for line in text.splitlines() if line.startswith('ERROR ')]
    assert len(rows)==64 and text.rstrip().endswith('PASS public BF16 GEMV records=64 full-output/stride/tail/fallback/CPU64')
    assert {(p['T'],p['profile'],p['padded'],p['fallback']) for p in rows}=={(t,p,s,f) for t in range(1,9) for p in range(2) for s in range(2) for f in range(2)}
    assert all(0<=p['nrmse']<=2e-6 and 0<=p['cpu_nrmse']<=2e-6 for p in rows)
    assert all(p['nrmse']==0 for p in rows)
    return rows


def compare(runs):
    a,c,z=[statistics.median(p['decode_ms'] for p in run['rows'] if not p['warmup']) for run in runs]
    base=math.sqrt(a*z)
    return dict(baseline=base,candidate=c,throughput_change_pct=100*(base/c-1),baseline_drift_pct=100*(z/a-1))


def qualifies(result):
    return result['throughput_change_pct']>=3 and abs(result['baseline_drift_pct'])<=3


def audit_model(d):
    assert d['complete'] and not d['adopted'] and not d['production_changed'] and not d['target_achieved']
    assert [r['label'] for r in d['runs']]==(['A1','B1','A2','A3','B2','A4'] if d['passes_screen'] else ['A1','B1','A2'])
    for run in d['runs']:
        assert run['exit_code']==0 and run['expert_slots']==5771 and run['dispatched']==bool(run['active'])
        assert len(run['rows'])==7
        for i,row in enumerate(run['rows']):
            assert row['cycle']==i and row['warmup']==(i<2)
            assert row['tokens']==d['reference'] and row['matches_reference']
            assert row['generated']==16 and row['prompt_tokens']==len(d['input_ids']) and row['reused']==0
    assert d['comparison']==compare(d['runs'][:3])
    assert d['passes_screen']==qualifies(d['comparison'])
    assert d['confirmation']==(compare(d['runs'][3:]) if d['passes_screen'] else {})
    assert d['passes_confirmation']==(d['passes_screen'] and qualifies(d['confirmation']))


def main():
    if '--freeze' in sys.argv:
        assert not REPORT.exists()
        build=json.loads((H/'build.json').read_text());assert build['complete']
        model=json.loads((H/'model/results.json').read_text());audit_model(model)
        prior=R/'build-sycl-a770-decode-gemv-graph-20261027/results.json'
        old=json.loads(prior.read_text());inputs={}
        for case in old['cases']:
            relative=case['raw'];digest=sha(R/relative);assert digest==old['source_sha256'][relative]
            inputs[relative]=digest
        d=dict(complete=True,adopted=False,production_changed=False,target_achieved=False,
            scope='Actual BF16 GEMV entry-point integration retaining original FMA/reduction order; Short16 decode only. No IQ2/Q2 expert, fused HC, long-context, or prefill speed claim.',
            build=build,model=model,inputs_sha256=inputs,operator={},failed_order_changing=json.loads((FAILED/'model/partial.json').read_text()),
            files_sha256={**build['files_sha256'],**model['files_sha256'],**inputs,
                prior.relative_to(R).as_posix():sha(prior),Path(__file__).relative_to(R).as_posix():sha(Path(__file__)),
                (H/'model/results.json').relative_to(R).as_posix():sha(H/'model/results.json')})
        for p in list(FAILED.glob('*.stdout'))+list((FAILED/'model').iterdir()):
            if p.suffix in ('.json','.stdout','.stderr','.protocol'):d['files_sha256'][p.relative_to(R).as_posix()]=sha(p)
        for name in ('gate-up','down'):
            text=(H/(name+'.stdout')).read_text();d['operator'][name]=dict(stdout=text,errors=errors(text))
        REPORT.write_text(json.dumps(d,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    d=json.loads(REPORT.read_text(encoding='utf8'))
    for case in d['operator'].values():assert errors(case['stdout'])==case['errors']
    audit_model(d['model'])
    if '--local' in sys.argv:
        for relative,digest in d['files_sha256'].items():assert sha(R/relative)==digest,relative
    print('PASS 128 operator records and actual Short16 model outputs/dispatch/paired timing; goal remains unachieved')


if __name__=='__main__':main()
