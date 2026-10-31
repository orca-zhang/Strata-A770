"""Freeze a two-line token byte-cache candidate, without changing live code."""
import hashlib
import json
import shutil
import sys
from pathlib import Path

F = Path(__file__).resolve().parent
R = F.parent
assert not (F / 'preparation.json').exists()
source = (R / 'tools/strata_tokenizer.py').read_bytes()
old = b'        self.tokens = tokens\r\n'
new = old + b'        self._bytes_cache: dict[int, bytes] = {}\r\n'
hit = b'        cache = self.__dict__.setdefault("_bytes_cache", {})'
assert source.count(old) == source.count(hit) == 1
candidate = source.replace(old, new).replace(hit, b'        cache = self._bytes_cache')
for arm, data in (('baseline', source), ('candidate', candidate)):
    (F / arm).mkdir()
    (F / arm / 'strata_tokenizer.py').write_bytes(data)
for p in (R / 'serve').iterdir():
    if p.is_file() and p.suffix in ('.py', '.jinja', '.pyd'):
        dest = F / 'frozen/serve' / p.name
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(p, dest)
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
production = [R / 'strata-iq2_xs.json', R / 'build-sycl-a770-unified-operators/bin/strata-unified-all-operators.exe']
tokenizer = Path(r'G:\GitHub\Strata-data\packs\iq2_xs\tokenizer')
preparation = dict(source_sha256=hashlib.sha256(source).hexdigest(),
                   candidate_sha256=hashlib.sha256(candidate).hexdigest(),
                   frozen_sha256={p.relative_to(F).as_posix(): sha(p) for p in (F / 'frozen').rglob('*') if p.is_file()},
                   production_sha256={str(p): sha(p) for p in production},
                   tokenizer_path=str(tokenizer), tokenizer_sha256={p.name: sha(p) for p in tokenizer.iterdir() if p.is_file()},
                   python=sys.version, seed=20261024,
                   protocol='CPU only, shared GPU lock, logical CPU 40; screen and confirm each independent-process '
                            'A/B/A/B/A; 5 warmups and 7 timed batches; all outputs compared; no HTTP/model measurement.',
                   cases={'warm_bytes16k': 64, 'cold_bytes4k': 32, 'decode16k': 64, 'stream4k': 64, 'decode64': 4096},
                   adoption_rule='Exact bytes, decode, incremental output/pending-state and error parity. In each '
                                 'phase at least 8/10 adjacent pairs have <=3% baseline drift; every case has '
                                 'at least one eligible pair in each phase. Every candidate round has positive '
                                 'eligible median and at least 75% positive eligible pairs. No eligible case '
                                 'pooled regression worse than 2%. No minimum 3% gain; one experiment, no repeat-until-pass.')
(F / 'preparation.json').write_text(json.dumps(preparation, indent=2) + '\n', encoding='utf-8')
print('FROZEN', preparation['source_sha256'], preparation['candidate_sha256'])
