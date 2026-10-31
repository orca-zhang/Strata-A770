"""Portable coverage, quality and paired timing audit for real model expert inputs."""
from pathlib import Path
import hashlib
import json
import runpy
import sys

R=Path(__file__).resolve().parents[3]
H=R/'build-sycl-a770-u4s4-expert-replay-20261027'
C=R/'build-sycl-a770-u4s4-expert-hot-inputs-20261027'
W=R/'build-sycl-a770-u4s4-expert-hot-weights-20261027'
REPORT=Path(__file__).with_name('u4-expert-replay-2026-10-27.json')
paired=runpy.run_path(str(Path(__file__).with_name('check_u4_expert_chain_20261027.py')))['summary']


def sha(path):
    with path.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()


def summarize(rows):
    return [dict(input=name,**p) for name in sorted({r['input'] for r in rows})
            for p in paired([r for r in rows if r['input']==name])]


def confirmed(d):
    if len(d['runs'])!=2:return []
    first={(p['input'],p['streamed']) for p in d['runs'][0]['summary'] if p['passed']}
    return [dict(input=p['input'],T=p['T'],streamed=p['streamed']) for p in d['runs'][1]['summary']
            if p['passed'] and (p['input'],p['streamed']) in first]


def audit(d):
    assert d['complete'] and not d['production_changed'] and not d['adopted'] and not d['target_achieved'] and not d['model_quality_tested']
    origin={p['name']:p for p in d['inputs']};assert len(origin)==len(d['inputs'])==6
    assert [r['label'] for r in d['runs']]==['screen','confirm']
    for run in d['runs']:
        assert run['exit_code']==0
        raw=run['raw'];errors=raw['ERROR'];pairs=raw['PAIR']
        assert len(errors)==6 and {p['input'] for p in errors}==set(origin)
        for p in errors:
            q=origin[p['input']]
            assert p['T']==q['rows'] and p['expert']==q['expert_index'] and p['case_index']==q['case_index']
            assert p['passed'] and 0<=p['u4_gu_nrmse']<=p['native_gu_nrmse']+1e-6 and 0<=p['u4_y_nrmse']<=p['native_y_nrmse']+1e-6
            assert 0<=p['cpu_gu_nrmse']<3e-6 and 0<=p['cpu_y_nrmse']<3e-6
        assert len(pairs)==96 and {(p['input'],p['streamed'],p['cycle']) for p in pairs}=={(name,s,c) for name in origin for s in (False,True) for c in range(8)}
        assert all(p['T']==origin[p['input']]['rows'] for p in pairs)
        assert run['summary']==summarize(pairs)
        assert len(raw['REFERENCE'])==len(raw['SUMMARY'])==1
        ref=raw['REFERENCE'][0]
        assert ref['original_bf16_fp16_rounding_nrmse']<1e-4 and ref['physical_banks']==24 and ref['native_bytes']==1510400 and ref['u4_bytes']==2764800
        assert raw['SUMMARY'][0]['quality_records']==6 and not raw['SUMMARY'][0]['model_quality_tested']
    assert d['confirmed_shapes']==confirmed(d)
    capture=d['capture']
    assert capture['complete'] and not capture['speed_acceptance'] and capture['exit_code']==0 and capture['expert_slots']==5771
    assert capture['initializer']['tokens']==capture['reference']['Short']
    assert [p['case'] for p in capture['rows']]==['Code2K','Prose2K']
    for row in capture['rows']:
        assert row['generated']==len(row['tokens'])==128 and row['prompt_tokens']==2048 and row['reused']==0
        assert row['tokens']==capture['reference'][row['case']] and {p['layer'] for p in row['inputs']}=={0,24,47}
        for p in row['inputs']:
            name=Path(p['file']).name;histogram=d['route_counts'][name]
            assert len(histogram)==512 and min(histogram)>=0 and sum(histogram)==20470
            assert p['nonzero']>0 and p['rows']==max(histogram)==histogram[p['expert']] and p['expert']==histogram.index(max(histogram))
            assert all(origin[name][key]==value for key,value in p.items())
    assert d['conversion']['complete'] and {(p['layer'],p['expert']) for p in d['conversion']['samples']}=={(p['layer'],p['expert']) for p in origin.values()}
    failed=d['original_capture_failure'];assert not failed['complete'] and failed['exit_code']!=0
    assert any(not any(row['tokens']) for row in failed['rows'])
    sparse=d['synchronized_sparse_capture'];assert not sparse['complete'] and sparse['exit_code']==0
    assert all(row['tokens']==sparse['reference'][row['case']] for row in sparse['rows'])
    assert {(p['layer'],p['expert']) for row in sparse['rows'] for p in row['inputs']}=={(0,0),(24,255)}


def main():
    if '--freeze' in sys.argv:
        assert not REPORT.exists()
        d=json.loads((H/'measure/results.json').read_text())
        d['capture']=json.loads((C/'model/results.json').read_text())
        d['conversion']=json.loads((W/'prepared.json').read_text())
        d['original_capture_failure']=json.loads((R/'build-sycl-a770-u4s4-expert-inputs-20261027/model/partial.json').read_text())
        d['synchronized_sparse_capture']=json.loads((R/'build-sycl-a770-u4s4-expert-inputs-sync-20261027/model/partial.json').read_text())
        text=(C/'model/capture.stderr').read_text()
        import re
        d['route_counts']={name:list(map(int,raw.split(','))) for name,raw in re.findall(r'^U4_EXPERT_COUNTS file=(\S+) total=20470 counts=([\d,]+)$',text,re.M)}
        d['confirmed_shapes']=confirmed(d)
        for p in (Path(__file__),Path(__file__).with_name('check_u4_expert_chain_20261027.py'),H/'measure/results.json'):
            d['files_sha256'][p.relative_to(R).as_posix()]=sha(p)
        audit(d);REPORT.write_text(json.dumps(d,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    d=json.loads(REPORT.read_text(encoding='utf8'));audit(d)
    if '--local' in sys.argv:
        for relative,digest in d['files_sha256'].items():assert sha(R/relative)==digest,relative
    print('PASS real expert-input provenance, full outputs, route counts, quality and two-process ABBA gates; no model gain acceptance')


if __name__=='__main__':main()
