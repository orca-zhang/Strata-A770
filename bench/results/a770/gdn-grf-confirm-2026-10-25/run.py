"""Fresh longer-batch confirmation; never replace or pool the rejected short-batch trial."""
from pathlib import Path
import ctypes
import json
import shutil

H=Path(__file__).resolve().parent
R=H.parent
J=R/'build-sycl-a770-gdn-grf-20261025'
P=R/'build-sycl-a770-gdn-sync-20261025'
text=(J/'experiment.cpp').read_text();assert text.count('sweep(8)')==2
text=text.replace('sweep(8)','sweep(64)').replace('/(8*L)','/(64*L)')
(H/'experiment.cpp').write_text(text)
for name in ('production.obj','reference.obj'):
    shutil.copyfile(J/name,H/name)
shutil.copytree(J/'headers',H/'headers')
manifest=json.loads((J/'preparation.json').read_text())
manifest['rule']='Fresh validation of unchanged final source after the previous final SG16 T1 baseline drift failed. '
manifest['rule']+='Keep all previous results rejected and separate. Freeze before fresh timing: each subgroup gets two new processes, '
manifest['rule']+='A/B/A/B/A, three warmups and nine retained batches of64 sweeps of36 independent layer states per arm. '
manifest['rule']+='Unchanged selection gate: all9 shapes have both adjacent baseline drifts<=3%, each no-prefix gain>=-1%, '
manifest['rule']+='no-prefix geometric gain>=1%, each prefix gain>=0%; all four processes must pass. No refill or sample exclusion.'
manifest['files_sha256'].update({str(p.relative_to(R)):__import__('hashlib').sha256(p.read_bytes()).hexdigest()
    for p in [Path(__file__).resolve(),H/'experiment.cpp',H/'production.obj',H/'reference.obj',J/'production.cpp']})
manifest['files_sha256'].update({str(p.relative_to(R)):__import__('hashlib').sha256(p.read_bytes()).hexdigest()
    for p in (H/'headers').rglob('*') if p.is_file()})
(H/'preparation.json').write_text(json.dumps(manifest,indent=2))
code=(P/'run.py').read_text();code=code[:code.index('\nwith b.gpu_lock():')]
code=code.replace('def measure(exe,mode,label):','def measure(exe,mode,label,sg=32):')
code=code.replace("'--bench',str(mode)","'--bench'+str(sg),str(mode)")
exec(code,globals())
ctypes.windll.kernel32.GetCurrentProcess.restype=ctypes.c_void_p
ctypes.windll.kernel32.SetPriorityClass.argtypes=[ctypes.c_void_p,ctypes.c_ulong]
assert ctypes.windll.kernel32.SetPriorityClass(ctypes.windll.kernel32.GetCurrentProcess(),0x8000)
with b.gpu_lock():
    parent=json.loads((J/'results.json').read_text());assert parent['complete'] and not parent['adopt']
    assert len(parent['final'])==4 and any(not r['passed'] for r in parent['final'])
    d['parent_sha256']=b.sha(J/'results.json')
    compile_obj('experiment',['-DGDN_FINAL_BUILD']);link('experiment',['experiment','production','reference'])
    out=call([str(H/'experiment.exe'),'--check','1'],'long-check');assert 'PASS checks=649 ' in out
    for i in range(2):
        for sg in (16,32):
            run=measure('experiment',1,f'long-{i}-sg{sg}',sg);run['sg']=sg
            d['final'].append(run);b.save(H/'partial.json',d)
    d.update(complete=True,adopt=all(r['passed'] for r in d['final']),
        production_sha256=b.sha(J/'production.cpp'),parent_regression=parent['regression'])
    d['artifacts_sha256']={str(p.relative_to(R)):b.sha(p) for p in H.rglob('*') if p.is_file() and p.name not in ('results.json','partial.json')}
    b.save(H/'results.json',d)
print('DONE longer-batch independent confirmation adopt',d['adopt'],flush=True)
