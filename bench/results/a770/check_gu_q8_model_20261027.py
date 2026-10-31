"""Publish/check the actual-model fusion gate; raw prompts and IDs stay in local artifacts."""
import copy
import hashlib
import json
import math
from pathlib import Path
import re
import statistics
import sys

H = Path(__file__).resolve().parent
R = H.parents[2]
RAW = R / 'build-sycl-a770-gu-q8-export-model-20261027'
STEM = 'gu-q8-model-2026-10-27'
CASES = ('Code2K', 'Prose2K')


def read(path):
    return json.loads(path.read_text(encoding='utf-8'))


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def compare(runs):
    result = {}
    for name in CASES:
        a, b, c = [statistics.median(r['decode_ms'] for r in run['rows'] if r['case'] == name and not r['warmup']) for run in runs]
        baseline = math.sqrt(a*c)
        result[name] = dict(baseline=baseline, candidate=b, throughput_change_pct=100*(baseline/b-1),
            baseline_drift_pct=100*(c/a-1), baseline_tps=128000/baseline, candidate_tps=128000/b)
    return result


def qualifies(result):
    return bool(result) and all(x['throughput_change_pct'] >= 1 and abs(x['baseline_drift_pct']) <= 3 for x in result.values())


def summarize(raw):
    def row(item):
        case = item.get('case', 'Short')
        return {**{k: item[k] for k in ('generated', 'prompt_tokens', 'decode_ms', 'finish', 'reused', 'accepted', 'offered')},
            'case': case, 'warmup': item.get('warmup', False), 'all_zero': not any(item['tokens']),
            'matches_reference': item['tokens'] == raw['reference'][case]}
    return [{**{k: run[k] for k in ('label', 'expert_slots', 'free_mib', 'exit_code')},
             'enabled': run.get('enabled', False), 'dispatch': run.get('dispatch', []),
             'initializer': row(run['initializer']) if 'initializer' in run else None,
             'rows': [row(r) for r in run['rows']]} for run in raw.get('smoke', []) + raw.get('runs', [])]


def check(d):
    assert not d['adopted'] and not d['production_changed'] and not d['target_achieved']
    assert d['minimum_gain_pct'] == 1 and d['maximum_baseline_drift_pct'] == 3
    assert d['startup']['complete'] and [r['label'] for r in d['startup']['rows']] == ['A1', 'B', 'A2']
    assert d['export_build']['complete'] and d['export_build']['checks_passed']
    assert [(r['label'], r['exit_code']) for r in d['export_build']['checks']] == [(f'{name}-{active}', 0) for name in ('outlier', 'random', 'graph') for active in (0, 1)]
    assert d['environment_fix']['literal_unset_removed'] and d['environment_fix']['regression_check_passed']
    finite = d['finite_diagnostic']
    assert finite['complete'] and finite['all_outputs_match_reference']
    assert [r['case'] for r in finite['rows']] == ['Short', 'Code2K', 'Prose2K', 'Code2K']
    assert all(r['matches_reference'] and not r['all_zero'] for r in finite['rows'])
    assert len(finite['nonfinite_diagnostics']) == 3
    assert all(re.findall(r'(\d+) non-finite', line) == ['0', '0', '0'] for line in finite['nonfinite_diagnostics'])
    envfix = d['whole_iq_envfix']
    assert [r['label'] for r in envfix] == ['C', 'A'] and envfix[0]['exit_code'] == 0
    assert len(envfix[0]['rows']) == 2 and all(r['matches_reference'] and not r['all_zero'] for r in envfix[0]['rows'])
    assert envfix[1]['exit_code'] != 0 and envfix[1]['rows'][0]['all_zero'] and not envfix[1]['rows'][0]['matches_reference']
    runs = d['model_runs']
    assert runs and runs[0]['label'] == 'C'
    for run in runs:
        assert run['expert_slots'] == 5771 and run['free_mib'] >= 1500
        if run['initializer']:
            assert run['initializer']['generated'] == 16 and run['initializer']['matches_reference']
        for row in run['rows']:
            assert row['generated'] == 128 and row['prompt_tokens'] == 2048 and row['reused'] == 0 and row['finish'] == 'length'
            assert math.isfinite(row['decode_ms']) and row['decode_ms'] > 0
    quality = len(runs) >= 3 and all(run['exit_code'] == 0 and all(r['matches_reference'] and not r['all_zero'] for r in run['rows']) for run in runs)
    assert d['latest_quality_passed'] == quality
    timed = [r for r in runs if r['label'] not in ('C', 'A', 'B')]
    assert d['comparison'] == (compare(timed[:3]) if len(timed) >= 3 else {})
    assert d['passes_screen'] == (quality and qualifies(d['comparison']))
    assert d['confirmation'] == (compare(timed[3:6]) if len(timed) == 6 else {})
    assert d['passes_confirmation'] == (d['passes_screen'] and qualifies(d['confirmation']))
    if d['complete']:
        assert quality and len(timed) in (3, 6)
    else:
        assert runs[-1]['exit_code'] != 0 and not timed and not d['passes_confirmation']


def publish():
    completed = RAW / 'results.json'
    path = completed if completed.exists() else RAW / 'partial.json'
    raw = read(path)
    assert raw['complete'] or raw['smoke'][-1].get('exit_code', 0) != 0, 'live/incomplete trial cannot be published'
    startup_path = R / 'build-sycl-a770-gu-q8-model-v3-20261027/ready-diagnostic-v2/results.json'
    build_path = R / 'build-sycl-a770-gu-q8-export-20261027/build.json'
    envfix_path = R / 'build-sycl-a770-gu-q8-model-envfix-20261027/partial.json'
    finite_path = R / 'build-sycl-a770-gu-q8-finite-control-20261027/results.json'
    finite = read(finite_path)
    d = dict(complete=raw['complete'], adopted=False, production_changed=False, target_achieved=False,
        minimum_gain_pct=1, maximum_baseline_drift_pct=3,
        hardware='A77016GB / PCIe3.0 / E5-2696v4 / Windows / DPC++2025.3.2 / driver32.0.101.9034',
        startup={k: read(startup_path)[k] for k in ('complete', 'rows')},
        export_build={k: read(build_path)[k] for k in ('complete', 'checks_passed', 'checks')},
        finite_diagnostic={**{k: finite[k] for k in ('complete', 'all_outputs_match_reference', 'nonfinite_diagnostics')},
            'rows': [{k: row[k] for k in ('case', 'generated', 'prompt_tokens', 'all_zero', 'matches_reference')}
                     for run in finite['runs'] for row in run['rows']]},
        environment_fix=dict(literal_unset_removed=True, regression_check_passed=True),
        whole_iq_envfix=summarize(read(envfix_path)), model_runs=summarize(raw),
        comparison=raw.get('comparison', {}), confirmation=raw.get('confirmation', {}),
        passes_screen=raw.get('passes_screen', False), passes_confirmation=raw.get('passes_confirmation', False),
        latest_quality_passed=raw['smoke_dispatch_passed'], rule=raw['rule'], raw_file=path.relative_to(R).as_posix(),
        files_sha256={p.relative_to(R).as_posix(): sha(p) for p in (path, startup_path, build_path, envfix_path, finite_path, RAW / 'run.py')})
    d['files_sha256'].update(raw['files_sha256'])
    d['files_sha256'].update(finite['files_sha256'])
    for directory in (RAW, envfix_path.parent):
        for item in directory.iterdir():
            if item.suffix in ('.stderr', '.protocol'):
                d['files_sha256'][item.relative_to(R).as_posix()] = sha(item)
    check(d)
    lines = ['# A770 GU/Q8 融合：整模型接入检查（2026-10-27）', '', d['hardware'], '',
        '40 tok/s 尚未达到，生产配置未改。算子 mixed T1/T2 的约1%–2%收益不能代替整模型结果。', '',
        '启动对照的原版/候选容量会变化；同为6113槽时，候选/原版空闲显存为1394/1393MiB，没有看到候选明显增加占用。容量不同的读数不用于估算模块显存。原6000缓存组因READY余量793MiB失败，未生成token。新组预先固定cache5500、5771槽和READY空闲至少1500MiB；没有降低原组门槛或改写其结果。', '',
        '复测脚本曾把历史记录中的UNSET标记作为字面环境值传入，意外启用依赖变量存在性的eager模式。已恢复删除语义，并检查删除已存在/不存在变量和保留/设置变量。此前两次原版小缓存诊断仍保留为错误环境下的失败记录，不作性能结论。修复环境后的第一组原版控制Code/Prose完整输出匹配，随后整体替换IQ对象的关闭融合组输出128个零token。后续窄化版本试验尚未运行候选时，原版控制的Code匹配、Prose却全零。因此问题具有间歇性，不能将全零归因于新IQ对象；也没有证明UNSET修复已解决全零。', '',
        '随后保留原IQ对象、预填/dequant/head等入口，只添加唯一命名空间内的融合函数，并对冻结原verifier作一处声明及一处调用名替换，使用匹配ABI头文件。关闭融合直接调用原对象；开启仅覆盖IQ2_S GU22/down42/N2560/F640/cap≤20及GU16/down4/SG16。随机/异常值组长1–7、全部Q8字节/完整链/守卫区和图重放均与原对象一致。算子检查只验证数值，缩短的图批次不用于性能筛选。', '',
        '| 组 | 场景 | 预热 | 全零 | 匹配完整参考 | 状态 |', '| --- | --- | --- | --- | --- | --- |']
    for run in d['model_runs']:
        for row in run['rows']:
            lines.append(f"| {run['label']} | {row['case']} | {row['warmup']} | {row['all_zero']} | {row['matches_reference']} | {run['exit_code']} |")
    if d['complete']:
        lines += ['', '| 场景 | baseline tok/s | candidate tok/s | 配对变化 | 基线耗时漂移 |', '| --- | ---: | ---: | ---: | ---: |']
        for name, p in d['comparison'].items():
            lines.append(f"| {name} | {p['baseline_tps']:.3f} | {p['candidate_tps']:.3f} | {p['throughput_change_pct']:+.3f}% | {p['baseline_drift_pct']:+.3f}% |")
        lines += ['', f"首组通过：{d['passes_screen']}；独立确认通过：{d['passes_confirmation']}。全部预热、低值和只读1Hz频率记录保留在本地原始文件。"]
    else:
        lines += ['', '最新整模型组在完整输出门槛失败后停止，尚未进入正式计时。失败请求的decode耗时不算有效性能，也不能据此称为收益或回退。']
    lines += ['', '随后在一个原版进程中启用既有STRATA_DBG_NAN诊断，完成Short16/Code128/Prose128/Code128，三个完整2K请求均匹配参考。每次预填结束时，最后残差行、PLE历史、首个GDN状态均记录0个非有限值。该诊断增加MoE层后的等待和读取，也会影响head检查，可能改变时序；没有在失败的同一次请求中定位到异常，不能据此证明同步是根因或问题已修复。这组不作测速。后续应针对关闭诊断时的缓冲生命周期和跨队列依赖复现，而非扩大融合分派或降低输出门槛。']
    lines += ['', '可运行的离线检查（--local额外在GPU独占锁内核对本机原始文件摘要）：', '', '```powershell',
        'python -X utf8 bench/results/a770/check_gu_q8_model_20261027.py --self-check', '```', '']
    report = H / (STEM + '.md')
    report.write_text('\n'.join(lines), encoding='utf-8')
    d.update(checker_sha256=sha(Path(__file__)), report_sha256=sha(report))
    (H / (STEM + '.json')).write_text(json.dumps(d, indent=2, ensure_ascii=False), encoding='utf-8')


def main():
    if '--publish' in sys.argv:
        publish()
    d = read(H / (STEM + '.json'))
    check(d)
    assert d['checker_sha256'] == sha(Path(__file__)) and d['report_sha256'] == sha(H / (STEM + '.md'))
    if '--self-check' in sys.argv:
        for key in ('latest_quality_passed', 'passes_screen', 'passes_confirmation'):
            bad = copy.deepcopy(d)
            bad[key] = not bad[key]
            try:
                check(bad)
            except AssertionError:
                pass
            else:
                raise AssertionError('accepted changed ' + key)
        assert qualifies({'Code2K': dict(throughput_change_pct=1, baseline_drift_pct=-3)})
        assert not qualifies({'Code2K': dict(throughput_change_pct=.99, baseline_drift_pct=0)})
    if '--local' in sys.argv:
        sys.path.insert(0, str(R / 'tools'))
        import bench_deepinfer as b
        with b.gpu_lock():
            for relative, digest in d['files_sha256'].items():
                assert b.sha(R / relative) == digest, relative
    print('PASS actual-model output gate, fixed cache and unchanged1%/3% screen rules; production unchanged')


if __name__ == '__main__':
    main()
