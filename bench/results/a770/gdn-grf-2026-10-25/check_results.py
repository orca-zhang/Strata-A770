"""Recompute the frozen selection and verify archived raw samples; no GPU required."""
from pathlib import Path
import hashlib
import json
import math
import re
import statistics

H=Path(__file__).resolve().parent
d=json.loads((H/'results.json').read_text(encoding='utf-8'))
assert d['complete']
for p,digest in d['archive_sha256'].items():
    assert hashlib.sha256((H/p).read_bytes()).hexdigest()==digest,p
count=0;eligible=0;drifts=[];all_fingerprints={}
sg16=None
for run in d['screen']+d['confirmation']+d['final']+(sg16['runs'] if sg16 else []):
    rows=run['samples'];assert len(rows)==405
    raw=(H/'raw'/(run['label']+'.stdout')).read_text()
    parsed=[dict(T=int(t),begin=int(b),arm=int(a),index=int(i),us=float(v))
        for t,b,a,i,v in re.findall(r'sample T=(\d+) begin=(\d+) arm=(\d+) index=(\d+) us=([\d.]+)',raw)]
    assert parsed==rows
    fps=re.findall(r'fingerprint T=(\d+) begin=(\d+) arm=(\d+) hash=(\w+)',raw)
    assert [list(f) for f in fps]==run['fingerprints'] and len(fps)==45
    for shape in run['shapes']:
        T,begin=shape['T'],shape['begin']
        hashes={v[3] for v in fps if (int(v[0]),int(v[1]))==(T,begin)}
        assert len(hashes)==1
        assert all_fingerprints.setdefault((T,begin),hashes)==hashes
        med=[statistics.median(r['us'] for r in rows if (r['T'],r['begin'],r['arm'])==(T,begin,a)) for a in range(5)]
        assert med==shape['medians']
        ratios=[];stable=True
        for arm,pair in zip((1,3),shape['pairs']):
            base=math.sqrt(med[arm-1]*med[arm+1]);ratio=base/med[arm];drift=100*(med[arm+1]/med[arm-1]-1)
            assert abs(base-pair['baseline'])<1e-9 and pair['candidate']==med[arm]
            assert abs(100*(ratio-1)-pair['throughput_change_pct'])<1e-9
            assert abs(drift-pair['baseline_drift_pct'])<1e-9
            ratios.append(ratio);drifts.append(abs(drift));eligible+=abs(drift)<=3
            stable &= abs(drift)<=3
        gain=100*(math.exp(statistics.mean(math.log(v) for v in ratios))-1)
        assert abs(gain-shape['gain_pct'])<1e-9 and shape['eligible']==stable
    pool=100*(math.exp(statistics.mean(math.log1p(s['gain_pct']/100) for s in run['shapes'] if not s['begin']))-1)
    score=statistics.mean(math.log1p(s['gain_pct']/100) for s in run['shapes'])
    passed=all(s['eligible'] and s['gain_pct']>=(0 if s['begin'] else -1) for s in run['shapes']) and pool>=1
    assert abs(pool-run['no_prefix_gain_pct'])<1e-9 and abs(score-run['score'])<1e-12 and passed==run['passed']
    count+=len(rows)
candidates=[m for m in (1,2) if all(r['passed'] for r in d['screen'] if r['mode']==m)]
assert len(d['screen'])==4 and {r['sg'] for r in d['screen']}=={16,32}
if candidates:
    assert d['selected']==min(candidates)
    assert len(d['confirmation'])==4
else:
    assert not d['confirmation'] and not d['final'] and not d['adopt']
if d['final']:
    assert all(r['passed'] for r in d['confirmation']) and len(d['final'])==4
    assert d['adopt']==all(r['passed'] for r in d['final'])
    assert len(d['native'])==8
    for record in d['native']:
        meta=(H/'native-final'/Path(record['file']).stem/'.ze_info').read_text()
        for key in ('grf_count','simd_size','slm_size'):
            assert int(re.search(r'^\s+'+key+r':\s+(\d+)',meta,re.M)[1])==record[key]
        assert record['grf_count']==(128 if record['baseline'] else 256)
        assert record['simd_size']==record['requested_sg']
checks=0
for p in (H/'raw').glob('*check*.stdout'):
    m=re.search(r'PASS checks=(\d+)',p.read_text())
    if m:checks+=int(m[1])
assert checks==649*(2+bool(d['final']))
if d['final']:
    assert '156 cases, 0 failures' in d['regression']['window']
    assert 'gdn_parity OK' in d['regression']['selftest']
    assert d['regression']['layers'].count('42 cases, 0 failures')==4
print(f'PASS: {checks} recorded differential checks, {count} samples, {eligible}/{len(drifts)} stable pairs, '
      f'max drift {max(drifts):.6f}%; frozen selection and artifact hashes verified; adopt={d["adopt"]}')
