import pathlib,subprocess,json,hashlib,sys
F=pathlib.Path(__file__).resolve().parent;H=F.parent;sys.path.insert(0,str(H));import build
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
sampler=F/'sampler.obj'
for side in ['baseline','candidate']:
    B=H/side;O=F/side;O.mkdir(exist_ok=True)
    cmd=subprocess.check_output(['ninja','-C',str(B),'-t','commands','strata'],text=True).splitlines()[-1]
    overrides=[sampler]
    if side=='candidate':overrides.append(H/'candidate-final/verify.obj')
    cmd=cmd.replace('CMakeFiles\\strata.dir\\src\\program\\generate.cpp.obj','CMakeFiles\\strata.dir\\src\\program\\generate.cpp.obj '+' '.join(map(str,overrides)))
    cmd=cmd.replace('--intdir=CMakeFiles\\strata.dir','--intdir='+str(O))
    for flag,name in [('out','strata.exe'),('implib','strata.lib'),('pdb','strata.pdb')]:cmd=cmd.replace('/'+flag+':'+name,'/'+flag+':'+str(O/name))
    d={'complete':False,'method':'Same FP32-capable sampler.obj before unchanged archives on both sides; candidate also uses final verify.obj.',
       'sampler_obj_sha256':sha(sampler),'original_exe_sha256':sha(B/'strata.exe'),'command':cmd,
       'sources':json.loads((F/'build.json').read_text())['sources'],
       'source_manifest_sha256':sha(H/('source-manifest.json' if side=='baseline' else 'final-source-manifest.json'))}
    (O/'build.json').write_text(json.dumps(d,indent=2))
    print('link',side,flush=True)
    with (O/'link.log').open('w') as log:p=subprocess.run(cmd,cwd=B,env=build.environment(),stdout=log,stderr=subprocess.STDOUT)
    if p.returncode:print((O/'link.log').read_text(errors='replace')[-8000:],flush=True)
    p.check_returncode()
    d['complete']=True;d['exe_sha256']=sha(O/'strata.exe');(O/'build.json').write_text(json.dumps(d,indent=2))
    print('PASS link',side,flush=True)
