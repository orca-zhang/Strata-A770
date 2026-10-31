"""Reuse the existing guarded adoption/archive procedure."""
import runpy
from pathlib import Path

F=Path(__file__).resolve().parent
source=(F.parent/'build-sycl-a770-token-bytes-cache-20261024/finalize.py').read_text(encoding='utf-8')
source=source.replace('token-bytes-cache-2026-10-24','token-bytes-map-2026-10-24')
source=source.replace("measurement['performance_pass']","measurement['retention_pass']")
target=F/'finalize.py'
assert not target.exists()
target.write_text(source,encoding='utf-8')
runpy.run_path(str(target),run_name='__main__')
