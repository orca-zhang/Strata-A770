"""Audit actual decode-window/cache diagnostics; never accept invalid-output timings."""
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import sys

H = Path(__file__).resolve().parent
R = H.parents[2]
STEM = 'window-cache-survey-2026-10-27'
CASES = ('Code2K', 'Prose2K')
FOLDERS = {mode: f'build-sycl-a770-gdn-{mode}-window-survey-20261027' for mode in ('widen', 'xmx')}
FOLDERS['xmx_inline_issuer'] = 'build-sycl-a770-issuer-off-window-20261027'
FOLDERS['xmx_fused_ab_trace'] = 'build-sycl-a770-gdn-ab-fused-trace-v2-20261027'
FOLDERS['xmx_hc_context'] = 'build-sycl-a770-hc-failure-context-20261027'
FOLDERS['xmx_hc_context_repeat'] = 'build-sycl-a770-hc-failure-context-repeat-20261027'
FOLDERS['xmx_hc_context_off'] = 'build-sycl-a770-hc-context-off-20261027'
FUSED = ('xmx_fused_ab_trace', 'xmx_hc_context', 'xmx_hc_context_repeat')
HC = ('xmx_hc_context', 'xmx_hc_context_repeat')
LONG = ('xmx_hc_context_repeat', 'xmx_hc_context_off')


def read(path):
    return json.loads(path.read_text(encoding='utf-8'))


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def windows(text):
    requests, rows, last = [], [], None
    for line in text.splitlines():
        host = re.fullmatch(r'verify host: pos=(\d+) T=(\d+) submit=([\d.]+) wait=([\d.]+) ms', line)
        route = re.fullmatch(r'strata device routing: (\d+)/(\d+) entries in VRAM', line)
        if host:
            pos, t, submit, wait = host.groups()
            last = dict(pos=int(pos), T=int(t), submit_ms=float(submit), wait_ms=float(wait))
        elif route:
            assert last is not None
            hits, entries = map(int, route.groups())
            assert 1 <= last['T'] <= 6 and 0 <= hits <= entries == 480 * last['T']
            rows.append(last | dict(hits=hits, entries=entries))
            last = None
        elif line.startswith('strata decode timing:'):
            count = int(re.search(r'(\d+) windows', line)[1])
            assert count == len(rows) > 0 and last is None
            assert all(a['pos'] < b['pos'] <= a['pos'] + a['T'] for a, b in zip(rows, rows[1:]))
            requests.append(rows)
            rows = []
    assert not rows and last is None
    return requests


def summarize(rows):
    result = {}
    for case in CASES:
        selected = [row for row in rows if row['case'] == case and row['matches_reference']]
        if not selected:
            continue
        events = [event for row in selected for event in row['windows']]
        counts = Counter(str(event['T']) for event in events)
        hits, entries = (sum(event[key] for event in events) for key in ('hits', 'entries'))
        submit, wait = (sum(event[key] for event in events) for key in ('submit_ms', 'wait_ms'))
        result[case] = dict(valid_requests=len(selected), windows=len(events), T_counts=dict(sorted(counts.items())),
            T_le2_pct=100 * sum(event['T'] <= 2 for event in events) / len(events),
            vram_entries=hits, routed_entries=entries, vram_entry_hit_pct=100 * hits / entries,
            submit_ms=submit, wait_ms=wait, submit_fraction_pct=100 * submit / (submit + wait))
    return result


def ab_profiles(text):
    result, bad, snapshots = [], [], []
    for line in text.splitlines():
        if line.startswith('GDN_AB_FUSED_BAD '):
            row = {key: int(value) for key, value in re.findall(r'(\w+)=(\d+)', line)}
            assert set(row) == {'call', 'T', 'nonfinite', 'first_row', 'first_col'}
            assert 0 < row['nonfinite'] <= row['T'] * 48 and 0 <= row['first_row'] < row['T'] and 0 <= row['first_col'] < 48
            bad.append(row)
        elif line.startswith('GDN_AB_FUSED_SNAPSHOT '):
            row = {key: float(value) if key.startswith('max_') else int(value)
                for key, value in re.findall(r'(\w+)=([\d.eE+\-]+)', line)}
            assert row['call'] == bad[0]['call'] and row['row'] == bad[0]['first_row']
            assert 0 <= row['x_nonfinite'] <= 2560 and 0 <= row['w_nonfinite'] <= 48 * 2560
            assert 0 <= row['cpu64_nonfinite'] <= 48 and 0 < row['gpu_nonfinite'] <= 48
            snapshots.append(row)
        elif line.startswith('strata decode timing:'):
            result.append(dict(bad_calls=bad, snapshots=snapshots))
            bad, snapshots = [], []
    assert not bad and not snapshots
    assert sum(len(row['snapshots']) for row in result) <= 1
    calls = [bad['call'] for row in result for bad in row['bad_calls']]
    assert calls == sorted(set(calls))
    return result


def hc_profiles(text):
    result, stages, cpu = [], [], None
    sizes = dict(residual=10240, row_scales=4, normalized_bf16=10240, hc_down=320, silu_bf16=320,
        hc_up=10240, mixed_fp32=2560, mixed_bf16=2560, mixed_fp16=2560,
        previous_half_projection=2560, hc_inject=4, norm_weight=10240)
    for line in text.splitlines():
        if line.startswith('HC_FAILURE_CONTEXT '):
            fields = dict(re.findall(r'(\w+)=([\w.eE+\-]+)', line))
            row = {key: fields[key] if key == 'source' else float(fields[key]) if key == 'max_abs' else int(fields[key]) for key in fields}
            assert 0 <= row['layer'] < 48 and 0 <= row['row'] < 2047
            assert row['elements'] == sizes[row['source']] and 0 <= row['nonfinite'] <= row['elements']
            assert row['max_abs'] >= 0
            stages.append(row)
        elif line.startswith('HC_FAILURE_CPU64 '):
            assert cpu is None and [row['source'] for row in stages] == list(sizes)
            fields = dict(re.findall(r'(\w+)=([\w.eE+\-]+)', line))
            cpu = {key: float(value) if key.startswith('max_') else int(value) for key, value in fields.items()}
            assert 0 <= cpu['scales_nonfinite'] <= 4 and cpu['max_abs_scale'] >= 0
            assert all((row['layer'], row['row']) == (cpu['layer'], cpu['row']) for row in stages)
        elif line.startswith('strata decode timing:'):
            assert bool(stages) == (cpu is not None)
            result.append(dict(stages=stages, cpu64=cpu))
            stages, cpu = [], None
    assert not stages and cpu is None and sum(bool(row['stages']) for row in result) <= 1
    return result


def check(d):
    assert d['complete'] and not any(d[k] for k in ('adopted', 'production_changed', 'speed_acceptance', 'target_achieved'))
    assert d['minimum_gain_pct'] == 1 and d['maximum_baseline_drift_pct'] == 3
    assert 'widen' in d['modes']
    assert d['widen_dispatch_review'] == dict(bf16_gpu_widen_enabled=False, resolved_ab_path='fallback_gemm', evidence='frozen source and archived effective configuration; no runtime dispatch log')
    for mode, data in d['modes'].items():
        assert mode in FOLDERS and data['complete']
        assert len(data['runs']) == 2 and [run['label'] for run in data['runs']] == ['D1', 'D2']
        for run in data['runs']:
            assert run['expert_slots'] == 5771 and run['free_mib'] >= 1500 and run['exit_code'] == 0
            assert run['initializer_matches_reference'] and not run['initializer_all_zero']
            assert [row['case'] for row in run['rows']] == list(CASES) * (4 if mode in LONG else 2)
            for row in run['rows']:
                assert row['generated'] == 128 and row['prompt_tokens'] == 2048 and row['reused'] == 0
                assert row['windows'] and row['windows'][0]['pos'] == 2047
                assert not (row['all_zero'] and row['matches_reference'])
        rows = [row for run in data['runs'] for row in run['rows']]
        assert data['large_requests'] == len(rows) == (16 if mode in LONG else 8)
        assert data['matching_large_requests'] == sum(row['matches_reference'] for row in rows)
        assert data['all_zero_large_requests'] == sum(row['all_zero'] for row in rows)
        assert data['summary_valid_outputs_only'] == summarize(rows)
        if mode == 'xmx_hc_context_off':
            terminal = data['runner_exit']
            assert terminal['runner_exit_code'] == 1 and terminal['winerror'] == 32
            assert terminal['engine_exit_codes'] == [run['exit_code'] for run in data['runs']]
            assert terminal['large_requests'] == data['large_requests'] and terminal['matching_large_requests'] == data['matching_large_requests']
            assert not terminal['gpu_or_compiler_processes_terminated']


def collect():
    d = dict(complete=True, adopted=False, production_changed=False, speed_acceptance=False, target_achieved=False,
        minimum_gain_pct=1, maximum_baseline_drift_pct=3, modes={}, files_sha256={},
        hardware='A77016GB / PCIe3.0 / Windows / DPC++2025.3.2 / driver32.0.101.9034')
    fixed_args, fixed_inputs, fixed_reference = None, None, None
    for mode, directory in FOLDERS.items():
        folder = R / directory / 'model'
        path = folder / 'results.json'
        if not path.exists():
            continue
        raw = read(path)
        assert raw['complete'] and not raw['speed_acceptance']
        if mode == 'widen':
            archive = R / 'build-sycl-a770-prefill-bf16-trace-20261027/config-runtime-snapshot.json'
            golden = R / 'build-sycl-a770-40tps-recovery-20261026/gdn-xmx-model/results.json'
            gemm = R / 'build-sycl-a770-40tps-recovery-20261026/gdn-build-v2/candidate.dp.cpp'
            checked = gemm.with_name('checked.json')
            link = archive.with_name('link-model.cmd')
            assert sha(archive) == raw['files_sha256']['strata-iq2_xs.json']
            effective = read(archive)['env'] | read(golden)['overrides']
            assert effective.get('STRATA_PREFILL_BF16_GPU_WIDEN', 'UNSET') == 'UNSET'
            assert sha(gemm) == read(checked)['source_sha256'][str(gemm.relative_to(R))]
            assert sha(link) == read(archive.with_name('checked.json'))['files_sha256'][link.relative_to(R).as_posix()]
            assert str(gemm.with_name('candidate.obj')) in link.read_text(encoding='utf-8')
            source = gemm.read_text(encoding='utf-8')
            assert 'if (!(bf16 ? bf16_enabled : enabled)' in source
            assert 'if (!staged) fallback_gemm(X,W,Y,T,N,K,ldx,ldy,1.f,beta,false,stream);' in source
            d['widen_dispatch_review'] = dict(bf16_gpu_widen_enabled=False, resolved_ab_path='fallback_gemm', evidence='frozen source and archived effective configuration; no runtime dispatch log')
            d['files_sha256'].update({p.relative_to(R).as_posix(): sha(p) for p in (archive, golden, gemm, checked, link)})
        if mode in FUSED or mode == 'xmx_hc_context_off':
            build_folder = R / FOLDERS['xmx_hc_context'] if mode in HC or mode == 'xmx_hc_context_off' else folder.parent
            built = read(build_folder / 'checked.json')
            assert built['complete'] and built['checks'] == 16
            assert [(x['mode'], x['checks'], x['exit_code']) for x in built['gdn_parity']] == [('xmx', 158, 0), ('widen', 158, 0)]
            if mode in HC or mode == 'xmx_hc_context_off':
                assert built['context_checks'] == 5 and read(build_folder / 'parity.json')['complete']
        if mode == 'xmx_hc_context_off':
            assert raw['ab_trace_enabled'] is False
            on = read(R / FOLDERS['xmx_hc_context_repeat'] / 'model/results.json')
            assert on['complete'] and len(on['runs']) == 2 and all(len(run['rows']) == 9 for run in on['runs'])
            exe = FOLDERS['xmx_hc_context'] + '/bin/strata-unified-all-operators.exe'
            assert raw['files_sha256'][exe] == on['files_sha256'][exe]
        fixed_args = raw['args'] if fixed_args is None else fixed_args
        fixed_inputs = raw['input_ids'] if fixed_inputs is None else fixed_inputs
        fixed_reference = raw['reference'] if fixed_reference is None else fixed_reference
        assert raw['args'] == fixed_args and '--spec-split' not in fixed_args
        assert raw['input_ids'] == fixed_inputs and raw['reference'] == fixed_reference
        assert fixed_args[fixed_args.index('--spec-min-p') + 1] == '0.5'
        runs = []
        for run in raw['runs']:
            stderr = (folder / (run['label'] + '.stderr')).read_text(encoding='utf-8')
            if mode == 'xmx_hc_context_off':
                assert not re.search(r'^(?:GDN_AB_FUSED_|HC_FAILURE_)', stderr, re.M)
            profile = windows(stderr)
            traces = ab_profiles(stderr) if mode in FUSED else [None] * len(profile)
            assert len(traces) == len(profile)
            contexts = hc_profiles(stderr) if mode in HC else [None] * len(profile)
            assert len(contexts) == len(profile)
            assert len(profile) == len(run['rows']) == (9 if mode in LONG else 5) and not run['trace']
            assert run['rows'][0]['case'] == 'Short' and profile[0][0]['pos'] == 22
            outputs, tokens = [], []
            for line in (folder / (run['label'] + '.protocol')).read_text(encoding='utf-8').splitlines():
                if line.startswith(('T ', 'TB ')):
                    tokens.extend(map(int, line.split()[1:] if line.startswith('T ') else line.split()[2:]))
                elif line.startswith('DONE '):
                    assert len(tokens) == int(line.split()[1])
                    outputs.append((line, tokens))
                    tokens = []
            assert not tokens and outputs == [(row['done'], row['tokens']) for row in run['rows']]
            for row in run['rows']:
                assert row['all_zero'] == (not any(row['tokens']))
                assert row['matches_reference'] == (row['tokens'] == raw['reference'][row['case']])
                assert row['generated'] == len(row['tokens']) == (16 if row['case'] == 'Short' else 128)
                assert row['prompt_tokens'] == (23 if row['case'] == 'Short' else 2048)
                assert row['reused'] == 0 and row['finish'] == 'length'
            rows = []
            for row, events, trace, context in zip(run['rows'][1:], profile[1:], traces[1:], contexts[1:]):
                rows.append({**{key: row[key] for key in ('case', 'generated', 'prompt_tokens', 'reused', 'all_zero', 'matches_reference')},
                    'output_sha256': hashlib.sha256(json.dumps(row['tokens'], separators=(',', ':')).encode()).hexdigest(), 'windows': events})
                if trace is not None:
                    rows[-1]['ab_trace'] = trace
                if context is not None:
                    rows[-1]['hc_context'] = context
                    assert bool(context['stages']) == bool(trace['snapshots'])
                    if context['stages']:
                        assert context['cpu64']['row'] == trace['snapshots'][0]['row']
            runs.append({**{key: run[key] for key in ('label', 'expert_slots', 'free_mib', 'exit_code')}, 'rows': rows,
                'initializer_matches_reference': run['rows'][0]['tokens'] == raw['reference']['Short'],
                'initializer_all_zero': run['rows'][0]['all_zero']})
        rows = [row for run in runs for row in run['rows']]
        d['modes'][mode] = dict(complete=True, runs=runs, large_requests=len(rows),
            matching_large_requests=sum(row['matches_reference'] for row in rows), all_zero_large_requests=sum(row['all_zero'] for row in rows),
            summary_valid_outputs_only=summarize(rows))
        manifests = raw['files_sha256'] | {path.relative_to(R).as_posix(): sha(path)}
        if mode == 'xmx_hc_context_off':
            terminal_path = folder.parent / 'runner-exit.json'
            d['modes'][mode]['runner_exit'] = read(terminal_path)
            manifests[terminal_path.relative_to(R).as_posix()] = sha(terminal_path)
        assert all(relative not in d['files_sha256'] or d['files_sha256'][relative] == digest for relative, digest in manifests.items()), 'Conflicting source provenance'
        d['files_sha256'].update(manifests)
    assert {'widen', 'xmx'} <= set(d['modes'])
    check(d)
    return d


def publish():
    d = collect()
    lines = ['# A770 实际窗口与缓存调查（2026-10-27）', '', d['hardware'], '',
        '两场景稳定40 tok/s尚未达到。本组仅诊断，关闭融合与有限值探针，保留原IQ，使用现有DECODE_TIMING主机日志。初始widen/XMX及内联提交组使用同一EXE，后续诊断对象变动分别说明。每进程Short16后两轮Code/Prose，延长组另标四轮；固定原2048/full128、5771槽、草稿阈值0.5；不改生产配置。', '',
        '每个窗口的主机计时、输出校验和路由计数保留在JSON。显存命中数是正常窗口等待后观察到的主机驻留表，不能证明GPU执行瞬间的指针快照。submit/wait为主机经过时间，不是GPU阶段份额；日志本身也有开销。', '']
    for mode, data in d['modes'].items():
        if mode == 'widen':
            lines += ['分派解释更正：本组虽设置STRATA_PREFILL_GDN_AB=widen，但与原记录哈希一致的运行配置快照和修正参考覆盖项均未开启STRATA_PREFILL_BF16_GPU_WIDEN。核对实际链接配方及哈希匹配的冻结GEMM源码后，此模式会拒绝GPU widening并执行直接SYCL fallback_gemm。这是源码与有效配置重建，没有运行时分派日志；保留原始模式名和全部数据，不能再将本组失败解释为真正FP32 widening已经验证失败，也不能据此认定oneMKL或4GiB分配是根因。', '']
        elif mode == 'xmx_hc_context_off':
            lines += ['补充同EXE关闭对照：在相同HC上下文EXE的16次traceON组完成后，另开两个新进程，保持原参数、输入、完整输出参考和每进程四轮Code/Prose，仅将STRATA_PREFILL_AB_FUSED_TRACE从1改为0；这同时关闭异常记录分配、散写计数/回读及HC主机指针登记。复用已完成的原语检查，不重编译。固定保留16次长请求及失败；用于检验记录开关是否影响复现，不构成性能配对或修复证明。', '', '两个引擎均以0退出；完整results.json落盘后，runner清理自有共享锁遇到WinError32并以1退出，错误另存runner-exit.json。人工清理前锁归属已变化，保护检查中止，没有删除新持有者的锁，也没有终止其他实验进程。不要将输出检查通过写成runner无错误退出。', '']
        elif mode == 'xmx_inline_issuer':
            lines += ['补充单变量对照：保持原XMX、同EXE与所有参数，仅用现有开关关闭预填充专家后台提交线程，改为内联提交。有限值探针仍关闭；这不是性能配对。', '']
        elif mode == 'xmx_fused_ab_trace':
            lines += ['补充融合异常记录：只更换隔离GEMM对象，保持原XMX与同一预填充对象，有限值探针关闭。异常计数并入原有等待的AB散写核；正常输出不增加GPU启动或等待。每进程首次异常后才读取该输入行、权重及投影输出，做CPU64复算。散写核与主机记账仍会改变时序；事后快照不能证明GEMM执行瞬间的输入相同。16项精确散写辅助函数检查及XMX/widen各158项原始GDN检查通过。初次编译及检查链接失败记录保留；无修复或性能采用结论。', '']
        elif mode in HC:
            lines += ['补充HC上下文：沿用相同AB异常散写核，仅增加主机指针登记，在每进程首次异常后抓取该行的12项残差/HC上下文及CPU64行归一化统计。无正常路径GPU探针或新增显式等待；这些是事后快照，不能单独证明最早生产者。新对象重跑XMX/widen各158项原GDN检查，统计辅助函数5项CPU检查通过；沿用此前16项散写检查。', '']
            if mode == 'xmx_hc_context_repeat':
                lines += ['短组结束后，另开两个新进程，每个Short16后四轮Code/Prose，共固定16次完整2K请求；EXE与参数相同。全部请求保留，延长诊断不作性能筛选。', '']
        lines += [f"{mode}：完整2K请求{data['large_requests']}次，{data['matching_large_requests']}次匹配，{data['all_zero_large_requests']}次全零。所有失败样本保留，但不进入有效输出统计。", '',
            '| 场景 | 匹配请求 | 窗口数 | T≤2窗口 | 显存条目命中 | 主机submit占submit+wait |', '| --- | ---: | ---: | ---: | ---: | ---: |']
        for case, row in data['summary_valid_outputs_only'].items():
            lines.append(f"| {case} | {row['valid_requests']} | {row['windows']} | {row['T_le2_pct']:.2f}% | {row['vram_entry_hit_pct']:.2f}% | {row['submit_fraction_pct']:.2f}% |")
        if not data['summary_valid_outputs_only']:
            lines += ['', '没有匹配参考的完整2K输出，本模式无有效窗口/缓存汇总。']
        if mode in FUSED:
            lines += ['', '| 进程 | 请求序号 | 场景 | AB异常调用数 | 首次异常行输入/权重非有限 | CPU64/GPU非有限 |', '| --- | ---: | --- | ---: | --- | --- |']
            captures = []
            for run in data['runs']:
                for index, row in enumerate(run['rows'], 1):
                    trace = row['ab_trace']
                    shot = trace['snapshots'][0] if trace['snapshots'] else None
                    inputs = f"{shot['x_nonfinite']}/{shot['w_nonfinite']}" if shot else '无首次快照'
                    outputs = f"{shot['cpu64_nonfinite']}/{shot['gpu_nonfinite']}" if shot else '无首次快照'
                    lines.append(f"| {run['label']} | {index} | {row['case']} | {len(trace['bad_calls'])} | {inputs} | {outputs} |")
                    if shot:
                        first = trace['bad_calls'][0]
                        captures.append(f"首次异常调用记录{first['nonfinite']}个非有限输出，首个位置为第{first['first_row']}行、第{first['first_col']}列（均从0开始）。该行事后激活快照已非有限，不能将此例单独归因于AB投影计算；尚未找到最早产生异常的残差或HC阶段。")
            lines += ['', *captures]
        if mode in HC:
            for run in data['runs']:
                for row in run['rows']:
                    context = row['hc_context']
                    if not context['stages']:
                        continue
                    cpu = context['cpu64']
                    lines += ['', f"{run['label']} {row['case']}：第{cpu['layer']}层、第{cpu['row']}行（从0开始）失败后上下文：", '', '| 数据 | 非有限元素 | 元素数 | 最大有限绝对值 |', '| --- | ---: | ---: | ---: |']
                    for stage in context['stages']:
                        lines.append(f"| {stage['source']} | {stage['nonfinite']} | {stage['elements']} | {stage['max_abs']:.9g} |")
                    lines += ['', f"按该残差行CPU64重算，4个通道的RMS逆尺度中有{cpu['scales_nonfinite']}个非有限值，最大有限绝对值为{cpu['max_abs_scale']:.9g}。"]
        lines += ['']
    lines += ['GU/Q8候选只覆盖T≤2且GU22的子集；窗口比例不能直接换算为算子耗时比例或整模型收益。不同投影模式未形成性能配对；非零但不同的输出也尚无质量等价结论。没有修复、收益或采用结论，1%/3%性能门槛保持。', '',
        '复核：`python -X utf8 bench/results/a770/check_window_cache_survey_20261027.py --self-check`；`--local`等待共享锁后验证原始日志、完整输出与文件哈希。', '']
    report = H / (STEM + '.md')
    report.write_text('\n'.join(lines), encoding='utf-8')
    d.update(checker_sha256=sha(Path(__file__)), report_sha256=sha(report))
    (H / (STEM + '.json')).write_text(json.dumps(d, ensure_ascii=False, indent=2), encoding='utf-8')


def main():
    if '--self-check' in sys.argv:
        fake = 'verify host: pos=0 T=6 submit=1.000 wait=2.000 ms\nverify host: pos=2047 T=1 submit=0.100 wait=20.000 ms\nstrata device routing: 240/480 entries in VRAM\nstrata decode timing: 1 windows, avg T 1.00\n'
        assert windows(fake)[0][0]['pos'] == 2047
        try:
            windows(fake.replace('1 windows', '2 windows'))
        except AssertionError:
            pass
        else:
            raise AssertionError('accepted missing routing windows')
        good = dict(case='Code2K', matches_reference=True, windows=windows(fake)[0])
        invalid = dict(case='Code2K', matches_reference=False, windows=[dict(T=6, hits=0, entries=2880, submit_ms=999, wait_ms=999)])
        assert summarize([invalid]) == {} and summarize([good, invalid]) == summarize([good])
        assert ab_profiles(fake) == [dict(bad_calls=[], snapshots=[])]
        assert hc_profiles(fake) == [dict(stages=[], cpu64=None)]
        try:
            hc_profiles('HC_FAILURE_CONTEXT layer=4 row=20 source=residual elements=10240 nonfinite=1 max_abs=3\nHC_FAILURE_CPU64 layer=4 row=20 scales_nonfinite=1 max_abs_scale=1\nstrata decode timing: 1 windows\n')
        except AssertionError:
            pass
        else:
            raise AssertionError('accepted incomplete HC context')
        witness = 'GDN_AB_FUSED_BAD call=3 T=2047 nonfinite=48 first_row=2046 first_col=0\nGDN_AB_FUSED_SNAPSHOT call=3 row=2046 x_nonfinite=0 w_nonfinite=0 cpu64_nonfinite=0 gpu_nonfinite=48 max_abs_x=1 max_abs_w=2 max_abs_cpu64=3 max_finite_error=0\nstrata decode timing: 1 windows\n'
        assert ab_profiles(witness)[0]['snapshots'][0]['gpu_nonfinite'] == 48
        try:
            ab_profiles(witness.replace('row=2046 x_', 'row=2000 x_'))
        except AssertionError:
            pass
        else:
            raise AssertionError('accepted snapshot from a different row')
    if '--publish' in sys.argv:
        publish()
    path = H / (STEM + '.json')
    if not path.exists():
        assert '--self-check' in sys.argv
        print('PASS window parser, missing-window rejection and invalid-output exclusion')
        return
    d = read(path)
    check(d)
    assert d['checker_sha256'] == sha(Path(__file__)) and d['report_sha256'] == sha(H / (STEM + '.md'))
    if '--local' in sys.argv:
        sys.path.insert(0, str(R / 'tools'))
        import bench_deepinfer as b
        with b.gpu_lock():
            recomputed = collect()
            assert recomputed == {key: d[key] for key in recomputed}
            for relative, digest in d['files_sha256'].items():
                assert b.sha(R / relative) == digest, relative
    print('PASS actual-window diagnostic; invalid outputs excluded; no speed acceptance')


if __name__ == '__main__':
    main()
