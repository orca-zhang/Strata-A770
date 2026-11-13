"""CPU-only regression for the GPU-checked Orca M16 projection and retained M4/BF16 kernels."""
import hashlib
import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('orca_prepare', ROOT/'sycl/orca/prepare.py')
prepare = importlib.util.module_from_spec(spec)
spec.loader.exec_module(prepare)
source = (ROOT/'sycl/src/prefill/gemm.dp.cpp').read_text(encoding='utf8')
generated = prepare.xmx_kernel(source)
assert hashlib.sha256(generated.encode()).hexdigest() == '12f4ff9a4005fb5a327e931a6b6ad616cdb533e5ddefdbe2061e05ef48ece336'
try:
    prepare.xmx_kernel(source.replace('c[r], a, b, c[r]', 'c[r], a, b, a'))
except ValueError:
    pass
else:
    raise AssertionError('changed math accepted without review')
print('PASS GPU-checked Orca M16/M4/BF16 generated source and rejection of changed SYCL math; no GPU')
