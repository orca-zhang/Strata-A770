import json,pathlib,subprocess,sys
H=pathlib.Path(__file__).resolve().parent.parent;sys.path.insert(0,str(H));import build
F=H/'sampler-fp32';B=H/'candidate'
d=json.loads((F/'build.json').read_text())
for i,cmd in enumerate(d['commands']):
    print('build step',i,flush=True)
    with (F/('build-'+str(i)+'.log')).open('w') as log:p=subprocess.run(cmd,cwd=B,env=build.environment(),stdout=log,stderr=subprocess.STDOUT)
    if p.returncode:print((F/('build-'+str(i)+'.log')).read_text(errors='replace')[-8000:],flush=True)
    p.check_returncode()
d['complete']=True
import hashlib
d['artifacts']={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in F.iterdir() if p.suffix in ('.obj','.exe')}
(F/'build.json').write_text(json.dumps(d,indent=2))
print('PASS sampler isolated build',flush=True)
