"""Frozen decode checks and independent-process CPU A/B/A/B/A."""
import ctypes
import hashlib
import importlib.util
import json
import math
import os
import random
import statistics
import subprocess
import sys
import timeit
import unittest
from pathlib import Path

F = Path(__file__).resolve().parent
R = F.parent
P = json.loads((F / 'preparation.json').read_text(encoding='utf-8'))


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def save(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8')


def load(arm):
    spec = importlib.util.spec_from_file_location('strata_tokenizer_' + arm, F / arm / 'strata_tokenizer.py')
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def tokenizer(mod):
    t = Path(P['tokenizer_path'])
    vocab = json.loads((t / 'vocab.json').read_text(encoding='utf-8'))
    tokens = [None] * len(vocab)
    for token, index in vocab.items():
        tokens[index] = token
    return mod.Tokenizer(tokens, (t / 'merges.txt').read_text(encoding='utf-8').split('\n'),
                         json.loads((t / 'token_type.json').read_text()))


def stream_class():
    sys.path.insert(0, str(F / 'frozen'))
    from serve.server import Detokenizer
    return Detokenizer


def outcome(call):
    try:
        return 'value', call()
    except (IndexError, KeyError, TypeError, UnicodeDecodeError) as error:
        return type(error).__name__, error.args


def checks():
    a, b = load('baseline'), load('candidate')
    ta, tb = tokenizer(a), tokenizer(b)
    byte_checks, errors = 0, 0
    for i in range(len(ta.tokens)):
        first = outcome(lambda: ta.token_bytes(i))
        assert first == outcome(lambda: tb.token_bytes(i)), i
        assert first == outcome(lambda: ta.token_bytes(i)) == outcome(lambda: tb.token_bytes(i)), i
        errors += first[0] != 'value'
        byte_checks += 2
    for i in (-1, -len(ta.tokens), len(ta.tokens), len(ta.tokens)+1, '0', None, [], {}):
        assert outcome(lambda: ta.token_bytes(i)) == outcome(lambda: tb.token_bytes(i))
    sa, sb = a.Tokenizer(['', a.BYTE_TO_UNICODE[0], chr(0x10ffff)], []), b.Tokenizer(['', b.BYTE_TO_UNICODE[0], chr(0x10ffff)], [])
    for i in range(3):
        assert outcome(lambda: sa.token_bytes(i)) == outcome(lambda: sb.token_bytes(i))
    assert sa.token_bytes(0) == sb.token_bytes(0) == b''
    other = b.Tokenizer([b.BYTE_TO_UNICODE[0]], [])
    other.token_bytes(0)
    assert other._bytes_cache is not sb._bytes_cache
    dclass = stream_class()
    ba = a.Tokenizer([a.BYTE_TO_UNICODE[i] for i in range(256)], [])
    bb = b.Tokenizer([b.BYTE_TO_UNICODE[i] for i in range(256)], [])
    rng, streaming_checks, decode_checks = random.Random(P['seed']), 0, 0
    for i in range(64):
        raw = [rng.randrange(256) for _ in range(rng.randrange(1, 256))]
        da, db = dclass(ba), dclass(bb)
        for token in raw:
            assert (da.push(token), da.pending()) == (db.push(token), db.pending())
            streaming_checks += 1
        for policy in ('replace', 'ignore', 'strict'):
            assert outcome(lambda: ba.decode(raw, errors=policy)) == outcome(lambda: bb.decode(raw, errors=policy))
            decode_checks += 1
    os.environ['STRATA_TOKENIZER'] = P['tokenizer_path']
    sys.modules['strata_tokenizer'] = b
    from serve import test_detok
    assert Path(test_detok.__file__).resolve().is_relative_to(F / 'frozen')
    suite = unittest.defaultTestLoader.loadTestsFromName('serve.test_detok')
    with (F / 'tests.log').open('w', encoding='utf-8') as log:
        result = unittest.TextTestRunner(stream=log, verbosity=2).run(suite)
    assert result.wasSuccessful() and not result.skipped
    save(F / 'checks.json', dict(complete=True, vocabulary=len(ta.tokens), repeated_byte_checks=byte_checks,
                               vocabulary_error_tokens=errors, invalid_id_checks=8, synthetic_token_checks=3,
                               streaming_prefix_checks=streaming_checks, decode_error_policy_checks=decode_checks,
                               regression_tests=result.testsRun, failures=len(result.failures), errors=len(result.errors),
                               skips=len(result.skipped)))
    print('PASS correctness', byte_checks, 'byte comparisons;', result.testsRun, 'regressions', flush=True)


def affinity():
    k = ctypes.WinDLL('kernel32', use_last_error=True)
    k.GetCurrentProcess.restype = ctypes.c_void_p
    k.SetProcessAffinityMask.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
    if not k.SetProcessAffinityMask(k.GetCurrentProcess(), 1 << 40):
        raise ctypes.WinError(ctypes.get_last_error())


def child(arm, out):
    affinity()
    mod, dclass = load(arm), stream_class()
    tok = tokenizer(mod)
    text = ('The quick brown fox jumps over the lazy dog. def f(x): return x**2\n'
            '\u4f60\u597d\uff0c\u4e16\u754c\u3002 caf\u00e9 e\u0301 \U0001f469\u200d\U0001f4bb \ud55c\uad6d\uc5b4\n') * 2048
    ids = tok.encode(text)[:16384]
    assert len(ids) == 16384
    code = tok.encode(('def f(x):\n    return {"square": x**2, "text": "hello"}  # comment\n') * 256)[:2048]
    prose = tok.encode(('A quick brown fox jumps over the lazy dog. Here is a longer sentence.\n') * 256)[:2048]
    assert len(code) == len(prose) == 2048
    cold = random.Random(P['seed']).sample(range(len(tok.tokens)), 4096)

    def cold_decode():
        tok._bytes_cache.clear()
        return tok.decode(cold)

    def stream():
        detok = dclass(tok)
        return ''.join(detok.push(i) for i in ids[:4096])

    calls = dict(decode64=lambda: tok.decode(ids[:64]), decode_code2k=lambda: tok.decode(code),
                 decode_prose2k=lambda: tok.decode(prose), decode16k=lambda: tok.decode(ids),
                 decode_cold4k=cold_decode, stream4k=stream)
    cases = []
    for name, call in calls.items():
        expected = call()
        for _ in range(5):
            assert call() == expected
        number = P['cases'][name]
        values = [v/number for v in timeit.repeat(call, number=number, repeat=7)]
        assert call() == expected
        encoded = expected if isinstance(expected, bytes) else expected.encode('utf-8')
        cases.append(dict(name=name, seconds=values, median_seconds=statistics.median(values),
                          calls_per_batch=number, output_sha256=hashlib.sha256(encoded).hexdigest()))
    save(out, dict(arm=arm, cpu=40, python=sys.version, source_sha256=sha(F / arm / 'strata_tokenizer.py'),
                   input_ids_sha256=hashlib.sha256(json.dumps(ids).encode()).hexdigest(), cases=cases))


def main():
    assert not (F / 'results.json').exists()
    for path, digest in P['production_sha256'].items():
        assert sha(Path(path)) == digest
    for path, digest in P['frozen_sha256'].items():
        assert sha(F / path) == digest
    checks()
    sys.path.insert(0, str(R / 'tools'))
    import bench_deepinfer as bench
    with bench.gpu_lock():
        state = dict(complete=False, runs=[], pairs=[], adoption_rule=P['adoption_rule'])
        save(F / 'results.json', state)
        for phase in ('screen', 'confirm'):
            runs = []
            for i, arm in enumerate(('baseline', 'candidate', 'baseline', 'candidate', 'baseline')):
                print('BENCH', phase, i, arm, flush=True)
                out = F / f'{phase}-{i}.json'
                result = subprocess.run([sys.executable, str(Path(__file__).resolve()), 'child', arm, str(out)],
                                        cwd=R, capture_output=True, text=True, timeout=240)
                (F / f'{phase}-{i}.log').write_text(result.stdout + result.stderr, encoding='utf-8')
                result.check_returncode()
                run = json.loads(out.read_text(encoding='utf-8'))
                runs.append(run)
                state['runs'].append(dict(run, phase=phase, ordinal=i, raw_file=out.name))
                save(F / 'results.json', state)
            for i in (1, 3):
                for case in runs[i]['cases']:
                    samples = [next(c for c in runs[j]['cases'] if c['name'] == case['name']) for j in (i-1, i, i+1)]
                    assert len({s['output_sha256'] for s in samples}) == 1
                    assert len({r['input_ids_sha256'] for r in runs}) == 1
                    a, b, c = (s['median_seconds'] for s in samples)
                    drift, gain = 100*(c/a-1), 100*(math.sqrt(a*c)/b-1)
                    state['pairs'].append(dict(phase=phase, candidate_round=i, name=case['name'],
                                               baseline_drift_pct=drift, throughput_gain_pct=gain, eligible=abs(drift) <= 3))
            save(F / 'results.json', state)
            valid = [p for p in state['pairs'] if p['phase'] == phase and p['eligible']]
            print('PHASE', phase, 'eligible', len(valid), 'positive', sum(p['throughput_gain_pct'] > 0 for p in valid), flush=True)
        for path, digest in P['production_sha256'].items():
            assert sha(Path(path)) == digest
        for name, digest in P['tokenizer_sha256'].items():
            assert sha(Path(P['tokenizer_path']) / name) == digest
        assert sha(R / 'tools/strata_tokenizer.py') == P['source_sha256']
        state.update(complete=True, production_unchanged=True, live_source_still_baseline=True)
        save(F / 'results.json', state)
        print('COMPLETE measurements; adoption requires separate gate audit', flush=True)


if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1] == 'child':
        child(sys.argv[2], Path(sys.argv[3]))
    else:
        main()
