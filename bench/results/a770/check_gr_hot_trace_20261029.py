"""Audit the closed GR 55-stage diagnostic, including its final stdout after the manifest snapshot."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import sys

R=Path(__file__).resolve().parents[3]
H=R/'build-sycl-a770-gr-hot-first-bad-20261029'
SCORE=R/'build-sycl-a770-gr-hot-score-from-20261029'


def read(path):return json.loads(path.read_text('utf8'))


def module(path):
    spec=importlib.util.spec_from_file_location('closed_gr_trace_'+path.stem,path)
    value=importlib.util.module_from_spec(spec);spec.loader.exec_module(value);return value


def queue_digest(raw,digest,reproduced):
    # The frozen worker hashes queue.stdout before its one final print. Verify that exact prefix and retain the closed digest.
    marker=b'GR_HOT_DIAGNOSTIC_COMPLETE reproduced '
    assert raw.count(marker)==1
    prefix,suffix=raw.split(marker)
    line=marker+str(bool(reproduced)).encode()+b' quality_passed False'
    assert marker+suffix in (line+b'\n',line+b'\r\n')
    assert hashlib.sha256(prefix).hexdigest()==digest
    return hashlib.sha256(raw).hexdigest()


def self_check():
    prefix=b'CASE A1 Short\r\n';digest=hashlib.sha256(prefix).hexdigest()
    for value in (False,True):
        for ending in (b'\n',b'\r\n'):
            raw=prefix+b'GR_HOT_DIAGNOSTIC_COMPLETE reproduced '+str(value).encode()+b' quality_passed False'+ending
            assert queue_digest(raw,digest,value)==hashlib.sha256(raw).hexdigest()
            for bad in (raw+b'x',b'x'+raw,raw.replace(b'quality_passed False',b'quality_passed True'),raw+raw):
                try:queue_digest(bad,digest,value)
                except AssertionError:pass
                else:raise AssertionError('changed prefix or nonexact final line accepted')
    print('PASS exact before-print digest,closed stdout digest,LF/CRLF and tampered prefix/suffix rejection; no GPU/binary reads')


def check():
    sys.path.insert(0,str(R/'tools'))
    import bench_deepinfer as b
    sys.path.insert(0,str(R/'build-sycl-a770-u4s4-grouped-20261027'))
    from lease import gpu_lock
    with gpu_lock():
        state=read(H/'state.json');launch=read(H/'child-launch.json');exit=read(H/'child-exit.json')
        assert state['complete'] and not state.get('error') and not state['quality_passed'] and not state['speed_measurement']
        assert launch['pid']==exit['pid']==5264 and launch['creation_ms']==exit['creation_ms']==1793280120125
        assert exit['exit_code']==0
        assert [r['label'] for r in state['runs']]==['A1','B1','B2','A2']
        assert len(state['commands'])==2 and all(c['exit_code']==0 for c in state['commands'])
        closed={}
        for name,digest in state['files_sha256'].items():
            path=R/Path(name)
            if path==H/'queue.stdout':closed[str(path)]=queue_digest(path.read_bytes(),digest,state['reproduced'])
            else:assert b.sha(path)==digest,name
        assert str(H/'queue.stdout') in closed
        guard=read(H/'guard/compiler-guard-results.json')
        assert guard['complete'] and guard['passed'] and not guard['events']
        for name,digest in guard['files_sha256'].items():assert b.sha(R/Path(name))==digest,name
        replay=module(H/'run.py');data=read(SCORE/'hot/partial.json')
        parser=module(R/'build-sycl-a770-hc-q8-model-20261025/quality.py')
        protocol=module(Path(__file__).with_name('check_gr_hot_failure_20261029.py')).check_protocol
        result=dict(complete=True,quality_passed=False,speed_measurement=False,reproduced=state['reproduced'],
                    queue_snapshot_prefix_verified=True,closed_log_sha256=closed,runs=[])
        for run in state['runs']:
            label=run['label'];rows=run['rows'];planned=list(replay.requests(data,run['candidate']))
            assert run['slots']==6297 and run['free_mib']>=1024 and len(rows)<=len(planned)
            protocol(H/f'{label}.protocol',rows)
            for actual,(name,ids,n,keys,expected) in zip(rows,planned):
                assert actual['case']==name and actual['generated']==n and actual['prompt_tokens']==len(ids)
                assert actual['finish']=='length' and actual['reused']==actual['resume']==0
                if expected:
                    for key in ('tokens','accepted','offered'):assert actual[key]==expected[key],(label,name,key)
            failed=run['nonfinite_reproduced']
            if failed:
                assert run.get('primary_error') and run['exit_code']!=0 and len(rows)<18
                assert run['active_case']==planned[len(rows)][0]
            else:assert run['exit_code']==0 and len(rows)==18 and not run.get('primary_error') and not run.get('close_error')
            text=(H/f'{label}.stderr').read_text('utf8')
            observed=[tuple(map(int,x)) for x in re.findall(r'^SCORE_FROM begin=(\d+) targets=(\d+)$',text,re.M)]
            requested=max(0,len(rows)-9)+int(failed and len(rows)>=9)
            assert observed==[(c['prompt_tokens'],len(c['tokens'])-c['prompt_tokens']) for _,c in data['cases'][:requested]]
            cases=[('Initializer',dict(prompt_tokens=1,tokens=data['input_ids']['Short'])),*data['cases'][:max(0,len(rows)-9)]]
            if rows:
                scores=parser.parse(H/f'{label}.logpos.tsv',cases);del scores['Initializer']
            else:
                assert not (H/f'{label}.logpos.tsv').read_text('utf8');scores={}
            for name,case in data['cases'][:max(0,len(rows)-9)]:
                assert [r['position'] for r in scores[name]['rows']]==list(range(case['prompt_tokens']-1,len(case['tokens'])-1))
            assert ('unwinding=1' in text)==failed and 'PREFILL_FAILURE_TRACE_ERROR' not in text
            trace=[line for line in text.splitlines() if line.startswith(
                ('PREFILL_FAILURE_TRACE','PREFILL_STAGE_MOE_BAD','PREFILL_EXPERT_BAD','DOWN_SCAN_REJECT'))]
            assert trace==run['failure_trace']
            result['runs'].append(dict(label=label,nonfinite_reproduced=failed,completed_requests=len(rows),
                completed_targets=sum(c['tokens'] for c in scores.values()),active_case=run['active_case'],failure_trace=trace))
        assert result['reproduced']==any(r['nonfinite_reproduced'] for r in result['runs'])
        print(json.dumps(result,indent=2));return result


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--self-check',action='store_true');args=parser.parse_args()
    if args.self_check:self_check()
    else:check()
