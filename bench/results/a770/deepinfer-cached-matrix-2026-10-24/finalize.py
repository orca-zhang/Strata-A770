"""Archive the completed screen without changing runtime defaults or production files."""
import importlib.util
import json
import shutil
from pathlib import Path

F = Path(__file__).resolve().parent
R = F.parent
A = R / 'bench/results/a770/deepinfer-cached-matrix-2026-10-24'
assert not (A / 'archive.json').exists()
spec = importlib.util.spec_from_file_location('matrix_audit', A / 'check_results.py')
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)
prep = json.loads((F / 'preparation.json').read_text(encoding='utf-8'))
data = json.loads((F / 'results.json').read_text(encoding='utf-8'))
assert data['complete'] and data['summary'] == audit.derive(data, prep)
for name, digest in prep['frozen_sha256'].items():
    assert audit.sha(R / name) == digest, name
for p in F.iterdir():
    if p.is_file() and p.suffix in ('.py', '.json', '.stderr', '.protocol'):
        shutil.copyfile(p, A / p.name)
(A / 'final-checks.json').write_text(json.dumps(dict(
    production_unchanged=True, runtime_defaults_changed=False, frozen_sha256=prep['frozen_sha256'],
    native_batch_prefill_validated=False, independent_confirmation=False), indent=2) + '\n', encoding='utf-8')
rows, latency, initial, counts = [], [], [], []
for c in data['summary']['comparisons']:
    name = c['case']
    d = c['metrics']['decode_ms']
    tps = [1000 / audit.statistics.median(v['decode_ms']/v['generated'] for v in r['cases'][name]['samples'])
           for r in data['runs']]
    rows.append(f"| {name} | {tps[0]:.2f} | {tps[1]:.2f} | {tps[2]:.2f} | {d['throughput_change_pct']:+.2f}% | "
                f"{d['baseline_drift_pct']:+.2f}% | {'合格' if c['decode_eligible'] else '排除'} |")
    tt = [audit.statistics.median(v['ttft_ms'] for v in r['cases'][name]['samples']) for r in data['runs']]
    latency.append(f"| {name} | {tt[0]:.2f} | {tt[1]:.2f} | {tt[2]:.2f} | {c['cached_reused_tokens']} |")
    for i, run in enumerate(data['runs']):
        case = run['cases'][name]
        v = case['initial']
        initial.append(f"| {i} {run['mode']} | {name} | {v['reused']} | {v['prompt_ms']:.1f} | {v['ttft_ms']:.1f} |")
        counts.append(f"| {i} {run['mode']} | {name} | {sum(v['accepted'] for v in case['samples'])} | "
                      f"{sum(v['offered'] for v in case['samples'])} |")
(A / 'README.md').write_text(f'''# DeepInfer 四场景前缀复用筛选（2026-10-24）

结论：{'通过本轮筛选，仍需独立确认' if data['summary']['screen_pass'] else '未通过本轮筛选'}。
四场景输出和前缀复用量一致；不调整运行时默认值，不替换生产 EXE 或配置。

A770 16GB、Xeon E5-2696 v4、Windows、oneAPI 2025.3.2。
三次独立进程 A/B/A，全部使用同一个冻结候选 EXE（41824ddcfa4f…）及其依赖，只切换四项 DeepInfer 开关。
阈值0.50，故固定长度草稿链连续提交不生效；Q8 KV，输入23/2048/2048/8000 tokens，输出上限128。
缓存请求5500，实际各臂5771 slots / 7922 MiB；prompt-cache 128，每512 token保存检查点。
保留自适应专家缓存和PCIe比例0.16；共享GPU锁、每请求前后检查其他GPU/编译进程，分析器和阶段打印关闭。
每进程每场景先发一次初始请求、再预热两次、保留三次；共72次请求、36次保留请求。
所有初始、预热、保留输出逐 token 相同，统计与原始协议独立核对。

以下为**缓存前缀后的完整模型生成**，每臂取三次每token解码耗时中位数，单位tok/s。
对照吞吐为两侧基线耗时几何平均除以候选耗时；漂移按后一基线耗时/前一基线耗时计算，绝对值超过3%排除。

| 场景 | A0 | B1组合 | A2 | 吞吐变化 | 基线耗时漂移 | 配对 |
|---|---:|---:|---:|---:|---:|---|
{chr(10).join(rows)}

四场景吞吐变化的描述性几何平均为{data['summary']['descriptive_decode_geomean_pct']:+.2f}%；该描述值包含漂移排除项，不能替代验收。
事先规则：全部四场景配对合格、几何平均至少+1%、任一场景不低于−2%、输出与复用量一致，才通过筛选。
本轮仅A/B/A一次筛选，没有独立确认，不声明最新全部优化已获得稳定整模型加速。
被测构建是冻结的DeepInfer候选，不包含后续CPU tokenizer或其他会话的XMX实验；结果不与其他算子收益相加或连乘。

| 场景 | A0 TTFT ms | B1 TTFT ms | A2 TTFT ms | 每次复用token |
|---|---:|---:|---:|---|
{chr(10).join(latency)}

TTFT为命令发出到收到首token的主机计时，包含检查点恢复；此表各指标独立取中位数。
缓存请求仍保留完整上下文，实际新读取token较少，不能代表首次长输入TTFT。

初始请求使用已有窗口读取路径，没有该场景预热。以下单次读数保留供排障，既不用于正式prefill性能判定，也不代表原生批量prefill：

| 进程 | 场景 | 复用token | 初始prompt ms | 初始TTFT ms |
|---|---|---:|---:|---:|
{chr(10).join(initial)}

| 进程 | 场景 | 三次保留请求接受草稿数合计 | 提议数合计 |
|---|---|---:|---:|
{chr(10).join(counts)}

全部原始DONE统计、TTFT、输出token和协议保留；preparation.json含事先规则、固定参数、输入、源码/构建/生产文件哈希。
source-manifest.json与candidate-source-manifest.json描述冻结引擎源码；不包含EXE、权重或新依赖。

```powershell
python bench/results/a770/deepinfer-cached-matrix-2026-10-24/check_results.py
```

复测：将 run.py、frozen_bench.py、prepare.py、preparation.json 放入仓库根目录下新的构建目录，不复制旧results.json；
准备记录中的冻结EXE和依赖必须仍存在且哈希相同，再运行（会等待共享GPU锁）：

```powershell
python -X utf8 build-sycl-deepinfer-20261023/check_bf16_arena.py <新目录>/run.py
```
''', encoding='utf-8')
(A / 'archive.json').write_text(json.dumps(dict(sha256={p.relative_to(A).as_posix(): audit.sha(p)
    for p in sorted(A.rglob('*')) if p.is_file() and '__pycache__' not in p.parts and p.name != 'archive.json'}),
    indent=2) + '\n', encoding='utf-8')
audit.main()
