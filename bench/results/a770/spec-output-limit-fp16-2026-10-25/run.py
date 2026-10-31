"""FP16 validation and timing using existing frozen A/B executables."""
import importlib.util
import json
from pathlib import Path
import sys
H=Path(__file__).resolve().parent
R=H.parent
sys.path.insert(0,str(R/'tools'))
import bench_deepinfer as b

def module(path):
    spec=importlib.util.spec_from_file_location(path.stem,path)
    m=importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m

def verify():
    frozen=json.loads((H/'frozen.json').read_text())
    for name,digest in frozen['files_sha256'].items():
        assert b.sha(R/name)==digest,name
    module(R/'build-sycl-window-limit-20261024/run.py').verify()

if __name__=='__main__':
    with b.gpu_lock():
        verify()
        print('PASS frozen FP16 test inputs and reused binaries',flush=True)
        module(H/'check.py').main()
        module(H/'measure.py').main()
        verify()
        b.save(H/'delivery.json',{'complete':True,'production_unchanged':True,'source_unchanged':True})
