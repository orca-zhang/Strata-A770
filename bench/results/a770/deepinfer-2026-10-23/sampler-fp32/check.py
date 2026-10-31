import pathlib,json,sys,subprocess,time
F=pathlib.Path(__file__).resolve().parent;H=F.parent;R=H.parent
sys.path.insert(0,str(R/'tools'));import bench_deepinfer as h
cfg=json.loads((H/'config-window-prompt.json').read_text(encoding='utf-8-sig'))
out=F/'checks';out.mkdir(exist_ok=False)
results=[]
try:
    with h.gpu_lock(allow_compiler=True):
        for name,old,block in [('split','0','0'),('one-block','0','1'),('old','1','0')]:
            env=h.environment(cfg);env['STRATA_OLD_SAMPLER']=old;env['STRATA_SAMPLER_ONE_BLOCK']=block
            started=time.perf_counter()
            with (out/(name+'.log')).open('w',encoding='utf-8') as log:
                p=subprocess.run([str(F/'sampler_parity.exe'),'--selftest'],cwd=R,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=900)
            results.append({'path':name,'returncode':p.returncode,'elapsed_seconds':time.perf_counter()-started})
            h.save(out/'results.json',{'exe_sha256':h.sha(F/'sampler_parity.exe'),'results':results})
            print(name,p.returncode,flush=True)
            print((out/(name+'.log')).read_text(errors='replace')[-3000:],flush=True)
            p.check_returncode()
except Exception as e:
    h.save(out/'failure.json',{'error':str(e)});raise
print('PASS sampler checks',flush=True)
