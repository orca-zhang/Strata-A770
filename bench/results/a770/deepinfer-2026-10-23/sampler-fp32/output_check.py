import pathlib,json,hashlib
F=pathlib.Path(__file__).resolve().parent;H=F.parent
paths=[H/'functional-q8-short-greedy/results.json',H/'functional-sampler-q8-short/results.json']
old,new=[json.loads(p.read_text()) for p in paths]
assert old['complete'] and new['complete']
ref=old['runs'][0]['rows']['Short']['tokens']
sampled=new['runs'][0]['functional']['sampled']['tokens']
checks=[]
for r in new['runs']:
    assert r['rows']['Short']['tokens']==ref
    assert r['functional']['sampled']['tokens']==sampled==r['functional']['sampled_repeat']['tokens']
    checks.append({'mode':r['mode'],'threshold':.5,'greedy_matches_original':True,'sampling_matches_fixed_seed_reference':True})
chain=H/'functional-sampler-chain-short/results.json'
complete=False
if chain.exists():
    d=json.loads(chain.read_text())
    if d['complete']:
        paths.append(chain);complete=True
        for r in d['runs']:
            assert r['rows']['Short']['tokens']==ref
            assert r['functional']['sampled']['tokens']==sampled==r['functional']['sampled_repeat']['tokens']
            checks.append({'mode':r['mode'],'threshold':0,'greedy_matches_original':True,'sampling_matches_fixed_seed_reference':True})
result={'complete':complete,'sources':{str(p.relative_to(H)):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths},
        'greedy_tokens':len(ref),'sampled_tokens':len(sampled),'checks':checks}
(F/'output-equivalence.json').write_text(json.dumps(result,indent=2))
print('PASS output equivalence',len(checks),'modes; threshold0 complete',complete)
