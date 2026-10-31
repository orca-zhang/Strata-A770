"""Freeze a native-iteration token byte conversion candidate."""
import hashlib
import json
import shutil
from pathlib import Path

F = Path(__file__).resolve().parent
R = F.parent
assert not (F/'preparation.json').exists()
source = (R/'tools/strata_tokenizer.py').read_bytes()
old = '''            raw = bytearray()
            for ch in self.tokens[i]:
                v = UNICODE_TO_BYTE.get(ch)
                if v is None:
                    raise KeyError("token %d contains a character outside the byte alphabet: %r" % (i, ch))
                raw.append(v)
            b = cache[i] = bytes(raw)'''.replace('\n','\r\n').encode()
new = '''            try:
                b = cache[i] = bytes(map(UNICODE_TO_BYTE.__getitem__, self.tokens[i]))
            except KeyError as exc:
                raise KeyError("token %d contains a character outside the byte alphabet: %r" % (i, exc.args[0])) from None'''.replace('\n','\r\n').encode()
assert source.count(old) == 1
candidate = source.replace(old, new)
for arm, data in (('baseline',source),('candidate',candidate)):
    (F/arm).mkdir()
    (F/arm/'strata_tokenizer.py').write_bytes(data)
for p in (R/'serve').iterdir():
    if p.is_file() and p.suffix in ('.py','.jinja','.pyd'):
        dest=F/'frozen/serve'/p.name
        dest.parent.mkdir(parents=True,exist_ok=True)
        shutil.copyfile(p,dest)
sha=lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
reference=R/'bench/results/a770/token-bytes-cache-2026-10-24'
prep=json.loads((reference/'preparation.json').read_text())
prep.update(source_sha256=hashlib.sha256(source).hexdigest(),candidate_sha256=hashlib.sha256(candidate).hexdigest(),
            frozen_sha256={p.relative_to(F).as_posix():sha(p) for p in (F/'frozen').rglob('*') if p.is_file()},
            adoption_rule='User rule: operator positive gain may be retained; other performance threshold 1%. '
                          'Exact bytes, errors, incremental states and decode parity required. Exclude >3% baseline drift. '
                          'Require a positive eligible pooled gain for cold_bytes4k in both phases and at least one '
                          'eligible cold pair in each phase. Other cases are unchanged-path controls and reported '
                          'separately, without requiring every control to improve. No repeats to seek a pass. '
                          'No model speedup claim.')
(F/'preparation.json').write_text(json.dumps(prep,indent=2)+'\n',encoding='utf-8')
shutil.copyfile(reference/'run.py',F/'run.py')
print('FROZEN',prep['source_sha256'],prep['candidate_sha256'])
