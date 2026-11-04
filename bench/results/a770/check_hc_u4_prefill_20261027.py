"""Recompute the isolated HC prefill fusion evidence without a GPU."""
from pathlib import Path
import hashlib
import json
import math
import statistics
import sys

R=Path(__file__).resolve().parents[3]
H=R/'build-sycl-a770-u4s4-hc-prefill-20261027'
REPORT=Path(__file__).with_name('hc-u4-prefill-2026-10-27.json')


def sha(p):
    with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()


def parse(text):
    rows={key:[] for key in ('ERROR','PAIR')}
    for line in text.splitlines():
        if line.startswith(('ERROR ','PAIR ')):
            key,s=line.split(' ',1);rows[key].append(json.loads(s))
    assert text.rstrip().splitlines()[-1].startswith('PASS HC prefill fused pack, CPU quantized corner, full output and guards checks=')
    assert len(rows['ERROR'])==42 and all(x['passed'] for x in rows['ERROR'])
    assert len(rows['PAIR'])==64
    cells=[]
    for t,v in sorted({(p['T'],p['variant']) for p in rows['PAIR']}):
        samples=[]
        for p in rows['PAIR']:
            if (p['T'],p['variant'])!=(t,v):continue
            a,c=math.sqrt(p['a0_us']*p['a1_us']),math.sqrt(p['b0_us']*p['b1_us'])
            drift=max(abs(p['a1_us']/p['a0_us']-1),abs(p['b1_us']/p['b0_us']-1))
            samples.append(dict(**p,baseline_us=a,candidate_us=c,gain_pct=100*(a/c-1),stable=drift<=.03))
        assert len(samples)==8 and {p['cycle'] for p in samples}==set(range(8))
        stable=[p for p in samples if p['stable']]
        gain=statistics.median(p['gain_pct'] for p in stable) if stable else None
        cells.append(dict(T=t,variant=v,stable_cycles=len(stable),gain_pct=gain,
            baseline_us=statistics.median(p['baseline_us'] for p in samples),candidate_us=statistics.median(p['candidate_us'] for p in samples),
            screen_passed=len(stable)>=6 and gain>=3,samples=samples))
    return dict(errors=rows['ERROR'],cells=cells)


def audit_model(d):
    assert d['complete'] and not d['adopted'] and not d['speed_acceptance']
    assert [r['label'] for r in d['runs']]==['A1','B1','A2']
    assert d['args'][d['args'].index('--expert-cache')+1]=='5500'
    for run in d['runs']:
        assert run['exit_code']==0 and run['expert_slots']==5771 and run['dispatched']==bool(run['active'])
        assert run['initializer']['tokens']==d['reference']['Short']
        assert [r['case'] for r in run['rows']]==['Code2K','Prose2K']
        for row in run['rows']:
            assert row['generated']==len(row['tokens'])==128 and row['prompt_tokens']==2048 and row['reused']==0
            assert row['all_zero']==(not any(row['tokens']))
            assert row['matches_reference']==(row['tokens']==d['reference'][row['case']])
    assert d['baseline_valid']==all(row['matches_reference'] and not row['all_zero'] for run in d['runs'] if not run['active'] for row in run['rows'])
    assert d['candidate_nonzero']==all(not row['all_zero'] for run in d['runs'] if run['active'] for row in run['rows'])


def main():
    if '--freeze' in sys.argv:
        assert not REPORT.exists()
        build=json.loads((H/'build.json').read_text());assert build['complete']
        d=dict(complete=True,adopted=False,production_changed=False,model_speed_accepted=False,
            scope='one-process HC-up original BF16 prefill fused SiLU/S4 pack screen, not independent confirmation',
            operator=parse((H/'check.stdout').read_text()),operator_stdout=(H/'check.stdout').read_text(),build=build,
            files_sha256={**build['files_sha256'],Path(__file__).relative_to(R).as_posix():sha(Path(__file__))})
        p=H/'model-v2/results.json'
        if p.exists():
            d['model']=json.loads(p.read_text());d['files_sha256'][p.relative_to(R).as_posix()]=sha(p)
            audit_model(d['model'])
            for p in (H/'model-v2').iterdir():
                if p.suffix in ('.stderr','.protocol'):d['files_sha256'][p.relative_to(R).as_posix()]=sha(p)
        for name in ('run_v2.py','cancelled-wait.json'):
            p=H/name;d['files_sha256'][p.relative_to(R).as_posix()]=sha(p)
        REPORT.write_text(json.dumps(d,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    d=json.loads(REPORT.read_text(encoding='utf8'))
    assert d['complete'] and not d['model_speed_accepted'] and not d['production_changed']
    assert parse(d['operator_stdout'])==d['operator']
    if 'model' in d:audit_model(d['model'])
    if '--local' in sys.argv:
        for relative,digest in d['files_sha256'].items():assert sha(R/relative)==digest,relative
    print('PASS 42 full-output accuracy records, 64 retained ABBA cycles and frozen evidence; no model speed acceptance')


if __name__=='__main__':main()
