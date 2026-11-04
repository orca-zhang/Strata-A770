"""Archive this isolated CLI change and append measured scope to the checklist."""
import difflib
import importlib.util
import json
from pathlib import Path
import shutil
import sys

H = Path(__file__).resolve().parent
R = H.parent
sys.path.insert(0, str(R/'tools'))
import bench_deepinfer as b
spec = importlib.util.spec_from_file_location('runner', H/'run.py')
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)


def main():
    runner.verify()
    data = json.loads((H/'checks-final-queue/results.json').read_text())
    measured = json.loads((H/'measure-final/results.json').read_text())
    assert data['complete'] and measured['complete']
    assert b.sha(R/'sycl/src/program/generate.cpp') == data['source_sha256']
    out = R/'bench/results/a770/cli-prompt-windows-2026-10-24'
    out.mkdir(exist_ok=False)
    def copy(source, dest):
        dest = out/dest
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, dest)
    for source, dest in [('checks-final-queue/results.json', 'results.json'), ('measure-final/results.json', 'measurements.json'),
                         ('frozen.json', 'frozen.json'), ('build.json', 'build.json'), ('config.json', 'config-original.json'),
                         ('compile.log', 'compiler-logs/compile.txt'), ('link.log', 'compiler-logs/link.txt'),
                         ('compile-queue-fix.log', 'compiler-logs/compile-queue-fix.txt'), ('link-queue-fix.log', 'compiler-logs/link-queue-fix.txt'),
                         ('before/sycl/src/program/generate.cpp', 'source/generate.before.cpp'),
                         ('after/sycl/src/program/generate.cpp', 'source/generate.after.cpp'),
                         ('before/sycl/include/strata/core/mtp.hpp', 'source/mtp.before.hpp'),
                         ('headers/strata/core/mtp.hpp', 'source/mtp.after.hpp'),
                         ('check_results.py', 'check_results.py')]:
        copy(H/source, dest)
    for source, dest in [('checks-final-queue', 'raw'), ('measure-final', 'timing'), ('probe', 'diagnosis'),
                         ('checks', 'failed/initial-check'), ('probe-sync', 'failed/phase-sync-runner'),
                         ('checks-final', 'failed/superseded-check'), ('queue-fix', 'queue-fix'), ('measure', 'failed/initial-measure')]:
        for path in (H/source).rglob('*'):
            if path.is_file() and path.suffix in ('.json', '.cpp', '.stdout', '.stderr', '.protocol'):
                copy(path, Path(dest)/path.relative_to(H/source))
    for path in (R/'build-sycl-cli-prefill-20261024/checks-final').rglob('*'):
        if path.is_file() and path.suffix in ('.json', '.cpp', '.stdout', '.stderr', '.protocol'):
            copy(path, Path('failed/initial-candidate')/path.relative_to(R/'build-sycl-cli-prefill-20261024/checks-final'))
    for name in ('run.py', 'check.py', 'measure.py', 'probe.py', 'probe_sync.py', 'publish.py', 'finish.py', 'queue_fix.py'):
        copy(H/name, Path('runners')/name)
    copy(R/'tools/test_spec_decode_entry.py', 'test_spec_decode_entry.py')
    before = (out/'source/generate.before.cpp').read_text()
    after = (out/'source/generate.after.cpp').read_text()
    patch = ''.join(difflib.unified_diff(before.splitlines(True), after.splitlines(True),
                                    fromfile='sycl/src/program/generate.cpp (before)', tofile='sycl/src/program/generate.cpp (after)'))
    patch += ''.join(difflib.unified_diff((out/'source/mtp.before.hpp').read_text().splitlines(True), (out/'source/mtp.after.hpp').read_text().splitlines(True),
                                    fromfile='sycl/include/strata/core/mtp.hpp (before)', tofile='sycl/include/strata/core/mtp.hpp (after)'))
    (out/'cli-prefill.patch').write_text(patch, encoding='utf-8')
    stable = [row for row in measured['comparisons'] if row['metric'] == 'ttft_ms' and row['passes_1pct']]
    ranges = {}
    for side in ('baseline', 'candidate'):
        ranges[side] = {metric: (min(row[metric] for row in measured['runs'] if row['side'] == side),
                                max(row[metric] for row in measured['runs'] if row['side'] == side))
                        for metric in ('prompt_ms', 'ttft_ms', 'decode_ms', 'wall_s')}
    table = '| 阶段 | 基线范围 | 候选范围 |\n|---|---:|---:|\n'
    for metric, label in [('prompt_ms', 'prefill (ms)'), ('ttft_ms', 'TTFT (ms)'), ('decode_ms', '6 token decode (ms)'), ('wall_s', '进程总耗时 (s)')]:
        a, c = ranges['baseline'][metric], ranges['candidate'][metric]
        table += f'| {label} | {a[0]:.1f}–{a[1]:.1f} | {c[0]:.1f}–{c[1]:.1f} |\n'
    gains = ', '.join(f"{row['latency_reduction_pct']:.2f}%" for row in measured['comparisons'] if row['metric'] == 'ttft_ms')
    drifts = ', '.join(f"{row['baseline_drift_pct']:+.2f}%" for row in measured['comparisons'] if row['metric'] == 'ttft_ms')
    conclusion = (f'两组相邻基线对照的 TTFT 均通过 1% 收益及 3% 漂移门槛；延迟下降 {gains}，相邻基线漂移 {drifts}。'
                  if len(stable) == 2 else f'TTFT 延迟下降观测为 {gains}，相邻基线漂移 {drifts}；仅 {len(stable)}/2 组通过门槛，不声明重复确认通过。')
    decode_rows = [row for row in measured['comparisons'] if row['metric'] == 'decode_ms']
    decode_observed = ', '.join(f"{-row['latency_reduction_pct']:+.2f}%" for row in decode_rows)
    decode_note = f'6-token decode 耗时变化为 {decode_observed}（正数表示变慢）；保留全部数据，未声明 decode 加速。这里比较的是提示读取路径造成的后续短输出阶段变化，并非新 decode 内核的算子测量。'
    report = f'''# A770 原生 CLI 短提示读取（2026-10-24）

原生 SYCL CLI 现在与服务端一样，用现有 `--short-read` 选择短提示的验证窗口读取，避免为很短的提示启动完整批量 prefill。单 token 提示沿用已有入口；`--short-read 0`、超过阈值或部分 `--prefill-until` 继续走原路径。没有新增内核、依赖、实验开关或 API。

- [x] 冻结原工作区、基线、工具链和依赖；只重编 SYCL generator。MTP/采样器/验证器对象与全部库不变；头文件仅叠加 MTP 未加载队列初始化的一行修复，其余头文件共用冻结版本。
- [x] 每个提示窗口关闭 head sampling、提交全部提示 token，按顺序填入 MTP KV，等待最后提交后处理最后一个提示 token。prefill 计时与 decode 阶段计数分开。
- [x] 456 项 CPU 检查：原有两种后端入口 216 项，新路由谓词 240 项，包含关闭窗口、阈值边界、非原生 pack 与部分 prefill。
- [x] 11 次真实 CLI、11 次服务端请求通过。1/2/7/8/23-token 提示、6-token 窗口及跨窗口、阈值 0/0.50、固定种子采样、自然 EOS、指定首 token EOS、关闭 MTP 和批量回退；对应输出逐 token 相同。
- [x] 修复未加载 MTP 时的队列所有权：初始队列设为空，`load()` 才创建自有队列。旧基线在正确输出单 token 后异常退出（Windows `0xC0000409`），初版候选无 MTP 两 token 场景在正确输出后返回 `0xC0000005`；最终候选正常退出。
- [x] 独立进程 A/B/A/B/A，两个 token 输入 `271,91`、最多 6 个输出 token；相同参数和 Q8 KV，实际缓存固定为 5771 slots / 7.74 GiB（服务 INFO 7922 MiB）。性能分析和诊断日志关闭；两条路径此前均已通过独立进程预热，正式记录各 CLI 进程的首次提示处理。
- [x] 原始输出、构建命令、源码/构建哈希和可离线审计脚本归档。生产 EXE 和配置哈希不变。
- [ ] 原生批量长提示和 Short/Code2K/Prose2K/Long8K 的完整整模型矩阵仍待完成。本轮没有修改 CUDA/HIP 入口，也没有为它声明验证或收益。

{table}
{conclusion}

{decode_note}

这些数字只代表上述 **两 token 原生 CLI 提示的 prefill/TTFT**。模型、缓存和输入相同，但选择的提示读取路径不同；不能当作服务端 decode 或四场景整模型吞吐提升。decode 单独保留，不凭这组短输出时延宣布 decode 加速。进程总耗时还包含模型与约 33 GiB 主机镜像的加载；TTFT 是引擎原有会话计时口径，不包含整个进程的加载。

诊断中的旧基线以 58938.6 ms 完成一个 conditioning token，输出 `42`，并非永久死锁。失败记录也保留：阶段同步 runner 错误要求首次诊断超时，因此没有启动引擎；首轮功能脚本把边界提示的正常 EOS 当成“未达到 6 token”；无 MTP 的退出崩溃；退出复现脚本首次只接受 access violation，后来按保留的 fast-fail 实际退出码校正。另一个已启动的旧候选检查在队列修复前被中止，进程和锁所有权检查也归档。首轮计时还出现缓存 tuple/list 比较误判，基线本身正确完成；修正后在新目录完整重跑五轮，未把该首轮拼入最终序列。失败及中止尝试不计入通过数量。

基线 EXE SHA256：`{data['exe_sha256']['baseline']}`。
候选 EXE SHA256：`{data['exe_sha256']['candidate']}`。

## 复测

离线检查无需 GPU：

```powershell
python -X utf8 bench/results/a770/cli-prompt-windows-2026-10-24/check_results.py
```

真实模型检查及计时使用保留的独立构建；每次指定新目录，GPU 通过已有共享锁串行运行：

```powershell
python -X utf8 build-sycl-deepinfer-20261023/check_bf16_arena.py build-sycl-cli-prefill-final-20261024/check.py build-sycl-cli-prefill-final-20261024/checks-repeat
python -X utf8 build-sycl-deepinfer-20261023/check_bf16_arena.py build-sycl-cli-prefill-final-20261024/measure.py build-sycl-cli-prefill-final-20261024/measure-repeat
```

`runners/` 保存本轮脚本来源，运行入口仍位于上面的独立构建目录。`build.json` 给出同一工具链的精确编译/链接命令，`frozen.json` 给出所有复用依赖的哈希。[功能结果](results.json)、[计时和漂移](measurements.json)、[源码差异](cli-prefill.patch)、[初始诊断](diagnosis/results.json)。
'''
    (out/'README.md').write_text(report, encoding='utf-8')
    docs = R/'docs/DEEPINFER_OPTIMIZATIONS.md'
    text = docs.read_text(encoding='utf-8')
    marker = '### 默认与交付\n'
    assert text.count(marker) == 1 and '### 补充：CLI 短提示验证窗口读取' not in text
    section = f'''### 补充：CLI 短提示验证窗口读取（2026-10-24）

- [x] 原生 SYCL CLI 复用服务端的短提示窗口策略，使用已有 `--short-read`；设为 0、超过阈值或部分 `--prefill-until` 保留批量路径。无新依赖/内核/API，生产 EXE 和配置未替换。
- [x] 提示阶段关闭 head sampling，提交所有提示 token，依次填入 MTP KV，等待提交后进入生成；提示耗时与 decode 阶段统计分开。
- [x] 456 项 CPU 检查、11 次 CLI、11 次服务端请求通过，覆盖窗口边界、固定种子采样、阈值 0/0.50、自然/指定 EOS、无 MTP 及批量回退；对应输出逐 token 一致。
- [x] 同时修复未加载 MTP 时释放默认队列导致的退出崩溃：旧基线单 token 复现异常退出；最终无 MTP 候选正常退出。只有该队列初始化头文件发生一行变化，MTP/采样器/验证器对象和其余库不变。
- [x] 两 token 提示独立进程 A/B/A/B/A，实际缓存 5771 slots / 7922 MiB，Q8 KV，诊断/性能分析关闭。{conclusion}
- [x] {decode_note}
- [x] 冻结源码/依赖、原始计时与失败记录、独立测试构建及离线审计归档；不将本轮短提示 prefill/TTFT 结果泛化为服务端 decode 或四场景整模型提速。
- [ ] 长提示原生批量 prefill、完整四场景整模型矩阵，以及 CUDA/HIP 的对应路径仍待验收。

[完整报告](../bench/results/a770/cli-prompt-windows-2026-10-24/README.md)、[功能结果](../bench/results/a770/cli-prompt-windows-2026-10-24/results.json)、[计时](../bench/results/a770/cli-prompt-windows-2026-10-24/measurements.json)和[源码差异](../bench/results/a770/cli-prompt-windows-2026-10-24/cli-prefill.patch)。

```powershell
python -X utf8 bench/results/a770/cli-prompt-windows-2026-10-24/check_results.py
```

'''
    docs.write_text(text.replace(marker, section+marker), encoding='utf-8')
    protected = ('build-sycl-a770-unified-operators/bin/strata-unified-all-operators.exe', 'strata-iq2_xs.json')
    frozen = json.loads((H/'frozen.json').read_text())
    protected_sha = {str(Path(name)): b.sha(R/name) for name in protected}
    for name, digest in protected_sha.items():
        assert digest == frozen['dependencies'][name], name
    b.save(out/'manifest.json', {'files_sha256': {path.relative_to(out).as_posix(): b.sha(path) for path in out.rglob('*') if path.is_file()},
                               'protected_sha256_after': protected_sha})
    print('PASS archived', len(list(out.rglob('*'))), 'entries', flush=True)


if __name__ == '__main__':
    main()
