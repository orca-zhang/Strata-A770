"""Audit completed diagnostic records only; no GPU, compiler, weight/EXE reads or rehashes."""
import json
from pathlib import Path
import re

R=Path(__file__).resolve().parents[3]
H=R/'build-sycl-a770-prefill-first-bad-20261029'


def read(path):return json.loads(path.read_text('utf8'))


def outputs(text):
    result=[];tokens=[]
    for line in text.splitlines():
        if line.startswith('T '):tokens.append(int(line[2:]))
        elif line.startswith('TB '):
            values=list(map(int,line.split()[1:]));assert values[0]==len(values)-1;tokens+=values[1:]
        elif line.startswith('DONE '):result.append(tokens);tokens=[]
    assert not tokens,'partial generated output must remain a failure'
    return result


def main():
    assert outputs('TB 2 7 8\nDONE 2')==[[7,8]]
    for bad in ('T 7','TB 2 7\nDONE 1'):
        try:outputs(bad)
        except AssertionError:pass
        else:raise AssertionError('malformed/partial protocol accepted')
    state=read(H/'state.json');assert state['complete'] and state['reproduced']
    assert not state['production_changed'] and not state['speed_acceptance']
    assert read(H/'child-exit.json')['exit_code']==0
    guard=read(H/'guard/compiler-guard-results.json');assert guard['complete'] and guard['passed'] and not guard['events']
    reference=read(R/'build-sycl-a770-value-fp16-after-desktop-20261029/performance/results.json')
    b1=next(r for r in reference['runs'] if r['label']=='B1');expected=[b1['initializer'],*b1['rows']]
    assert [r['label'] for r in state['runs']]==['D1','D2']
    for run,n in zip(state['runs'],(9,5)):
        assert run['slots']==6297 and run['free_mib']>=1024 and len(run['rows'])==n
        assert outputs((H/(run['label']+'.protocol')).read_text('utf8'))==[r['tokens'] for r in run['rows']]
        for row,prior in zip(run['rows'],expected):
            for field in ('tokens','accepted','offered','generated','prompt_tokens','reused','finish'):assert row[field]==prior[field]
    d1,d2=state['runs'];assert d1['exit_code']==0 and not d1.get('primary_error')
    assert d2['exit_code']==0xc0000005 and 'layer=27' in d2['primary_error'] and 'engine exit' in d2['close_error']
    trace=lambda s:re.findall(r'^PREFILL_FAILURE_TRACE tokens=2047 chunks=1 stage_samples=(\d+) bad_stages=(\d+) expert_samples=(\d+) bad_experts=(\d+) unwinding=(\d+)$',s,re.M)
    a=(H/'D1.stderr').read_text('utf8');b=(H/'D2.stderr').read_text('utf8')
    assert len(trace(a))==8 and all(r[1]==r[3]==r[4]=='0' for r in trace(a))
    assert len(trace(b))==5 and all(r[1]==r[3]==r[4]=='0' for r in trace(b)[:-1])
    assert trace(b)[-1]==('1262','20','30435','30','1')
    stages=re.findall(r'^PREFILL_STAGE_MOE_BAD chunk=(\d+) layer=(\d+) source=(\S+) nonfinite=(\d+)$',b,re.M)
    assert len(stages)==20 and all(c=='0' and l=='27' for c,l,_,_ in stages)
    assert stages[0]==('0','27','projection_attn','40960')
    experts=re.findall(r'^PREFILL_EXPERT_BAD chunk=0 layer=27 expert=(\d+) source=(\S+) nonfinite=(\d+)$',b,re.M)
    assert experts==[(str(e),name,str(count)) for e in range(10) for name,count in
                     (('GU_before_swiglu',20480),('H_before_down',10240),('D_after_down',40960))]
    assert 'gpu_count=409600 cpu_nonfinite=409600' in b and 'PREFILL_FAILURE_TRACE_ERROR' not in b
    print('PASS all completed outputs,retained real failure,compiler guard and earlier attention-projection nonfinite evidence; no speed/adoption claim')


if __name__=='__main__':main()
