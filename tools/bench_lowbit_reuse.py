"""A770 native-layout low-bit kernel screen; excludes packing, transfers and model execution."""
import json
import math
import os
from pathlib import Path
import statistics
import subprocess
import sys

import bench_deepinfer as b

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'sycl/orca/lowbit_reuse_probe.cpp'
MODES = {0: 'U4 M8 N16 / U2 M1/2/4/8 N8 baseline', 1: 'M1/2/4/8 N16', 2: 'M8 N32',
         3: 'M16 N16 weight reuse', 4: 'M1/2/4/8 N8'}


def parse(output, smoke, reverse, samples=31):
    rows = [json.loads(line[7:]) for line in output.splitlines() if line.startswith('RESULT ')]
    shapes = [(1,13,64),(3,17,192),(9,33,128),(17,9,64)] if smoke else [
        (1,1280,2560),(2,1280,2560),(4,1280,2560),(8,1280,2560),(40,1280,2560),(160,1280,2560),
        (1,2560,640),(4,2560,640),(40,2560,640),(160,2560,640),(1,320,10240),(128,320,10240)]
    assert [(r['bits'],r['M'],r['N'],r['K'],r['phase'],r['mode']) for r in rows] == [
        (bits,*shape,phase,mode) for shape in shapes for bits in (4,2)
        for phase,mode in enumerate([0,4,0,3,0,2,0,1,0] if reverse else [0,1,0,2,0,3,0,4,0])]
    assert samples in (9,31)
    assert f'PASS {len(shapes)*2*(9*(1+(3 if smoke else samples))+5)} full CPU output/input/guard checks' in output
    for r in rows:
        assert len(r['samples_us']) == (3 if smoke else samples)
        assert all(math.isfinite(t) and t > 0 for t in r['samples_us'])
        assert r['median_us'] == statistics.median(r['samples_us'])
    comparisons = []
    for i in range(0,len(rows),9):
        for phase in (1,3,5,7):
            a,v,z = rows[i+phase-1:i+phase+2]
            control = math.sqrt(a['median_us'] * z['median_us'])
            drift = (z['median_us']/a['median_us']-1)*100
            pct = (control/v['median_us']-1)*100
            comparisons.append(dict(bits=v['bits'],M=v['M'],N=v['N'],K=v['K'],mode=v['mode'],
                control_us=control,candidate_us=v['median_us'],throughput_pct=pct,
                control_duration_drift_pct=drift,
                passes_screen=pct>=1 and abs(drift)<=3 and not
                    ((v['bits']==4 and v['mode']==1 and v['M']>4) or (v['bits']==2 and v['mode']==4))))
    return dict(rows=rows,comparisons=comparisons)


def verify(stage):
    r = json.loads((stage/'results.json').read_text('utf8'))
    assert r['complete'] and not r.get('error') and not r['production_changed']
    # Verify archived experiment sources even when the live probe/runner has since changed.
    frozen = {SOURCE:stage/'probe.cpp',Path(__file__).resolve():stage/'runner.py'}
    assert all(b.sha(frozen.get(Path(p),ROOT/p))==h for p,h in r['sha256'].items())
    orders = ['forward','reverse'] + (['forward-repeat','reverse-repeat'] if 'forward-repeat' in r else [])
    assert [v['label'] for v in r['commands']] == ['build','smoke']+orders
    samples = len(r['forward']['rows'][0]['samples_us'])
    for command in r['commands']:
        assert command['exit_code'] == 0
        label = command['label']
        if label != 'build':
            assert r[label] == parse((stage/(label+'.stdout')).read_text('utf8'),label=='smoke',label.startswith('reverse'),samples)
    key = lambda v: tuple(v[k] for k in ('bits','M','N','K','mode'))
    assert r['confirmed'] == [v for v in r['forward']['comparisons'] if all(
        next(w for w in r[order]['comparisons'] if key(w)==key(v))['passes_screen'] for order in orders)]
    print(f'PASS {328+len(orders)*24*(9*(1+samples)+5)} CPU-output/input/guard checks; all samples, orders and frozen hashes',flush=True)


def run(stage):
    with b.gpu_lock():
        stage.mkdir(parents=True,exist_ok=False)
        source = stage/'probe.cpp'
        source.write_bytes(SOURCE.read_bytes())
        (stage/'runner.py').write_bytes(Path(__file__).read_bytes())
        compiler = Path('C:/Program Files (x86)/Intel/oneAPI/compiler/2025.3/bin/icx.exe')
        # Run from a oneAPI/MSVC development shell; no private build snapshots are required.
        env = os.environ.copy()
        env = {k:v for k,v in env.items() if not k.startswith(('STRATA_','SYCL_','UR_','ONEAPI_','ZES_'))}
        env['ONEAPI_DEVICE_SELECTOR'] = 'level_zero:gpu'
        exe = stage/'probe.exe'
        r = dict(complete=False,production_changed=False,scope=__doc__,modes=MODES,commands=[],sha256={},
            notes=['Synthetic packed integer operands; U4 has one weight/two activation planes; U2 one/one.',
                   'Timed cases use power-of-two scales; smoke uses irregular mantissas with exact CPU FP32 comparison.',
                   'Synthetic integer/scale correctness, not a quantization quality study.',
                   'Explicit FP32 FMA in GPU and CPU oracle; /fp:precise. Four independent performance processes.',
                   '64 queued warmups/31 queued event samples per phase, forward/reverse independent processes.',
                   'Each queued sample has independent output/guards; complete readback follows the timed batch.',
                   'Hot device USM; excludes native Q2 gather/repacking, quantization, transfers, CPU and model time.',
                   'No frequency lock or occupancy/spill/hardware-counter measurements; shared experiment lock.',
                   'Baseline preserves experimental U4 M8N16 / U2 adaptive-M N8 tile geometry; not production GEMV.',
                   'Identical-kernel controls (U4 mode1 M>4; U2 mode4) cannot qualify as gains.',
                   'Confirmation: each independent order >=1% gain with <=3% adjacent control duration drift.'])
        save = lambda: b.save(stage/'results.json',r)
        save()
        try:
            build = [str(compiler),'/nologo','/std:c++20','/EHsc','/MD','/O2','/fp:precise','-fsycl',
                     '-fsycl-targets=spir64',str(source),'-o',str(exe)]
            for label,cmd in [('build',build),('smoke',[str(exe),'--smoke']),
                              ('forward',[str(exe)]),('reverse',[str(exe),'--reverse']),
                              ('forward-repeat',[str(exe)]),('reverse-repeat',[str(exe),'--reverse'])]:
                if label != 'build': b.busy()
                (stage/(label+'.command.txt')).write_text(subprocess.list2cmdline(cmd),'utf8')
                p = subprocess.run(cmd,cwd=ROOT,env=env,capture_output=True,timeout=600)
                (stage/(label+'.stdout')).write_bytes(p.stdout)
                (stage/(label+'.stderr')).write_bytes(p.stderr)
                r['commands'].append(dict(label=label,exit_code=p.returncode));save()
                print(label,'exit',p.returncode,flush=True)
                if p.returncode: print((p.stdout+p.stderr).decode(errors='replace')[-5000:],flush=True)
                assert p.returncode == 0,label
                if label != 'build':
                    r[label] = parse(p.stdout.decode(),label=='smoke',label.startswith('reverse'));save()
            key = lambda v: tuple(v[k] for k in ('bits','M','N','K','mode'))
            r['confirmed'] = [v for v in r['forward']['comparisons'] if all(
                next(w for w in r[order]['comparisons'] if key(w)==key(v))['passes_screen']
                for order in ('forward','reverse','forward-repeat','reverse-repeat'))]
            paths = {p for p in stage.iterdir() if p.is_file() and p.name!='results.json'}
            paths.update([ROOT/'tools/bench_deepinfer.py',compiler])
            r['sha256'] = {str(p):b.sha(p) for p in paths}
            r['complete'] = True;save();verify(stage)
            for v in r['confirmed']: print('CONFIRMED',json.dumps(v),flush=True)
        except BaseException as error:
            r['error'] = repr(error);save();raise


if __name__ == '__main__':
    if len(sys.argv)==3 and sys.argv[1]=='--verify':
        verify((ROOT/sys.argv[2]).resolve())
    else:
        if len(sys.argv)!=2: raise SystemExit('usage: bench_lowbit_reuse.py [--verify] BUILD_DIRECTORY')
        stage = (ROOT/sys.argv[1]).resolve()
        if not stage.is_relative_to(ROOT) or stage==ROOT: raise ValueError('build directory must be inside workspace')
        run(stage)
