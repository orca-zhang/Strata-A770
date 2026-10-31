"""Freeze a one-line decode candidate and reuse the previous experiment runner."""
import hashlib
import json
import shutil
from pathlib import Path

F = Path(__file__).resolve().parent
R = F.parent
OLD = R / 'build-sycl-a770-token-bytes-map-20261024'
assert not (F / 'preparation.json').exists()
sha = lambda data: hashlib.sha256(data).hexdigest()
source = (R / 'tools/strata_tokenizer.py').read_bytes()
assert sha(source) == 'baf22f4806974a8fff5a54cef6526d4d05f06e81344f3f081dc9effec59fcad8'
old = b'b"".join(self.token_bytes(i) for i in ids)'
new = b'b"".join(map(self.token_bytes, ids))'
assert source.count(old) == 1
candidate = source.replace(old, new)
for arm, data in (('baseline', source), ('candidate', candidate)):
    (F / arm).mkdir()
    (F / arm / 'strata_tokenizer.py').write_bytes(data)
for p in (R / 'serve').iterdir():
    if p.is_file() and p.suffix in ('.py', '.jinja', '.pyd'):
        dest = F / 'frozen/serve' / p.name
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(p, dest)
prep = json.loads((OLD / 'preparation.json').read_text())
prep.update(source_sha256=sha(source), candidate_sha256=sha(candidate),
            frozen_sha256={p.relative_to(F).as_posix(): sha(p.read_bytes())
                           for p in (F / 'frozen').rglob('*') if p.is_file()},
            runner_template_sha256=sha((OLD / 'run.py').read_bytes()),
            cases=dict(decode64=8192, decode_code2k=256, decode_prose2k=256,
                       decode16k=64, decode_cold4k=8, stream4k=32),
            protocol='CPU only; shared GPU/compiler lock; logical CPU 40; independent-process '
                     'screen and confirm A/B/A/B/A; five warmups, seven timed batches; '
                     'construction and encode outside timing; profiler disabled.',
            adoption_rule='User rule: positive operator gain may be retained, general threshold 1%. '
                          'Require exact outputs/errors and regression tests with zero skips. '
                          'Exclude absolute baseline drift >3%. Primary decode16k must have positive '
                          'eligible pooled gain in each phase (at least one eligible pair each). '
                          'Report all cases, exclusions and unchanged streaming control; '
                          'no additional attempts to seek a pass. No model speedup claim.')
for path, digest in prep['production_sha256'].items():
    assert sha(Path(path).read_bytes()) == digest
for name, digest in prep['tokenizer_sha256'].items():
    assert sha((Path(prep['tokenizer_path']) / name).read_bytes()) == digest
run = (OLD / 'run.py').read_text(encoding='utf-8')
start = run.index('    cold = [i for i, word')
end = run.index('    cases = []', start)
run = run[:start] + '''    code = tok.encode(('def f(x):\\n    return {"square": x**2, "text": "hello"}  # comment\\n') * 256)[:2048]
    prose = tok.encode(('A quick brown fox jumps over the lazy dog. Here is a longer sentence.\\n') * 256)[:2048]
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
''' + run[end:]
run = run.replace('"""Frozen byte-cache checks and independent-process CPU A/B/A/B/A."""',
                  '"""Frozen decode checks and independent-process CPU A/B/A/B/A."""')
(F / 'run.py').write_text(run, encoding='utf-8')
(F / 'preparation.json').write_text(json.dumps(prep, indent=2) + '\n', encoding='utf-8')
print('FROZEN', prep['source_sha256'], prep['candidate_sha256'])
