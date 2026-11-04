"""Compare accepted-prefix catch-up or a cumulative-confidence default on Orca L0.

python tools/bench_mtp_prefix.py --exe <strata.exe> --out <new-directory>
Use --functional for threshold/sampling regression; otherwise collect an A/B/A bracket.
Use --candidate-exe to check a rebuilt candidate with its default switch unset.
Add --candidate-cumulative-default for a rebuilt 0.5 confidence default; draft work may differ.
Use --cumulative 0.5 to compare other candidates against the current confidence baseline.
"""
import argparse
import json
from pathlib import Path
import statistics

import bench_deepinfer as b


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--exe', type=Path, required=True)
    ap.add_argument('--candidate-exe', type=Path)
    ap.add_argument('--candidate-cumulative-default', action='store_true')
    ap.add_argument('--cumulative', type=float, default=0.0, help='Confidence product floor for both arms')
    ap.add_argument('--config', type=Path, default=b.ROOT / 'strata-iq2_xs.json')
    ap.add_argument('--out', type=Path, required=True)
    ap.add_argument('--cases', default='Short,Code2K,Prose2K,Long8K')
    ap.add_argument('--tokens', type=int, default=128)
    ap.add_argument('--warmup', type=int, default=2)
    ap.add_argument('--retained', type=int, default=3)
    ap.add_argument('--functional', action='store_true')
    ap.add_argument('--reuse-prompt', action='store_true', help='Group repeated prompts and reuse their cached state')
    ap.add_argument('--keep-suffix', action='store_true', help='Keep the configured suffix lookup policy')
    a = ap.parse_args()
    assert a.tokens >= 1 and a.warmup >= 0 and a.retained >= 1
    assert 0 <= a.cumulative <= 1
    assert a.exe.is_file(), a.exe
    assert a.candidate_exe is None or a.candidate_exe.is_file(), a.candidate_exe
    assert not a.candidate_cumulative_default or a.candidate_exe
    assert not a.candidate_cumulative_default or a.cumulative == 0
    cfg = json.loads(a.config.read_text('utf8'))
    inputs = b.prompts(cfg)
    fixture = json.loads((b.ROOT / 'sycl/orca/short-anchor-v1.json').read_text('utf8'))
    inputs['Anchor'] = fixture['input_ids']
    names = a.cases.split(',')
    assert names and len(set(names)) == len(names) and all(n in inputs for n in names)
    a.out.mkdir(parents=True, exist_ok=False)
    (a.out / Path(__file__).name).write_bytes(Path(__file__).read_bytes())
    args = b.model_args(cfg, cache_mib=5500, reproducible=a.functional)
    if a.reuse_prompt:
        args[args.index('--prompt-cache') + 1] = '128'
    # ponytail: keep the production precision and expert policy; fixed cache capacity makes comparisons meaningful.
    if not a.keep_suffix:
        if '--suffix-draft' in args:
            args[args.index('--suffix-draft') + 1] = '0'
        else:
            args += ['--suffix-draft', '0']
    paths = [a.exe, a.config, Path(__file__), Path(b.__file__)]
    if a.candidate_exe:
        paths.append(a.candidate_exe)
    hashes = {str(p.resolve()): b.sha(p) for p in paths}
    images = Path(cfg['env']['STRATA_ORCA_IMAGES'])
    image_hashes = {str(p): b.sha(p) for p in images.rglob('*') if p.is_file()}
    d = dict(complete=False, functional=a.functional, reuse_prompt=a.reuse_prompt,
             args=args, input_ids={n: inputs[n] for n in names},
             files_sha256=hashes, images_sha256=image_hashes, runs=[], comparisons=[], production_changed=False)
    save = lambda: b.save(a.out / 'results.json', d)
    save()
    reference = {}
    settings = [(0.5, a.cumulative, 'temperature=0')]
    if a.functional and not a.candidate_cumulative_default and a.cumulative != 0.5:
        settings += [(0.5, 0.5, 'temperature=0')]
    try:
        with b.gpu_lock():
            for index, (floor, cumulative, sampling) in enumerate(settings):
                arms = ['serial', 'prefix'] if a.functional else ['A0', 'B', 'A1']
                current = []
                for label in arms:
                    launch = args.copy()
                    launch[launch.index('--spec-min-p') + 1] = str(floor)
                    env = b.environment(cfg, reproducible=a.functional)
                    env.update(STRATA_MTP_CHAIN_SUBMIT='0', STRATA_MTP_CUM_MIN_P=str(cumulative))
                    prefix = label not in ('serial', 'A0', 'A1')
                    executable = a.candidate_exe if prefix and a.candidate_exe else a.exe
                    if prefix and a.candidate_exe:
                        env.pop('STRATA_MTP_ACCEPTED_ONLY', None)
                    else:
                        env['STRATA_MTP_ACCEPTED_ONLY'] = '1' if prefix else '0'
                    if prefix and a.candidate_cumulative_default:
                        env.pop('STRATA_MTP_CUM_MIN_P', None)
                    if a.functional:
                        env['STRATA_SPEC_COUPLED'] = '1'
                    run = dict(label=label, setting=index, prefix_only=prefix and not a.candidate_cumulative_default,
                               executable=str(executable.resolve()), floor=floor,
                               cumulative=0.5 if prefix and a.candidate_cumulative_default else cumulative,
                               sampling=sampling, rows=[], runtime_env={k: v for k, v in env.items() if k.startswith('STRATA_')})
                    d['runs'].append(run)
                    print('START', index, label, flush=True)
                    engine = b.Engine(executable.resolve(), launch, env, a.out / f'{index}-{label}')
                    try:
                        run['info'] = engine.info
                        repeats = range(a.warmup + a.retained)
                        requests = ([(i, name) for name in names for i in repeats] if a.reuse_prompt else
                                    [(i, name) for i in repeats for name in names])
                        queries = [(floor, sampling)]
                        if a.functional:
                            # Per-request floors avoid restarting the model for each threshold or sampler.
                            queries = [(0.0, 'temperature=0'), (0.5, 'temperature=0'), (0.95, 'temperature=0'),
                                       (0.5, 'temperature=0.8 top_k=40 top_p=0.9 seed=123'),
                                       (0.5, 'temperature=0.8 top_k=40 seed=123 penalty_repeat=1.1 penalty_freq=0.1 penalty_present=0.2'),
                                       (0.5, 'temperature=0')]
                        for repeat, name, query, request_floor, keys in [
                                (i, name, q, f, k) for i, name in requests for q, (f, k) in enumerate(queries)]:
                                keys += ' spec_min_p=' + str(request_floor)
                                row = engine.gen(inputs[name], a.tokens, keys=keys)
                                row.update(case=name, repeat=repeat, query=query, keys=keys, retained=repeat >= a.warmup)
                                run['rows'].append(row)
                                save()
                                assert row['generated'] >= 1 and any(row['tokens']) and row['decode_ms'] > 0
                                if not a.reuse_prompt:
                                    assert row['reused'] == row['resume'] == 0
                                key = index, repeat, name, query
                                fields = ('tokens', 'finish', 'reused', 'resume')
                                if not a.candidate_cumulative_default:
                                    fields += ('accepted', 'offered')
                                workload = {k: row[k] for k in fields}
                                if label in ('serial', 'A0'):
                                    reference[key] = workload
                                else:
                                    assert workload == reference[key], (label, key, 'output or draft workload differs')
                                print(label, name, repeat, round(1000 * row['generated'] / row['decode_ms'], 3),
                                      row['accepted'], row['offered'], flush=True)
                    finally:
                        engine.close()
                        run['exit_code'] = engine.p.returncode
                        save()
                    current.append(run)
                if not a.functional:
                    for name in names:
                        ms = [statistics.median(r['decode_ms'] / r['generated'] for r in run['rows']
                                                if r['retained'] and r['case'] == name) for run in current]
                        comparison = dict(case=name, median_ms_per_token=ms, **b.paired(*ms))
                        comparison['passes_1pct'] = comparison['throughput_change_pct'] >= 1 and abs(comparison['baseline_drift_pct']) <= 3
                        d['comparisons'].append(comparison)
                        print('RESULT', comparison, flush=True)
            assert all(b.sha(p) == digest for p, digest in hashes.items())
            assert all(b.sha(p) == digest for p, digest in image_hashes.items())
            d['complete'] = True
    except BaseException as error:
        d['error'] = repr(error)
        raise
    finally:
        save()
    print('PASS exact outputs, normal exits and unchanged inputs' +
          ('; cumulative policy may change draft counts' if a.candidate_cumulative_default else '; draft counts equal'), flush=True)


if __name__ == '__main__':
    main()
