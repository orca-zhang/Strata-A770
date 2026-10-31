"""Check the corrected read-only host screen without replacing its predecessor."""
import importlib.util
import json
from pathlib import Path
import sys

H = Path(__file__).resolve().parent
R = H.parents[2]
spec = importlib.util.spec_from_file_location('memory_check', H / 'check_weight_memory_20261025.py')
c = importlib.util.module_from_spec(spec)
spec.loader.exec_module(c)


def main():
    data = json.loads((H / 'host-readonly-cpuinit-2026-10-25.json').read_text(encoding='utf-8'))
    assert data['complete'] and not data['production_changed'] and not data['whole_model_gain_claimed']
    assert data['predecessor_qualified'] and data['cpu_only_weight_initialization']
    assert c.sha(Path(__file__)) == data['checker_sha256']
    assert c.sha(Path(c.__file__)) == data['helper_sha256']
    assert c.sha(H / 'host-readonly-cpuinit-2026-10-25.md') == data['report_sha256']
    d = data['experiment']
    assert d['name'] == 'host-readonly'
    c.check(d)
    if '--local' in sys.argv:
        assert c.sha(R / data['source_results']) == data['source_results_sha256']
        for name, digest in d['files_sha256'].items():
            assert c.sha(R / name) == digest, name
        before = R / 'build-sycl-a770-host-readonly-20261025/worker.cpp'
        after = (R / data['source_results']).parent / 'worker.cpp'
        old = 'q->memcpy(weights+l*raw.size(),raw.data(),raw.size()).wait();'
        new = 'std::memcpy(weights+l*raw.size(),raw.data(),raw.size());'
        source = before.read_text(encoding='utf-8')
        assert source.count(old) == 1 and after.read_text(encoding='utf-8') == source.replace(old, new)
        assert c.sha(before) == data['predecessor_worker_sha256']
    print('PASS CPU-only weight initialization screen,360 checks,165 batches and all paired gains/drift')


if __name__ == '__main__':
    main()
