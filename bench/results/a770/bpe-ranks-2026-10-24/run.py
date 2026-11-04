"""Frozen tokenizer A/B/A/B/A, CPU only; no model or HTTP throughput measurement."""
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
from pathlib import Path

F = Path(__file__).resolve().parent
R = F.parent
def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def save(path, data):
    path.write_text(json.dumps(data, indent=2), encoding='utf-8')

def load(arm):
    path = F / arm / 'strata_tokenizer.py'
    spec = importlib.util.spec_from_file_location('strata_tokenizer_' + arm, path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod

def affinity():
    k = ctypes.WinDLL('kernel32', use_last_error=True)
    k.GetCurrentProcess.restype = ctypes.c_void_p
    k.GetProcessAffinityMask.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_size_t), ctypes.POINTER(ctypes.c_size_t)]
    k.SetProcessAffinityMask.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
    p, allowed, system = k.GetCurrentProcess(), ctypes.c_size_t(), ctypes.c_size_t()
    if not k.GetProcessAffinityMask(p, ctypes.byref(allowed), ctypes.byref(system)):
        raise ctypes.WinError(ctypes.get_last_error())
    cpus = [i for i in range(64) if allowed.value & (1 << i)]
    cpu = cpus[-4] if len(cpus) >= 4 else cpus[-1]
    if not k.SetProcessAffinityMask(p, 1 << cpu):
        raise ctypes.WinError(ctypes.get_last_error())
    return cpu

def tokenizer(mod, preparation):
    t = Path(preparation['tokenizer_path'])
    vocab = json.loads((t / 'vocab.json').read_text(encoding='utf-8'))
    tokens = [None] * len(vocab)
    for token, index in vocab.items(): tokens[index] = token
    return mod.Tokenizer(tokens, (t / 'merges.txt').read_text(encoding='utf-8').split('\n'),
                         json.loads((t / 'token_type.json').read_text()))

def child(arm, out):
    cpu = affinity()
    preparation = json.loads((F / 'preparation.json').read_text())
    mod = load(arm)
    tok = tokenizer(mod, preparation)
    sys.path.insert(0, str(F / 'frozen'))
    from serve import frontend, server
    assert Path(server.__file__).resolve().is_relative_to(F / 'frozen')
    svc = server.Service(server.MockEngine(tok, 'ok', max_context=131072), tok,
                         frontend.ChatTemplate(Path(preparation['tokenizer_path']) / 'chat_template.jinja'))
    prose = 'Reference text about rivers, systems and clear explanations, with values 123 and punctuation.\n'
    code = 'def update(items, value):\n    return [item + value for item in items if item >= 0]  # stable order\n'
    unicode = '\u4f60\u597d\uff0c\u4e16\u754c\u3002\u65e5\u672c\u8a9e\u306e\u30c6\u30ad\u30b9\u30c8 \U0001f600 caf\u00e9 e\u0301 \ud55c\uad6d\uc5b4\n'
    def repeat(text, length): return (text * (length // len(text) + 1))[:length]
    inputs = [('short', 'Explain this code and give one concrete example.'),
              ('prose8k', repeat(prose, 8192)), ('code8k', repeat(code, 8192)),
              ('unicode8k', repeat(unicode, 8192)), ('prose32k', repeat(prose, 32768))]
    checks = []
    for name, word in [('n1', '1'), ('n2', 'if'), ('n4', 'test'), ('n8', 'function'),
                       ('n16', 'abcdefghijklmnop'), ('n32', 'abcdefgh' * 4), ('n64', 'a' * 64),
                       ('unmergeable64', mod.BYTE_TO_UNICODE[0] * 64), ('heap128', 'a' * 128)]:
        expected = tok._bpe(word)
        call = lambda word=word: tok._bpe(word)
        for _ in range(3): assert call() == expected
        samples = [v/256 for v in timeit.repeat(call, number=256, repeat=7)]
        assert call() == expected
        checks.append({'scope':'BPE.operator', 'name':name, 'chars':len(word), 'calls_per_batch':256,
                       'seconds':samples, 'median_seconds':statistics.median(samples), 'tokens':len(expected),
                       'token_ids_sha256':hashlib.sha256(json.dumps(expected).encode('ascii')).hexdigest()})
    for scope in ('Tokenizer.encode', 'Service.encode_prompt'):
        for name, text in inputs:
            if scope == 'Service.encode_prompt' and name == 'short': text = 'Explain </think> and <|im_end|> literally.'
            call = (lambda text=text: tok.encode(text, parse_special=True)) if scope == 'Tokenizer.encode' else (
                lambda text=text: svc.encode_prompt([{'role':'user', 'content':text}], None, {}))
            ids = call()
            number = 100 if len(text) < 256 else 3 if len(text) < 10000 else 1
            for _ in range(3): call()
            samples = [v / number for v in timeit.repeat(call, number=number, repeat=7)]
            observed = call()
            assert observed == ids
            row = {'scope':scope, 'name':name, 'chars':len(text), 'calls_per_batch':number,
                   'seconds':samples, 'median_seconds':statistics.median(samples), 'tokens':len(ids),
                   'token_ids_sha256':hashlib.sha256(json.dumps(ids).encode('ascii')).hexdigest()}
            checks.append(row)
    save(out, {'arm':arm, 'cpu':cpu, 'python':sys.version, 'vocab_size':len(tok.tokens), 'merges':len(tok.ranks),
               'native_text_scan':str(frontend._text_scan.__file__) if frontend._text_scan else None,
               'source_sha256':sha(F / arm / 'strata_tokenizer.py'), 'cases':checks})

def differential():
    a, b = load('baseline'), load('candidate')
    for arm in (a, b):
        alphabet = bytes(range(256))
        assert ''.join(arm.BYTE_TO_UNICODE[v] for v in alphabet) == alphabet.decode('latin1').translate(arm.BYTE_TO_UNICODE)
    ta = a.Tokenizer([a.BYTE_TO_UNICODE[i] for i in range(256)], [])
    tb = b.Tokenizer([b.BYTE_TO_UNICODE[i] for i in range(256)], [])
    rng = random.Random(20261024)
    for i in range(10000):
        text = ''.join(chr(rng.choice([rng.randrange(0xD800), rng.randrange(0xE000,0x110000)]))
                       for _ in range(rng.randrange(70)))
        expected = list(text.encode('utf-8'))
        assert ta.encode(text) == tb.encode(text) == expected, i
    for text in ('\ud800', 'before\udfffafter'):
        for tok in (ta, tb):
            try: tok.encode(text)
            except UnicodeEncodeError: pass
            else: raise AssertionError('surrogate accepted')
    preparation = json.loads((F / 'preparation.json').read_text())
    ta, tb = tokenizer(a, preparation), tokenizer(b, preparation)
    sys.path.insert(0, str(R))
    os.environ['STRATA_TOKENIZER'] = preparation['tokenizer_path']
    sys.path.insert(0, str(F/'frozen'))
    sys.modules['strata_tokenizer'] = b
    from serve.test_detok import random_text
    for i in range(400):
        text = random_text(rng, 12)
        for special in (False, True):
            plain = [(j, j + 1) for j in range(0, len(text), 53)]
            assert ta.encode(text,special) == tb.encode(text,special)
            assert ta.encode(text,special,plain) == tb.encode(text,special,plain)
    save(F / 'differential.json', {'pass':True, 'seed':20261024, 'raw_alphabet_bytes':256,
                                  'byte_only_utf8_comparisons':10000, 'surrogate_errors':4,
                                  'real_vocab_token_id_comparisons':1600})

    # Existing heap implementation is independent of the changed short-word scan.
    import unittest
    from serve import test_detok
    assert Path(test_detok.__file__).resolve().is_relative_to(F/'frozen')
    suite = unittest.defaultTestLoader.loadTestsFromName('serve.test_detok')
    with (F/'tests.log').open('w',encoding='utf-8') as log:
        result = unittest.TextTestRunner(stream=log,verbosity=2).run(suite)
    assert result.wasSuccessful() and not result.skipped
    save(F/'regressions.json',dict(tests=result.testsRun,failures=len(result.failures),errors=len(result.errors),skips=len(result.skipped)))

def main():
    assert not (F / 'results.json').exists()
    preparation = json.loads((F / 'preparation.json').read_text())
    for path,digest in preparation['production_sha256'].items(): assert sha(Path(path)) == digest
    differential()
    state = {'complete':False, 'adoption_rule':preparation['adoption_rule'], 'runs':[], 'pairs':[]}
    save(F / 'results.json',state)
    for phase in ('screen','confirm'):
        runs=[]
        for i,arm in enumerate(('baseline','candidate','baseline','candidate','baseline')):
            print('BENCH',phase,i,arm,flush=True)
            out=F/f'{phase}-{i}.json'
            p=subprocess.run([sys.executable,str(Path(__file__).resolve()),'child',arm,str(out)],
                             cwd=R,capture_output=True,text=True,timeout=180)
            (F/f'{phase}-{i}.log').write_text(p.stdout+p.stderr,encoding='utf-8')
            p.check_returncode()
            run=json.loads(out.read_text());run.update(phase=phase,ordinal=i,raw_file=out.name)
            runs.append(run);state['runs'].append(run)
            save(F/'results.json',state)
        for i in (1,3):
            for case in runs[i]['cases']:
                same=lambda c:(c['scope'],c['name'])==(case['scope'],case['name'])
                samples=[next(c for c in runs[j]['cases'] if same(c)) for j in (i-1,i,i+1)]
                assert len({c['token_ids_sha256'] for c in samples})==1
                a,b,c=[s['median_seconds'] for s in samples]
                drift,gain=100*(c/a-1),100*(math.sqrt(a*c)/b-1)
                state['pairs'].append({'phase':phase,'candidate_round':i,'scope':case['scope'],'name':case['name'],
                                       'baseline_seconds':math.sqrt(a*c),'candidate_seconds':b,
                                       'baseline_drift_pct':drift,'throughput_gain_pct':gain,'eligible':abs(drift)<=3})
        valid=[p for p in state['pairs'] if p['phase']==phase and p['eligible']]
        save(F/'results.json',state)
        round_values={i:[p['throughput_gain_pct'] for p in valid if p['candidate_round']==i] for i in (1,3)}
        print('RESULT',phase,'positive',sum(p['throughput_gain_pct']>0 for p in valid),'/',len(valid),
              'round_medians',{i:statistics.median(v) if v else None for i,v in round_values.items()},flush=True)
        save(F/'results.json',state)
    for path,digest in preparation['production_sha256'].items(): assert sha(Path(path))==digest
    assert sha(R/'tools/strata_tokenizer.py')==preparation['source_sha256']
    for path,digest in preparation['tokenizer_sha256'].items(): assert sha(Path(preparation['tokenizer_path'])/path)==digest
    state.update(complete=True,production_unchanged=True,workspace_source_unchanged=True)
    save(F/'results.json',state)
    print('PASS frozen tokenizer experiment',flush=True)

if __name__ == '__main__':
    if len(sys.argv)>1 and sys.argv[1]=='child': child(sys.argv[2],Path(sys.argv[3]))
    else:
        sys.path.insert(0,str(R/'tools'))
        import bench_deepinfer as b
        # Reserve the shared slot to avoid concurrent GPU/compiler work during CPU timing.
        with b.gpu_lock(): main()
