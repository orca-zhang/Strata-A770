from pathlib import Path
import ctypes
import json
import shutil

H=Path(__file__).resolve().parent
R=H.parent
P=R/'build-sycl-a770-gdn-sync-20261025'
code=(P/'run.py').read_text();code=code[:code.index('\nwith b.gpu_lock():')]
code=code.replace('def measure(exe,mode,label):','def measure(exe,mode,label,sg=32):')
code=code.replace("'--bench',str(mode)","'--bench'+str(sg),str(mode)")
exec(code,globals())
ctypes.windll.kernel32.GetCurrentProcess.restype=ctypes.c_void_p
ctypes.windll.kernel32.SetPriorityClass.argtypes=[ctypes.c_void_p,ctypes.c_ulong]
assert ctypes.windll.kernel32.SetPriorityClass(ctypes.windll.kernel32.GetCurrentProcess(),0x8000)
d.update(scope='dual-subgroup-GRF256',source_sha256=b.sha(H/'before.cpp'))

def both(exe,mode,label):
    results=[]
    for sg in (16,32):
        v=measure(exe,mode,label+'-sg'+str(sg),sg);v['sg']=sg;results.append(v)
    return results

with b.gpu_lock():
    for name in ('reference','grf-only','replay-grf','experiment'):compile_obj(name)
    link('experiment',['experiment','reference','grf-only','replay-grf'])
    for mode,name in manifest['variants'].items():
        mode=int(mode)
        out=call([str(H/'experiment.exe'),'--check',str(mode)],'check-'+name);assert 'PASS checks=649 ' in out
        d['screen']+=both('experiment',mode,'screen-'+name);b.save(H/'partial.json',d)
    candidates=[m for m in (1,2) if all(v['passed'] for v in d['screen'] if v['mode']==m)]
    if candidates:
        d['selected']=winner=min(candidates)
        for i in range(2):d['confirmation']+=both('experiment',winner,'confirm-'+str(i));b.save(H/'partial.json',d)
        if all(v['passed'] for v in d['confirmation']):
            source=H/('grf-only-production.cpp' if winner==1 else 'replay-grf-production.cpp')
            shutil.copyfile(source,H/'production.cpp');shutil.copyfile(H/'experiment.cpp',H/'final-experiment.cpp')
            compile_obj('production');compile_obj('final-experiment',['-DGDN_FINAL_BUILD'])
            link('final-experiment',['final-experiment','production','reference'])
            out=call([str(H/'final-experiment.exe'),'--check','1'],'final-check');assert 'PASS checks=649 ' in out
            for i in range(2):d['final']+=both('final-experiment',1,'final-confirm-'+str(i));b.save(H/'partial.json',d)
            shutil.copyfile(P/'gdn-parity.obj',H/'gdn-parity.obj')
            prefill=[str(R/'build-sycl-deepinfer-20261023/baseline/CMakeFiles/strata_prefill.dir/src/prefill/kernels.dp.cpp.obj'),
                str(base/'strata_prefill.lib'),'C:/Program Files (x86)/Intel/oneAPI/mkl/2025.3/lib/mkl_sycl_blas_dll.lib']
            d['link_inputs'].update({str(Path(p).relative_to(R)) if Path(p).is_relative_to(R) else p:b.sha(p) for p in prefill})
            link('gdn-parity',['gdn-parity','production'],prefill)
            d['regression']={name:call([str(H/'gdn-parity.exe'),*args],'regression-'+name) for name,args in
                [('window',['--window-check']),('selftest',['--selftest','--prefill-tokens','7']),('layers',['--commit-layers-check'])]}
            d['adopt']=all(v['passed'] for v in d['final'])
            native_audit(['production','reference'],'final')
    verify();d['complete']=True
    d['artifacts_sha256']={str(p.relative_to(R)):b.sha(p) for p in H.rglob('*') if p.is_file() and p.name not in ('results.json','partial.json')}
    b.save(H/'results.json',d)
print('DONE dual-subgroup GRF256 adopt',d['adopt'],flush=True)
