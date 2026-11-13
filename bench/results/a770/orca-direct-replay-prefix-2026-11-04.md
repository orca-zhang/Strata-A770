# Orca Direct 图重放完成证明复用：同一 EXE 消融与当前 L0 控制

2026-11-04，Windows 10 22H2，Intel Arc A770 16 GB，驱动 `32.0.101.9034`，oneAPI 2025.3。

这是隔离候选，没有修改已接入的仓库源码、生产配置或部署 EXE。
在图重放之前，原代码先确认上一轮整段完成并发布回读台账，随后仍逐个刷新段内 inline 事件的缓存行。
候选复用已经确认的完成证明：检查旧点与尾点的队列、原生提交序号、逻辑顺序和回读台账一致，
并且旧 inline 点没有独立资源租约，然后发布旧点完成。普通事件继续走原收集路径。
**重置新一轮 tag 的 store/clflush 和每段一次 mfence 均保留**；省去的是旧完成查询中的重复 clflush/mfence。

## 正确性与构建

- 单编译单元筛选在关/开两种策略下，共检查 12,800 个未被观察过的旧事件点，覆盖 16 轮重放、被 gate 挡住的新一轮、在途重放拒绝和 guards。
- 扩展检查再覆盖 12,800 个旧点，其中 800 个点明确依赖更早、尚未发布的 staged readback；核对旧回读发布和新 inline 点在 gate 关闭时保持未完成。
- 模型候选重编全部 69 个适配器使用者；其余 11 个 Host 单元根据实际依赖复用，原生 runtime 和数学 pack 不变，避免新旧内联定义混用。
- 候选通过 840 项双专家完整输出检查、36 组 FP16/BF16 GEMM、engine/queue 检查，以及 BCS=0/1 两种队列检查。
- 五组完整模型共 **85 次请求**的全部 token、生成数、草稿采纳/提供数、终止原因和复用状态匹配。

## 独立微型实验

每次重放含 200 个 inline 信号；先丢弃 8 次，再保留 32 次。Host 时间在 owner 内包围 replay，
whole 时间还包括调用交接和原生提交尾部等待。没有推理运算，这些数值不能当作模型收益。

| 进程序号 | 策略 | Host 中位数 µs | whole 中位数 µs |
| --- | --- | ---: | ---: |
| bench-0-0 | 关 | 113.25 | 368.45 |
| bench-1-1 | 开 | 72.25 | 319.50 |
| bench-2-0 | 关 | 194.55 | 407.25 |
| bench-3-1 | 开 | 72.85 | 316.45 |
| bench-4-0 | 关 | 99.30 | 344.80 |

旧策略的中间进程出现明显波动：Host 对照从 113.25 到 194.55 再到 99.30 µs。
因此不把该微型实验的名义百分比投射到端到端，也不将它作为稳定模型加速的证据。

## 完整模型对照

顺序为**当前源码 L0 → 同一 Direct EXE 关 → 开 → 关 → 当前源码 L0**。
两条路径均来自[已接入源码重建](orca-source-adoption-2026-11-04.md)，L0 已包含 M16 与专家补槽 copy-role 变化。
Direct 的唯一策略变量为隔离候选启动开关 `STRATA_ORCA_DG2_COMPLETED_PREFIX=0/1`；BCS 固定为 1。
五组使用逐字节相同的数学 pack，参数与输入历史相同。

固定 5,771 专家槽、1,536 MiB 预留、read256、decay0.95、margin1.5、MTP4、spec-min-p0.5、
cumulative0、suffix0、cost-window0。这是锚定配置，不是日常平衡默认。
每进程 Short 15 次（丢弃 10 次，保留 5 次），随后 2K、8K 各一次；无前缀复用。
Short 取保留请求中位数，两侧同路径控制取几何均值。
净 decode 使用 DONE 时间，排除初始化、准备和 prefill；TTFT 单列。
计时期间没有内部探针、GPU 时间戳、遥测或并行编译，未锁 GPU 时钟。

| 输入 | 路径 | prefill ms | TTFT ms | 净 decode tok/s |
| --- | --- | ---: | ---: | ---: |
| Short28 | 当前源码 L0 | 900.59 | 936.45 | 59.0178 |
| Short28 | Direct 关 | 894.97 | 932.69 | 57.9139 |
| Short28 | Direct 开 | 891.90 | 929.22 | 58.4660 |
| Code2K | 当前源码 L0 | 27215.39 | 27265.87 | 19.0236 |
| Code2K | Direct 关 | 24949.65 | 25003.00 | 18.8012 |
| Code2K | Direct 开 | 24294.30 | 24346.28 | 19.0686 |
| Long8K | 当前源码 L0 | 71331.44 | 71382.56 | 19.1444 |
| Long8K | Direct 关 | 70661.83 | 70716.98 | 18.8937 |
| Long8K | Direct 开 | 69823.20 | 69878.84 | 19.0267 |

时间减少为负，吞吐提高为正。Direct 关/开隔离完成证明复用；与 L0 比较保留完整传输/适配器差异。

| 输入 | 候选/控制 | prefill 时间变化 | TTFT 变化 | 净 decode 吞吐变化 |
| --- | --- | ---: | ---: | ---: |
| Short28 | Direct 开/关 | -0.3431% | -0.3724% | +0.9535% |
| Short28 | Direct 开/L0 | -0.9646% | -0.7725% | -0.9349% |
| Short28 | Direct 关/L0 | -0.6236% | -0.4017% | -1.8705% |
| Code2K | Direct 开/关 | -2.6267% | -2.6266% | +1.4223% |
| Code2K | Direct 开/L0 | -10.7332% | -10.7078% | +0.2365% |
| Code2K | Direct 关/L0 | -8.3252% | -8.2992% | -1.1692% |
| Long8K | Direct 开/关 | -1.1868% | -1.1852% | +0.7040% |
| Long8K | Direct 开/L0 | -2.1144% | -2.1066% | -0.6148% |
| Long8K | Direct 关/L0 | -0.9387% | -0.9324% | -1.3095% |

## 控制漂移与边界

| 输入 | 控制 | prefill 时间漂移 | 净 decode 吞吐漂移 |
| --- | --- | ---: | ---: |
| Short28 | Direct 关 | -1.5961% | +0.6115% |
| Short28 | L0 | +1.0717% | -1.8007% |
| Code2K | Direct 关 | -1.7339% | +2.5224% |
| Code2K | L0 | +1.5559% | -1.2113% |
| Long8K | Direct 关 | +1.5253% | -1.9845% |
| Long8K | L0 | +1.2444% | -2.3149% |

每个长请求在每个进程只跑一次，这是有限的输入序号匹配观察；没有建立长请求稳态、随机化或跨日重复性。
候选只有一个模型进程，控制漂移不能代替候选重复性。不同测试历史的绝对值不能拼接，独立优化百分比不能相加。
五组匹配**不能关闭此前冷历史 Direct 8K 间歇性连续 token0/草稿0/0 问题**。
本轮没有新的 D2D/加锁普查、PPL 或 Linux GPU 结果；完成证明复用仍未接入生产源码。
当前仍推荐 Orca L0；是否采用候选应依据完整模型结果，不能凭微型实验判定。

## 复查

```powershell
python -X utf8 build-orca-slm-async-20261104/replay_prefix_experiment.py --verify
python -X utf8 build-orca-slm-async-20261104/build_replay_prefix.py --verify
python -X utf8 build-orca-slm-async-20261104/check_replay_prefix_readbacks.py --verify
python -X utf8 build-orca-slm-async-20261104/replay_prefix_contexts.py r1 --verify
python -X utf8 build-orca-slm-async-20261104/publish_replay_prefix.py --check
```

重新计量使用 `replay_prefix_contexts.py r2` 等新编号，不覆盖 r1。
档案提供候选头文件、独立检查源码、构建命令/依赖、配置、原始协议/日志与 SHA-256；
未变的完整基础源码见前述源码接入档案。数学 pack 与构建二进制记录哈希，不分发模型或二进制。

精确 JSON (`direct-replay-prefix-a770-windows-2026-11-04.json`, raw capture retained locally) ·
原始证据 (`direct-replay-prefix-a770-windows-2026-11-04.json.gz`, raw capture retained locally)
