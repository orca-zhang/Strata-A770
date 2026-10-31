import pathlib,json,time,subprocess,sys
F=pathlib.Path(__file__).resolve().parent;H=F.parent;R=H.parent
print('WAIT sampler parity and both isolated model links',flush=True)
while True:
    if (F/'checks/failure.json').exists():raise RuntimeError('Sampler parity failed; integration not run')
    checks=json.loads((F/'checks/results.json').read_text()) if (F/'checks/results.json').exists() else {}
    ready=len(checks.get('results',[]))==3 and all(x['returncode']==0 for x in checks['results'])
    for side in ['baseline','candidate']:
        try:ready=ready and json.loads((F/side/'build.json').read_text())['complete']
        except (FileNotFoundError,json.JSONDecodeError):ready=False
    if ready:break
    time.sleep(3)
base=[sys.executable,'-X','utf8',str(R/'tools/bench_deepinfer.py'),'--build-root',str(F),
      '--phase','functional','--config',str(H/'config-window-prompt.json'),'--kv','int8',
      '--cases','Short','--max-tokens','48','--expert-cache','5500']
for name,modes,threshold in [('functional-sampler-q8-short','baseline,batch,cost,fused,combined','0.5'),
                              ('functional-sampler-chain-short','baseline,chain,combined','0')]:
    args=base+['--modes',modes,'--threshold',threshold,'--out',str(H/name)]
    (F/(name+'-command.json')).write_text(json.dumps(args,indent=2))
    print('RUN',name,flush=True)
    subprocess.run(args,cwd=R,check=True)
print('PASS sampler model integration',flush=True)
