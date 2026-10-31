"""Check the local diagnostic evidence; no model run and no downloads."""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
WORK = ROOT / 'build-sycl-a770-40tps-recovery-20261026'


def main():
    rows = []
    for mode in ('nan', 'graph', 'expertwait', 'ordinary', 'gdn-xmx-model', 'gdn-capacity', 'gdn-spec3'):
        result = json.loads((WORK / mode / 'results.json').read_text(encoding='utf8'))
        assert result['complete'] and result['exit_code'] == 0, mode
        expected = [('Short', 16), ('Code2K', 128), ('Prose2K', 128), ('Code2K', 128)]
        assert [(r['case'], r['generated']) for r in result['rows']] == expected, mode
        for row, (case, count) in zip(result['rows'], expected):
            assert len(row['tokens']) == count and row['finish'] == 'length' and row['reused'] == 0
            assert row['prompt_tokens'] == (23 if case == 'Short' else 2048)
            assert any(row['tokens']), mode
            if not mode.startswith('gdn-'):
                assert row['matches'], (mode, case)
        assert result['rows'][1]['tokens'] == result['rows'][3]['tokens'], mode
        rows.append(result)
    for row_index in (0, 1, 2, 3):
        assert len({tuple(r['rows'][row_index]['tokens']) for r in rows[-3:]}) == 1
    checked = json.loads((WORK / 'gdn-build-v2/checked.json').read_text(encoding='utf8'))
    assert checked['complete']
    assert [(r['mode'], r['exit_code']) for r in checked['checks']] == [('widen', 0), ('xmx', 0), ('0', 1)]
    for check in checked['checks']:
        log = WORK / 'gdn-build-v2' / ('check-' + check['mode'] + '-resume.log')
        assert hashlib.sha256(log.read_bytes()).hexdigest() == check['log_sha256']
    for path, sha in {
        'strata-iq2_xs.json': 'f3412f382cd9b546aec53db27c8f2917e8869f9502805402fc37421f25d6a6d8',
        'build-sycl-a770-prefill-cache-tail-production-v3-20261026/bin/strata-unified-all-operators.exe':
            '820d23e6771ced1ba18817b8863c9cb39413369d86b707d5156a7563f7bc56b3',
        'build-sycl-a770-40tps-recovery-20261026/gdn-build-v2/bin/strata-unified-all-operators.exe':
            checked['exe_sha256'],
    }.items():
        assert hashlib.sha256((ROOT / path).read_bytes()).hexdigest() == sha, path
    print('PASS: 28 requests, 21 complete 2K/128 outputs, repeats, GDN checks and unchanged production hashes')


if __name__ == '__main__':
    main()
