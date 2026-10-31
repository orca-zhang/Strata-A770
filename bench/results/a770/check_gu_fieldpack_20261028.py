"""Reuse complete-sample dual-control accounting for same-size field packing."""
import importlib.util
from pathlib import Path

H = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('gu_control_audit', H / 'check_gu_schedule_20261028.py')
p = importlib.util.module_from_spec(spec); spec.loader.exec_module(p)
p.RAW = p.R / 'build-sycl-a770-gu-fieldpack-20261028/screen-results.json'
p.OUT = H / 'gu-fieldpack-2026-10-28.json'
p.ORDER = [(label, 1 if mode == 128 else mode) for label, mode in p.ORDER]

if __name__ == '__main__':
    p.main()
