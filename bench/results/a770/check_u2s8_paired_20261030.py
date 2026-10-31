"""A770 full-chain U2S8 group64/M8 follow-up. --self-check uses no GPU.

Default: build and measure under the existing shared GPU/compiler lock.
--check: recompute the existing cohort and verify its provenance; no GPU.
"""
from contextlib import nullcontext
import importlib.util
import inspect
import json
from pathlib import Path
import subprocess
import sys

R = Path(__file__).resolve().parents[3]
SCRIPT = Path(__file__).resolve()
H = R / 'build-sycl-a770-u2s8-paired-20261030-v2'
P = R / 'build-sycl-a770-u2-down-chain-20261029/fixed-audit-path'
HEADER = SCRIPT.with_name('u2s8-paired-2026-10-30.hpp')
SHAPES = (1, 2, 4, 8)


def module(path):
    spec = importlib.util.spec_from_file_location('paired_' + path.parent.name + path.stem, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


old = module(P.parent / 'run.py')
b = old.b
source = inspect.getsource(old.parse).replace('(1, 6)', 'SHAPES')
assert source.count('SHAPES') == 3
exec(compile(source, str(SCRIPT), 'exec'), globals() | {'math': old.math, 'statistics': old.statistics,
     'PASS': old.PASS, 'methods': old.methods}, namespace := {})
parse = namespace['parse']


def worker():
    original = (P / 'worker.cpp').read_text(encoding='utf-8')
    edits = [
        ('if constexpr (MODE == 3)', 'if constexpr (MODE >= 2)'),
        ('template<int MODE>\nvoid submit_down', HEADER.read_text(encoding='utf-8') + '\ntemplate<int MODE>\nvoid submit_down'),
        ('            for (; e + 4 <= end;', '''            if constexpr (MODE == 3) {
                for (; e + 8 <= end; e += 8) paired_down_tile<8>(w, hq, out, dst, e, row, L);
                if (e + 4 <= end) { paired_down_tile<4>(w, hq, out, dst, e, row, L); e += 4; }
                if (e + 2 <= end) { paired_down_tile<2>(w, hq, out, dst, e, row, L); e += 2; }
                if (e < end) paired_down_tile<1>(w, hq, out, dst, e, row, L);
            } else {
            for (; e + 4 <= end;'''),
        ('            if (e < end) down_tile<1, MODE>(w, hq, out, dst, e, row, L);',
         '            if (e < end) down_tile<1, MODE>(w, hq, out, dst, e, row, L);\n            }'),
        ('6 * N', '8 * N'), ('q, 60)', 'q, 80)'),
        ('for (int t : {1, 6})', 'for (int t : {1, 2, 4, 8})'),
        ('groups = t == 1 ? 10 : 30, per = t == 1 ? 1 : 2', 'groups = 10, per = t'),
        ('((g * per + j) * 7 + 3) % cap', '(g * per + j + 3) % cap'),
    ]
    value = original
    for before, after in edits:
        assert before in value, before
        value = value.replace(before, after)
    restored = value
    for before, after in reversed(edits): restored = restored.replace(after, before)
    assert restored == original, 'reversible kernel/shape changes'
    return value


def self_check():
    old.self_check()
    text = worker()
    assert text.count('paired_down_tile<8>') == 1 and 'groups = 10, per = t' in text
    for stride in (180, 720):
        for i in range(128):
            j = i // 2
            paired = ((j % 16) // 2) * stride + ((j % 32) // 16) * 4 + (j % 2) * 2 + (j // 32) * 8 + i % 2
            h, c = divmod(i, 64)
            prior = ((c % 32) // 4) * stride + (c // 32) * 4 + c % 4 + h * 8
            assert paired == prior
    # Independent U2 layout oracle, all integer corners, M1/2/4/8 and both Q8 halves.
    for m in SHAPES:
        acts = [[((i * 71 + t * 19) % 256) - 128 for i in range(64)] for t in range(m)]
        weights = [[(i + n * 3) % 4 for i in range(64)] for n in range(8)]
        packed = []
        for half in range(2):
            for slab in range(2):
                for n in range(8):
                    for byte in range(4):
                        packed.append(sum(weights[n][half * 32 + slab * 16 + byte * 4 + k] << (2*k) for k in range(4)))
        for t in range(m):
            for n in range(8):
                for half in range(2):
                    dot = 0
                    for k in range(32):
                        code = packed[half*64 + (k//16)*32 + n*4 + (k%16)//4]
                        dot += ((code >> (2*(k%4))) & 3) * acts[t][half*32+k]
                    dot -= sum(acts[t][half*32:half*32+32])
                    assert dot == sum((weights[n][half*32+k]-1)*acts[t][half*32+k] for k in range(32))
        cap = m * 10 + 1
        dest = [(i + 3) % cap for i in range(m * 10)]
        assert len(set(dest)) == len(dest) and all(0 <= i < cap for i in dest)
    errors = [dict(T=t, profile=p, placement=d, mode=m, max_abs=0, nrmse=0)
              for t in SHAPES for p in range(2) for d in range(3) for m in range(4)]
    for order in ('forward', 'reverse'):
        samples = [dict(T=t, placement=d, arm=a, mode=m, sample=i, total_us=100 if m == -1 else 98)
                   for t in SHAPES for d in range(3) for a, m in enumerate(old.methods(order)) for i in range(11)]
        def output(rows):
            return '\n'.join(['ERROR '+json.dumps(e) for e in errors] + ['SAMPLE '+json.dumps(s) for s in rows] + [old.PASS])
        assert len(parse(output(samples), order)['summary']) == 36
        for rows in (samples[:-1], samples + samples[-1:], [dict(s, total_us=float('nan')) for s in samples]):
            try: parse(output(rows), order)
            except AssertionError: pass
            else: raise AssertionError('bad timing coverage accepted')
    print('PASS group64 addresses, U2S8 integer dots, M1/2/4/8, unique scatter, reversible source, timing coverage/gates; no GPU')


def check(data):
    assert data['complete'] and not data['production_changed'] and not data['model_tested']
    assert [(r['type'], r['order']) for r in data['runs']] == old.ORDER
    for row in data['runs']:
        assert row['exit_code'] == 0
        assert {k: row[k] for k in ('errors', 'samples', 'summary')} == parse((H/(row['label']+'.stdout')).read_text('utf8'), row['order'])
    expected = [dict(type=ty, T=t, placement=d, mode=m) for ty in (22,16,42) for t in SHAPES for d in range(3) for m in (1,2,3)
                if all(next(s for s in r['summary'] if (s['T'],s['placement'],s['mode']) == (t,d,m))['passes_screen'] for r in data['runs'] if r['type'] == ty)]
    assert data['confirmed'] == expected
    assert old.read(H/'compiler-guard-results.json')['passed']
    for path, sha in data['files_sha256'].items(): assert b.sha(R/path) == sha, path
    print('PASS six complete-chain processes, 5544 timing samples, full outputs/Q8/guards, independent orders and provenance')


def run():
    assert not H.exists(), 'preserve previous cohort'
    H.mkdir()
    (H/'worker.cpp').write_text(worker(), encoding='utf-8')
    (H/'chain.inc').write_bytes((P/'chain.inc').read_bytes())
    prior = old.read(P/'results.json')
    assert prior['complete']
    paths = [SCRIPT, HEADER, P/'worker.cpp', P/'chain.inc', P/'results.json', H/'worker.cpp', H/'chain.inc', old.IQ,
             old.RUNTIME, R/'tools/bench_deepinfer.py', R/'build-sycl-a770-value-fp16-current-v2-20261029/run.py']
    paths += [R/f'build-sycl-a770-q2-dpas-20261021/experts-{ty}.bin' for ty in (22,16,42)]
    failed = R / 'build-sycl-a770-u2s8-paired-20261030'
    paths += [failed/'results.json', failed/'compile.log', failed/'worker.cpp',
              *sorted((failed/'frozen').iterdir())]
    # Hash large fixtures only after the shared slot is available.
    data = dict(complete=False, production_changed=False, model_tested=False, runs=[],
                rule='Full original IQ chain vs S8 M4, U2 halfword M4, U2 group64 paired-load M8. 10 experts each with actual M1/2/4/8 rows; eight banks, 80 distinct used experts. All compressed bytes unchanged. Host/device/alternating mixed; normal/16x outlier. Frozen original object, exact rebuilt output/Q8; all candidates exact each other, original NRMSE<=1e-5. Per-replay full output/guards/plan checks; independent forward/reverse, gain>=1% and drift<=3%,abs<0.3% noise. No model speed claim.')
    b.save(H/'queued.json', dict(state='waiting_for_shared_gpu_compiler_lock'))
    try:
        with b.gpu_lock():
            for p in paths:
                if p in (old.IQ, P/'worker.cpp', P/'chain.inc') or p.name.startswith('experts-'):
                    assert b.sha(p) == prior['files_sha256'][p.relative_to(R).as_posix()], p
            data['files_sha256'] = {p.relative_to(R).as_posix(): b.sha(p) for p in paths}
            # Preserve the frozen compile headers; reject changes to the prior include tree.
            for name, sha in prior['files_sha256'].items():
                if name.startswith('build-sycl-deepinfer-20261023/source/'):
                    assert b.sha(R/name) == sha, name
                    data['files_sha256'][name] = sha
            commands = [(label, command.replace(str(P),str(H)) if isinstance(command,str) else [v.replace(str(P),str(H)) for v in command]) for label,command in prior['commands']]
            data['commands'] = commands
            b.save(H/'protocol.json', data)
            b.save(H/'queued.json', dict(state='building'))
            builder = module(R/'build-sycl-deepinfer-20261023/build.py')
            for label, command in commands:
                b.busy(); print('BUILD',label,flush=True)
                with (H/(label+'.log')).open('x',encoding='utf-8') as log:
                    subprocess.run(command,cwd=R,env=builder.environment(),stdout=log,stderr=subprocess.STDOUT,check=True,timeout=1200)
            guard = module(R/'build-sycl-a770-value-fp16-current-v2-20261029/run.py')
            guard.H = H; guard.old_lock = nullcontext
            b.save(H/'queued.json',dict(state='measuring'))
            runtime = old.read(old.RUNTIME)
            with guard.guarded_lock():
                for ty,order in old.ORDER:
                    guard.busy();label=f'{ty}-{order}';print('RUN',label,flush=True)
                    with (H/(label+'.stdout')).open('x',encoding='utf-8') as out,(H/(label+'.stderr')).open('x',encoding='utf-8') as err:
                        child=subprocess.run([str(H/'strata-memory-path.exe'),str(R/f'build-sycl-a770-q2-dpas-20261021/experts-{ty}.bin'),str(ty),order,str(H/f'reference-{ty}.bin')],cwd=R,env=runtime,stdout=out,stderr=err,timeout=1800)
                    row=dict(type=ty,order=order,label=label,exit_code=child.returncode)
                    data['runs'].append(row);b.save(H/'partial.json',data);child.check_returncode()
                    row.update(parse((H/(label+'.stdout')).read_text('utf8'),order));b.save(H/'partial.json',data);guard.busy()
            data['confirmed']=[dict(type=ty,T=t,placement=d,mode=m) for ty in (22,16,42) for t in SHAPES for d in range(3) for m in (1,2,3)
                if all(next(s for s in r['summary'] if (s['T'],s['placement'],s['mode'])==(t,d,m))['passes_screen'] for r in data['runs'] if r['type']==ty)]
            for p in H.iterdir():
                if p.suffix in ('.obj','.exe','.log','.stdout','.stderr','.bin','.jsonl','.ps1') or p.name=='compiler-guard-results.json':
                    data['files_sha256'][p.relative_to(R).as_posix()]=b.sha(p)
            data['complete']=True;check(data)
            b.save(H/'queued.json',dict(state='complete'))
    except BaseException as e:
        data.update(complete=False,error=f'{type(e).__name__}: {e}')
        b.save(H/'queued.json',dict(state='failed',error=data['error']))
        raise
    finally: b.save(H/'results.json',data)


if __name__=='__main__':
    if sys.argv[1:]==['--self-check']: self_check()
    elif sys.argv[1:]==['--check']: self_check();check(old.read(H/'results.json'))
    elif not sys.argv[1:]: self_check();run()
    else: raise SystemExit(__doc__)
