"""Compile/check the exact retained source with the CMake option OFF and ON."""
import json
from pathlib import Path
import shutil
import sys

H = Path(__file__).resolve().parent
sys.path.insert(0, str(H))
import run as e

def main():
    assert not (H / 'integrated-build.json').exists()
    shutil.copyfile(H.parent/'sycl/src/kernels/cuda/kv_q8.dp.cpp', H/'integrated-kv_q8.dp.cpp')
    shutil.copyfile(H.parent/'sycl/CMakeLists.txt', H/'integrated-CMakeLists.txt')
    prior = json.loads((H/'build.json').read_text())['commands']
    final_link = json.loads((H/'graph-build.json').read_text())['commands'][1]
    env = e.build.environment()
    commands = []
    with e.b.gpu_lock():
        cfg = json.loads((H.parent/'strata-iq2_xs.json').read_text(encoding='utf-8-sig'))
        run_env = e.b.environment(cfg)
        run_env['PATH'] = env['PATH']
        e.save(H/'environment.json', {k:v for k,v in run_env.items() if k.startswith(('STRATA_', 'SYCL_', 'ONEAPI_', 'UR_', 'ZES_'))})
        for value in (0, 1):
            side = H/f'integrated-{value}'
            side.mkdir()
            cmd = prior[2].copy()
            cmd[-1] = str(H/'integrated-kv_q8.dp.cpp')
            cmd = [('/Fo'+str(side/'q8.obj')) if a.startswith('/Fo') else a for a in cmd]
            cmd.insert(2, '-DSTRATA_SYCL_Q8_SINGLE_SUBGROUP='+str(value))
            commands.append(cmd)
            e.command(cmd, f'integrated-compile-{value}', env)
            for test, template in [('steps', prior[3]), ('q8', prior[4]), ('final', final_link)]:
                cmd = template.copy()
                cmd[3] = str(side/'q8.obj')
                cmd = [('/out:'+str(side/(test+'.exe'))) if a.startswith('/out:') else a for a in cmd]
                commands.append(cmd)
                e.command(cmd, f'integrated-link-{value}-{test}', env)
                output = e.command([side/(test+'.exe')], f'integrated-check-{value}-{test}', run_env)
                if test=='steps':
                    assert output == (H/'check-baseline-steps.log').read_text()
                if test=='q8':
                    assert 'kv_q8_parity: OK' in output
                if test=='final':
                    assert 'PASS kv_steps: 198 ' in output
        e.save(H/'integrated-build.json', {'commands':commands, 'source_sha256':e.sha(H/'integrated-kv_q8.dp.cpp'),
            'artifacts':{str(p.relative_to(H)):e.sha(p) for p in H.glob('integrated-*/*') if p.suffix in ('.obj','.exe')}})
        arms = []
        for phase in ('screen','confirm'):
            for mode in (0,1):
                group = []
                for arm, value in enumerate((0,1,0,1,0)):
                    e.b.busy()
                    name = f'integrated-{phase}-mode{mode}-{arm}-{value}'
                    output = e.command([H/f'integrated-{value}/steps.exe','--bench',str(mode)], name, run_env)
                    rows = [json.loads(line[7:]) for line in output.splitlines() if line.startswith('RESULT ')]
                    assert len(rows)==3 and {r['T'] for r in rows}=={1,4,6}
                    entry = dict(phase=phase, mode=mode, arm=arm, side='candidate' if value else 'baseline', rows=rows, log=name+'.log')
                    arms.append(entry)
                    group.append(entry)
                    e.save(H/'integrated-raw.json',arms)
                for arm in (1,3):
                    for row in group[arm]['rows']:
                        n=row['T']
                        a=next(x['us'] for x in group[arm-1]['rows'] if x['T']==n)
                        c=next(x['us'] for x in group[arm+1]['rows'] if x['T']==n)
                        print('integrated',phase,mode,arm,n,e.b.paired(a,row['us'],c),flush=True)
    print('PASS exact retained source: OFF/ON correctness and independent direct timings',flush=True)

if __name__=='__main__':
    main()
