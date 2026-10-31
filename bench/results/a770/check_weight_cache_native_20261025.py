"""Check the cache-hint GU native-code audit; --local also verifies raw evidence."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path

H = Path(__file__).resolve().parent
R = H.parents[2]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--local', action='store_true')
    args = ap.parse_args()
    d = json.loads((H / 'weight-cache-native-2026-10-25.json').read_text(encoding='utf-8'))
    assert d['same_instructions_through_unique_eot'] and not d['same_complete_assembly']
    assert len(d['active_prefix_sha256']) == len(d['complete_assembly_sha256']) == 2
    assert len(set(d['active_prefix_sha256'])) == 1 and len(set(d['complete_assembly_sha256'])) == 2
    assert d['unique_eot_count'] == [1, 1] and d['trailing_label_unreferenced'] == [True, True]
    assert sha(H / 'weight-cache-hint-2026-10-25.json') == d['benchmark_sha256']
    if args.local:
        path = R / 'build-sycl-a770-weight-cache-native-audit-20261025/run.py'
        spec = importlib.util.spec_from_file_location('native_audit', path)
        obj = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(obj)
        result = obj.H / 'results.json'
        assert sha(result) == d['raw_audit_sha256']
        obj.audit(json.loads(result.read_text(encoding='utf-8')))
        files = [next((obj.S / mode / 'disasm').glob('*gu_pair_wg128*.asm')) for mode in ('baseline', 'candidate')]
        assert [sha(p) for p in files] == d['complete_assembly_sha256']
        assert [hashlib.sha256(obj.active(p.read_text(encoding='utf-8')).encode('utf-8')).hexdigest() for p in files] == d['active_prefix_sha256']
    print('PASS GU native instruction audit; no new performance claim')


if __name__ == '__main__':
    main()
