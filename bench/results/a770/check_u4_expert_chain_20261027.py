"""Independent arithmetic and coverage audit for the complete expert-chain experiment."""
from pathlib import Path
import hashlib
import json
import math
import statistics
import sys

R=Path(__file__).resolve().parents[3]
H=R/'build-sycl-a770-u4s4-expert-chain-20261027'
REPORT=Path(__file__).with_name('u4-expert-chain-2026-10-27.json')


def sha(p):
    with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()


def summary(rows):
    result=[]
    for t,streamed in sorted({(r['T'],r['streamed']) for r in rows}):
        cycles=[]
        for r in rows:
            if (r['T'],r['streamed'])!=(t,streamed):continue
            baseline=math.sqrt(r['a0_us']*r['a1_us']);candidate=math.sqrt(r['b0_us']*r['b1_us'])
            da=100*(r['a1_us']/r['a0_us']-1);dc=100*(r['b1_us']/r['b0_us']-1)
            cycles.append(dict(**r,baseline_us=baseline,candidate_us=candidate,gain_pct=100*(baseline/candidate-1),baseline_drift_pct=da,candidate_drift_pct=dc,stable=max(abs(da),abs(dc))<=3))
        assert len(cycles)==8 and [p['cycle'] for p in cycles]==list(range(8))
        stable=[p for p in cycles if p['stable']]
        gain=statistics.median(p['gain_pct'] for p in stable) if stable else None
        result.append(dict(T=t,streamed=streamed,cycles=cycles,stable_cycles=len(stable),gain_pct=gain,
                           baseline_us=statistics.median(p['baseline_us'] for p in cycles),candidate_us=statistics.median(p['candidate_us'] for p in cycles),passed=len(stable)>=6 and gain>=10))
    return result


def confirmed(d):
    if len(d['runs'])!=2:return []
    first={(p['T'],p['streamed']) for p in d['runs'][0]['summary'] if p['passed']}
    return [dict(T=p['T'],streamed=p['streamed']) for p in d['runs'][1]['summary']
            if p['passed'] and (p['T'],p['streamed']) in first]


def audit(d):
    assert d['complete'] and not d['production_changed'] and not d['adopted'] and not d['target_achieved'] and not d['model_quality_tested']
    assert [r['label'] for r in d['runs']]==(['screen','confirm'] if any(p['passed'] for p in d['runs'][0]['summary']) else ['screen'])
    shapes={1,7,8,9,16,40,64,128,256}
    for run in d['runs']:
        assert run['exit_code']==0
        raw=run['raw'];errors=raw['ERROR']
        assert len(errors)==54 and {(p['expert'],p['profile'],p['T']) for p in errors}=={(e,p,t) for e in range(3) for p in range(2) for t in shapes}
        for p in errors:
            assert p['passed'] and p['u4_gu_nrmse']<=p['native_gu_nrmse']+1e-6 and p['u4_y_nrmse']<=p['native_y_nrmse']+1e-6
            assert 0<=p['cpu_gu_nrmse']<3e-6 and 0<=p['cpu_y_nrmse']<3e-6
        assert len(raw['REFERENCE'])==len(raw['SUMMARY'])==1
        ref=raw['REFERENCE'][0]
        assert ref['original_bf16_fp16_rounding_nrmse']<1e-4 and ref['native_bytes']==1510400 and ref['u4_bytes']==2764800 and ref['physical_banks']==24
        assert raw['SUMMARY'][0]['quality_records']==54 and not raw['SUMMARY'][0]['model_quality_tested']
        assert len(raw['PAIR'])==144 and {(p['T'],p['streamed']) for p in raw['PAIR']}=={(t,s) for t in shapes for s in (False,True)}
        assert summary(raw['PAIR'])==run['summary']
    if 'confirmed_shapes' in d:assert d['confirmed_shapes']==confirmed(d)


def main():
    if '--freeze' in sys.argv:
        assert not REPORT.exists()
        d=json.loads((H/'measure/results.json').read_text());audit(d)
        d['prepared']=json.loads((H/'prepared.json').read_text())
        d['confirmed_shapes']=confirmed(d)
        d['files_sha256'].update({Path(__file__).relative_to(R).as_posix():sha(Path(__file__)),
                                 (H/'measure/results.json').relative_to(R).as_posix():sha(H/'measure/results.json')})
        REPORT.write_text(json.dumps(d,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    d=json.loads(REPORT.read_text(encoding='utf8'));audit(d)
    if '--local' in sys.argv:
        for relative,digest in d['files_sha256'].items():assert sha(R/relative)==digest,relative
    print('PASS expert-chain full-output/quality coverage, resident/streamed ABBA arithmetic and hashes; full-model goal remains unachieved')


if __name__=='__main__':main()
