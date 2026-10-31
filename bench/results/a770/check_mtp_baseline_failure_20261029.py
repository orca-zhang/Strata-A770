"""Audit the closed MTP cohort's original-baseline failure without GPU or binary reads."""
import ast
import json
from pathlib import Path
import re

from check_gr_hot_failure_20261029 import check_protocol

R=Path(__file__).resolve().parents[3]
H=R/'build-sycl-a770-mtp-prefix-confirm-20261029'


def read(path):return json.loads(path.read_text('utf8'))


def check():
    state=read(H/'state.json');exit=read(H/'child-exit.json');speed=read(H/'performance/results.json')
    assert not state['complete'] and not state['adopted'] and not speed['complete']
    assert exit['pid']==12936 and exit['creation_ms']==1793276771763 and exit['exit_code']==1
    assert len(speed['runs'])==1
    run=speed['runs'][0]
    assert run['label']=='A1' and not run['candidate'] and run['exit_code']==0xc0000005
    assert run['exe']=='build-sycl-a770-down-device-counter-20261028/baseline/bin/strata-unified-all-operators.exe'
    assert run['expert_slots']==6297 and run['ready_free_mib']==1625
    assert len(run['rows'])==7
    expected=[(case,warm) for warm in (True,False,False,False) for case in ('Code2K','Prose2K')]
    assert [(r['case'],r['warmup']) for r in run['rows']]==expected[:-1]
    assert run['initializer']['tokens']==speed['reference']['Short']
    for row in run['rows']:
        assert row['tokens']==speed['reference'][row['case']] and row['generated']==128
        assert row['prompt_tokens']==2048 and row['finish']=='length' and row['reused']==row['resume']==0
    check_protocol(H/'performance/A1.protocol',[run['initializer'],*run['rows']])
    text=(H/'performance/A1.stderr').read_text('utf8')
    assert 'MTP_PREFIX_' not in text
    assert 'DOWN_SCAN_CPU layer=40 stage=down elements=52403200 nonfinite=24960000 samples=8' in text
    assert 'DOWN_SCAN_CPU layer=40 stage=H_after_down_reject elements=13100800 nonfinite=6240000 samples=8' in text
    assert 'bits=ffc00000 width=4' in text and 'bits=0000fe00 width=2' in text
    assert (H/'performance/A1.protocol').read_text('utf8').splitlines()[-1]=='ERR down retry scan rejected layer=40 stage=down gpu_count=24960000 cpu_nonfinite=24960000 reason=overflow'
    guard=read(H/'guard/compiler-guard-results.json')
    assert guard['complete'] and guard['passed'] and not guard['events']
    assert not (H/'quality-validation.json').exists()
    # Reuse only the pure telemetry parsers, without importing the collector and its binary manifests.
    source=R/'build-sycl-a770-cache6824-current-20261028/run.py'
    tree=ast.parse(source.read_text('utf8'));names={'memory_samples','memory_coverage'}
    functions=[node for node in tree.body if isinstance(node,ast.FunctionDef) and node.name in names]
    assert {node.name for node in functions}==names
    namespace=dict(re=re,H=H/'performance')
    exec(compile(ast.Module(body=functions,type_ignores=[]),str(source),'exec'),namespace)
    memory=namespace['memory_coverage'](speed);assert len(memory)==7
    samples=namespace['memory_samples']((H/'performance/monitor.stdout').read_text('utf8'))
    after=[s for s in samples if run['rows'][-1]['host_end_ns']<s['host_ns']<=speed['monitor_end_ns']]
    assert after
    result=dict(complete=True,baseline_failed=True,worker_exit_code=1,model_exit_code=0xc0000005,
                completed_long_requests=7,failed_case='Prose2K',failed_long_request_index=8,layer=40,
                down_nonfinite=24960000,h_nonfinite=6240000,candidate_launched=False,
                speed_comparison_available=False,quality_passed=False,compiler_guard_passed=True,
                completed_request_sample_min_free_mib=min(row['min_free_mib'] for row in memory),
                post_last_completed_request_samples=len(after),
                post_last_completed_request_sample_min_free_mib=min(s['free_bytes'] for s in after)/2**20,
                attribution='Original baseline itself failed; this cohort cannot measure the MTP candidate gain or regression')
    print(json.dumps(result,indent=2));return result


if __name__=='__main__':check()
