"""Bounded graph batches; retains the aborted large-batch record separately."""
import json
from pathlib import Path
import sys

H = Path(__file__).resolve().parent
sys.path.insert(0, str(H))
import run as e

def main():
    assert not (H / 'graph-raw.json').exists()
    source = (H / 'graph.cpp').read_text().replace('i<512', 'i<64').replace('2048', '128')
    (H / 'graph_short.cpp').write_text(source, encoding='utf-8')
    env = e.build.environment()
    prior = json.loads((H / 'graph-build.json').read_text())['commands']
    commands = []
    with e.b.gpu_lock():
        cmd = prior[3].copy()
        assert cmd[-1].endswith('graph.cpp')
        cmd[-1] = str(H / 'graph_short.cpp')
        cmd = [('/Fo' + str(H / 'graph_short.obj')) if a.startswith('/Fo') else a for a in cmd]
        commands.append(cmd)
        e.command(cmd, 'compile-graph-short', env)
        for side, index in [('baseline', 4), ('candidate', 5)]:
            cmd = prior[index].copy()
            cmd[2] = str(H / 'graph_short.obj')
            cmd = [('/out:' + str(H / side / 'graph_short.exe')) if a.startswith('/out:') else a for a in cmd]
            commands.append(cmd)
            e.command(cmd, 'link-' + side + '-graph-short', env)
        e.save(H / 'graph-short-build.json', {'commands': commands, 'warmups':64, 'batches':9, 'iterations':128})
        cfg = json.loads((H.parent / 'strata-iq2_xs.json').read_text(encoding='utf-8-sig'))
        run_env = e.b.environment(cfg)
        run_env['PATH'] = env['PATH']
        arms = []
        for phase in ('screen', 'confirm'):
            for mode in (0, 1):
                group = []
                for arm, side in enumerate(('baseline', 'candidate', 'baseline', 'candidate', 'baseline')):
                    e.b.busy()
                    name = f'graph-short-{phase}-mode{mode}-{arm}-{side}'
                    out = e.command([H / side / 'graph_short.exe', '--bench', str(mode)], name, run_env)
                    rows = [json.loads(line[7:]) for line in out.splitlines() if line.startswith('RESULT ')]
                    assert len(rows) == 3 and {r['T'] for r in rows} == {1, 4, 6}
                    entry = dict(phase=phase, mode=mode, arm=arm, side=side, rows=rows, log=name + '.log')
                    arms.append(entry)
                    group.append(entry)
                    e.save(H / 'graph-raw.json', arms)
                for arm in (1, 3):
                    for row in group[arm]['rows']:
                        n = row['T']
                        a = next(x['us'] for x in group[arm-1]['rows'] if x['T']==n)
                        c = next(x['us'] for x in group[arm+1]['rows'] if x['T']==n)
                        print('graph',phase,mode,arm,n,e.b.paired(a,row['us'],c),flush=True)
    print('PASS bounded graph measurements',flush=True)

if __name__ == '__main__':
    main()
