import pathlib,json,subprocess,sys,os
F=pathlib.Path(__file__).resolve().parent;H=F.parent;R=H.parent
sys.path.insert(0,str(H));import build
sys.path.insert(0,str(R/'tools'));import bench_deepinfer as h
d=json.loads((F/'cpu-direct-build.json').read_text())
for i,cmd in enumerate(d['commands']):
    with (F/('cpu-direct-build-'+str(i)+'.log')).open('w') as log:p=subprocess.run(cmd,cwd=H/'candidate',env=build.environment(),stdout=log,stderr=subprocess.STDOUT)
    if p.returncode:print((F/('cpu-direct-build-'+str(i)+'.log')).read_text(errors='replace')[-5000:],flush=True)
    p.check_returncode()
results=[]
cfg=json.loads((H/'config-window-prompt.json').read_text(encoding='utf-8-sig'))
for name,old,block in [('one-block','0','1'),('old','1','0')]:
    env=h.environment(cfg);env.update(ONEAPI_DEVICE_SELECTOR='opencl:cpu',STRATA_OLD_SAMPLER=old,STRATA_SAMPLER_ONE_BLOCK=block)
    with (F/('cpu-direct-'+name+'.log')).open('w') as log:p=subprocess.run([str(F/'cpu_direct.exe')],cwd=R,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=180)
    print(name,p.returncode,(F/('cpu-direct-'+name+'.log')).read_text(errors='replace')[-1800:],flush=True)
    results.append({'path':name,'returncode':p.returncode})
    h.save(F/'cpu-direct-results.json',{'exe_sha256':h.sha(F/'cpu_direct.exe'),'results':results})
    p.check_returncode()
