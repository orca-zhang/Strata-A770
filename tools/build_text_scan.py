"""Optional SSE2 parser scans: python tools/build_text_scan.py build_ext --inplace.

Uses the installed Python headers and system C compiler; no model/GPU build.
Other CPUs compile the same scanner with a scalar loop.
"""
from pathlib import Path
import os

from setuptools import Extension, setup

os.chdir(Path(__file__).resolve().parents[1])
setup(name="strata-text-scan", ext_modules=[Extension("serve._text_scan", ["serve/_text_scan.c"])])
