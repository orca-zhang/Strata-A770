"""Keep our GPU reservation while replacing an unnecessarily slow model baseline."""
import contextlib
import datetime
import json
import os
import runpy
import subprocess
import sys
import time
from pathlib import Path
from unittest.mock import patch

H = Path(__file__).resolve().parent
R = H.parent
sys.path.insert(0,str(R/'tools'))
import bench_deepinfer as b

F = H/'prefill-widen-f16-stride'
B = F/'build'
M = F/'model-checks'
lock = R/'gpu-experiment.lock'
owner = f'{os.getpid()} deepinfer-resume'
# Both IDs are checked against their command lines and parent relationship before stopping our engine.
old_pid,engine_pid = 17876,13632
query = 'Get-CimInstance Win32_Process | Select-Object ProcessId,ParentProcessId,CommandLine | ConvertTo-Json'
processes = json.loads(subprocess.check_output(['powershell','-NoProfile','-Command',query],text=True))
old = next(p for p in processes if p['ProcessId']==old_pid)
engine = next(p for p in processes if p['ProcessId']==engine_pid)
assert 'check_f16_stride.py' in old['CommandLine'] and str(F.relative_to(R)) in old['CommandLine'].replace('/', '\\')
assert engine['ParentProcessId']==old_pid and str(B/'model/strata.exe') in engine['CommandLine']
M.mkdir(exist_ok=False)
(M/Path(__file__).name).write_bytes(Path(__file__).read_bytes())
data = {'complete':False,'functional_only':True,'interrupted_baseline_pid':engine_pid,
        'interruption_reason':'FP16-off 256-token batch baseline is redundant with frozen reference; use Short off/on plus frozen Code/Prose output references',
        'interruption_time':datetime.datetime.now().isoformat(timespec='seconds'),'steps':[]}
b.save(M/'results.json',data)
# ponytail: transfer our existing reservation before stopping our child; other sessions keep waiting on the same file.
with lock.open('r+') as f:
    assert f.read()==f'{old_pid} deepinfer'
    f.seek(0); f.write(owner); f.truncate()
try:
    subprocess.run(['powershell','-NoProfile','-Command',f'Stop-Process -Id {engine_pid} -ErrorAction Stop'],check=True)
    for _ in range(30):
        try:
            b.busy()
            break
        except RuntimeError:
            time.sleep(1)
    else:
        raise RuntimeError('Our engine did not exit')
    with patch.object(b,'gpu_lock',contextlib.nullcontext):
        def run(out,dtype,switches,case,length=0,reference=None):
            sys.argv = [str(H/'test_prefill_widen.py'),'--build-root',str(B),'--phase','model','--dtype',dtype,
                        '--switches',switches,'--cases',case,'--max-tokens','8','--expert-cache','4000',
                        '--expected-cache','4191,5760','--out',str(out)]
            if length:
                sys.argv += ['--prefix-tokens',str(length),'--keep-tail-tokens','64']
            if reference:
                sys.argv += ['--reference',str(reference)]
            print('STEP',*sys.argv,flush=True)
            runpy.run_path(sys.argv[0],run_name='__main__')
            result = json.loads((out/'results.json').read_text())
            assert result['complete'] and result['functional_acceptance']
            requests = [s for r in result['runs'] for s in r['requests']]
            assert all(s['generated']==8 and s['tokens']==requests[0]['tokens'] for s in requests)
            assert len({(s['accepted'],s['offered']) for s in requests})==1
            data['steps'].append({'argv':sys.argv.copy(),'results_sha256':b.sha(out/'results.json')})
            b.save(M/'results.json',data)
        run(M/'Short','f16','0,1','Short')
        for case,length,dtype in [('Code2K',256,'f16'),('Prose2K',512,'bf16')]:
            reference = H/'prefill-widen-bf16-batch-chat-fixed-cache'/f'{case}-{length}'/'results.json'
            run(M/f'{case}-{length}',dtype,'1',case,length,reference)
        run(M/'combined-Short','bf16','1','Short',reference=M/'Short/results.json')
    data.update(complete=True,functional_acceptance=True,model_speed_claim=False)
    b.save(M/'results.json',data)
    print('PASS resumed model checks',flush=True)
finally:
    if lock.exists() and lock.read_text()==owner:
        lock.unlink()
