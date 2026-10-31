"""Publish allocation liveness and successful program calls; traced timings are not speed evidence."""
from collections import Counter
import copy
import hashlib
import json
from pathlib import Path
import re
import sys

H = Path(__file__).resolve().parent; R = H.parents[2]
TASK = R/'build-sycl-a770-usm-allocation-trace-20261028'
OUT = H/'usm-allocation-2026-10-29.json'
CASES = ('READY','Short','Code2K','Prose2K')
OPS = ('urProgramCreateWithIL','urProgramBuildExp','urProgramBuild','urKernelCreate')


def programs(lines):
    stage = 'startup'; done = 0; result = {}; checkpoints = []
    for line in lines:
        line = line.strip()
        if line.startswith('READY '):
            assert stage == 'startup'; checkpoints.append('READY'); stage = 'Short'
        elif line.startswith('DONE '):
            assert stage in CASES[1:]; checkpoints.append(stage); done += 1
            stage = ('Code2K','Prose2K','after-requests')[done-1]
        if any(line.startswith('<--- '+op+'(') for op in OPS):
            match = re.fullmatch(r'<--- ('+'|'.join(OPS)+r')\(.*\) -> (UR_RESULT_[A-Z_]+);',line)
            assert match,'malformed/interleaved program return'
            if match[2] == 'UR_RESULT_SUCCESS': result.setdefault(stage,Counter())[match[1]] += 1
    assert checkpoints == list(CASES)
    return {stage:dict(counts) for stage,counts in result.items()}


def check(d):
    assert d['complete'] and d['scope'] == 'observed-ur-usm-and-program-diagnostic'
    assert not any(d[k] for k in ('production_changed','adopted','speed_acceptance','target_achieved'))
    assert d['unmatched_free_count'] == 0 and not d['failed_calls']
    points = d['checkpoints']; assert [r['case'] for r in points] == list(CASES)
    for previous,current in zip(points,points[1:]):
        counts = d['allocation_counts'][current['case']]
        for kind in ('Device','Host','Shared'):
            assert current['live_bytes'][kind] == previous['live_bytes'][kind]+counts[kind]['allocated_bytes']-counts[kind]['freed_bytes']
            assert current['live_allocations'][kind] == previous['live_allocations'][kind]+counts[kind]['allocations']-counts[kind]['frees']
    assert [r['case'] for r in d['physical_memory']] == list(CASES)
    assert all(r['samples'] > 0 and 0 <= r['min_free_mib'] <= r['median_free_mib'] <= r['max_free_mib'] for r in d['physical_memory'])
    assert d['program_calls']['Short']['urProgramCreateWithIL'] > 0
    assert d['trace_sha256'] and d['full_outputs_match'] and d['engine_exit_code'] == 0


def build():
    sys.path.insert(0,str(R/'build-sycl-a770-u4s4-grouped-20261027'))
    from lease import gpu_lock
    import bench_deepinfer as b
    with gpu_lock():
        analysis = json.loads((TASK/'analysis.json').read_text(encoding='utf8'))
        raw = json.loads((TASK/'results.json').read_text(encoding='utf8'))
        assert raw['complete'] and analysis['complete']
        trace = TASK/'U1.protocol'
        assert b.sha(trace) == analysis['trace_sha256'] == raw['files_sha256'][trace.relative_to(R).as_posix()]
        with trace.open(encoding='utf8') as lines: calls = programs(lines)
        ledger = analysis['ledger']; arm = raw['runs'][0]
        d = dict(complete=True,scope='observed-ur-usm-and-program-diagnostic',production_changed=False,
            adopted=False,speed_acceptance=False,target_achieved=False,hardware='A77016GB / PCIe3.0 / Windows / DPC++2025.3.2',
            allocation_counts=ledger['counts'],checkpoints=ledger['checkpoints'],program_calls=calls,
            unmatched_free_count=len(ledger['unmatched_frees']),failed_calls=ledger['failed_calls'],
            physical_memory=analysis['physical_memory'],trace_sha256=analysis['trace_sha256'],engine_exit_code=arm['exit_code'],
            full_outputs_match=all(row['tokens'] == raw['reference'][row['case']] for row in [arm['initializer'],*arm['rows']]),
            limits=analysis['limits']+' Program calls do not prove allocation causality or native module byte size.')
        files = [TASK/'analysis.json',TASK/'results.json',TASK/'loaded-runtime.json',TASK/'analyze.py',Path(__file__)]
        d['files_sha256'] = {p.relative_to(R).as_posix():b.sha(p) for p in files}
        check(d); return d


if __name__ == '__main__':
    if '--self-check' in sys.argv:
        rows = ['READY 1','<--- urKernelCreate(.x = 1) -> UR_RESULT_SUCCESS;','DONE 16','DONE 128','DONE 128']
        assert programs(rows) == {'Short':{'urKernelCreate':1}}
        for bad in (rows[:-1],[row.replace(' -> UR_RESULT_SUCCESS;','') for row in rows]):
            try: programs(bad)
            except AssertionError: pass
            else: raise AssertionError('accepted incomplete/malformed trace')
    if '--write' in sys.argv:
        assert not OUT.exists(); OUT.write_text(json.dumps(build(),indent=2)+'\n',encoding='utf8')
    if OUT.exists():
        data = json.loads(OUT.read_text(encoding='utf8')); check(data)
        if '--self-check' in sys.argv:
            bad = copy.deepcopy(data); bad['checkpoints'][1]['live_bytes']['Device'] += 1
            try: check(bad)
            except AssertionError: pass
            else: raise AssertionError('accepted broken liveness accounting')
        if '--local' in sys.argv:
            for path,digest in data['files_sha256'].items():
                assert hashlib.sha256((R/path).read_bytes()).hexdigest() == digest,path
    else: assert '--self-check' in sys.argv
    print('PASS successful program-call parser and observed allocation ledger;no speed or causality acceptance')
