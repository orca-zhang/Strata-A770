"""Check the published raw snapshot measurements without a GPU."""
import hashlib
import json
import math
from pathlib import Path
import re
import statistics


def check(report):
    build, measured = report['build'], report['components']
    assert report['component_complete'] and build['complete'] and measured['complete']
    assert all(command['exit_code']==0 for command in build['commands']+measured['commands'])
    assert report['source_sha256']['sycl/src/core/conversation_snapshot.cpp']==build['candidate_source_sha256']
    assert report['source_sha256']['sycl/src/core/conversation_snapshot_test.cpp']==build['harness_source_sha256']
    for label, count in (('original',4280),('candidate',4392)):
        record=report['checks'][label]
        assert hashlib.sha256(record['raw'].encode('utf-8')).hexdigest()==record['sha256']
        assert f'conversation_snapshot_test: {count} checks passed' in record['raw']
    assert [run['label'] for run in measured['runs']]==['A0','B0','A1','B1','A2']
    for run in measured['runs']:
        raw=report['raw_component_logs'][run['label']]
        assert hashlib.sha256(raw.encode('utf-8')).hexdigest()==run['log_sha256']
        assert 'conversation_snapshot_copy_bench: 1242 checks passed' in raw
        rows=[]
        for line in raw.splitlines():
            if line.startswith('COPY_BENCH '):
                _,fmt,cells,size,repeat,capture,restore=line.split()
                rows.append(dict(fmt=int(fmt),cells=int(cells),bytes=int(size),repeat=int(repeat),
                                 save_us=float(capture),restore_us=float(restore)))
        assert rows==run['rows'] and len(rows)==108
        assert {(r['fmt'],r['cells'],r['repeat']) for r in rows}=={
            (fmt,cells,repeat) for fmt in range(4) for cells in (8192,32768,65536) for repeat in range(9)}
        assert all(math.isfinite(r[k]) and r[k]>0 for r in rows for k in ('save_us','restore_us'))
        assert all(r['bytes']>0 for r in rows)
    assert all(len({row['bytes'] for run in measured['runs'] for row in run['rows']
                    if row['fmt']==fmt and row['cells']==cells})==1
               for fmt in range(4) for cells in (8192,32768,65536))
    comparisons=measured['comparisons']
    assert len(comparisons)==24 and len({(c['fmt'],c['cells'],c['metric']) for c in comparisons})==24
    for comparison in comparisons:
        values=[statistics.median(row[comparison['metric']] for row in run['rows']
                if row['fmt']==comparison['fmt'] and row['cells']==comparison['cells']) for run in measured['runs']]
        assert values==comparison['medians']
        confirmed=True
        for offset,pair in zip((0,2),comparison['pairs'],strict=True):
            a,candidate,c=values[offset:offset+3]
            baseline=math.sqrt(a*c)
            expected=dict(baseline=baseline,candidate=candidate,throughput_change_pct=100*(baseline/candidate-1),
                          baseline_drift_pct=100*(c/a-1))
            assert all(math.isclose(pair[key],value,rel_tol=1e-12,abs_tol=1e-9) for key,value in expected.items())
            confirmed &= abs(expected['baseline_drift_pct'])<=3 and expected['throughput_change_pct']>=1
        assert comparison['confirmed']==confirmed
    assert sum(c['confirmed'] for c in comparisons)==9
    for attempt in report.get('model_attempts', []):
        assert not attempt['complete'] and attempt['error']
        check_logs(attempt)


def check_logs(record):
    raw = record.get('raw_logs', {})
    assert raw.keys() == record.get('log_sha256', {}).keys()
    for name, value in raw.items():
        assert hashlib.sha256(value.encode('utf-8')).hexdigest() == record['log_sha256'][name]
    if 'runner_source' in record:
        digest = hashlib.sha256(record['runner_source'].encode('utf-8')).hexdigest()
        assert digest == record['runner_source_sha256']
        assert digest == next(value for path,value in record['files_sha256'].items() if path.endswith('\\model.py'))


def check_model(report):
    if not report['model_complete']:
        return False
    model = report['model']
    assert model['complete'] and model['files_unchanged'] and not model['production_changed']
    assert [arm['label'] for arm in model['runs']] == ['A0','B0','A1','B1','A2']
    check_logs(model)
    work = {}
    for arm in model['runs']:
        assert arm['exit_code'] == 0
        assert arm['exe_sha256'] == report['current_build']['executables_sha256'][arm['exe']]
        raw = model['raw_logs'][arm['label'] + '.protocol']
        assert [line for line in raw.splitlines() if line.startswith('INFO ')] == [arm['info']]
        info = dict(re.findall(r'(\w+)=([^ ]+)', arm['info']))
        assert {k:v for k,v in info.items() if k!='vram_free_mib'} == model['runtime_contract']
        assert int(info['vram_free_mib']) >= 1024
        requests, tokens, resume = [], [], 0
        for line in raw.splitlines():
            if line.startswith('RESUME '):
                resume = int(line.split()[1])
            elif line.startswith(('T ', 'TB ')):
                tokens.extend(map(int, line.split()[1:] if line.startswith('T ') else line.split()[2:]))
            elif line.startswith('DONE '):
                fields = line.split()
                requests.append(dict(tokens=tokens, generated=int(fields[1]), prompt_tokens=int(fields[2]),
                    prompt_ms=float(fields[3]), decode_ms=float(fields[4]), finish=fields[5],
                    accepted=int(fields[6]), offered=int(fields[7]), reused=int(fields[8]), resume=resume, done=line))
                tokens, resume = [], 0
        assert not tokens and len(requests) == 9 and len(arm['rows']) == 8
        for parsed, row in zip(requests, [arm['warmup'], *arm['rows']], strict=True):
            assert all(row[key] == value for key, value in parsed.items())
            assert len(parsed['tokens']) == parsed['generated']
        assert [(r['case'],r['repeat'],r['retained']) for r in arm['rows']] == [
            (case,repeat,repeat>0) for repeat in range(4) for case in ('Code2K','Long8K')]
        for row in arm['rows']:
            assert row['generated'] == 128 and row['tokens'] == model['reference'][row['case']]
            assert row['prompt_tokens'] == len(model['input_ids'][row['case']])
            assert all(math.isfinite(row[k]) and row[k]>0 for k in ('ttft_ms','decode_ms','wall_ms'))
            assert math.isclose(row['wall_ms'],(row['host_end_ns']-row['host_start_ns'])/1e6)
            key = (row['case'], row['repeat'])
            current = {k:row[k] for k in ('accepted','offered','reused','resume','finish')}
            work.setdefault(key, current)
            assert work[key] == current
            if row['retained']:
                assert row['reused'] >= .9*row['prompt_tokens']
        log = model['raw_logs'][arm['label'] + '.stderr']
        for action, key in (('restored','restore_ms'), ('parked','park_ms')):
            assert list(map(float,re.findall(r'conversation cache: '+action+r' .*? in ([0-9.]+) ms;',log))) == arm[key]
        assert len(arm['restore_ms']) >= 6
        memory = model['raw_logs'][arm['label'] + '-memory.stdout'].replace('\r\n','\n')
        assert len(re.findall(r'^MEM_PROP index=0 error=0 location=1 physical_bytes=\d+$',memory,re.M)) == 1
        samples = [tuple(map(int,values)) for values in re.findall(
            r'^MEM index=0 error=(\d+) free_bytes=(\d+) size_bytes=(\d+) health=(\d+)$',memory,re.M)]
        assert samples and all(error==0 and 0<=free<=size and size>0 for error,free,size,health in samples)
        assert min(s[1] for s in samples)/2**20 == arm['min_free_mib'] >= 1024
        stamps = [int(re.match(r'\d+ host_ns=(\d+)',block)[1]) for block in memory.split('SAMPLE index=')[1:]
            if re.search(r'^MEM index=0 error=0 free_bytes=\d+',block,re.M)]
        for row in arm['rows']:
            assert any(row['host_start_ns']<=stamp<=row['host_end_ns'] for stamp in stamps)
            assert max(stamp for stamp in stamps if stamp<=row['host_end_ns']) >= row['host_end_ns']-2_000_000_000
    comparisons = model['comparisons']
    assert {(c['case'],c['metric']) for c in comparisons} == {
        (case,metric) for case in ('Code2K','Long8K') for metric in ('ttft_ms','decode_ms','wall_ms')}
    assert len(comparisons) == 6
    for comparison in comparisons:
        values = [statistics.median(r[comparison['metric']] for r in arm['rows']
            if r['case']==comparison['case'] and r['retained']) for arm in model['runs']]
        assert values == comparison['medians']
        confirmed = True
        for offset,pair in zip((0,2),comparison['pairs'],strict=True):
            a,candidate,c = values[offset:offset+3]
            baseline = math.sqrt(a*c)
            expected = dict(baseline=baseline,candidate=candidate,baseline_drift_pct=100*(c/a-1),
                throughput_change_pct=100*(baseline/candidate-1),latency_reduction_pct=100*(1-candidate/baseline))
            assert all(math.isclose(pair[k],v,rel_tol=1e-12,abs_tol=1e-9) for k,v in expected.items())
            confirmed &= abs(expected['baseline_drift_pct'])<=3 and expected[
                'latency_reduction_pct' if comparison['metric']=='ttft_ms' else 'throughput_change_pct']>=1
        assert comparison['confirmed'] == confirmed
    assert model['performance_confirmed'] == any(c['confirmed'] for c in comparisons if c['metric']=='ttft_ms')
    return True


if __name__=='__main__':
    report = json.loads(Path(__file__).with_name('snapshot-copy-batch-2026-10-31.json').read_text(encoding='utf-8'))
    check(report)
    model_checked = check_model(report)
    print('PASS 5 raw logs,540 retained measurements,unit checks,source hashes and9/24 confirmed copy results; no GPU')
    print('Full-model evidence:', 'PASS 5 arms and 40 full requests' if model_checked else 'pending')
