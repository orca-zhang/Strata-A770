"""Freeze the current tokenizer and a local rank-table reference candidate."""
import cProfile
import hashlib
import importlib.util
import io
import json
import pstats
import shutil
from pathlib import Path

F = Path(__file__).resolve().parent
R = F.parent
assert not (F / 'preparation.json').exists()
reference = R / 'bench/results/a770/tokenizer-translate-2026-10-24/repeat'
source = (R / 'tools/strata_tokenizer.py').read_bytes()
old = b'            best, best_rank = None, None\r\n            for i in range(len(parts) - 1):'
new = b'            best, best_rank = None, None\r\n            ranks = self.ranks\r\n            for i in range(len(parts) - 1):'
lookup = b'                r = self.ranks.get((parts[i], parts[i + 1]))'
assert source.count(old) == source.count(lookup) == 1
candidate = source.replace(old, new).replace(lookup, b'                r = ranks.get((parts[i], parts[i + 1]))')
for arm, data in (('baseline', source), ('candidate', candidate)):
    (F / arm).mkdir()
    (F / arm / 'strata_tokenizer.py').write_bytes(data)
for p in (R / 'serve').iterdir():
    if p.is_file() and p.suffix in ('.py', '.jinja', '.pyd'):
        dest = F / 'frozen/serve' / p.name
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(p, dest)
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
prep = json.loads((reference / 'preparation.json').read_text(encoding='utf-8'))
prep.pop('repeat_reason')
prep.pop('repeat_gate')
prep.update(purpose='Reuse the rank dictionary locally inside short-word BPE, as the existing heap path already does.',
            source_sha256=hashlib.sha256(source).hexdigest(), candidate_sha256=hashlib.sha256(candidate).hexdigest(),
            frozen_serving_sha256={p.relative_to(F).as_posix(): sha(p) for p in (F/'frozen').rglob('*') if p.is_file()},
            adoption_rule='User policy: operator gains above zero may be retained; other performance threshold 1%. '
                          'Exact token IDs, errors and output parity required. Exclude adjacent baseline drift >3%; '
                          'retain if each measured scope has positive eligible pooled gain in both screen and confirm, '
                          'with at least one eligible case in every candidate round, and no full-prompt case pooled regression >2%. '
                          'No repeat-until-pass. Sparse coverage remains reported; no model speedup claim.',
            operator_protocol='Logical CPU 40; screen and confirm each independent-process A/B/A/B/A; '
                              'warmup 3, 7 batches, 256 BPE calls per batch. '
                              'Input lengths 1/2/4/8/16/32/64, 64-character unmergeable, and 128-character heap fallback.',
            prompt_protocol='Same two-phase A/B/A/B/A, warmup 3, 7 batches; short 100 calls, '
                            '8K characters 3 calls, 32K characters 1 call. Same immutable dependencies and templates.')
for p, digest in prep['production_sha256'].items():
    assert sha(Path(p)) == digest
(F / 'preparation.json').write_text(json.dumps(prep, indent=2)+'\n', encoding='utf-8')
# Keep the existing independently launched timing scaffold; only add an operator mode below it.
copy = (reference / 'run.py').read_text(encoding='utf-8')
(F / 'run.py').write_text(copy, encoding='utf-8')
spec = importlib.util.spec_from_file_location('timing', F / 'run.py')
timing = importlib.util.module_from_spec(spec)
spec.loader.exec_module(timing)
tok = timing.tokenizer(timing.load('baseline'), prep)
profiler = cProfile.Profile()
profiler.enable()
for text in ('The quick brown fox jumps over a measured system.\n' * 1024,
             'def f(x): return [v+x for v in range(12345)]\n' * 1024):
    tok.encode(text)
profiler.disable()
output = io.StringIO()
pstats.Stats(profiler, stream=output).strip_dirs().sort_stats('tottime').print_stats(16)
(F / 'profile.txt').write_text(output.getvalue(), encoding='utf-8')
print(output.getvalue())
print('FROZEN', prep['source_sha256'], prep['candidate_sha256'])
