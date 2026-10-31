import hashlib
import json
from pathlib import Path
import shutil
import subprocess

H = Path(__file__).resolve().parent
R = H.parent
B = R / 'build-sycl-deepinfer-20261023/candidate'

def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()

def main():
    for directory in ('include', 'sycl/include', 'third_party/ggml'):
        shutil.copytree(R / directory, H / 'frozen' / directory)
    for side in ('baseline', 'candidate'):
        (H / side).mkdir()
        shutil.copyfile(R / 'sycl/src/kernels/cuda/kv_q8.dp.cpp', H / side / 'kv_q8.dp.cpp')
    for name in ('kv_steps_parity.cpp', 'kv_q8_parity.cpp'):
        shutil.copyfile(R / 'sycl/src/kernels' / name, H / name)
    # Same reference code and same dependency objects in both arms.
    for name in ('strata_kernels.lib', 'strata_core.lib'):
        shutil.copyfile(B / name, H / name)
    commands = subprocess.run(['ninja', '-C', str(B), '-t', 'commands', 'kv_steps_parity.exe'],
                              capture_output=True, text=True, check=True).stdout
    (H / 'original-commands.txt').write_text(commands, encoding='utf-8')
    protected = [R / 'build-sycl-a770-unified-operators/bin/strata-unified-all-operators.exe', R / 'strata-iq2_xs.json']
    files = {str(p.relative_to(H)): sha(p) for p in H.rglob('*') if p.is_file()}
    result = {'files': files, 'protected': {str(p.relative_to(R)): sha(p) for p in protected},
              'source_sha256': sha(R / 'sycl/src/kernels/cuda/kv_q8.dp.cpp'),
              'git_status': subprocess.run(['git', 'status', '--short'], capture_output=True, text=True, check=True).stdout}
    (H / 'preparation.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print('PASS frozen source, headers, dependencies and production hashes')

if __name__ == '__main__':
    main()
