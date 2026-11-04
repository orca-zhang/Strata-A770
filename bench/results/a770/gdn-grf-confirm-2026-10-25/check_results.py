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

assert not d['screen'] and not d['confirmation'] and len(d['final'])==4
assert {r['sg'] for r in d['final']}=={16,32}
assert d['adopt']==all(r['passed'] for r in d['final'])
assert 'PASS checks=649 ' in (H/'raw/long-check.stdout').read_text()
assert len(d['native'])==8
for record in d['native']:
    meta=(H/'native'/(Path(record['file']).stem+'.ze_info')).read_text()
    for key in ('grf_count','simd_size','slm_size'):
        assert int(re.search(r'^\s+'+key+r':\s+(\d+)',meta,re.M)[1])==record[key]
    spill=re.search(r'^\s+spill_size:\s+(\d+)',meta,re.M)
    assert (int(spill[1]) if spill else 0)==record['spill_size']
    assert record['grf_count']==(128 if record['baseline'] else 256)
    assert record['simd_size']==record['requested_sg']
parent=json.loads((H/'short-batch-results.json').read_text())
assert hashlib.sha256((H/'short-batch-results.json').read_bytes()).hexdigest()==d['parent_sha256']
assert parent['complete'] and not parent['adopt'] and any(not r['passed'] for r in parent['final'])
for name in ('production.obj','reference.obj'):
    suffix='/'+name
    old=[v for k,v in parent['artifacts_sha256'].items() if k.replace('\\','/').endswith(suffix)]
    new=[v for k,v in d['preparation']['files_sha256'].items() if k.replace('\\','/').endswith(suffix)]
    assert len(old)==len(new)==1 and old==new
assert '156 cases, 0 failures' in d['parent_regression']['window']
assert 'gdn_parity OK' in d['parent_regression']['selftest']
assert d['parent_regression']['layers'].count('42 cases, 0 failures')==4
assert hashlib.sha256((H/'production.cpp').read_bytes()).hexdigest()==d['production_sha256']
print(f'PASS: 649 recorded differential checks, {count} independent long-batch samples, '
      f'{eligible}/{len(drifts)} stable pairs, max drift {max(drifts):.6f}%; '
      f'unchanged source, frozen gates and native GRF verified; adopt={d["adopt"]}')
