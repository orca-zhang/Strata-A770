"""Sanitized evidence for the reproduced prefill nonfinite failure; no speed claim."""
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import sys

H = Path(__file__).resolve().parent
R = H.parents[2]
RAW = R / 'build-sycl-a770-prefill-finite-trace-20261027'
STEM = 'prefill-finite-trace-2026-10-27'


def read(path):
    return json.loads(path.read_text(encoding='utf-8'))


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def check(d):
    assert d['complete'] and not any(d[k] for k in ('adopted', 'production_changed', 'speed_acceptance', 'target_achieved'))
    assert d['counter_checks'] == 6 and d['minimum_gain_pct'] == 1 and d['maximum_baseline_drift_pct'] == 3
    assert [r['label'] for r in d['runs']] == ['O1', 'T1', 'T2']
    traced = []
    for run in d['runs']:
        assert run['exit_code'] == 0 and run['expert_slots'] == 5771 and run['free_mib'] >= 1500
        assert [r['case'] for r in run['rows']] == ['Short', 'Code2K', 'Prose2K', 'Code2K']
        assert run['rows'][0]['matches_reference'] and not run['rows'][0]['all_zero']
        for row in run['rows']:
            assert row['generated'] == (16 if row['case'] == 'Short' else 128) and row['reused'] == 0
            assert row['prompt_tokens'] == (23 if row['case'] == 'Short' else 2048)
            assert not (row['all_zero'] and row['matches_reference'])
        for row in run['rows'][1:]:
            trace = row['trace']
            if run['active']:
                assert trace and trace['samples'] == trace['chunks'] * 48 * 4 and trace['tokens'] == row['prompt_tokens'] - 1
                assert trace['bad_samples'] == len(trace['bad'])
                assert row['all_zero'] == bool(trace['bad_samples'])
                assert row['matches_reference'] == (not trace['bad_samples'])
                for sample in trace['bad']:
                    assert 0 <= sample['chunk'] < trace['chunks'] and 0 <= sample['layer'] < 48 and sample['half'] in (0, 1)
                    assert 0 < sample['nonfinite'] <= (2560 if sample['source'] == 'projection' else 10240)
                traced.append(row)
            else:
                assert trace is None
    assert len(traced) == 6 and sum(r['all_zero'] for r in traced) == 4
    assert [r['trace']['bad'][0]['layer'] for r in traced if r['all_zero']] == [6, 6, 8, 32]
    stage = d['full_stage_diagnostic']
    assert stage['complete'] and stage['counter_checks'] == 12
    assert [r['label'] for r in stage['runs']] == ['T1', 'T2']
    for run in stage['runs']:
        assert run['exit_code'] == 0 and run['expert_slots'] == 5771 and run['free_mib'] >= 1500
        assert [r['case'] for r in run['rows']] == ['Short', 'Code2K', 'Prose2K', 'Code2K']
        assert all(r['matches_reference'] and not r['all_zero'] for r in run['rows'])
        for row in run['rows'][1:]:
            trace = row['trace']
            assert row['generated'] == 128 and row['prompt_tokens'] == 2048 and row['reused'] == 0
            assert trace['tokens'] == 2047 and trace['counter_slots'] == trace['chunks'] * 48 * 19
            assert trace['bad_samples'] == 0 and trace['bad'] == []
    tail = d['tail_stage_diagnostic']
    assert tail['complete'] and tail['counter_checks'] == 12 and tail['element_cap'] == 10240
    for kind, labels in (('enabled', ['T1', 'T2']), ('disabled', ['O1', 'O2'])):
        assert [r['label'] for r in tail[kind]] == labels
        for run in tail[kind]:
            assert run['exit_code'] == 0 and run['expert_slots'] == 5771 and run['free_mib'] >= 1500
            assert [r['case'] for r in run['rows']] == ['Short', 'Code2K', 'Prose2K', 'Code2K']
            for row in run['rows']:
                assert row['matches_reference'] and not row['all_zero'] and row['reused'] == 0
                assert row['generated'] == (16 if row['case'] == 'Short' else 128)
                if row['case'] != 'Short' and kind == 'enabled':
                    assert row['trace']['tokens'] == 2047 and row['trace']['bad_samples'] == 0 and row['trace']['bad'] == []
                else:
                    assert row['trace'] is None
    repeat = d['longer_tail_diagnostic']
    assert repeat['complete'] and repeat['same_exe_as_failed_trial'] and repeat['counter_checks'] == 12
    assert [run['label'] for run in repeat['runs']] == ['T1', 'T2']
    large = []
    for run in repeat['runs']:
        assert run['exit_code'] == 0 and run['expert_slots'] == 5771 and run['free_mib'] >= 1500
        assert [row['case'] for row in run['rows']] == ['Short'] + ['Code2K', 'Prose2K'] * 4
        for row in run['rows']:
            assert row['generated'] == (16 if row['case'] == 'Short' else 128) and row['reused'] == 0
            assert row['prompt_tokens'] == (23 if row['case'] == 'Short' else 2048)
            assert not (row['all_zero'] and row['matches_reference'])
            if row['case'] == 'Short':
                assert row['matches_reference'] and row['trace'] is None
            else:
                trace = row['trace']
                assert trace['tokens'] == 2047 and trace['counter_slots'] == trace['chunks'] * 48 * 19
                assert trace['bad_samples'] == len(trace['bad'])
                assert all(0 <= bad['layer'] < 48 and 0 <= bad['chunk'] < trace['chunks'] and 0 < bad['nonfinite'] <= 10240 for bad in trace['bad'])
                large.append(row)
    assert repeat['large_requests'] == len(large) == 16
    assert repeat['matching_large_requests'] == sum(row['matches_reference'] for row in large)
    assert repeat['all_zero_large_requests'] == sum(row['all_zero'] for row in large)
    if 'bf16_stage_diagnostic' in d:
        focused = d['bf16_stage_diagnostic']
        assert focused['complete'] and focused['counter_checks'] == 36
        assert [run['label'] for run in focused['runs']] == ['T1', 'T2']
        for run in focused['runs']:
            assert run['exit_code'] == 0 and run['expert_slots'] == 5771 and run['free_mib'] >= 1500
            assert [row['case'] for row in run['rows']] == ['Short'] + ['Code2K', 'Prose2K'] * 4
            for row in run['rows']:
                assert row['generated'] == (16 if row['case'] == 'Short' else 128) and row['reused'] == 0
                assert row['prompt_tokens'] == (23 if row['case'] == 'Short' else 2048)
                assert not (row['matches_reference'] and row['all_zero'])
                if row['case'] == 'Short':
                    assert row['matches_reference'] and row['trace'] is None
                    continue
                trace = row['trace']
                assert trace['tokens'] == 2047 and trace['counter_slots'] == trace['chunks'] * 48 * 21
                assert trace['bad_samples'] == len(trace['bad'])
                for bad in trace['bad']:
                    limit = 2047 * (96 if bad['source'] == 'ab' else 2560) if bad['source'] in ('mixed', 'mixed_bf_before', 'mixed_bf_after_proj', 'ab') else 10240
                    assert 0 <= bad['layer'] < 48 and 0 <= bad['chunk'] < trace['chunks'] and 0 < bad['nonfinite'] <= limit
        large = [row for run in focused['runs'] for row in run['rows'][1:]]
        assert focused['large_requests'] == len(large) == 16
        assert focused['matching_large_requests'] == sum(row['matches_reference'] for row in large)
        assert focused['all_zero_large_requests'] == sum(row['all_zero'] for row in large)


def publish():
    path = RAW / 'model/results.json'
    raw, built = read(path), read(RAW / 'checked.json')
    assert raw['complete'] and built['complete']
    runs = []
    for run in raw['runs']:
        rows = [{**{k: row[k] for k in ('case', 'generated', 'prompt_tokens', 'reused', 'all_zero', 'matches_reference')},
                 'trace': row.get('trace')} for row in run['rows']]
        runs.append({**{k: run[k] for k in ('label', 'active', 'exit_code', 'expert_slots', 'free_mib')}, 'rows': rows})
    d = dict(complete=True, adopted=False, production_changed=False, speed_acceptance=False, target_achieved=False,
        counter_checks=built['checks'], minimum_gain_pct=1, maximum_baseline_drift_pct=3,
        hardware='A77016GB / PCIe3.0 / Windows / DPC++2025.3.2 / driver32.0.101.9034', runs=runs,
        limitation='Only last projection/residual rows, after each half, are sampled. Additional counter kernels/allocation change scheduling. Correlation does not identify the producing operation, a race, or a fix. No performance acceptance.',
        files_sha256=raw['files_sha256'] | {path.relative_to(R).as_posix(): sha(path)})
    stage_path = R / 'build-sycl-a770-prefill-stage-trace-20261027/model/results.json'
    stage = read(stage_path)
    stage_built = read(stage_path.parent.parent / 'checked.json')
    d['full_stage_diagnostic'] = dict(complete=stage['complete'], counter_checks=stage_built['checks'],
        runs=[{**{k: run[k] for k in ('label', 'exit_code', 'expert_slots', 'free_mib')},
               'rows': [{**{k: row[k] for k in ('case', 'generated', 'prompt_tokens', 'reused', 'all_zero', 'matches_reference')},
                         'trace': row.get('trace')} for row in run['rows']]} for run in stage['runs']])
    d['files_sha256'].update(stage['files_sha256'] | {stage_path.relative_to(R).as_posix(): sha(stage_path)})
    tail_dir = R / 'build-sycl-a770-prefill-stage-tail-trace-20261027'
    tail_on, tail_off = [read(tail_dir / folder / 'results.json') for folder in ('model', 'off-control')]
    assert tail_on['complete'] and tail_off['complete']
    exe_key = (tail_dir / 'bin/strata-unified-all-operators.exe').relative_to(R).as_posix()
    assert tail_on['files_sha256'][exe_key] == tail_off['files_sha256'][exe_key]
    d['tail_stage_diagnostic'] = dict(complete=True, counter_checks=read(tail_dir / 'checked.json')['checks'], element_cap=10240)
    for kind, raw_tail in (('enabled', tail_on), ('disabled', tail_off)):
        d['tail_stage_diagnostic'][kind] = [{**{k: run[k] for k in ('label', 'exit_code', 'expert_slots', 'free_mib')},
            'rows': [{**{k: row[k] for k in ('case', 'generated', 'prompt_tokens', 'reused', 'all_zero', 'matches_reference')},
                      'trace': row.get('trace')} for row in run['rows']]} for run in raw_tail['runs']]
        d['files_sha256'].update(raw_tail['files_sha256'])
    for folder in ('model', 'off-control'):
        p = tail_dir / folder / 'results.json'
        d['files_sha256'][p.relative_to(R).as_posix()] = sha(p)
    repeat_path = R / 'build-sycl-a770-prefill-tail-repeat-20261027/model/results.json'
    repeat = read(repeat_path)
    failed_dir = R / 'build-sycl-a770-gu-q8-export-tailoff-model-20261027'
    failed = read(failed_dir / 'partial.json')
    assert repeat['complete'] and not failed['complete'] and failed['runs'][-1]['exit_code'] != 0
    key = (failed_dir / 'bin/strata-unified-all-operators.exe').relative_to(R).as_posix()
    assert repeat['files_sha256'][key] == failed['files_sha256'][key]
    large = [row for run in repeat['runs'] for row in run['rows'] if row['case'] != 'Short']
    d['longer_tail_diagnostic'] = dict(complete=True, counter_checks=12, same_exe_as_failed_trial=True,
        exe_sha256=repeat['files_sha256'][key], large_requests=len(large),
        matching_large_requests=sum(row['matches_reference'] for row in large), all_zero_large_requests=sum(row['all_zero'] for row in large),
        runs=[{**{k: run[k] for k in ('label', 'exit_code', 'expert_slots', 'free_mib')},
            'rows': [{**{k: row[k] for k in ('case', 'generated', 'prompt_tokens', 'reused', 'all_zero', 'matches_reference')},
                      'trace': row.get('trace')} for row in run['rows']]} for run in repeat['runs']])
    d['files_sha256'].update(repeat['files_sha256'] | {p.relative_to(R).as_posix(): sha(p) for p in (repeat_path, failed_dir / 'partial.json')})
    archive = R / 'build-sycl-a770-prefill-bf16-trace-20261027/config-original-snapshot.json'
    if archive.exists():
        assert sha(archive) == repeat['files_sha256']['strata-iq2_xs.json']
        d['files_sha256'][archive.relative_to(R).as_posix()] = sha(archive)
    focused_path = R / 'build-sycl-a770-prefill-bf16-trace-20261027/model/results.json'
    if focused_path.exists():
        focused = read(focused_path)
        focused_build = read(focused_path.parent.parent / 'checked.json')
        assert focused['complete'] and focused_build['complete'] and focused_build['checks'] == 36
        assert focused['args'] == repeat['args'], 'Focused diagnostic changed the fixed benchmark arguments'
        focused_large = [row for run in focused['runs'] for row in run['rows'][1:]]
        d['bf16_stage_diagnostic'] = dict(complete=True, counter_checks=36, large_requests=len(focused_large),
            matching_large_requests=sum(row['matches_reference'] for row in focused_large), all_zero_large_requests=sum(row['all_zero'] for row in focused_large),
            runs=[{**{k: run[k] for k in ('label', 'exit_code', 'expert_slots', 'free_mib')},
                'rows': [{**{k: row[k] for k in ('case', 'generated', 'prompt_tokens', 'reused', 'all_zero', 'matches_reference')},
                          'trace': row.get('trace')} for row in run['rows']]} for run in focused['runs']])
        d['files_sha256'].update(focused['files_sha256'] | {focused_path.relative_to(R).as_posix(): sha(focused_path)})
    check(d)
    lines = ['# A770 预填充非有限值追踪（2026-10-27）', '', d['hardware'], '',
        '40 tok/s 目标未达到，本诊断未修改生产配置。本轮用于定位全零输出，不作速度验收。', '',
        '保留原 IQ、校正后的 GDN 和其余模型对象，仅替换带默认关闭计数器的预填充对象。相同 EXE 运行 OFF/ON/ON 三个独立进程，固定 cache5500、5771 槽位、READY 空闲至少 1500 MiB；每进程依次 Short16、Code2K128、Prose2K128、Code2K128，均保留完整输出。恢复环境变量时正确删除 UNSET 项，未启用 debug_nan、逐层主机等待或分析器。', '',
        '计数核在原计算队列中排队，每个 chunk、每层两半分别检查投影和残差的最后一行，在已有最终等待后读取计数。本轮 2048 输入中，预填充追踪覆盖前 2047 token、实际 1 个 chunk；最后一个输入 token 走原有单步路径。精确源代码的 GPU 自检通过空输入、跨工作组长度、NaN/Inf、守卫区和输入不变等 6 项检查。', '',
        '| 进程 | 请求 | 全零 | 匹配参考 | 非有限采样数 | 首次异常（chunk/层/半/来源） |',
        '| --- | --- | --- | --- | ---: | --- |']
    for run in runs:
        for row in run['rows'][1:]:
            trace = row['trace']
            first = trace['bad'][0] if trace and trace['bad'] else None
            where = '/'.join(str(first[k]) for k in ('chunk', 'layer', 'half', 'source')) if first else '—'
            count = str(trace['bad_samples']) if trace else '关闭'
            lines.append(f"| {run['label']} | {row['case']} | {row['all_zero']} | {row['matches_reference']} | {count} | {where} |")
    lines += ['', '开启追踪的 6 次完整 2K 请求中，4 次全零均伴随非有限值；另 2 次全部采样为有限值，且完整输出匹配参考。关闭追踪的对照也有 1 次全零。首次异常均在首 chunk 的注意力输出投影后，层号为 6/6/8/32（从 0 开始），随后传播。', '',
        '这些计数只覆盖最后一行，不能据此认定投影是异常的源头；更早的其他行、GDN 内部状态仍可能先异常。计数核也会改变调度，不能单凭通过/失败判定同步缺陷或已修复。下一步在独立构建中细分完整数组的输入、状态、卷积、递推和投影。无效输出的耗时不用于收益判断，原 1% 收益及 3% 基线漂移门槛保持。', '',
        '后续完整数组分阶段追踪检查 HC 输入、GDN 状态与卷积历史的前后值、输入投影、门控、卷积、递推的 FP32/FP16 输出，以及两半投影和残差。计数核仍在原队列中排队，无逐层主机等待或读取。FP32/FP16 原语共 12 项 GPU 自检通过。两个独立进程共 6 次完整 2K 输出全部匹配参考，未检出非有限值；完整扫描增加明显开销，可能改变触发条件，不能据此宣称修复。日志 samples=912 计的是分配槽位，包含 QSA 层清零但未采样的 GDN 专用槽。', '',
        '再将每个计数器限制为尾部最多 10240 个元素，保留同样的阶段。FP32/FP16 共 12 项 GPU 检查覆盖读取上限两侧、NaN/Inf、守卫区及输入不变。两个独立 traceON 进程的 6 次完整 2K 输出均匹配参考，未检出异常。随后同一 EXE 的两个 traceOFF 进程，关闭计数器分配和所有探针核，6 次完整 2K 输出也均匹配参考。此处不能把通过归因于追踪同步，也不能将重编译后的通过当成旧构建缺陷已修复；这不是交错的性能配对实验。仍需进一步复现并解释构建差异，生产保持不变。', '',
        f"关闭追踪后的实际融合验收随后在末端OFF基线复现Prose全零，之前18次诊断通过没有解决缺陷。再用这个发生失败的精确EXE关闭融合、开启尾部探针：两个新进程各做四轮Code/Prose，完整2K请求共{len(large)}次，{sum(row['matches_reference'] for row in large)}次匹配、{sum(row['all_zero'] for row in large)}次全零，非有限采样槽共{sum(row['trace']['bad_samples'] for row in large)}个。保留所有输出和计数，探针仍可能改变触发条件。此组不作性能验收。", '',
        '| 延长诊断进程 | 完整请求序号 | 场景 | 首次非有限阶段 | 元素数 |', '| --- | ---: | --- | --- | ---: |']
    for run in repeat['runs']:
        for index, row in enumerate(run['rows'][1:], 1):
            if row['trace']['bad']:
                first = row['trace']['bad'][0]
                lines.append(f"| {run['label']} | {index} | {row['case']} | chunk{first['chunk']}/层{first['layer']}/{first['source']} | {first['nonfinite']} |")
    lines += ['', '第一处采样异常已提前到alpha/beta投影结果ab；同层FP16输入、QKV与z尾部尚未报异常。现有探针没有直接检查BF16输入，且仅覆盖各数组尾部，不能断定投影本身是根因。另一次最早在递推y中发现异常，也可能遗漏更早的非尾部异常。下一步优先区分BF16输入、交错投影及递推，暂不追加融合速度配对。', '',
        '离线复核：`python -X utf8 bench/results/a770/check_prefill_finite_trace_20261027.py --self-check`。`--local` 额外等待 GPU 独占锁后复核本机原始文件摘要。', '']
    if 'bf16_stage_diagnostic' in d:
        focused = d['bf16_stage_diagnostic']
        lines += [f"后续完整BF16/AB诊断：36项FP32/FP16/BF16完整/尾部原语检查通过；两个独立进程共{focused['large_requests']}次完整2K请求，{focused['matching_large_requests']}次匹配、{focused['all_zero_large_requests']}次全零。完整扫描FP32混合输入、QKV/z前后的BF16混合输入和交错AB输出，其余阶段仍只采样尾部10240元素。计算路径、权重和GDN对象保留，关闭融合；探针改变调度，此组没有修复或性能结论。", '',
            '| BF16诊断进程 | 完整请求序号 | 场景 | 输出匹配 | 最早非有限阶段 |', '| --- | ---: | --- | --- | --- |']
        for run in focused['runs']:
            for index, row in enumerate(run['rows'][1:], 1):
                first = row['trace']['bad'][0] if row['trace']['bad'] else None
                where = f"chunk{first['chunk']}/层{first['layer']}/{first['source']}" if first else '未检出'
                lines.append(f"| {run['label']} | {index} | {row['case']} | {row['matches_reference']} | {where} |")
        lines += ['', '等待期间另一项任务将生产草稿阈值更新为0.8，本诊断仍使用原固定参数0.5。新旧配置来源分别保留，原配置快照与此前冻结SHA256相同；本地核对同时验证原快照及当前配置，不将独立任务的配置更新归为本诊断优化。', '']
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
        for key in ('all_zero', 'matches_reference'):
            bad = copy.deepcopy(d)
            bad['runs'][1]['rows'][1][key] = not bad['runs'][1]['rows'][1][key]
            try:
                check(bad)
            except AssertionError:
                pass
            else:
                raise AssertionError('accepted changed ' + key)
        fake = copy.deepcopy(d)
        fake['bf16_stage_diagnostic'] = copy.deepcopy(d['longer_tail_diagnostic'])
        fake['bf16_stage_diagnostic']['counter_checks'] = 36
        for run in fake['bf16_stage_diagnostic']['runs']:
            for row in run['rows'][1:]:
                row['trace']['counter_slots'] = row['trace']['chunks'] * 48 * 21
        check(fake)
        trace = fake['bf16_stage_diagnostic']['runs'][0]['rows'][1]['trace']
        trace.update(bad_samples=1, bad=[dict(chunk=0, layer=0, source='mixed_bf_before', nonfinite=2047 * 2560 + 1)])
        try:
            check(fake)
        except AssertionError:
            pass
        else:
            raise AssertionError('accepted BF16 count above full input length')
    if '--local' in sys.argv:
        sys.path.insert(0, str(R / 'tools'))
        import bench_deepinfer as b
        with b.gpu_lock():
            manifests = set(d['files_sha256'].items())
            tail_dir = R / 'build-sycl-a770-prefill-stage-tail-trace-20261027'
            sources = [(RAW, 'run_v2.py', 'model'), (R / 'build-sycl-a770-prefill-stage-trace-20261027', 'run.py', 'model'),
                                               (tail_dir, 'run.py', 'model'), (tail_dir, 'run_off.py', 'off-control'),
                                               (R / 'build-sycl-a770-prefill-tail-repeat-20261027', 'run.py', 'model')]
            if 'bf16_stage_diagnostic' in d:
                sources.append((R / 'build-sycl-a770-prefill-bf16-trace-20261027', 'run.py', 'model'))
            for directory, script, folder in sources:
                spec = importlib.util.spec_from_file_location('finite_raw_checker_' + directory.name, directory / script)
                runner = importlib.util.module_from_spec(spec)
                spec.loader.exec_module(runner)
                raw = read(directory / folder / 'results.json')
                manifests.update(raw['files_sha256'].items())
                for run in raw['runs']:
                    assert run['trace'] == runner.parse_trace((directory / folder / (run['label'] + '.stderr')).read_text(encoding='utf-8'))
                    for row in run['rows']:
                        assert row['all_zero'] == (not any(row['tokens']))
                        assert row['matches_reference'] == (row['tokens'] == raw['reference'][row['case']])
            for relative, digest in manifests:
                path = R / relative
                if relative == 'strata-iq2_xs.json' and b.sha(path) != digest:
                    path = R / 'build-sycl-a770-prefill-bf16-trace-20261027/config-original-snapshot.json'
                assert b.sha(path) == digest, relative
    print('PASS reproduced prefill nonfinite evidence; no speed or fix claim')


if __name__ == '__main__':
    main()
