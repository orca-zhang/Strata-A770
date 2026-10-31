"""Long-input sampling and cached-prefix parity on the frozen FP32-capable engines.

python build-sycl-deepinfer-20261023/sample_long.py --out <new-directory>
"""
import argparse
import json
from pathlib import Path
import sys

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
sys.path.insert(0, str(ROOT/'tools'))
import bench_deepinfer as bench

ap = argparse.ArgumentParser(description=__doc__)
ap.add_argument('--out', type=Path, required=True)
ap.add_argument('--threshold', type=float, default=.5)
ap.add_argument('--modes', default='baseline,combined')
args = ap.parse_args()
assert 0 <= args.threshold <= 1
modes = args.modes.split(',')
assert modes[0] == 'baseline' and all(m in bench.MODES for m in modes)
build = HERE/'sampler-fp32'
cfg = json.loads((HERE/'config-window-prompt.json').read_text(encoding='utf-8-sig'))
argv = bench.model_args(cfg, reproducible=True, threshold=args.threshold, cache_mib=5500)
argv[argv.index('--prompt-cache')+1] = '128'
# ponytail: reuse the existing checkpoints to make seed replay affordable; these are correctness timings only.
argv += ['--prompt-cache-every', '512', '--kv', 'int8']
sampling = 'temperature=0.7 top_p=0.9 top_k=20 seed=1234 penalty_repeat=1.1 penalty_freq=0.1 penalty_present=0.2 penalty_last_n=128'
inputs = bench.prompts(cfg)
data = {'complete': False, 'purpose': 'long sampling and restored-prefix correctness, no performance claim',
        'threshold': args.threshold, 'modes': modes, 'args': argv, 'sampling': sampling,
        'config': cfg, 'prompts': inputs, 'max_tokens': 48, 'runs': [],
        'exe_sha256': {s: bench.sha(build/s/'strata.exe') for s in ('baseline', 'candidate')},
        'source_manifest_sha256': bench.sha(build/'source-manifest.json'),
        'candidate_manifest_sha256': bench.sha(build/'candidate-source-manifest.json'),
        'script_sha256': bench.sha(__file__), 'bench_script_sha256': bench.sha(ROOT/'tools/bench_deepinfer.py')}
reference = None
with bench.gpu_lock(allow_compiler=True):
    args.out.mkdir(parents=True, exist_ok=False)
    (args.out/'sample_long.py').write_bytes(Path(__file__).read_bytes())
    (args.out/'bench_deepinfer.py').write_bytes((ROOT/'tools/bench_deepinfer.py').read_bytes())
    target = args.out/'results.json'
    bench.save(target, data)
    try:
        for i, mode in enumerate(modes):
            side = 'baseline' if i == 0 else 'candidate'
            env = bench.environment(cfg, mode, reproducible=True)
            env.update(STRATA_SPEC_COUPLED='1', STRATA_SNAPSHOT_VERIFY='1')
            print('SAMPLING MODEL', mode, flush=True)
            engine = bench.Engine(build/side/'strata.exe', argv, env, args.out/f'{i}-{mode}')
            run = {'mode': mode, 'build': side, 'runtime_info': engine.info, 'cases': {}}
            data['runs'].append(run)
            try:
                slots = int(engine.info.split('expert_slots=')[1].split()[0])
                assert slots == data.setdefault('expert_slots', slots)
                run['primer'] = engine.gen(inputs['Short'], 16)
                for name, ids in inputs.items():
                    first = engine.gen(ids, 48, sampling)
                    run['cases'][name] = {'first': first}
                    bench.save(target, data)
                    repeat = engine.gen(ids, 48, sampling)
                    run['cases'][name]['repeat'] = repeat
                    assert first['tokens'] == repeat['tokens'] and first['generated'] > 0, (mode, name)
                    if name != 'Short':
                        assert repeat['reused'] >= 512, (mode, name, repeat['done'])
                    if reference is not None:
                        assert first['tokens'] == reference[name], (mode, name, 'candidate differs')
                    bench.save(target, data)
                    print('SAMPLING CASE', mode, name, 'PASS', 'reused', repeat['reused'], flush=True)
            finally:
                engine.close()
            if reference is None:
                reference = {name: row['first']['tokens'] for name, row in run['cases'].items()}
        data['complete'] = True
        bench.save(target, data)
    except Exception as error:
        data['failure'] = repr(error)
        bench.save(target, data)
        raise
print('PASS long sampling', target, flush=True)
