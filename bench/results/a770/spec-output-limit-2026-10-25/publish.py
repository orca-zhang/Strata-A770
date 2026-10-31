"""Archive text evidence only, without replacing any production executable/config."""
import difflib
import importlib.util
import json
from pathlib import Path
import shutil
import sys

H = Path(__file__).resolve().parent
R = H.parent
sys.path.insert(0, str(R/'tools'))
import bench_deepinfer as b

spec = importlib.util.spec_from_file_location('experiment', H/'run.py')
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)
runner.verify()
checks = json.loads((H/'checks/results.json').read_text())
measured = json.loads((H/'measure/results.json').read_text())
assert checks['complete'] and measured['complete']
assert (R/'sycl/src/program/generate.cpp').read_text() == (H/'candidate.cpp').read_text()
out = R/'bench/results/a770/spec-output-limit-2026-10-25'
out.mkdir(exist_ok=False)
for folder in ('checks', 'checks-attempt1', 'measure'):
    shutil.copytree(H/folder, out/folder)
for name in ('run.py', 'check.py', 'measure.py', 'publish.py', 'check_results.py', 'frozen.json',
             'build.json', 'config.json', 'config-external.json', 'external-config-change.json',
             'cpu.txt', 'cpu-final.txt', 'compile-queue-fix.txt', 'link-queue-fix.txt'):
    shutil.copyfile(H/name, out/name)
(out/'source').mkdir()
for name in ('before.cpp', 'candidate.cpp'):
    shutil.copyfile(H/name, out/'source'/name)
shutil.copyfile(R/'tools/test_spec_window_limit.py', out/'test_spec_window_limit.py')
(out/'window-limit.patch').write_text(''.join(difflib.unified_diff(
    (H/'before.cpp').read_text().splitlines(True), (H/'candidate.cpp').read_text().splitlines(True),
    fromfile='a/sycl/src/program/generate.cpp', tofile='b/sycl/src/program/generate.cpp')), encoding='utf-8')
b.save(out/'delivery.json', {'runtime_source_sha256': b.sha(R/'sycl/src/program/generate.cpp'),
    'regression_sha256': b.sha(R/'tools/test_spec_window_limit.py'),
    'production_exe_sha256': b.sha(Path(json.loads((H/'config.json').read_text())['exe'])),
    'production_config_sha256': b.sha(R/'strata-iq2_xs.json'),
    'screen_passed': measured['screen_passed'], 'confirm_passed': measured['confirm_passed'],
    'scope': 'SYCL output-budget bounds; fresh Short prompt, 4/6/16 output-token measurements. Not the four-scenario model matrix.'})
print('PASS text evidence archived; write README then manifest and run check_results.py', flush=True)
