"""Apply the measured candidate under the user's explicit updated retention rule."""
import datetime
import hashlib
import json
import shutil
import subprocess
import sys
from pathlib import Path

R = Path(__file__).resolve().parent.parent
A = R / 'bench/results/a770/token-bytes-cache-2026-10-24'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read(name):
    return json.loads((A / name).read_text(encoding='utf-8'))


def save(name, data):
    (A / name).write_text(json.dumps(data, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')


assert not (A / 'adoption-update.json').exists()
prep, summary, manifest = read('preparation.json'), read('summary.json'), read('archive.json')
assert summary['decision'] == 'not_adopted'
assert all(c['eligible'] and c['pooled_gain_pct'] > 0 for c in summary['measurement']['cases'])
assert all(p['throughput_gain_pct'] > 0 for p in read('results.json')['pairs'])
for name, digest in manifest['sha256'].items():
    assert sha(A / name) == digest, name
for name, digest in prep['production_sha256'].items():
    assert sha(Path(name)) == digest
live = R / 'tools/strata_tokenizer.py'
baseline, candidate = (A / 'baseline/strata_tokenizer.py').read_bytes(), (A / 'candidate/strata_tokenizer.py').read_bytes()
assert live.read_bytes() == baseline and hashlib.sha256(candidate).hexdigest() == prep['candidate_sha256']
for name in ('summary.json', 'final-checks.json', 'live-tests.log'):
    stem, suffix = name.rsplit('.', 1)
    shutil.copyfile(A / name, A / (stem + '.before-adoption.' + suffix))
live.write_bytes(candidate)
try:
    result = subprocess.run([sys.executable, '-m', 'unittest',
                             'serve.test_detok.HeapBpe.test_token_bytes_cold_and_cached',
                             'serve.test_detok.HeapBpe.test_byte_mapping_is_utf8', '-v'],
                            cwd=R, capture_output=True, text=True)
    (A / 'live-tests.log').write_text(result.stdout + result.stderr, encoding='utf-8')
    result.check_returncode()
except BaseException:
    if live.read_bytes() == candidate:
        live.write_bytes(baseline)
    raise
for name, digest in prep['production_sha256'].items():
    assert sha(Path(name)) == digest
save('adoption-update.json', dict(
    user_instruction='门槛更新成1%，算子的收益只要是正的都接受',
    updated_at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
    general_performance_min_gain_pct=1.0, operator_gain_must_exceed_pct=0.0,
    baseline_drift_exclusion_pct=3.0, original_coverage_required_for_retention=False,
    reason='User explicitly accepts positive operator gains after reviewing sparse coverage. '
           'All five cases have positive eligible pooled gains, all 10 eligible pairs are positive. '
           'Preserve original coverage failures; retain source without a model speedup claim.',
    operator_retention_pass=True,
    original_measurement_sha256={name: digest for name, digest in manifest['sha256'].items()
                                 if name in ('results.json', 'checks.json', 'preparation.json')
                                 or name.startswith(('screen-', 'confirm-'))}))
summary.update(decision='adopted', decision_basis='adoption-update.json; explicit user update after measurements')
save('summary.json', summary)
final = read('final-checks.json')
final.update(runtime_source_sha256=sha(live), live_regression_returncode=result.returncode)
save('final-checks.json', final)
shutil.copyfile(Path(__file__), A / 'tooling/adopt.py')
manifest['sha256'] = {p.relative_to(A).as_posix(): sha(p) for p in sorted(A.rglob('*'))
                      if p.is_file() and '__pycache__' not in p.parts and p.name != 'archive.json'}
save('archive.json', manifest)
print('ADOPTED', sha(live), '; production hashes unchanged; two live regressions pass')
