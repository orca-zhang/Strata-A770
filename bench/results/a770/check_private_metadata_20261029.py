"""CPU-only audit of ten existing native metadata blocks; no compiler or GPU calls."""
import hashlib
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[3]
OUT = Path(__file__).with_suffix('.json')
SOURCES = ('build-sycl-a770-igc-spill-native-20261029/resource-recovery.json',
           'build-sycl-a770-gr-native-20261029/results.json')


def sha(path):
    assert path.stat().st_size < 1_000_000, 'offline audit only hashes small metadata'
    return hashlib.sha256(path.read_bytes()).hexdigest()


def parse(text, kernel):
    # ponytail: parse only this fixed ze_info schema; reject ambiguous names and fields.
    body = text.split('\nkernels_misc_info:', 1)[0]
    blocks = [s for s in body.split('\n  - name:')[1:]
              if s.splitlines()[0].strip() == kernel and '\n    execution_env:' in s]
    assert len(blocks) == 1, 'missing or duplicate exact kernel'
    block = blocks[0]
    props = {}
    for key in ('grf_count', 'simd_size', 'private_size', 'spill_size', 'slm_size'):
        values = re.findall(r'^      ' + key + r':\s+(\d+)\s*$', block, re.M)
        assert len(values) <= 1, key
        if values:
            props[key] = int(values[0])
    assert {'grf_count', 'simd_size'} <= props.keys()
    buffers = []
    if '\n    per_thread_memory_buffers:' in block:
        section = block.split('\n    per_thread_memory_buffers:', 1)[1]
        section = re.split(r'\n    [a-z_]+:', section, maxsplit=1)[0]
        for item in section.split('\n      - type:')[1:]:
            values = re.fullmatch(r'\s*(\w+)\s+usage:\s+(\w+)\s+size:\s+(\d+)\s*'
                                  r'(?:is_simt_thread:\s+(true|false)\s*)?', item)
            assert values, 'unsupported memory buffer schema'
            buffers.append(dict(type=values[1], usage=values[2], size=int(values[3]),
                                is_simt_thread=values[4] == 'true'))
    return dict(properties=props, buffers=buffers,
                private_base_arguments=len(re.findall(r'arg_type:\s+private_base_stateless\b', block)))


def collect():
    rows = []
    hashes = {s: sha(ROOT / s) for s in SOURCES}
    for source in SOURCES:
        data = json.loads((ROOT / source).read_text('utf8'))
        assert data['complete']
        targets = {t['label']: t['kernel'] for t in data.get('targets', []) if 'label' in t}
        selected = [r for r in data['runs'] if r.get('mode') in (None, 'baseline')]
        assert len(selected) == (6 if targets else 4)
        for row in selected:
            path = Path(row['metadata'])
            digest = sha(path)
            assert digest == data['files_sha256'][str(path)], str(path)
            hashes[path.relative_to(ROOT).as_posix()] = digest
            kernel = targets[row['target']] if targets else row['kernel']
            parsed = parse(path.read_text('utf8'), kernel)
            assert all(parsed['properties'][k] == v for k, v in row['properties'].items()
                       if k in parsed['properties'])
            rows.append(dict(label=row.get('target', 'gr_' + row.get('stage', '')),
                             kernel=kernel, metadata=path.relative_to(ROOT).as_posix(), **parsed))
    private = [r['label'] for r in rows if r['properties'].get('private_size', 0)
               or r['private_base_arguments'] or any(b['usage'] == 'private_space' for b in r['buffers'])]
    assert set(private) == {'gr_up', 'gr_reduce', 'gr_norm'}, private
    return dict(complete=True, gpu_calls=0, compiler_calls=0, model_speed_acceptance=False,
                scope='Existing metadata only; no current EXE or archive identity recheck.',
                global_private_labels=private, rows=rows, files_sha256=hashes)


def self_check():
    fixture = '\nkernels:\n  - name: exact\n    execution_env:\n      grf_count: 128\n      simd_size: 16\n      private_size: 20096\n    payload_arguments:\n      - arg_type: private_base_stateless\n    per_thread_memory_buffers:\n      - type: global\n        usage: private_space\n        size: 1256\nkernels_misc_info:\n  - name: exact\n    args_info:\n      - address_qualifier: __private\n'
    p = parse(fixture, 'exact')
    assert p['properties']['private_size'] == 20096 and p['private_base_arguments'] == 1
    assert p['buffers'] == [dict(type='global', usage='private_space', size=1256, is_simt_thread=False)]
    assert parse(fixture.replace('size: 1256', 'size: 1256\n        is_simt_thread: true'), 'exact')['buffers'][0]['is_simt_thread']
    for bad in ('', fixture.replace('name: exact', 'name: other'),
                fixture.split('\nkernels_misc_info:')[0] * 2,
                fixture.replace('      grf_count: 128', '      grf_count: 128\n      grf_count: 256')):
        try:
            parse(bad, 'exact')
        except AssertionError:
            pass
        else:
            raise AssertionError('ambiguous metadata accepted')


if __name__ == '__main__':
    assert sys.argv[1:] in ([], ['--check'], ['--self-check'])
    self_check()
    if '--self-check' not in sys.argv:
        result = collect()
        if '--check' in sys.argv:
            assert result == json.loads(OUT.read_text('utf8'))
        else:
            with OUT.open('x', encoding='utf8') as stream:
                json.dump(result, stream, indent=2)
                stream.write('\n')
        print('PASS ten exact metadata blocks; only GR up/reduce/norm report global private; no GPU/speed claim')
    else:
        print('PASS exact-name, duplicate-field and private-buffer parser checks; no GPU')
