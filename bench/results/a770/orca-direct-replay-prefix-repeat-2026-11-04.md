# Orca Direct 完成证明复用：反向 Short 复测

2026-11-04，Windows 10 22H2，Intel Arc A770 16 GB，驱动 `32.0.101.9034`，oneAPI 2025.3。

本轮只验证[前轮隔离候选](orca-direct-replay-prefix-2026-11-04.md)的 Short 趋势。
不重编、不修改 EXE、数学 pack、运行时或参数；顺序反转为 **Direct 开 → 关 → 当前源码 L0 → 关 → 开**。
每进程 10 次丢弃、5 次保留，共 **75 次请求全部匹配**完整 token、生成数、草稿采纳/提供数、
终止原因和零复用状态。此前完整模型对照为 85 次请求，两轮分别报告，不合并为一个性能样本。

固定 5,771 专家槽、1,536 MiB 预留、read256、decay0.95、margin1.5、MTP4、spec-min-p0.5、
cumulative0、suffix0、cost-window0；这是锚定配置，不是日常平衡默认。
两条路径逐字节共用数学 pack；L0 已含源码接入的 M16 和 copy-role 改动，BCS 对 Direct 固定为 1。
净 decode 使用 DONE 时间，排除准备、初始化、prefill；TTFT 单列。
保留请求取进程内中位数，同策略两侧进程取几何均值。未锁 GPU 时钟；计时无内部探针、遥测或并行编译。

| 轮次 | 路径 | prefill ms | TTFT ms | 净 decode tok/s |
| --- | --- | ---: | ---: | ---: |
| 正向 | L0 | 900.587 | 936.451 | 59.0178 |
| 正向 | Direct 关 | 894.971 | 932.690 | 57.9139 |
| 正向 | Direct 开 | 891.900 | 929.217 | 58.4660 |
| 反向 | L0 | 912.200 | 948.354 | 58.4031 |
| 反向 | Direct 关 | 926.127 | 964.482 | 56.8993 |
| 反向 | Direct 开 | 917.300 | 956.462 | 56.9738 |

| 轮次 | 比较 | TTFT 变化 | 净 decode 吞吐变化 |
| --- | --- | ---: | ---: |
| 正向 | Direct 开/关 | -0.3724% | +0.9535% |
| 正向 | Direct 开/L0 | -0.7725% | -0.9349% |
| 正向 | Direct 关/L0 | -0.4017% | -1.8705% |
| 反向 | Direct 开/关 | -0.8315% | +0.1311% |
| 反向 | Direct 开/L0 | +0.8549% | -2.4473% |
| 反向 | Direct 关/L0 | +1.7006% | -2.5750% |

| 轮次 | 两侧同策略控制 | TTFT 漂移 | 净 decode 吞吐漂移 |
| --- | --- | ---: | ---: |
| 正向 | Direct 关 | -1.5280% | +0.6115% |
| 正向 | L0 | +1.0399% | -1.8007% |
| 反向 | Direct 开 | -0.0469% | -0.5159% |
| 反向 | Direct 关 | -2.3614% | +0.6158% |

## 判断边界

两轮使用相反顺序，全部原始进程和请求都保留。控制漂移是观测值，不是置信区间；
每轮候选的进程数有限，不能据此证明跨日稳定收益或把剩余百分比归因于单一驱动模块。
本次只新增 Short 数据，不更新长上下文、PPL 或 Linux GPU 结论；此前间歇性 Direct 8K 问题继续保持打开。
完成证明复用仍是隔离候选，生产源码、部署 EXE、默认等待模式与配置未因此改变。
微型 replay 的相对加速不能投射到完整模型，独立实验的收益不能相加。

## 复查

```powershell
python -X utf8 build-orca-slm-async-20261104/replay_prefix_short_reverse.py r1 --verify
python -X utf8 build-orca-slm-async-20261104/publish_replay_prefix_repeat.py --check
```

新计量使用 `replay_prefix_short_reverse.py r2` 等新编号，不覆盖成功证据。
前轮报告提供候选头文件、构建和安全检查；本档案补充反向 runner、全部配置、原始协议/日志和 SHA-256。

[精确 JSON](../../../sycl/orca/direct-replay-prefix-repeat-a770-windows-2026-11-04.json) ·
原始证据 (`direct-replay-prefix-repeat-a770-windows-2026-11-04.json.gz`, raw capture retained locally)
