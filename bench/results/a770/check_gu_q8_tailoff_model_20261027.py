"""Recompute the original2K fusion screen without exposing prompts or generated IDs."""
import copy
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import re
import sys

H = Path(__file__).resolve().parent
R = H.parents[2]
RAW = R / 'build-sycl-a770-gu-q8-export-tailoff-model-20261027'
STEM = 'gu-q8-tailoff-model-2026-10-27'
CASES = ('Code2K', 'Prose2K')


def read(path):
    return json.loads(path.read_text(encoding='utf-8'))


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def check(d):
    assert not d['complete'] and d['trial_terminal'] and not d['adopted'] and not d['production_changed']
    assert d['minimum_gain_pct'] == 1 and d['maximum_baseline_drift_pct'] == 3 and d['smoke_dispatch_passed']
    assert [run['label'] for run in d['smoke']] == ['C', 'A', 'B']
    for run in d['smoke'] + d['runs']:
        assert run['exit_code'] == (1 if run['label'] == 'A2' else 0) and run['expert_slots'] == 5771 and run['free_mib'] >= 1500
        assert run['initializer_matches_reference'] and not run['initializer_all_zero']
        expected = [(c, False) for c in CASES] if run['label'] in ('C', 'A', 'B') else [(c, warm) for warm in (True, False, False, False) for c in CASES]
        assert [(row['case'], row['warmup']) for row in run['rows']] == (expected[:2] if run['label'] == 'A2' else expected)
        for row in run['rows']:
            assert row['generated'] == 128 and row['prompt_tokens'] == 2048 and row['reused'] == 0 and row['finish'] == 'length'
            assert math.isfinite(row['decode_ms']) and row['decode_ms'] > 0
            failed = run['label'] == 'A2' and row['case'] == 'Prose2K'
            assert row['matches_reference'] == (not failed) and row['all_zero'] == failed
        if run['label'] != 'C':
            assert run['dispatch'] and all(active == int(run['enabled'] and cap <= 20) for cap, active in run['dispatch'])
            assert not run['enabled'] or any(active for cap, active in run['dispatch'])
        stem = ('smoke-' if run['label'] in ('C', 'A', 'B') else 'performance-') + run['label']
        for suffix in ('.stderr', '.protocol'):
            assert (RAW / (stem + suffix)).relative_to(R).as_posix() in d['files_sha256']
    assert [run['label'] for run in d['runs'][:3]] == ['A1', 'B1', 'A2']
    assert len(d['runs']) == 3 and not d['comparison'] and not d['confirmation']
    assert not d['passes_screen'] and not d['passes_confirmation'] and not d['stable_40_screen_passed']
    assert d['monitor_terminal'] and len(d['telemetry']) == sum(len(run['rows']) for run in d['runs'])
    assert all(row['samples'] > 0 for row in d['telemetry'])
    assert [(row['label'], row['case'], row['warmup']) for row in d['telemetry']] == [
        (run['label'], row['case'], row['warmup']) for run in d['runs'] for row in run['rows']]
    for suffix in ('.stdout', '.stderr'):
        assert (RAW / ('monitor' + suffix)).relative_to(R).as_posix() in d['files_sha256']


def publish():
    path = RAW / 'partial.json'
    raw = read(path)
    assert not raw['complete'] and raw['runs'][-1]['exit_code'] != 0 and raw['monitor_exit'] != 0, 'Do not publish a live model trial'
    spec = importlib.util.spec_from_file_location('tailoff_telemetry', R / 'build-sycl-a770-decode2k-cache-confirm2-20261025/run.py')
    telemetry = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(telemetry)
    telemetry.H = RAW
    d = {k: raw[k] for k in ('complete', 'production_changed', 'adopted', 'smoke_dispatch_passed', 'rule')}
    d.update(trial_terminal=True, monitor_terminal=True, comparison={}, confirmation={}, passes_screen=False, passes_confirmation=False, telemetry=telemetry.telemetry(raw))
    artifacts = [path, RAW / 'checked.json'] + [p for p in RAW.iterdir() if p.suffix in ('.stderr', '.protocol', '.stdout')]
    archive = R / 'build-sycl-a770-prefill-bf16-trace-20261027/config-original-snapshot.json'
    if archive.exists():
        assert sha(archive) == raw['files_sha256']['strata-iq2_xs.json']
        artifacts.append(archive)
    d.update(minimum_gain_pct=1, maximum_baseline_drift_pct=3,
        hardware='A77016GB / PCIe3.0 / E5-2696v4 / Windows / DPC++2025.3.2 / driver32.0.101.9034',
        files_sha256=raw['files_sha256'] | {p.relative_to(R).as_posix(): sha(p) for p in artifacts})
    for kind in ('smoke', 'runs'):
        d[kind] = []
        for run in raw[kind]:
            rows = [{**{k: row[k] for k in ('case', 'warmup', 'generated', 'prompt_tokens', 'reused', 'finish', 'prompt_ms', 'decode_ms', 'accepted', 'offered')},
                'all_zero': not any(row['tokens']), 'matches_reference': row['tokens'] == raw['reference'][row['case']],
                'output_sha256': hashlib.sha256(json.dumps(row['tokens'], separators=(',', ':')).encode()).hexdigest()} for row in run['rows']]
            stem = RAW / (('smoke-' if kind == 'smoke' else 'performance-') + run['label'])
            dispatch = [[int(c), int(a)] for c, a in re.findall(r'^MODEL_GU_Q8 cap=(\d+) active=([01])$', stem.with_suffix('.stderr').read_text(encoding='utf-8'), re.M)]
            assert 'dispatch' not in run or dispatch == run['dispatch']
            d[kind].append({**{k: run[k] for k in ('label', 'enabled', 'binary_role', 'exit_code', 'expert_slots', 'free_mib')}, 'dispatch': dispatch,
                'initializer_all_zero': not any(run['initializer']['tokens']),
                'initializer_matches_reference': run['initializer']['tokens'] == raw['reference']['Short'], 'rows': rows})
    d['stable_40_screen_passed'] = d['passes_confirmation'] and all(128000 / row['decode_ms'] >= 40 for run in d['runs'] if run['enabled'] for row in run['rows'] if not row['warmup'])
    check(d)
    lines = ['# A770 GU/Q8：关闭追踪后的整模型验收（2026-10-27）', '', d['hardware'], '',
        '本轮在末端关闭融合的基线A2中再次出现Prose2K完整128输出全零，按门槛停止，没有有效配对收益，40目标未达到。生产未改。', '',
        '原 IQ/head/dequant 对象保留，仅增加已通过数值检查的融合入口及冻结 verifier 的一处调用。两臂使用相同的预填充对象，关闭有限值追踪、分配和探针核；C 为同预填充对象但不含融合入口的控制，A/B 为同一候选 EXE 关闭/开启融合。之前18次诊断通过不足以证明间歇缺陷修复，本轮关闭融合的失败也不能归因于开启融合。', '',
        '固定 cache5500、实际5771槽、READY空闲至少1500MiB，原 Code2K/Prose2K 各2048输入、完整128输出，prompt-cache0。每进程Short16初始化，正式臂各场景一次预热、三次交错保留；每次请求均核对全部输出。GPU/编译独占，计时关闭分析器，统一1Hz只读Sysman采样。', '',
        '融合仅用于IQ2_S GU22/down42/N2560/F640、cap10/20及原SG16/GU16/down4配置，其余调用保留原内核。本轮采用GPU混合镜像计划，显存与映射主存指针共同处理；分开处理是源码的另一条分支。合成mixed指针的固定比例不代表模型实际驻留分布，因此算子收益仍不能直接外推。', '',
        'C/A/B各两次完整输出检查通过，B在cap10/20启用融合，其他形状保留原内核。正式A1与B1各8次请求也全部匹配；A2的Code预热匹配，随后Prose全零。共24次完整2K请求，23次匹配、1次全零；不得删除失败后只比较A1/B1。', '']
    for case, row in d['comparison'].items():
        lines.append(f"| {case} | {row['baseline_tps']:.3f} | {row['candidate_tps']:.3f} | {row['throughput_change_pct']:+.3f}% | {row['baseline_drift_pct']:+.3f}% |")
    lines += ['', '收益门槛仍为两场景均至少1%、两端基线耗时漂移至多3%。A2未形成完整保留样本，故配对结果与确认均为空。', '',
        '| 正式臂 | 场景 | 预热 | 输出匹配 | decode ms | accepted/offered |', '| --- | --- | --- | --- | ---: | --- |']
    for run in d['runs']:
        for row in run['rows']:
            elapsed = f"{row['decode_ms']:.1f}" if row['matches_reference'] else '无效输出，不计性能'
            lines.append(f"| {run['label']} | {row['case']} | {row['warmup']} | {row['matches_reference']} | {elapsed} | {row['accepted']}/{row['offered']} |")
    if d['confirmation']:
        lines += ['', '独立确认：', '', '| 场景 | 吞吐变化 | 基线耗时漂移 |', '| --- | ---: | ---: |']
        for case, row in d['confirmation'].items():
            lines.append(f"| {case} | {row['throughput_change_pct']:+.3f}% | {row['baseline_drift_pct']:+.3f}% |")
    lines += ['', '保留所有预热、低值、输出摘要、分派记录及逐请求频率范围；无通用质量/PPL结论。频率采样不能排除短暂变化，READY显存不是峰值余量。未通过初筛时不追加确认以挑选样本。', '',
        '离线复核：`python -X utf8 bench/results/a770/check_gu_q8_tailoff_model_20261027.py --self-check`。`--local`等待共享GPU锁后核对原始输出、分派日志及文件摘要。', '']
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
        for key in ('passes_screen', 'passes_confirmation', 'stable_40_screen_passed'):
            bad = copy.deepcopy(d)
            bad[key] = not bad[key]
            try:
                check(bad)
            except AssertionError:
                pass
            else:
                raise AssertionError('accepted changed ' + key)
        bad = copy.deepcopy(d)
        bad['runs'][-1]['dispatch'][0][1] = 1
        try:
            check(bad)
        except AssertionError:
            pass
        else:
            raise AssertionError('accepted enabled dispatch in failed OFF baseline')
    if '--local' in sys.argv:
        sys.path.insert(0, str(R / 'tools'))
        import bench_deepinfer as b
        with b.gpu_lock():
            spec = importlib.util.spec_from_file_location('tailoff_fusion_audit', RAW / 'run_v2.py')
            runner = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(runner)
            raw = read(RAW / 'partial.json')
            for kind in ('smoke', 'runs'):
                for run, small in zip(raw[kind], d[kind]):
                    for row, summary in zip(run['rows'], small['rows']):
                        assert summary['all_zero'] == (not any(row['tokens']))
                        assert summary['matches_reference'] == (row['tokens'] == raw['reference'][row['case']])
                        assert summary['output_sha256'] == hashlib.sha256(json.dumps(row['tokens'], separators=(',', ':')).encode()).hexdigest()
                    stem = RAW / (('smoke-' if kind == 'smoke' else 'performance-') + run['label'])
                    outputs, tokens = [], []
                    for line in stem.with_suffix('.protocol').read_text(encoding='utf-8').splitlines():
                        if line.startswith(('T ', 'TB ')):
                            tokens.extend(map(int, line.split()[1:] if line.startswith('T ') else line.split()[2:]))
                        elif line.startswith('DONE '):
                            outputs.append((line, tokens))
                            tokens = []
                    assert not tokens and outputs == [(row['done'], row['tokens']) for row in [run['initializer'], *run['rows']]]
                    dispatch = [[int(c), int(a)] for c, a in re.findall(r'^MODEL_GU_Q8 cap=(\d+) active=([01])$', stem.with_suffix('.stderr').read_text(encoding='utf-8'), re.M)]
                    assert dispatch == small['dispatch']
            telemetry = runner.m.module(R / 'build-sycl-a770-decode2k-cache-confirm2-20261025/run.py')
            telemetry.H = RAW
            assert d['telemetry'] == telemetry.telemetry(raw)
            for relative, digest in d['files_sha256'].items():
                path = R / relative
                if relative == 'strata-iq2_xs.json' and b.sha(path) != digest:
                    path = R / 'build-sycl-a770-prefill-bf16-trace-20261027/config-original-snapshot.json'
                assert b.sha(path) == digest, relative
    print('PASS failed full2K fusion trial audit; no accepted speed comparison; unchanged1%/3% gates')


if __name__ == '__main__':
    main()
