"""CPU-only check of version metrics, scoring and integrity; no GPU process."""
from contextlib import nullcontext, redirect_stdout
from io import StringIO
from pathlib import Path
from tempfile import TemporaryDirectory
from types import ModuleType, SimpleNamespace
from unittest.mock import patch
import json
import math
import runpy
import sys

import bench_deepinfer as real

fake = ModuleType('bench_deepinfer')
for name in ('sha', 'save', 'paired'):
    setattr(fake, name, getattr(real, name))
fake.gpu_lock = nullcontext
fake.prompts = lambda cfg: dict(Short=[1]*23, Code2K=[2]*2048,
                                Prose2K=[3]*2048, Long8K=[4]*8000)
regression = False
nonfinite = False


class Engine:
    def __init__(self, exe, args, env, stem):
        self.native = exe.name == 'orca.exe'
        self.env = env
        self.stem = stem
        self.info = 'INFO expert_slots=6297 expert_cache_mib=8641 spec=6'
        self.p = SimpleNamespace(pid=0, returncode=None)

    def gen(self, ids, count, keys):
        if 'STRATA_LOGPOS' in self.env:
            raw, conditional = (-1.005, -.79) if self.native else (-1., -.8)
            if self.native and regression: raw = -1.05
            if nonfinite and not self.native and len(ids)>8000: raw = float('nan')
            cut=max(i for i in range(1,len(ids)-1) if ids[i]==248045)
            priming=6 if not self.native and len(ids)>1025 else 0
            self.stem.with_suffix('.stderr').write_text('strata serve: priming Windows before the first batched prompt\n' if priming else '',encoding='utf8')
            positions=list(range(priming))+[i for a,z in ((0,cut),(cut,len(ids)-1)) if z-a<=1024 for i in range(a,z)]
            Path(self.env['STRATA_LOGPOS']).write_text(''.join(
                f'{i}\t{ids[i+1]}\t{raw}\t{ids[i+1]}\t-1\t1\t-2\t{conditional if ids[i+1]!=248046 else "-nan(ind)"}\n'
                for i in positions), encoding='utf-8')
        return dict(tokens=[2 if self.native else 1]*count, generated=count,
                    prompt_tokens=len(ids), decode_ms=8 if self.native else 10,
                    prompt_ms=120 if self.native else 100, ttft_ms=121 if self.native else 101,
                    accepted=2 if self.native else 1, offered=3, reused=0, resume=0, finish='length')

    def close(self):
        self.p.returncode = 0


fake.Engine = Engine
audit_module = ModuleType('check_orca_http')
audit_module.audit = lambda pid: []

with TemporaryDirectory(prefix='orca-upgrade-check-') as temporary:
    root = Path(temporary)
    images = root/'images'
    images.mkdir()
    (images/'test.bin').write_bytes(b'fixed image')
    paths = []
    for backend in ('sycl', 'orca'):
        exe = root/f'{backend}.exe'
        exe.write_bytes(backend.encode())
        cfg = dict(backend=backend, exe=str(exe), args=['--spec', '4'],
                   tokenizer='same-tokenizer', env={'STRATA_ORCA_IMAGES':str(images)})
        p = root/f'{backend}.json'
        p.write_text(json.dumps(cfg), encoding='utf-8')
        paths.append(p)
    argv = ['bench_orca_upgrade.py', '--baseline-config', str(paths[0]),
            '--candidate-config', str(paths[1]), '--out', str(root/'results')]
    with redirect_stdout(StringIO()), \
         patch.dict(sys.modules, bench_deepinfer=fake, check_orca_http=audit_module), \
         patch.object(sys, 'argv', argv), patch('time.perf_counter', side_effect=iter(range(1000))):
        namespace=runpy.run_path(str(Path(__file__).with_name('bench_orca_upgrade.py')), run_name='__main__')
    d = json.loads((root/'results/results.json').read_text(encoding='utf-8'))
    assert d['complete'] and d['version_comparison_valid']
    assert not d['same_token_backend_comparison'] and not d['quality_equivalence_established']
    assert all(v for k,v in d.items() if k.endswith('_unchanged'))
    rows = [row for arm in d['runs'] for row in arm['rows']]
    assert len(rows)==48 and sum(r['generated'] for r in rows)==3072
    assert sum(r['retained'] for r in rows)==36
    q = d['quality_comparison']
    assert q['targets']==502 and q['passed'] and q['primary_metric']=='raw_ppl'
    assert all('conditional_mean_nll' in c and 'mean_nll' not in c for a in d['quality'] for c in a['cases'])
    assert all(a['args'][a['args'].index('--expert-cache')+1]=='3000' for a in d['quality'])
    assert q['metric_direction_conflict']
    assert math.isclose(q['aggregate_conditional_ppl_change_pct'],100*math.expm1(-.01))
    assert math.isclose(q['aggregate_raw_ppl_change_pct'],100*math.expm1(.005))
    assert math.isclose(q['aggregate_end_penalty_nll_delta'],.015)
    for row in d['comparisons']:
        assert not row['same_outputs'] and not row['same_draft_workloads']
        assert row['metrics']['decode_ms']['throughput_change_pct']==25
        assert math.isclose(row['metrics']['prompt_ms']['throughput_change_pct'], -100/6)
        assert row['metrics']['wall_ms']['throughput_change_pct']==0

    compare=namespace['compare_quality']
    fixed=namespace['fixed_scores']
    case=d['quality_corpus']['cases'][d['quality'][0]['cases'][0]['case']]
    path=root/'scores.tsv'
    original=d['quality'][0]['cases'][0]['scores_tsv']
    path.write_bytes(original.encode('utf8'))
    assert fixed(path,case)['targets']==len(case['tokens'])-case['prompt_tokens']
    lines=original.splitlines()
    for changed in (lines[1:],lines+[lines[-1]],list(reversed(lines)),
                    [lines[0].replace('0\t','999\t',1)]+lines[1:],
                    [lines[0].replace('\t-1.0\t','\tnan\t',1)]+lines[1:],
                    [lines[0].replace('\t-1.0\t','\t0.1\t',1)]+lines[1:]):
        path.write_text('\n'.join(changed)+'\n',encoding='utf8')
        try: fixed(path,case)
        except AssertionError: pass
        else: raise AssertionError('invalid score coverage/probability accepted')
    def summary(name,n,raw,conditional):
        return dict(case=name,targets=n,raw_mean_nll=raw,mean_nll=conditional)
    # Opposite directions must fail when raw PPL regresses, even if conditional improves.
    reference=[summary('a',1,1.,.8),summary('b',9,1.,.8)]
    candidate=[summary('a',1,1.02,.79),summary('b',9,1.,.8)]
    weighted=compare(reference,candidate)
    assert weighted['passed']
    assert math.isclose(weighted['aggregate_raw_ppl_change_pct'],100*math.expm1(.002))
    regressed=[summary('a',1,1.05,.79),summary('b',9,1.05,.79)]
    assert not compare(reference,regressed)['passed']
    assert not compare(reference,[summary('a',1,1.04,.79),candidate[1]])['passed']
    assert not compare(reference,[summary('a',1,1.,.79),summary('b',9,1.02,.79)])['passed']
    # The actual 1022-position historical disagreement is rejected by the new policy.
    archived=json.loads((real.ROOT/'sycl/orca/deployed-versions-a770-windows-2026-10-31.json').read_text(encoding='utf8'))
    reviewed=compare(*(a['cases'] for a in archived['quality']))
    assert not reviewed['passed'] and reviewed['metric_direction_conflict']
    assert math.isclose(reviewed['aggregate_raw_ppl_change_pct'],6.938935196688524)

    argv[-1]=str(root/'quality-only')
    argv.append('--quality-only')
    argv.extend(['--quality-cache-slots','4000'])
    with redirect_stdout(StringIO()), \
         patch.dict(sys.modules, bench_deepinfer=fake, check_orca_http=audit_module), \
         patch.object(sys, 'argv', argv):
        runpy.run_path(str(Path(__file__).with_name('bench_orca_upgrade.py')), run_name='__main__')
    quality_only=json.loads((root/'quality-only/results.json').read_text(encoding='utf8'))
    assert quality_only['complete'] and quality_only['quality_comparison']['passed']
    assert not quality_only['runs'] and not quality_only['version_comparison_valid']
    assert all(a['args'][a['args'].index('--expert-cache')+1]=='4000' for a in quality_only['quality'])
    regression=True
    failure_argv=argv[:argv.index('--out')+1]+[str(root/'raw-failure')]
    with redirect_stdout(StringIO()), \
         patch.dict(sys.modules, bench_deepinfer=fake, check_orca_http=audit_module), \
         patch.object(sys, 'argv', failure_argv):
        try: runpy.run_path(str(Path(__file__).with_name('bench_orca_upgrade.py')), run_name='__main__')
        except AssertionError as error: assert 'raw PPL quality gate failed' in str(error)
        else: raise AssertionError('raw regression reached performance measurement')
    failed=json.loads((root/'raw-failure/results.json').read_text(encoding='utf8'))
    assert not failed['complete'] and not failed['runs'] and not failed['quality_comparison']['passed']
    assert failed['quality_comparison']['aggregate_conditional_ppl_change_pct']<0
    assert failed['quality_evaluation_complete'] and all(v for k,v in failed.items() if k.endswith('_unchanged'))
    regression=False
    nonfinite=True
    failure_argv[-1]=str(root/'nonfinite-failure')
    with redirect_stdout(StringIO()), \
         patch.dict(sys.modules, bench_deepinfer=fake, check_orca_http=audit_module), \
         patch.object(sys, 'argv', failure_argv):
        try: runpy.run_path(str(Path(__file__).with_name('bench_orca_upgrade.py')), run_name='__main__')
        except AssertionError as error: assert 'raw PPL quality gate failed' in str(error)
        else: raise AssertionError('nonfinite scores reached performance measurement')
    failed=json.loads((root/'nonfinite-failure/results.json').read_text(encoding='utf8'))
    assert failed['quality_evaluation_complete'] and not failed['runs']
    q=failed['quality_comparison']
    assert not q['passed'] and q['expected_targets']==502 and q['targets']==0
    assert q['valid_targets_by_arm']==[368,502]
    assert {c['case'] for c in q['invalid_cases']}=={'followup','long'}
    assert 'aggregate_raw_ppl_change_pct' not in q
print('PASS: raw-PPL gate, weighted NLL, fixed assistant coverage, historical disagreement rejection, quality-only run and workload separation; no GPU used.')
