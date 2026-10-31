"""Freeze the existing engines, inputs and runner dependencies for a four-case screen."""
import hashlib
import json
import shutil
from pathlib import Path

F = Path(__file__).resolve().parent
R = F.parent
B = R / 'build-sycl-deepinfer-20261023/sampler-fp32'
assert not (F / 'preparation.json').exists()
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
reference = R / 'bench/results/a770/deepinfer-2026-10-23/functional-sampler-q8-allcases/results.json'
prior = json.loads(reference.read_text(encoding='utf-8'))
cfg = json.loads((B / 'config-original.json').read_text(encoding='utf-8'))
argv = cfg['args'].copy()
for flag, value in (('--expert-cache', '5500'), ('--spec-min-p', '0.5'), ('--prompt-cache', '128')):
    argv[argv.index(flag)+1] = value
argv += ['--prompt-cache-every', '512', '--kv', 'int8']
shutil.copyfile(R / 'tools/bench_deepinfer.py', F / 'frozen_bench.py')
prep = dict(config=cfg, args=argv, prompts=prior['prompts'], modes=['baseline', 'combined', 'baseline'],
            exe=str(B / 'candidate/strata.exe'), max_tokens=128, warmups=2, retained=3,
            expected_expert_slots=5771, expected_expert_cache_mib=7922,
            rule='Four-case cached-prefix model A/B/A SCREEN, common frozen candidate EXE and dependencies. '
                 'Existing user threshold 1%; exclude absolute adjacent-baseline drift >3%. '
                 'Only if all four decode comparisons are eligible, geometric mean >=1%, no case below -2%, '
                 'and exact outputs/reuse/capacity agree does the screen pass. '
                 'No independent confirmation or default enablement in this screen. '
                 'Initial requests and cached-prefix requests reported separately; initial requests have no '
                 'per-case warmup and use the window prompt path, so no native batched-prefill speedup claim. '
                 'Two warmups then three retained requests per case. No profiler/stage-timer flags. '
                 'No reruns to seek a pass.',
            frozen_sha256={str(p.relative_to(R)): sha(p) for p in
                           (B / 'candidate/strata.exe', B / 'candidate/build.json', B / 'source-manifest.json',
                            B / 'candidate-source-manifest.json', R / 'tools/strata_tokenizer.py',
                            R / 'tools/bench_deepinfer.py', reference,
                            R / 'strata-iq2_xs.json', R / 'data/expert-profile.bin',
                            R / 'build-sycl-a770-unified-operators/bin/strata-unified-all-operators.exe')})
for path in ('source-manifest.json', 'candidate-source-manifest.json'):
    shutil.copyfile(B / path, F / path)
shutil.copyfile(B / 'candidate/build.json', F / 'engine-build.json')
(F / 'preparation.json').write_text(json.dumps(prep, indent=2) + '\n', encoding='utf-8')
print('FROZEN', prep['frozen_sha256'][str((B / 'candidate/strata.exe').relative_to(R))], flush=True)
