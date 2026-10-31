"""Complete serving checks; test the exact CLI observer branch without native prefill."""
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import sys

H = Path(__file__).resolve().parent
R = H.parent
D = H/'checks-complete'
sys.path.insert(0, str(R/'tools'))
import bench_deepinfer as b
spec = importlib.util.spec_from_file_location('builder', R/'build-sycl-deepinfer-20261023/build.py')
builder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)


def main():
    with b.gpu_lock():
        data = json.loads((D/'results.json').read_text())
        diagnostic = [run for run in data['runs'] if not run.get('clean')]
        assert len(diagnostic) == 4 and all(len(run['requests']) == 11 for run in diagnostic)
        pattern = r'if \(timed_round[^\n]*?\)\s*policy\.observe\(from_sfx, T, a, sfx_match, round_ms\);'
        paths = [H/'before/sycl/src/program/generate.cpp', R/'sycl/src/program/generate.cpp']
        guards = []
        for path in paths:
            matches = re.findall(pattern, path.read_text())
            assert len(matches) == 1
            guards.append(matches[0])
        cpp = '''#include "strata/spec/draft_policy.hpp"
#include <cstdint>
#include <cstdio>
#include <vector>
using strata::spec::DraftPolicy;
'''
        for side, guard in zip(('baseline', 'candidate'), guards):
            cpp += f'''void {side}(DraftPolicy& policy, int T, int64_t n, int64_t cap, bool timed_round) {{
    std::vector<int32_t> produced((size_t)n);
    struct {{ int64_t max_new; }} o{{cap}};
    const bool from_sfx = false;
    const int a = T - 1, sfx_match = 0;
    const double round_ms = 12345.0;
    {guard}
}}
'''
        cpp += '''int main() {
    int pairs = 0, differences = 0;
    for (int T : {1, 4, 6}) for (int cap : {1, 4, 6})
        for (int n : {cap - 1, cap, cap + 1}) for (bool timed : {false, true}) {
            DraftPolicy old(6), fixed(6);
            baseline(old, T, n, cap, timed);
            candidate(fixed, T, n, cap, timed);
            const bool before = old.cost_ms(T) == 12345.0;
            const bool after = fixed.cost_ms(T) == 12345.0;
            if (before != timed || after != (timed && n < cap)) return 1;
            differences += before != after;
            ++pairs;
        }
    std::printf("SUMMARY {\\"pairs\\":%d,\\"checks\\":%d,\\"terminal_differences\\":%d}\\n", pairs, pairs*2, differences);
}
'''
        (H/'cli-caller.cpp').write_text(cpp)
        command = [str(builder.ICX/'bin/icx.exe'), '/nologo', '/TP', '/O2', '-Qstd:c++20', '/MD', '/EHsc',
                   '/I'+str(H/'headers/include'), str(H/'cli-caller.cpp'),
                   str(H/'before/src/spec/draft_policy.cpp'), '/Fe'+str(H/'cli-caller.exe')]
        with (H/'cli-caller-build.log').open('w') as log:
            subprocess.run(command, cwd=H, env=builder.environment(), stdout=log,
                           stderr=subprocess.STDOUT, check=True, timeout=120)
        p = subprocess.run([str(H/'cli-caller.exe')], cwd=H, env=builder.environment(),
                           capture_output=True, text=True, check=True, timeout=30)
        (D/'cli-caller.stdout').write_text(p.stdout)
        (D/'cli-caller.stderr').write_text(p.stderr)
        result = json.loads(p.stdout.removeprefix('SUMMARY ').strip())
        assert result == {'pairs': 54, 'checks': 108, 'terminal_differences': 18}
        b.save(H/'cli-caller-build.json', {'complete': True, 'command': command, 'guards': guards,
            'sources_sha256': {str(path): b.sha(path) for path in paths}, 'exe_sha256': b.sha(H/'cli-caller.exe'),
            'test_source_sha256': b.sha(H/'cli-caller.cpp'), 'result': result})
        data['cli_caller_check'] = result
        data['cli_model_check'] = {'status': 'not_validated',
            'reason': 'Existing baseline native CLI does not reach decode in multi-token prompt attempts; single-token prompt exits with zero generated tokens (spec_pos=0 does not enter spec_pos>0 loop). These records are not a model pass.'}
        cfg = data['config']
        reference = next(row['tokens'] for row in diagnostic[0]['requests'] if row['name'] == 'greedy')
        for side in ('baseline', 'candidate'):
            if any(run.get('clean') and run['side'] == side for run in data['runs']):
                continue
            env = b.environment(cfg, 'baseline', reproducible=True)
            env['STRATA_SPEC_COUPLED'] = '1'
            argv = b.model_args(cfg, reproducible=True, threshold=.5, cache_mib=5500)+['--kv', 'int8']
            stem = D/(side+'-clean-final')
            engine = b.Engine(H/side/'strata.exe', argv, env, stem)
            try:
                row = engine.gen(data['prompt_ids'], 32)
                assert row['tokens'] == reference
                data['runs'].append({'side': side, 'clean': True, 'stem': stem.name, 'args': argv,
                                     'runtime_info': engine.info, 'requests': [dict(row, name='greedy')]})
            finally:
                engine.close()
        tokens = {(run['threshold'], row['name']): row['tokens'] for run in diagnostic
                  for row in run['requests'] if row['name'] in ('greedy', 'sampled')}
        assert tokens[(0, 'greedy')] == tokens[(.5, 'greedy')]
        assert tokens[(0, 'sampled')] == tokens[(.5, 'sampled')]
        data['scope'] = '46 serving model requests, plus 108 CPU checks of the exact CLI observer branch. Native CLI model/prefill integration is not validated. No performance claim.'
        data['raw_sha256'] = {path.name: b.sha(path) for path in D.iterdir()
                              if path.suffix in ('.stderr', '.stdout', '.protocol')}
        data['complete'] = True
        b.save(D/'results.json', data)
        print('PASS serving requests=46; CLI caller CPU checks=108; CLI model integration NOT validated', flush=True)


if __name__ == '__main__':
    main()
