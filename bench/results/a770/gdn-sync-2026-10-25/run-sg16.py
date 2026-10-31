"""Mandatory production-SG16 confirmation, frozen before observing any SG16 timing."""
from pathlib import Path
import ctypes
import hashlib
import json

H=Path(__file__).resolve().parent
R=H.parent
# Reuse the exact frozen compiler, lock, command and comparison implementation.
code=(H/'run.py').read_text();exec(code[:code.index('\nwith b.gpu_lock():')],globals())
ctypes.windll.kernel32.GetCurrentProcess.restype=ctypes.c_void_p
ctypes.windll.kernel32.SetPriorityClass.argtypes=[ctypes.c_void_p,ctypes.c_ulong]
assert ctypes.windll.kernel32.SetPriorityClass(ctypes.windll.kernel32.GetCurrentProcess(),0x8000)
text=(H/'experiment.cpp').read_text()
needle='        _putenv_s("STRATA_GDN_SG16","0");'
assert text.count(needle)==1
(H/'sg16-experiment.cpp').write_text(text.replace(needle,'        _putenv_s("STRATA_GDN_SG16","1");'))
protocol=dict(rule='Additional mandatory check after discovering production config uses SG16: '
    'the final source and original reference must pass the identical per-process selection threshold '
    'in two fresh SG16 processes. Do not adopt if either fails. No SG16 timing was examined before this protocol.',
    parent_protocol_sha256=b.sha(H/'preparation.json'),
    files_sha256={str(p.relative_to(R)):b.sha(p) for p in [Path(__file__).resolve(),H/'sg16-experiment.cpp',
        H/'production.cpp',H/'reference.cpp',H/'production.obj',H/'reference.obj']})
b.save(H/'sg16-protocol.json',protocol)
with b.gpu_lock():
    assert all(b.sha(R/p)==sha for p,sha in protocol['files_sha256'].items())
    parent=json.loads((H/'results.json').read_text());assert parent['complete'] and parent['final']
    compile_obj('sg16-experiment',['-DGDN_FINAL_BUILD'])
    link('sg16-experiment',['sg16-experiment','production','reference'])
    out=call([str(H/'sg16-experiment.exe'),'--check','1'],'sg16-final-check');assert 'PASS checks=649 ' in out
    runs=[measure('sg16-experiment',1,'sg16-confirm-'+str(i)) for i in range(2)]
    assert all(b.sha(R/p)==sha for p,sha in protocol['files_sha256'].items())
    result=dict(complete=True,protocol=protocol,runs=runs,passed=all(r['passed'] for r in runs),
        binary_sha256=b.sha(H/'sg16-experiment.exe'),files_sha256={str(p.relative_to(R)):b.sha(p)
        for p in H.glob('sg16-*') if p.is_file() and p.name!='sg16-results.json'})
    b.save(H/'sg16-results.json',result)
print('DONE mandatory SG16 confirmation',result['passed'],flush=True)
