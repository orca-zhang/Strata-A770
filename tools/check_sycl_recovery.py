"""Cold-process SYCL recovery check; reuse the frozen assistant quality policy.

Pass a scoring profile with explicit turn splitting and a fresh output directory.
Each required case must have complete, finite scores and pass the raw-PPL gate.
"""
import argparse
import json
from pathlib import Path

import bench_deepinfer as b
from bench_orca_upgrade import compare_quality, fixed_scores
from check_orca_regression import runtime_env


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--cases', default='short,code,gcd,memory,followup,prose,long')
    parser.add_argument('--repeats', type=int, default=3)
    args = parser.parse_args()
    source = b.ROOT/'sycl/orca/assistant-quality-policy-a770-windows-2026-10-31.json'
    policy = json.loads(source.read_text(encoding='utf-8'))
    cfg = json.loads(args.config.read_text(encoding='utf-8'))
    cases = args.cases.split(',')
    if (args.repeats < 1 or len(set(cases)) != len(cases)
            or not set(cases) <= policy['quality_corpus']['cases'].keys()):
        parser.error('positive repeats and distinct known cases required')
    assert cfg['backend'] == 'sycl'
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    (out/Path(__file__).name).write_bytes(Path(__file__).read_bytes())
    (out/'config.json').write_bytes(args.config.read_bytes())
    files = [source, args.config, Path(cfg['exe']), Path(__file__),
             Path(b.__file__), b.ROOT/'tools/bench_orca_upgrade.py',
             b.ROOT/'tools/check_orca_regression.py']
    hashes = {str(p.resolve()): b.sha(p) for p in files}
    d = dict(complete=False, passed=False, config=cfg, files_sha256=hashes,
             cases=cases, repetitions=[], quality_equivalence_established=False)
    save = lambda: b.save(out/'results.json', d)
    save()
    with b.gpu_lock():
        assert all(b.sha(Path(p)) == h for p,h in hashes.items())
        for repeat in range(args.repeats):
            arm = dict(repeat=repeat, cases=[])
            d['repetitions'].append(arm)
            for name in cases:
                case = policy['quality_corpus']['cases'][name]
                stem = out/f'r{repeat}-{name}'
                env, _ = runtime_env(cfg.get('env', {}), cfg.get('lib_dirs', []))
                env.update(STRATA_LOGPOS=str(stem.with_suffix('.tsv')),
                           STRATA_LOGPOS_EXTRA='248046')
                item = dict(case=name, targets=0, valid=False,
                            expected_targets=len(case['tokens'])-case['prompt_tokens'])
                arm['cases'].append(item)
                save()
                print('START', repeat, name, flush=True)
                engine = b.Engine(Path(cfg['exe']), cfg['args'], env, stem)
                try:
                    item['generated'] = engine.gen(case['tokens'], 1, keys='temperature=0')
                finally:
                    engine.close()
                item.update(info=engine.info, exit_code=engine.p.returncode)
                log = stem.with_suffix('.stderr').read_text(encoding='utf-8')
                priming = log.count('strata serve: priming Windows before the first batched prompt')
                assert priming in (0,1)
                info = dict(field.split('=',1) for field in engine.info.split()[1:])
                priming = min(int(info['spec']), len(case['tokens'])-1) if priming else 0
                try:
                    generated = item['generated']
                    assert generated['generated'] == 1 and generated['tokens'] != [0], 'invalid generated token'
                    assert generated['resume'] == generated['reused'] == 0, 'not a cold request'
                    item.update(fixed_scores(stem.with_suffix('.tsv'), case, priming), valid=True)
                except (AssertionError, ValueError, OSError) as error:
                    item['error'] = f'{type(error).__name__}: {error}'
                    if stem.with_suffix('.tsv').exists():
                        item['scores_tsv'] = stem.with_suffix('.tsv').read_bytes().decode('utf-8')
                print('DONE', repeat, name, item['valid'], item.get('error', ''), flush=True)
                save()
            reference = {c['case']: c for c in policy['quality'][1]['cases']}
            arm['comparison_to_orca'] = compare_quality([reference[n] for n in cases], arm['cases'])
            save()
        d['files_unchanged'] = all(b.sha(Path(p)) == h for p,h in hashes.items())
        d['complete'] = True
        d['passed'] = d['files_unchanged'] and all(r['comparison_to_orca']['passed'] for r in d['repetitions'])
        save()
    print('RESULT', d['passed'], out, flush=True)
    raise SystemExit(0 if d['passed'] else 1)


if __name__ == '__main__':
    main()
