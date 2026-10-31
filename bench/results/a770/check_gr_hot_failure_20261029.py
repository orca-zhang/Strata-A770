"""Rebuild the closed GR hot-score failure from small raw logs; no GPU or binary hashing."""
import importlib.util
import json
from pathlib import Path
import re
import sys

R=Path(__file__).resolve().parents[3]
H=R/'build-sycl-a770-gr-hot-score-from-20261029'


def read(path):return json.loads(path.read_text('utf8'))


def check_protocol(path,rows):
    tokens=[];completed=0
    for line in path.read_text('utf8').splitlines():
        fields=line.split()
        if fields and fields[0]=='T':tokens.append(int(fields[1]))
        elif fields and fields[0]=='TB':
            assert int(fields[1])==len(fields)-2
            tokens.extend(map(int,fields[2:]))
        elif fields and fields[0]=='DONE':
            assert line==rows[completed]['done'] and tokens==rows[completed]['tokens']
            completed+=1;tokens=[]
    assert completed==len(rows) and not tokens


def check():
    state=read(H/'state.json');exit=read(H/'child-exit.json');data=read(H/'hot/partial.json')
    assert not state['complete'] and not state['adopted'] and state['error']=='RuntimeError: model protocol failed'
    assert exit['pid']==4312 and exit['creation_ms']==1793275036699 and exit['exit_code']==1
    assert not (H/'hot/results.json').exists() and not (R/'strata-iq2_xs-gr-fixed-index.json').exists()
    guard=read(H/'guard/compiler-guard-results.json')
    assert guard['complete'] and guard['passed'] and not guard['events']
    spec=importlib.util.spec_from_file_location('closed_gr_probability_parser',R/'build-sycl-a770-hc-q8-model-20261025/quality.py')
    q=importlib.util.module_from_spec(spec);spec.loader.exec_module(q)
    assert [r['label'] for r in data['runs']]==['A1','B1','B2']
    quality={}
    for run in data['runs']:
        label=run['label'];failed=label=='B2';cases=data['cases'][:5] if failed else data['cases']
        assert run['expert_slots']==6297 and run['free_mib']>=1024 and len(run['history'])==8
        assert [r['case'] for r in run['rows']]==[name for name,_ in cases]
        assert run['exit_code']==(0xc0000005 if failed else 0)
        check_protocol(H/'hot'/f'{label}.protocol',[run['initializer'],*run['history'],*run['rows']])
        parsed=q.parse(H/'hot'/f'{label}.logpos.tsv',[
            ('Initializer',dict(prompt_tokens=1,tokens=data['input_ids']['Short'])),*cases])
        del parsed['Initializer']
        assert sum(c['tokens'] for c in parsed.values())==(518 if failed else 758)
        for name,case in cases:
            assert [r['position'] for r in parsed[name]['rows']]==list(range(case['prompt_tokens']-1,len(case['tokens'])-1))
        quality[label]=parsed
    assert quality['A1']==quality['B1']
    assert quality['B2']=={name:quality['B1'][name] for name,_ in data['cases'][:5]}
    text=(H/'hot/B2.stderr').read_text('utf8')
    boundaries=[tuple(map(int,row)) for row in re.findall(r'^SCORE_FROM begin=(\d+) targets=(\d+)$',text,re.M)]
    assert boundaries==[(c['prompt_tokens'],len(c['tokens'])-c['prompt_tokens']) for _,c in data['cases'][:6]]
    assert 'DOWN_SCAN_REJECT layer=20 stage=down elements=52403200 gpu_count=10009600 guard=12345678 reason=overflow' in text
    assert 'DOWN_SCAN_CPU layer=20 stage=down elements=52403200 nonfinite=10009600 samples=8' in text
    assert 'DOWN_SCAN_CPU layer=20 stage=H_after_down_reject elements=13100800 nonfinite=2502400 samples=8' in text
    assert 'bits=ffc00000 width=4' in text and 'bits=0000fe00 width=2' in text
    assert (H/'hot/B2.protocol').read_text('utf8').splitlines()[-1]=='ERR down retry scan rejected layer=20 stage=down gpu_count=10009600 cpu_nonfinite=10009600 reason=overflow'
    result=dict(complete=True,quality_passed=False,adopted=False,worker_exit_code=1,model_exit_code=0xc0000005,
                completed_target_counts={label:sum(c['tokens'] for c in values.values()) for label,values in quality.items()},
                first_pair_exact=True,completed_B2_prefix_exact=True,failed_case='CurrentProse2K',
                failed_request_target_count=128,uncompleted_fixed_targets=240,layer=20,
                down_nonfinite=10009600,h_nonfinite=2502400,compiler_guard_passed=True,
                attribution='Observed in GR candidate B2; origin and GR-specific causality remain unproven')
    print(json.dumps(result,ensure_ascii=False,indent=2))
    return result


if __name__=='__main__':
    assert sys.argv[1:] in ([],['--write'])
    result=check()
    if sys.argv[1:]:
        with (H/'closed-failure-audit.json').open('x',encoding='utf8') as out:json.dump(result,out,indent=2);out.write('\n')
