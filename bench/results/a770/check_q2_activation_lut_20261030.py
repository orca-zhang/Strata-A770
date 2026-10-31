"""Isolated A770 full-expert-chain Q2 activation-LUT experiment; no production edits.

--self-check is CPU-only; --build/--run reuse the local frozen experiment objects.
"""
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import re
import statistics
import subprocess
import sys

R = Path(__file__).resolve().parents[3]
H = R / 'build-sycl-a770-q2-activation-lut-20261030'
PAIR = '--pair' in sys.argv
if PAIR:
    sys.argv.remove('--pair')
    H = H.with_name(H.name + '-pair')
P = R / 'build-sycl-a770-down-unroll-20261028'
FRAGMENT = Path(__file__).with_name('q2-activation-lut-2026-10-30.hpp')
sys.path.insert(0, str(R / 'tools'))
import bench_deepinfer as b


def module(path):
    spec = importlib.util.spec_from_file_location('lut_' + path.parent.name, path)
    m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
    return m


def self_check():
    for code in range(256):
        lo = (code & 1) | ((code >> 1) & 2) | ((code >> 2) & 4) | ((code >> 3) & 8)
        hi_byte = code >> 1
        hi = (hi_byte & 1) | ((hi_byte >> 1) & 2) | ((hi_byte >> 2) & 4) | ((hi_byte >> 3) & 8)
        for x in ((-128,) * 4, (127,) * 4, (0,) * 4, (-128, 127, -1, 1), (13, -79, 52, -117)):
            table = [sum(x[i] for i in range(4) if mask & (1 << i)) for mask in range(16)]
            expected = sum((((code >> (2 * i)) & 3) - 1) * x[i] for i in range(4))
            assert table[lo] + 2 * table[hi] - table[15] == expected
            assert all(-32768 <= v <= 32767 for v in table)
            direct = [[sum((((mask >> (2 * i)) & 3) - 1) * x[start + i] for i in range(2))
                       for mask in range(16)] for start in (0, 2)]
            assert direct[0][code & 15] + direct[1][code >> 4] == expected
    print('PASS: all 256 packed Q2 bytes, signed int8 corners/zero, exact subset-LUT integer dots')


def build():
    H.mkdir(exist_ok=True)
    if (H / 'build.json').exists():
        raise RuntimeError('build already recorded')
    source = (P / 'schedule.cpp').read_text(encoding='utf-8')
    source = source.replace('down_unroll', 'q2_activation_lut').replace('downunroll', 'q2activationlut')
    source = source.replace('DOWN_UNROLL', 'Q2_ACTIVATION_LUT')
    marker = '} // namespace strata::q2_activation_lut_detail'
    assert source.count(marker) == 1
    source = source.replace(marker, ('#define STRATA_Q2_LUT_PAIR 1\n' if PAIR else '') + FRAGMENT.read_text(encoding='utf-8') + '\n' + marker)
    start = source.rindex('    if (mode == 2) {')
    stop = source.index('    } else {', start)
    source = source[:start] + '''    if (mode == 2) {
        auto* table = reinterpret_cast<int16_t*>(static_cast<uint8_t*>(scratch) + native_expert_scratch_bytes(cap_entries,640));
        q2_lut_build(hq,grp_start,n_groups,int(cap_entries),table,s);
        q2_lut_down(grp_ptr,grp_start,n_groups,ent_dst,hq,table,L,out,int(gy),s);
''' + source[stop:]
    worker = (P / 'worker.cpp').read_text(encoding='utf-8')
    worker = worker.replace('downunroll', 'q2activationlut').replace('DOWN_UNROLL', 'Q2_ACTIVATION_LUT')
    worker = worker.replace('scratch_bytes=k::native_expert_scratch_bytes(CAP,640)',
                            'scratch_bytes=k::native_expert_scratch_bytes(CAP,640)+size_t(CAP)*20*128*sizeof(int16_t)')
    worker = worker.replace('if(edge)continue;', 'if(edge || (t!=1 && t!=4 && t!=6))continue;')
    worker = worker.replace('l<48;++l)launch', 'l<16;++l)launch').replace('i<16;++i)replay()', 'i<4;++i)replay()')
    worker = worker.replace('b<11;++b', 'b<5;++b').replace('j<4;++j)replay()', 'j<2;++j)replay()')
    worker = worker.replace('/4/48', '/2/16').replace('edge?0:6', 'edge?0:3')
    if PAIR: worker = worker.replace('size_t(CAP)*20*128', 'size_t(CAP)*20*256')
    assert 'q2_lut_build' in source and 'sizeof(int16_t)' in worker
    (H / 'schedule.cpp').write_text(source, encoding='utf-8')
    (H / 'worker.cpp').write_text(worker, encoding='utf-8')
    prior = json.loads((P / 'build.json').read_text())
    commands = [(label, cmd.replace(str(P), str(H)) if isinstance(cmd, str)
                 else [s.replace(str(P), str(H)) for s in cmd]) for label, cmd in prior['commands']]
    env = module(R / 'build-sycl-deepinfer-20261023/build.py').environment()
    data = dict(complete=False, commands=commands, files_sha256={str(p.relative_to(R)): b.sha(p) for p in
                (Path(__file__), FRAGMENT, P / 'schedule.cpp', P / 'worker.cpp', H / 'schedule.cpp', H / 'worker.cpp',
                 R / 'build-sycl-a770-vram-down-sg16/iq.obj')})
    b.save(H / 'build.json', data)
    with b.gpu_lock():
        for label, cmd in commands:
            print('BUILD', label, flush=True)
            with (H / (label + '.log')).open('x', encoding='utf-8') as log:
                p = subprocess.run(cmd, cwd=R, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=600)
            if p.returncode:
                print((H / (label + '.log')).read_text(errors='replace')[-5000:])
            p.check_returncode()
    data['complete'] = True
    data['files_sha256'].update({str(p.relative_to(R)): b.sha(p) for p in (H / 'schedule.obj', H / 'worker.obj', H / 'probe.exe')})
    b.save(H / 'build.json', data)


def parse(text, edge=False):
    assert text.rstrip().endswith('PASS')
    if edge:
        assert 'EDGE_CHECKS 48 PASS' in text
        return None
    rows = [json.loads(s[7:]) for s in text.splitlines() if s.startswith('RESULT ')]
    samples = [json.loads(s[8:]) for s in text.splitlines() if s.startswith('SAMPLES ')]
    assert [r['T'] for r in rows] == [1, 4, 6]
    assert [r['checks'] for r in rows] == [8, 32, 48]
    assert '"empty":1,"graph":3' in text
    for r, s in zip(rows, samples):
        assert s['T'] == r['T'] and len(s['us']) == 5
        assert all(math.isfinite(v) and v > 0 for v in s['us'])
        assert abs(statistics.median(s['us']) - r['us']) <= 0.00000051
    return dict(rows=rows, samples=samples)


def comparison(runs):
    result = {}
    for layout in ('vram', 'mixed8', 'mapped'):
        arms = {r['label']: {v['T']: v['us'] for v in r['rows']} for r in runs if r['layout'] == layout}
        result[layout] = {str(t): dict(vs_original=b.paired(arms['A1'][t], arms['B1'][t], arms['A2'][t]),
                                      vs_control=b.paired(arms['C1'][t], arms['B1'][t], arms['C2'][t])) for t in (1, 4, 6)}
    return result


def witness(run, edge=False):
    text = (H / (run['stem'] + '.stderr')).read_text(encoding='utf-8')
    rows = [tuple(map(int, row)) for row in re.findall(r'MODEL_Q2_ACTIVATION_LUT cap=(\d+) mode=(\d+) active=(\d+)', text)]
    caps = {10, 12, 16, 19, 20} if edge else {10, 20, 30, 40, 50, 60}
    assert len(rows) == len(caps) and {row[0] for row in rows} == caps
    assert all(row[1:] == (run['mode'], int(run['mode'] != 0)) for row in rows)


def run():
    built = json.loads((H / 'build.json').read_text()); assert built['complete']
    for name, sha in built['files_sha256'].items(): assert b.sha(R / built.get('archived_files', {}).get(name, name)) == sha, name
    if (H / 'protocol.json').exists(): raise RuntimeError('run already recorded')
    weights = R / 'build-sycl-a770-q2-dpas-20261021/experts-22.bin'
    env = module(R / 'build-sycl-a770-decode-gpu-profile-20261028/run.py').environment()
    d = dict(complete=False, adopted=False, production_changed=False, gpu_speed_tested=True, model_tested=False,
             rule='Full IQ2_S/Q2 expert chain; unchanged GU,SwiGLU,Q8 and payload.0 frozen,1 rebuilt control,2 exact-int16 activation LUT+Down4.64-entry extra LUT scratch327680bytes;table build included each chain.No timing claims outside measured fixtures.8banks/240 real experts;graph16chains,warm4,5batches*2replays.T1/4/6;A1/C1/B1/C2/A2 in VRAM,mixed every8th mapped,all mapped.Gain>=1%,drift<=3% vs both original/control;confirm any passing candidate independently.Quantization/row layout unchanged.All48 eager bank/shape outputs+Q8+guards and3graphs checked each timing worker;random/outlier group lengths1..7 edge checks.No retries/exclusions.',
             files_sha256=built['files_sha256'] | {str(weights.relative_to(R)): b.sha(weights)}, checks=[], runs=[])
    d['lut_group'] = 2 if PAIR else 4
    d['extra_scratch_bytes'] = 655360 if PAIR else 327680
    if PAIR:
        d['rule'] = d['rule'].replace('exact-int16 activation LUT', 'exact-int16 direct two-activation LUT')
        d['rule'] = d['rule'].replace('327680bytes', '655360bytes')
    with b.gpu_lock():
        b.save(H / 'protocol.json', d)
        def worker(label, mode, layout, reference, write=False, edge=None):
            runtime = env | {'STRATA_Q2_ACTIVATION_LUT': str(mode), 'STRATA_Q2_ACTIVATION_LUT_MIXED_EVERY': '8'}
            if edge: runtime['STRATA_GU_Q8_EDGE_CHECK'] = edge
            command = [str(H / 'probe.exe'), str(weights), '22', str(reference), 'write' if write else 'check', '0',
                       'mixed' if layout == 'mixed8' else layout]
            stem = label + '-' + layout
            print('RUN', stem, 'mode', mode, flush=True)
            with (H / (stem + '.stdout')).open('x', encoding='utf-8') as out, (H / (stem + '.stderr')).open('x', encoding='utf-8') as err:
                p = subprocess.run(command, cwd=R, env=runtime, stdout=out, stderr=err, timeout=300)
            text = (H / (stem + '.stdout')).read_text(encoding='utf-8')
            if p.returncode: print((H / (stem + '.stderr')).read_text(encoding='utf-8')[-3000:], flush=True)
            p.check_returncode()
            parsed = parse(text, bool(edge))
            run = dict(label=label, mode=mode, layout=layout, exit_code=p.returncode, stem=stem, **(parsed or {}))
            witness(run, bool(edge))
            return run
        for fixture in ('random', 'outlier'):
            ref = H / (fixture + '.bin')
            for mode in (0, 1, 2):
                d['checks'].append(worker(f'check-{fixture}-{mode}', mode, 'mixed8', ref, mode == 0, fixture))
                b.save(H / 'partial.json', d)
        for label, mode in (('A1', 0), ('C1', 1), ('B1', 2), ('C2', 1), ('A2', 0)):
            for layout in ('vram', 'mixed8', 'mapped'):
                d['runs'].append(worker(label, mode, layout, H / (layout + '.bin'), label == 'A1'))
                b.save(H / 'partial.json', d)
        d['comparison'] = comparison(d['runs']); d['complete'] = True
        for r in d['checks'] + d['runs']:
            for ext in ('.stdout', '.stderr'):
                p = H / (r['stem'] + ext); d['files_sha256'][str(p.relative_to(R))] = b.sha(p)
        b.save(H / 'results.json', d)
    for layout, rows in d['comparison'].items(): print(layout, json.dumps(rows), flush=True)


def check():
    d = json.loads((H / 'results.json').read_text()); assert d['complete'] and not d['adopted']
    assert len(d['checks']) == 6 and len(d['runs']) == 15
    assert d['comparison'] == comparison(d['runs'])
    for r in d['checks'] + d['runs']:
        got = parse((H / (r['stem'] + '.stdout')).read_text(), r in d['checks'])
        witness(r, r in d['checks'])
        if got: assert got == {k: r[k] for k in ('rows', 'samples')}
    for name, sha in d['files_sha256'].items(): assert b.sha(R / d.get('archived_files', {}).get(name, name)) == sha, name
    print('PASS: full eager/graph outputs, Q8 bytes/guards, all 225 timing batches, pair calculations and file hashes')


if __name__ == '__main__':
    if sys.argv[1:] == ['--self-check']: self_check()
    elif sys.argv[1:] == ['--build']: self_check(); build()
    elif sys.argv[1:] == ['--run']: run()
    elif sys.argv[1:] == ['--check']: self_check(); check()
    else: raise SystemExit(__doc__)
