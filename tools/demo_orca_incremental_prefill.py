"""Compare bulk or serial chunks with incremental input using Orca's live-prefix cache.

python tools/demo_orca_incremental_prefill.py --self-check
python tools/demo_orca_incremental_prefill.py --config strata-iq2_xs.json --out build-orca-input-stream --interval-ms 30000

This demo sends already-tokenized prefixes over a local socket. Intermediate
GEN 1 predictions are discarded and counted as overhead. It does not implement
HTTP JSON streaming, incremental text tokenization or a prefill-only engine API.
"""
from pathlib import Path
import argparse
import json
import math
import os
import socket
import threading
import time

import bench_deepinfer as bench


def receive(engine, ids, chunk_size, interval_ms, incremental, max_new, serial_chunks=False):
    """One producer continues sending while the main thread waits on GPU work."""
    reader, writer = socket.socketpair()
    reader.settimeout(1200)
    writer.settimeout(1200)
    cancel = threading.Event()
    sent, errors = [], []
    started = time.perf_counter()

    def produce():
        try:
            with writer:
                for index, offset in enumerate(range(0, len(ids), chunk_size)):
                    deadline = started + index * interval_ms / 1000
                    if cancel.wait(max(0, deadline - time.perf_counter())):
                        return
                    frame = json.dumps(ids[offset:offset+chunk_size]).encode() + b'\n'
                    writer.sendall(frame)
                    sent.append(time.perf_counter())
        except BaseException as error:
            errors.append(str(error))

    producer = threading.Thread(target=produce, daemon=True)
    producer.start()
    prefix, steps = [], []

    def prefill(count):
        previous = steps[-1]['prefix_tokens'] if steps else 0
        step_start = time.perf_counter()
        # ponytail: reuse GEN 1; many chunks resend growing prefixes and compute extra heads.
        # A prefill-only delta command removes both costs if this prototype proves useful.
        row = engine.gen(ids[:count], 1, keys='temperature=0')
        assert row['generated'] == 1 and row['resume'] == previous, row
        steps.append(dict(prefix_tokens=count, row=row, started_ms=(step_start-started)*1000,
                          completed_ms=(time.perf_counter()-started)*1000))

    try:
        with reader, reader.makefile('rb') as stream:
            for line in stream:
                block = json.loads(line)
                assert block and len(block) <= chunk_size
                prefix.extend(block)
                assert prefix == ids[:len(prefix)]
                if incremental and len(prefix) < len(ids):
                    prefill(len(prefix))
            producer.join(timeout=5)
            assert not producer.is_alive() and not errors, errors
            assert prefix == ids and len(sent) == (len(ids)+chunk_size-1)//chunk_size
            if serial_chunks:
                for count in range(chunk_size, len(ids), chunk_size):
                    prefill(count)
            gen_start = time.perf_counter()
            final = engine.gen(prefix, max_new, keys='temperature=0')
            expected = steps[-1]['prefix_tokens'] if steps else 0
            assert final['resume'] == expected, (final['resume'], expected)
            first_at = gen_start + final['ttft_ms']/1000
            return dict(mode='incremental' if incremental else 'serial_chunks' if serial_chunks else 'bulk', steps=steps, final=final,
                        intermediate_generated_tokens=sum(s['row']['generated'] for s in steps),
                        input_complete_ms=(sent[-1]-started)*1000,
                        first_input_to_first_token_ms=(first_at-started)*1000,
                        all_input_to_first_token_ms=(first_at-sent[-1])*1000,
                        total_ms=(time.perf_counter()-started)*1000,
                        chunk_sent_ms=[(stamp-started)*1000 for stamp in sent])
    finally:
        cancel.set()
        reader.close()
        producer.join(timeout=5)


def self_check():
    class Fake:
        def __init__(self):
            self.prefix = []

        def gen(self, ids, count, keys):
            reused = len(self.prefix) if ids[:len(self.prefix)] == self.prefix else 0
            self.prefix = ids.copy()
            return dict(tokens=[sum(ids)]*count, generated=count, resume=reused, ttft_ms=1)

    ids = list(range(1, 13))
    a = receive(Fake(), ids, 5, 0, False, 3)
    b = receive(Fake(), ids, 5, 2, True, 3)
    c = receive(Fake(), ids, 5, 2, False, 3, serial_chunks=True)
    assert a['final']['tokens'] == b['final']['tokens'] == c['final']['tokens'] == [78]*3
    assert a['intermediate_generated_tokens'] == 0 and b['intermediate_generated_tokens'] == 2
    assert [s['prefix_tokens'] for s in b['steps']] == [5, 10]
    assert [s['row']['resume'] for s in b['steps']] == [0, 5]
    assert b['final']['resume'] == 10 and len(b['chunk_sent_ms']) == 3
    assert [s['row']['resume'] for s in c['steps']] == [0, 5]
    assert c['steps'][0]['started_ms'] >= c['input_complete_ms']
    print('PASS: local socket arrival, exact prefixes, cache reuse, final equivalence and counted intermediate predictions; no GPU.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--self-check', action='store_true')
    parser.add_argument('--config', type=Path)
    parser.add_argument('--out', type=Path)
    parser.add_argument('--chunk-tokens', type=int, default=4000)
    parser.add_argument('--interval-ms', type=float, default=0)
    parser.add_argument('--max-new', type=int, default=64)
    parser.add_argument('--baseline', choices=('bulk', 'serial-chunks'), default='bulk',
                        help='bulk tests the overall change; serial-chunks isolates arrival/compute overlap with identical math')
    args = parser.parse_args()
    if args.self_check:
        self_check()
        return
    if not args.config or not args.out:
        parser.error('--config and --out are required')
    if args.chunk_tokens <= 0 or args.max_new <= 0 or not math.isfinite(args.interval_ms) or args.interval_ms < 0:
        parser.error('positive chunk/output sizes and finite nonnegative interval required')
    cfg = json.loads(args.config.read_text(encoding='utf-8'))
    if cfg.get('backend') != 'orca':
        parser.error('use a native Orca config')
    inputs = bench.prompts(cfg)
    ids = inputs['Long8K']
    if args.chunk_tokens >= len(ids):
        parser.error('chunk size must split the 8000-token input')
    model_args = cfg['args'].copy()
    for key,value in (('--expert-cache','6000'),('--adapt-swaps','0'),('--pcie-frac','0'),
                      ('--prompt-cache','1')):
        if key in model_args:
            model_args[model_args.index(key)+1] = value
        else:
            model_args.extend([key,value])
    if any(key in model_args for key in ('--batch','--parallel')):
        parser.error('use a single-sequence config without --batch/--parallel')
    env = {k:v for k,v in os.environ.items() if not k.startswith(('STRATA_','SYCL_','UR_','ONEAPI_','ZES_'))}
    env.update(cfg.get('env',{}))
    env['PATH'] = os.pathsep.join(cfg.get('lib_dirs',[]))+os.pathsep+env.get('PATH','')
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    (out/Path(__file__).name).write_bytes(Path(__file__).read_bytes())
    images = Path(env['STRATA_ORCA_IMAGES'])
    manifest = lambda:{p.relative_to(images).as_posix():bench.sha(p) for p in images.rglob('*') if p.is_file()}
    report = dict(complete=False, performance_comparison_valid=False, linux_gpu_validated=False,
                  scope=__doc__, config_sha256=bench.sha(args.config), exe_sha256=bench.sha(cfg['exe']),
                  helper_sha256=bench.sha(bench.ROOT/'tools/bench_deepinfer.py'),
                  script_sha256=bench.sha(Path(__file__)), image_sha256=manifest(),
                  config=cfg, model_args=model_args, chunk_tokens=args.chunk_tokens,
                  interval_ms=args.interval_ms, max_new=args.max_new, baseline=args.baseline, input_ids=ids, runs=[],
                  limitations=['Already-tokenized IDs over a local socket; arrival interval is simulated.',
                               'Intermediate GEN 1 computes and discards a prediction; this overhead is included.',
                               'One fresh-process A/B/A, with one Short warmup per arm; not production throughput.',
                               'Fixed cache budget 6000, adaptation off, PCIe fraction 0, live-prefix cache on.',
                               'No HTTP endpoint or text-stream tokenizer is changed.'])
    save = lambda:bench.save(out/'results.json',report)
    save()
    print('OUT',out,flush=True)
    try:
        with bench.gpu_lock():
            for label,incremental in (('A0',False),('B',True),('A1',False)):
                print('RUN',label,flush=True)
                engine = bench.Engine(Path(cfg['exe']), model_args, env, out/label)
                arm = dict(label=label, info=engine.info)
                report['runs'].append(arm)
                try:
                    arm['warmup'] = engine.gen(inputs['Short'], 1, keys='temperature=0')
                    arm.update(receive(engine, ids, args.chunk_tokens, args.interval_ms, incremental, args.max_new,
                                       serial_chunks=not incremental and args.baseline=='serial-chunks'))
                    assert arm['final']['generated'] == args.max_new and any(arm['final']['tokens'])
                finally:
                    engine.close()
                    arm['exit_code'] = engine.p.returncode
                    save()
                print('DONE',label,'TTFT',arm['first_input_to_first_token_ms'],'tail',arm['all_input_to_first_token_ms'],flush=True)
            a,b,c = report['runs']
            report['same_output'] = a['final']['tokens'] == b['final']['tokens'] == c['final']['tokens']
            report['same_final_decode_work'] = all(a['final'][key] == b['final'][key] == c['final'][key]
                                                  for key in ('generated','accepted','offered','finish'))
            report['metrics'] = {}
            for key in ('first_input_to_first_token_ms','all_input_to_first_token_ms','total_ms'):
                m = bench.paired(a[key],b[key],c[key])
                m['latency_change_pct'] = 100*(m['candidate']/m['baseline']-1)
                report['metrics'][key] = m
            report['config_unchanged'] = bench.sha(args.config)==report['config_sha256']
            report['exe_unchanged'] = bench.sha(cfg['exe'])==report['exe_sha256']
            report['images_unchanged'] = manifest()==report['image_sha256']
            report['helper_unchanged'] = bench.sha(bench.ROOT/'tools/bench_deepinfer.py')==report['helper_sha256']
            report['script_unchanged'] = bench.sha(Path(__file__))==report['script_sha256']
            assert all(v for k,v in report.items() if k.endswith('_unchanged'))
            report['performance_comparison_valid'] = report['same_output'] and report['same_final_decode_work'] and all(
                abs(m['baseline_drift_pct'])<=3 for m in report['metrics'].values())
            report['complete'] = True
    except BaseException as error:
        report['error'] = f'{type(error).__name__}: {error}'
        raise
    finally:
        save()
    print('COMPLETE',json.dumps({k:report[k] for k in ('same_output','performance_comparison_valid','metrics')}),flush=True)


if __name__=='__main__':
    main()
