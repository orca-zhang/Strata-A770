"""Audit all-layer native conversion and CPU-only HC precision; optional sidecar byte/hash check."""
from pathlib import Path
import argparse, hashlib, json, math

def analyze(d):
    assert d['cpu_only'] and not d['gpu_tested'] and not d['model_quality_tested'] and not d['engine_enabled']
    if 'compilation' in d:
        c=d['compilation']
        assert c['compiled'] and c['linked'] and not c['gpu_tested']
        assert c['files']['hc.exe']['bytes']==1203712
        assert c['files']['hc.exe']['sha256']=='d77d65e9068765f760c258d4cb3e96eba10faa5674b7105051c4eda56cbea6f6'
    native=d['native']
    assert native['complete'] and not native['weight_screen_passed']
    assert native['weight_screen_nrmse_limit']==.01 and len(native['files'])==194
    assert len({v['file'] for v in native['files']})==194
    assert sum(v['bytes'] for v in native['files'])==native['native_bytes']==724107328
    assert native['source_bytes']==1279262720
    weights=[(field,e) for v in native['files'] for field,e in v['errors'].items()]
    assert len(weights)==290
    assert {f:sum(k==f for k,e in weights) for f in ('down','up','inject')}==dict(down=97,up=97,inject=96)
    for field,e in weights:
        assert math.isfinite(e['nrmse']) and math.isfinite(e['max_abs'])
        assert e['weight_screen_passed']==(e['nrmse']<=.01)
    assert sum(not e['weight_screen_passed'] for field,e in weights)==79
    prefixes=d['prefixes'];assert len(prefixes)==len(set(prefixes))==97 and 'output_hc' in prefixes
    # Columns: prefix index, profile, apply, T, scope(0=up), mode(0=four), field(0=lo,1=mixed,2=inject), NRMSE, cosine.
    rows=d['precision'];assert len(rows)==27840
    expected={(ci,p,a,t,s,m,f) for ci,prefix in enumerate(prefixes) for p in range(3) for a in (0,1)
              for t in (1,4,6,8) for s in (0,1) for m in (0,1) for f in ((0,1) if prefix=='output_hc' else (0,1,2))}
    assert {tuple(v[:7]) for v in rows}==expected
    summary=[]
    for s in (0,1):
        for m in (0,1):
            cells=[v for v in rows if v[4:6]==[s,m]]
            failed=[]
            for v in cells:
                assert math.isfinite(v[7]) and math.isfinite(v[8])
                if not (v[7]<=(.1 if v[1]==2 else .05) and v[8]>=(.98 if v[1]==2 else .995)):
                    failed.append(v)
            if s==0:assert not failed
            else:assert len(failed)==(20 if m==0 else 77) and all(v[6]==2 for v in failed)
            summary.append(dict(scope=('up','both')[s],mode=('four-dpas','three-dpas')[m],
                                cells=len(cells),failed=len(failed),failed_pairs=len({v[0] for v in failed}),
                                max_nrmse=max(v[7] for v in cells),
                                max_normal_nrmse=max(v[7] for v in cells if v[1]<2)))
    assert summary==d['summary']
    assert len(d['bf16_inject'])==96 and sum(v['bytes'] for v in d['bf16_inject'])==7864320
    assert d['hybrid_native_bytes']==native['native_bytes']+7864320
    print('PASS 194 native files / 290 weights; 79 fail unchanged1% weight gate; 27,840 CPU precision cells; all lo/mixed pass, quantized inject fails; no GPU/model gain')

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--sidecars',type=Path,help='Optionally verify local native U4 and BF16 inject files')
    args=p.parse_args()
    d=json.loads(Path(__file__).with_name('u4s4-hc-model-cpu-2026-10-26.json').read_text(encoding='utf-8'))
    analyze(d)
    if args.sidecars:
        for v in d['native']['files']+d['bf16_inject']:
            raw=(args.sidecars/v['file']).read_bytes()
            assert len(raw)==v['bytes'] and hashlib.sha256(raw).hexdigest()==v['sha256'],v['file']
        print('PASS 290 on-disk native/BF16 sidecars, sizes and SHA256')

if __name__=='__main__':main()
