"""Archive the rejected rank-local-reference experiment without changing source."""
import importlib.util
import json
import shutil
import subprocess
import sys
from pathlib import Path

F = Path(__file__).resolve().parent
R = F.parent
A = R/'bench/results/a770/bpe-ranks-2026-10-24'
assert not (A/'archive.json').exists()
spec=importlib.util.spec_from_file_location('audit',A/'check_results.py')
audit=importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)


def copy(source,target):
    target.parent.mkdir(parents=True,exist_ok=True)
    shutil.copyfile(source,target)


def save(path,data):
    path.write_text(json.dumps(data,indent=2)+'\n',encoding='utf-8')


for p in F.iterdir():
    if p.is_file() and p.suffix in ('.json','.log','.txt','.py'):
        copy(p,A/p.name)
for p in (F/'precheck-skipped').iterdir():
    copy(p,A/'precheck-skipped'/p.name)
prep=audit.read('preparation.json')
for arm in ('baseline','candidate'):
    copy(F/arm/'strata_tokenizer.py',A/arm/'strata_tokenizer.py')
for name in prep['frozen_serving_sha256']:
    if not name.endswith('.pyd'):
        copy(F/name,A/name)
runs,pairs=audit.derive()
assert audit.read('results.json')['runs']==runs and audit.read('results.json')['pairs']==pairs
measurement=audit.summarize(pairs)
assert not measurement['retention_pass']
live=R/'tools/strata_tokenizer.py'
assert audit.sha(live)==prep['source_sha256']
result=subprocess.run([sys.executable,'-m','unittest','serve.test_detok.HeapBpe.test_token_bytes_cold_and_cached',
                       'serve.test_detok.HeapBpe.test_byte_mapping_is_utf8','-v'],cwd=R,capture_output=True,text=True)
(A/'live-tests.log').write_text(result.stdout+result.stderr,encoding='utf-8')
result.check_returncode()
copy(R/'serve/test_detok.py',A/'regression/test_detok.py')
for name,digest in prep['production_sha256'].items():
    assert audit.sha(Path(name))==digest
save(A/'summary.json',dict(decision='not_adopted',measurement=measurement,
                           scope='BPE operator and CPU prompt preparation; no model timing.'))
save(A/'final-checks.json',dict(runtime_source_sha256=audit.sha(live),production_sha256=prep['production_sha256'],
                               live_regression_returncode=result.returncode,model_speedup_claim=False))
for name in ('tools/bench_deepinfer.py','build-sycl-deepinfer-20261023/check_bf16_arena.py'):
    copy(R/name,A/'tooling'/Path(name).name)
save(A/'archive.json',dict(sha256={p.relative_to(A).as_posix():audit.sha(p) for p in sorted(A.rglob('*'))
                                  if p.is_file() and '__pycache__' not in p.parts and p.name!='archive.json'},
                          omitted_binary_sha256={name:digest for name,digest in prep['frozen_serving_sha256'].items()
                                                 if name.endswith('.pyd')}))
audit.main()
