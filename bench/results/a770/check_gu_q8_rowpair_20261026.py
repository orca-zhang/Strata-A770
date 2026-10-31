"""Offline complete-chain fusion accounting; never treats operator gain as model gain."""
import copy
import hashlib
import json
import math
from pathlib import Path
import statistics
import sys

H = Path(__file__).resolve().parent
R = H.parents[2]
RAW = R / 'build-sycl-a770-gu-q8-rowpair-20261026'
STEM = 'gu-q8-rowpair-2026-10-26'
LABELS = ('A1', 'B1', 'A2', 'B2', 'A3')
FORMATS = (22,)
MEMORIES = ('mapped', 'mixed', 'vram')


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def compare(runs):
    result = {}
    for gu in FORMATS:
        result[str(gu)] = {}
        for mem in MEMORIES:
            arms = [next(r for r in runs if r['label'] == label and r['gu'] == gu and r['memory'] == mem) for label in LABELS]
            shapes = result[str(gu)][mem] = {}
            for t in range(1, 7):
                shapes[str(t)] = []
                for i in (1, 3):
                    a, b, c = (arm['rows'][t - 1]['us'] for arm in arms[i - 1:i + 2])
                    pair = dict(baseline=math.sqrt(a*c), candidate=b, throughput_change_pct=100*(math.sqrt(a*c)/b-1), baseline_drift_pct=100*(c/a-1))
                    pair['passes'] = pair['throughput_change_pct'] >= 1 and abs(pair['baseline_drift_pct']) <= 3
                    shapes[str(t)].append(pair)
    return result


def eligible(comparison):
    return {gu: {mem: [int(t) for t, pairs in shapes.items() if len(pairs) == 2 and all(r['passes'] for r in pairs)]
                 for mem, shapes in memories.items()} for gu, memories in comparison.items()}


def check(d):
    assert d['complete'] and not d['adopted'] and not d['production_changed'] and not d['target_achieved']
    assert d['scope'] == 'native-chain-operator-screen' and not d['model_gain_confirmed']
    assert d['minimum_gain_pct'] == 1 and d['maximum_baseline_drift_pct'] == 3
    assert d['pair_check'] == dict(exit_code=0, blocks=331)
    assert [(r['gu'], r['fixture'], r['active'], r['checks'], r['exit_code']) for r in d['edge_checks']] == [(22, fixture, active, 48, 0) for fixture in ('outlier', 'random') for active in (0, 1)]
    assert [(r['label'], r['gu'], r['memory']) for r in d['runs']] == [(label, gu, mem) for label in LABELS for gu in FORMATS for mem in MEMORIES]
    for run in d['runs']:
        active = int(run['label'].startswith('B'))
        assert run['dispatch'] == dict(gu=run['gu'], active=active, wg=256 if active else 0)
        assert run['exit_code'] == 0 and run['extra_checks'] == dict(empty=1, graph=6)
        assert len(run['rows']) == len(run['samples']) == 6
        for t, row, sample in zip(range(1, 7), run['rows'], run['samples']):
            assert row['T'] == sample['T'] == t and row['checks'] == t * 8 and row['grid_groups'] == 0
            assert row['groups'] == (10 if t == 1 else t * 5) and row['entries'] == t * 10
            assert len(sample['us']) == 11 and all(math.isfinite(v) and v > 0 for v in sample['us'])
            assert math.isfinite(row['us']) and row['us'] > 0 and abs(row['us'] - statistics.median(sample['us'])) <= 0.000000501
    assert d['comparison'] == compare(d['runs']) and d['eligible_shapes'] == eligible(d['comparison'])
    maximum = max(abs(p['baseline_drift_pct']) for memories in d['comparison'].values() for shapes in memories.values() for pairs in shapes.values() for p in pairs)
    assert d['maximum_observed_drift_pct'] == maximum


def publish():
    raw = json.loads((RAW / 'results.json').read_text(encoding='utf-8'))
    d = {k: raw[k] for k in ('complete', 'adopted', 'production_changed', 'target_achieved', 'scope',
        'minimum_gain_pct', 'maximum_baseline_drift_pct', 'pair_check', 'edge_checks', 'rule', 'runs', 'comparison', 'eligible_shapes')}
    d.update(model_gain_confirmed=False,
        hardware='A77016GB / PCIe3.0 / E5-2696v4 / Windows / DPC++2025.3.2 / driver32.0.101.9034',
        raw_file=(RAW / 'results.json').relative_to(R).as_posix(), raw_sha256=sha(RAW / 'results.json'),
        raw_audit='Runner completed numerical,dispatch,all-sample/source/binary audits under its exclusive GPU lock and exited0. Publication reads small results only.')
    d['maximum_observed_drift_pct'] = max(abs(p['baseline_drift_pct']) for memories in d['comparison'].values() for shapes in memories.values() for pairs in shapes.values() for p in pairs)
    check(d)
    lines = ['# A770 native GU/Q8 fusion with WG256 adjacent row pairs', '', d['hardware'] + '.', '',
        'Following the completed WG512 fusion screen,keep SG16 and the unchanged integer dots,FP32 accumulation,Q8 arithmetic and down kernel. IQ2_S uses WG256 with each subgroup computing two adjacent rows sequentially. The original three-kernel baseline remains in the same EXE. This isolates smaller workgroup/row scheduling; it does not add shared-dot or approximate arithmetic.', '',
        'Native resource inspection precedes the new compile and uses the exact frozen measured runtime options. The previous IQ2_S WG512 fusion reports128GRF,1KiB SLM,224spill bytes and256scratch bytes; Q2 fusion/baseline do not list spill. This static evidence does not establish performance cause. The candidate retains1KiB SLM,original4/2/1 entry chunks and original Q8 reduction order. Real240experts,eight physical sets,N2560/F640,downQ2_0 lanes4. Syntheticmixed maps every fourth physical expert pointer to host memory; this is not measured model routing/cache residency.', '',
        'Before timing,331 quantizer blocks compare every byte against the original SG32 quantizer,including signed zeros,large finite values,NaN/Inf and guards.192 edge complete-chain outputs/Q8 arrays pass with group lengths1..7,including4/2/1 chunks and random/outlier inputs. Across15 timing arms,735 eager/empty checks and90 final graph outputs pass,including full Q8 arrays,inactive sentinels,nonzero starts,scattered destinations and scratch/output guards. Each T=1..6 uses a48-chain graph,16warmups and11 retained batches of4replays;990 timing batches are retained. Timing does not read events. Both independent paired rounds must gain>=1% with<=3% baseline time drift per format/T/memory.', '',
        '| GU | Memory | T | Gain1 | Gain2 | Drift1 | Drift2 | Pass both |', '| --- | --- | ---: | ---: | ---: | ---: | ---: | --- |']
    for gu, memories in d['comparison'].items():
        for mem, shapes in memories.items():
            for t, (a, b) in shapes.items():
                lines.append(f"| {gu} | {mem} | {t} | {a['throughput_change_pct']:+.3f}% | {b['throughput_change_pct']:+.3f}% | {a['baseline_drift_pct']:+.3f}% | {b['baseline_drift_pct']:+.3f}% | {a['passes'] and b['passes']} |")
    lines += ['', f"No shape passes both rounds. Maximum adjacent baseline drift is {d['maximum_observed_drift_pct']:.3f}%; pairs exceeding3% are excluded from performance conclusions. The remaining pairs also do not establish a repeatable1% gain. No adoption or model timing follows this candidate.", '',
        'Only eligible syntheticmixed shapes can proceed to separate actual-model dispatch,full-output and original-binary speed confirmation. The corrected GDN layout requires a separately registered model reference; historical frozen-output hashes are not a correctness reference for that layout. This result is an operator screen; no model speed,production adoption or stable40tok/s claim.', '',
        d['raw_audit'], '', f'[All samples]({STEM}.json). Offline check:', '', '```powershell',
        'python bench/results/a770/check_gu_q8_rowpair_20261026.py --self-check', '```', '']
    report = H / (STEM + '.md')
    report.write_text('\n'.join(lines), encoding='utf-8')
    d.update(checker_sha256=sha(Path(__file__)), report_sha256=sha(report))
    (H / (STEM + '.json')).write_text(json.dumps(d, indent=2), encoding='utf-8')


def main():
    if '--publish' in sys.argv:
        publish()
    d = json.loads((H / (STEM + '.json')).read_text(encoding='utf-8'))
    assert d['checker_sha256'] == sha(Path(__file__)) and d['report_sha256'] == sha(H / (STEM + '.md'))
    check(d)
    if '--self-check' in sys.argv:
        for kind in ('missing', 'nan', 'dispatch'):
            bad = copy.deepcopy(d)
            if kind == 'missing': bad['runs'].pop()
            if kind == 'nan': bad['runs'][0]['samples'][0]['us'][0] = float('nan')
            if kind == 'dispatch': bad['runs'][3]['dispatch']['active'] = 0
            try:
                check(bad)
            except (AssertionError, StopIteration):
                pass
            else:
                raise AssertionError('accepted ' + kind)
    print('PASS331 quantizer blocks,192 edge chains,735 eager/empty checks,90 graph outputs and990 batches; no model claim')


if __name__ == '__main__':
    main()
