import json
from pathlib import Path
import re
import sys

H = Path(__file__).resolve().parent
sys.path.insert(0, str(H))
import run as experiment
b = experiment.b

def main():
    env = experiment.build.environment()
    build = json.loads((H / 'build.json').read_text())
    source = (H.parent / 'sycl/src/kernels/kv_steps_parity.cpp').read_text()
    (H / 'final-kv_steps_parity.cpp').write_text(source, encoding='utf-8')
    graph_source = source.replace('if(bench && mode>=2 && mode<=3 && fmt!=1)continue;', 'if(bench && fmt!=1)continue;')
    graph_source = graph_source.replace('// ponytail: include submission overhead, one wait per batch; each A/B arm runs in its own process.', '''using namespace sycl::ext::oneapi::experimental;
        command_graph graph(q.get_context(), q.get_device());
        graph.begin_recording(q); call(); graph.end_recording(q);
        auto executable = graph.finalize();
        const auto replay = [&] { q.ext_oneapi_graph(executable); };''')
    graph_source = graph_source.replace('++i)call();q.wait_and_throw();', '++i)replay();q.wait_and_throw();')
    (H / 'graph.cpp').write_text(graph_source, encoding='utf-8')
    commands = []
    with b.gpu_lock():
        for name, source in [('final', 'final-kv_steps_parity.cpp'), ('graph', 'graph.cpp')]:
            cmd = build['commands'][0].copy()
            cmd[-1] = str(H / source)
            cmd = [('/Fo' + str(H / (name + '.obj'))) if a.startswith('/Fo') else a for a in cmd]
            commands.append(cmd)
            experiment.command(cmd, 'compile-' + name, env)
            for side in ('baseline', 'candidate'):
                cmd = build['commands'][3].copy()  # identical original link dependencies/flags
                assert cmd[2].endswith('steps.obj') and cmd[3].endswith('baseline\\q8.obj')
                cmd[2] = str(H / (name + '.obj'))
                cmd[3] = str(H / side / 'q8.obj')
                cmd = [('/out:' + str(H / side / (name + '.exe'))) if a.startswith('/out:') else a for a in cmd]
                commands.append(cmd)
                experiment.command(cmd, 'link-' + side + '-' + name, env)
        experiment.save(H / 'graph-build.json', {'commands': commands})
        cfg = json.loads((H.parent / 'strata-iq2_xs.json').read_text(encoding='utf-8-sig'))
        run_env = b.environment(cfg)
        run_env['PATH'] = env['PATH']
        for side in ('baseline', 'candidate'):
            output = experiment.command([H / side / 'final.exe'], 'final-check-' + side, run_env)
            assert 'PASS kv_steps: 198 ' in output
        arms = []
        for phase in ('screen', 'confirm'):
            for mode in (0, 1):
                group = []
                for arm, side in enumerate(('baseline', 'candidate', 'baseline', 'candidate', 'baseline')):
                    b.busy()
                    name = f'graph-{phase}-mode{mode}-{arm}-{side}'
                    output = experiment.command([H / side / 'graph.exe', '--bench', str(mode)], name, run_env)
                    rows = [json.loads(line[7:]) for line in output.splitlines() if line.startswith('RESULT ')]
                    assert len(rows) == 3 and {r['T'] for r in rows} == {1, 4, 6}
                    entry = {'phase': phase, 'mode': mode, 'arm': arm, 'side': side, 'rows': rows, 'log': name + '.log'}
                    arms.append(entry)
                    group.append(entry)
                    experiment.save(H / 'graph-raw.json', arms)
                for arm in (1, 3):
                    for row in group[arm]['rows']:
                        n = row['T']
                        a = next(x['us'] for x in group[arm - 1]['rows'] if x['T'] == n)
                        c = next(x['us'] for x in group[arm + 1]['rows'] if x['T'] == n)
                        print('graph', phase, mode, arm, n, b.paired(a, row['us'], c), flush=True)
    print('PASS expanded graph checks and graph operator measurements', flush=True)

if __name__ == '__main__':
    main()
