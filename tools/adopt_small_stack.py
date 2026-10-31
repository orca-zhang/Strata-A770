"""Publish the user-accepted five-change profile after fixed-target and server checks."""
import argparse
from contextlib import nullcontext
import importlib.util
import json
import math
from pathlib import Path
import sys
import threading

SCRIPT = Path(__file__).resolve()
R = SCRIPT.parents[1]
H = R / 'build-sycl-a770-small-stack-adopt-20261030'
CONFIG = R / 'strata-iq2_xs-small-stack.json'
spec = importlib.util.spec_from_file_location('accepted_small_stack', SCRIPT.with_name('bench_small_stack_retry.py'))
retry = importlib.util.module_from_spec(spec)
spec.loader.exec_module(retry)
stack, b = retry.stack, retry.stack.b
quality = stack.gr.module(R / 'build-sycl-a770-gr-arg-select-confirm-20261029/quality.py').template
REFERENCE = quality.CURRENT / 'quality/results.json'
SPEED = retry.OUT / 'performance/results.json'
PREFIXES = ('STRATA_', 'SYCL_', 'UR_', 'ONEAPI_', 'ZES_')
sys.path.insert(0, str(R))
from serve import server


def read(path):
    return json.loads(path.read_text('utf8'))


def profile(speed):
    base = read(R / 'strata-iq2_xs.json')
    cfg = {key: base[key] for key in ('cwd', 'tokenizer', 'model_name', 'port', 'backend', 'lib_dirs')}
    exe = stack.H / 'bin/strata-unified-all-operators.exe'
    cfg.update(exe=str(exe), args=speed['args'], log=str(R / 'strata-iq2_xs-small-stack.log'),
               env=speed['runtime_flags'] | {flag: '1' for flag in stack.FLAGS} | {'STRATA_SYCL_BIN': str(exe)})
    assert server.engine_args(cfg) == speed['args']
    env = server.child_env(cfg)
    actual = {key: value for key, value in env.items() if key.startswith(PREFIXES) and key != 'ONEAPI_ROOT'}
    assert actual == cfg['env'] and 'STRATA_VERIFY_EAGER' not in actual
    return cfg


def scores(path, cases):
    # ponytail: reuse the existing758-target parser and frozen reference; two fresh candidates check repetition.
    for line in path.read_text('utf8').splitlines():
        fields = line.split('\t')
        assert len(fields) >= 8 and all(math.isfinite(float(fields[i])) for i in (2, 4, 6))
    parsed = quality.q.parse(path, cases)
    del parsed['Initializer']
    assert sum(value['tokens'] for value in parsed.values()) == 758
    return parsed


def check_scores(rows, reference):
    assert len(rows) == 2 and rows[0]['quality'] == rows[1]['quality'] == reference
    assert all(row['exit_code'] == 0 for row in rows)


def fixed_targets(speed, cfg, state):
    cases = quality.g.quality_cases(speed)
    all_cases = [('Initializer', dict(prompt_tokens=1, tokens=speed['input_ids']['Short'])), *cases]
    args = cfg['args'].copy()
    for flag, value in (('--prompt-cache', '1'), ('--short-read', '256')):
        args[args.index(flag) + 1] = value
    close = stack.gr.module(R / 'build-sycl-a770-down-scan-context-20261028/run.py').close
    for label in ('B1', 'B2'):
        row = dict(label=label, rows=[], args=args)
        state['quality'].append(row)
        engine = b.Engine.__new__(b.Engine)
        logpos = H / (label + '.logpos.tsv')
        try:
            b.busy()
            b.Engine.__init__(engine, Path(cfg['exe']), args, server.child_env(cfg) | {'STRATA_LOGPOS': str(logpos)}, H / label)
            info = dict(item.split('=', 1) for item in engine.info.split() if '=' in item)
            assert int(info['expert_slots']) == 6297
            row['ready_free_mib'] = int(info['vram_free_mib'])
            row['initializer'] = engine.gen(speed['input_ids']['Short'], 16)
            assert row['initializer']['tokens'] == speed['reference']['Short']
            for name, case in cases:
                b.busy(engine.p.pid)
                result = engine.gen(case['tokens'], 1)
                assert result['generated'] == 1 and result['prompt_tokens'] == len(case['tokens'])
                row['rows'].append(result | {'case': name})
                b.save(H / 'state.json', state)
                print('FIXED_TARGET', label, name, flush=True)
        except Exception as error:
            row['primary_error'] = str(error)
            raise
        finally:
            if hasattr(engine, 'p'):
                close(engine, row)
            b.save(H / 'state.json', state)
        assert row['exit_code'] == 0 and not row.get('close_error')
        quality.protocol(H / label, [row['initializer'], *row['rows']])
        row['quality'] = scores(logpos, all_cases)
        text = (H / (label + '.stderr')).read_text('utf8')
        assert all(marker in text for marker in ('IQ4_XS_COMPACT ni=', 'XPU_SHARED_PAIR enabled=1',
                   'MODEL_GU_Q8 cap=10 active=1', 'GDN_TAIL_CURRENT enabled=1', 'QSA_TAIL_CURRENT active=1'))
        b.save(H / 'state.json', state)
    reference = read(REFERENCE)['runs'][0]['quality']
    check_scores(state['quality'], reference)
    state['fixed_targets_passed'] = True


def server_smoke(speed, cfg, state):
    engine = server.StrataEngine.__new__(server.StrataEngine)
    rows = state['server_rows'] = []
    try:
        b.busy()
        server.StrataEngine.__init__(engine, cfg['exe'], server.engine_args(cfg), cwd=cfg['cwd'],
                                    log=str(H / 'server.stderr'), env=server.child_env(cfg))
        assert engine.info['expert_slots'] == 6297
        for case, count in (('Short', 16), ('Code2K', 128), ('Prose2K', 128)):
            b.busy(engine.proc.pid)
            tokens = [token for token in engine.generate(speed['input_ids'][case], count, {}, threading.Event()) if token is not None]
            assert tokens == speed['reference'][case]
            rows.append(dict(case=case, tokens=tokens, generated=engine.last['generated']))
            print('SERVER_OUTPUT', case, len(tokens), flush=True)
        proc = engine.proc
        proc.stdin.write('QUIT\n')
        proc.stdin.flush()
        proc.stdin.close()
        state['server_exit_code'] = proc.wait(timeout=180)
        assert state['server_exit_code'] == 0
    finally:
        if getattr(engine, 'proc', None) is not None:
            engine.close()
        b.save(H / 'state.json', state)


def self_check():
    speed = read(SPEED)
    assert speed['complete'] and not speed['passes_confirmation'] and not speed['sampled_memory_passed']
    cfg = profile(speed)
    assert all(cfg['env'][flag] == '1' for flag in stack.FLAGS)
    good = dict(quality={'target': 1}, exit_code=0)
    check_scores([good, good], good['quality'])
    for bad in (good | {'quality': {}}, good | {'exit_code': 1}):
        try:
            check_scores([good, bad], good['quality'])
        except AssertionError:
            pass
        else:
            raise AssertionError('failed/repeated quality accepted')
    print('PASS exact measured profile, user-accepted speed/memory exceptions and score/exit rejection; no GPU')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--self-check', action='store_true')
    mode.add_argument('--check', action='store_true')
    args = parser.parse_args()
    if args.self_check:
        self_check()
        return
    if args.check:
        data = read(H / 'state.json')
        assert data['complete'] and data['adopted'] and data['server_exit_code'] == 0
        assert read(CONFIG) == profile(read(SPEED))
        check_scores(data['quality'], read(REFERENCE)['runs'][0]['quality'])
        for path, digest in data['files_sha256'].items():
            assert b.sha(Path(path)) == digest, path
        print('PASS accepted optional profile, fixed-target repetition and actual server checks')
        return
    assert not H.exists() and not CONFIG.exists()
    H.mkdir()
    frozen = {str(path): b.sha(path) for path in (SCRIPT, SCRIPT.with_name('bench_small_stack.py'),
              SCRIPT.with_name('bench_small_stack_retry.py'), SPEED, REFERENCE, R / 'strata-iq2_xs.json')}
    state = dict(complete=False, adopted=False, default_changed=False, speed_measurement=False, quality=[],
                 user_acceptance='User accepts the measured five-change version; speed and sampled-memory failures retained.',
                 files_sha256=frozen)
    b.save(H / 'state.json', state)
    try:
        with b.gpu_lock():
            for path, digest in frozen.items():
                assert b.sha(Path(path)) == digest, path
            m, guard, _ = retry.collector()
            speed = read(SPEED)
            m.check(speed)
            cfg = profile(speed)
            guard.H, guard.SCRIPT, guard.old_lock, guard.old_busy = H / 'guard', SCRIPT, nullcontext, b.busy
            guard.H.mkdir()
            b.busy = guard.busy
            with guard.guarded_lock():
                fixed_targets(speed, cfg, state)
                server_smoke(speed, cfg, state)
            assert read(H / 'guard/compiler-guard-results.json')['passed']
            with CONFIG.open('x', encoding='utf8') as stream:
                json.dump(cfg, stream, ensure_ascii=False, indent=2)
                stream.write('\n')
            state.update(complete=True, adopted=True, config=str(CONFIG))
            state['files_sha256'].update({str(path): b.sha(path) for path in H.rglob('*') if path.is_file() and path.name != 'state.json'})
            state['files_sha256'][str(CONFIG)] = b.sha(CONFIG)
            print('ADOPTED', CONFIG, flush=True)
    except Exception as error:
        state['error'] = f'{type(error).__name__}: {error}'
        raise
    finally:
        b.save(H / 'state.json', state)


if __name__ == '__main__':
    main()
