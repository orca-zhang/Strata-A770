"""Standalone stdlib audit of native-iteration token-byte conversion."""
import hashlib
import json
import math
import statistics
from pathlib import Path

F=Path(__file__).resolve().parent


def read(name):
    return json.loads((F/name).read_text(encoding='utf-8'))


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def derive():
    prep,runs,pairs=read('preparation.json'),[],[]
    for phase in ('screen','confirm'):
        phase_runs=[]
        for i,arm in enumerate(('baseline','candidate','baseline','candidate','baseline')):
            name=f'{phase}-{i}.json'
            run=read(name)
            assert run['arm']==arm and run['cpu']==40
            assert run['source_sha256']==prep['source_sha256' if arm=='baseline' else 'candidate_sha256']
            assert len(run['cases'])==5 and {c['name'] for c in run['cases']}==set(prep['cases'])
            for case in run['cases']:
                values=case['seconds']
                assert len(values)==7 and all(math.isfinite(v) and v>0 for v in values)
                assert case['median_seconds']==statistics.median(values)
                assert case['calls_per_batch']==prep['cases'][case['name']]
            phase_runs.append(run)
            runs.append(dict(run,phase=phase,ordinal=i,raw_file=name))
        assert len({r['input_ids_sha256'] for r in phase_runs})==1
        for i in (1,3):
            for case in phase_runs[i]['cases']:
                samples=[next(c for c in phase_runs[j]['cases'] if c['name']==case['name']) for j in (i-1,i,i+1)]
                assert len({c['output_sha256'] for c in samples})==1
                a,b,c=(s['median_seconds'] for s in samples)
                drift,gain=100*(c/a-1),100*(math.sqrt(a*c)/b-1)
                pairs.append(dict(phase=phase,candidate_round=i,name=case['name'],baseline_drift_pct=drift,
                                  throughput_gain_pct=gain,eligible=abs(drift)<=3))
    return runs,pairs


def pooled(values):
    return 100*(math.exp(statistics.mean(math.log1p(v/100) for v in values))-1) if values else None


def summarize(pairs):
    phases=[]
    for phase in ('screen','confirm'):
        for name in read('preparation.json')['cases']:
            valid=[p for p in pairs if p['phase']==phase and p['name']==name and p['eligible']]
            phases.append(dict(phase=phase,name=name,eligible=len(valid),positive=sum(p['throughput_gain_pct']>0 for p in valid),
                               pooled_gain_pct=pooled([p['throughput_gain_pct'] for p in valid])))
    cold=[p for p in phases if p['name']=='cold_bytes4k']
    passed=all(p['eligible'] and p['pooled_gain_pct']>0 for p in cold)
    return dict(phases=phases,retention_pass=bool(passed),
                rule='Cold conversion operator has positive eligible gain in both phases; warm paths are controls.')


def main():
    archive,prep=read('archive.json'),read('preparation.json')
    for name,digest in archive['sha256'].items():
        assert sha(F/name)==digest,name
    assert set(archive['sha256'])=={p.relative_to(F).as_posix() for p in F.rglob('*')
                                   if p.is_file() and '__pycache__' not in p.parts and p.name!='archive.json'}
    baseline=(F/'baseline/strata_tokenizer.py').read_bytes()
    candidate=(F/'candidate/strata_tokenizer.py').read_bytes()
    old='''            raw = bytearray()
            for ch in self.tokens[i]:
                v = UNICODE_TO_BYTE.get(ch)
                if v is None:
                    raise KeyError("token %d contains a character outside the byte alphabet: %r" % (i, ch))
                raw.append(v)
            b = cache[i] = bytes(raw)'''.replace('\n','\r\n').encode()
    new='''            try:
                b = cache[i] = bytes(map(UNICODE_TO_BYTE.__getitem__, self.tokens[i]))
            except KeyError as exc:
                raise KeyError("token %d contains a character outside the byte alphabet: %r" % (i, exc.args[0])) from None'''.replace('\n','\r\n').encode()
    assert baseline.count(old)==1 and baseline.replace(old,new)==candidate
    assert sha(F/'baseline/strata_tokenizer.py')==prep['source_sha256']
    assert sha(F/'candidate/strata_tokenizer.py')==prep['candidate_sha256']
    for name,digest in prep['frozen_sha256'].items():
        if name.endswith('.pyd'):
            assert archive['omitted_binary_sha256'][name]==digest
        else:
            assert sha(F/name)==digest
    checks=read('checks.json')
    assert checks['complete'] and checks['vocabulary']==248320 and checks['repeated_byte_checks']==496640
    assert checks['vocabulary_error_tokens']==0 and checks['invalid_id_checks']==8 and checks['synthetic_token_checks']==3
    assert checks['streaming_prefix_checks']==7576 and checks['decode_error_policy_checks']==192
    assert checks['regression_tests']==10 and checks['failures']==checks['errors']==checks['skips']==0
    assert 'Ran 10 tests' in (F/'tests.log').read_text(encoding='utf-8')
    runs,pairs=derive()
    result,summary,final=read('results.json'),read('summary.json'),read('final-checks.json')
    assert result['complete'] and result['production_unchanged'] and result['live_source_still_baseline']
    assert result['runs']==runs and result['pairs']==pairs
    for name in prep['cases']:
        assert len({c['output_sha256'] for r in runs for c in r['cases'] if c['name']==name})==1
    assert len({r['input_ids_sha256'] for r in runs})==1
    assert summary['measurement']==summarize(pairs)
    assert summary['decision']==('adopted' if summary['measurement']['retention_pass'] else 'not_adopted')
    assert final['production_sha256']==prep['production_sha256'] and final['live_regression_returncode']==0
    assert final['runtime_source_sha256']==prep['candidate_sha256' if summary['decision']=='adopted' else 'source_sha256']
    assert final['live_regression_source_sha256']==sha(F/'regression/test_detok.py')
    assert 'Ran 2 tests' in (F/'live-tests.log').read_text(encoding='utf-8')
    packaging=read('packaging-status.json')
    assert packaging['source_applied'] and packaging['live_tests_passed'] and packaging['offline_audit_passed']
    for name,digest in packaging['raw_measurement_sha256'].items():
        assert sha(F/name)==digest
    print('PASS: 10 processes, 50 medians, 350 batches, 20 pairs; outputs and token bytes agree.')
    print('Decision:',summary['decision'])
    for phase in summary['measurement']['phases']:
        print(phase['phase'],phase['name'],phase['eligible'],phase['pooled_gain_pct'])


if __name__=='__main__':
    main()
