# Orca Direct 当前源码：完成证明复用与等待路径隔离

2026-11-04，Windows 10 22H2，Intel Arc A770 16 GB，驱动 `32.0.101.9034`，oneAPI 2025.3。

本实验在[完成证明复用候选](orca-direct-replay-prefix-2026-11-04.md)上隔离原生 completion 的等待路径。
事件版保留原来的 D3DKMT 异步事件注册和 OS 等待；轮询版仅将这段等待替换为主动读 fence、检查超时和 `_mm_pause()`。
它是 OS 唤醒延迟的诊断上界，需要占用 CPU，不是新的默认等待策略。
两条 Direct 路径均启用相同的完成证明复用，BCS 固定为 1。
共享相同 Host 对象、数学 pack、内存放置、命令编码和提交逻辑；只重编原生 runtime 并重新链接。
L0 使用已接入 M16/copy-role 的当前源码构建，逐字节共用数学 pack。

## 正确性

- runtime 通过 14 项 CTest，包含 timeout/observer、host gate、多队列、recording、release/reclaim 和分配恢复。
- 新链接的 kernel/engine/queue 检查通过，包含 840 项双专家完整输出检查，以及 BCS=0/1 队列重放。
- **75 次完整模型请求全部匹配**完整 token、生成数、草稿采纳/提供数、结束原因和零复用状态。
- 候选保持隔离，没有修改生产源码、部署 EXE 或默认配置；本轮不关闭已有间歇性 Direct 8K 问题。

## 完整模型 Short

顺序为 **Direct 事件 → Direct 轮询 → L0 → Direct 轮询 → Direct 事件**。
每进程先丢弃 10 次 Short，再保留 5 次，固定 76 个生成 token。
固定 5,771 专家槽、1,536 MiB 预留、read256、decay0.95、margin1.5、MTP4、spec-min-p0.5、
cumulative0、suffix0、cost-window0；这是锚定配置，不是日常平衡默认。
使用 DONE 净 decode，排除初始化、准备与 prefill；TTFT 单列。
进程内取保留请求中位数，同路径两侧进程取几何均值。
计时无内部探针、GPU 时间戳、遥测或并行编译；GPU 时钟未锁。

| 进程 | 路径 | prefill ms | TTFT ms | 净 decode tok/s |
| --- | --- | ---: | ---: | ---: |
| 0 | Direct 事件 | 911.000 | 949.319 | 56.4007 |
| 1 | Direct 轮询 | 909.600 | 948.211 | 57.3066 |
| 2 | L0 | 912.200 | 948.581 | 58.3225 |
| 3 | Direct 轮询 | 902.900 | 947.299 | 57.4756 |
| 4 | Direct 事件 | 922.500 | 962.271 | 56.8267 |

| 路径汇总 | prefill ms | TTFT ms | 净 decode tok/s |
| --- | ---: | ---: | ---: |
| Direct 事件 | 916.732 | 955.773 | 56.6133 |
| Direct 轮询 | 906.244 | 947.755 | 57.3910 |
| L0 | 912.200 | 948.581 | 58.3225 |

时间下降为负，吞吐提高为正。

| 比较 | prefill 时间变化 | TTFT 变化 | 净 decode 吞吐变化 |
| --- | ---: | ---: | ---: |
| 轮询 / 事件 | -1.1441% | -0.8389% | +1.3738% |
| 轮询 / L0 | -0.6529% | -0.0870% | -1.5970% |
| 事件 / L0 | +0.4968% | +0.7582% | -2.9305% |

| 两侧控制 | prefill 时间漂移 | TTFT 漂移 | 净 decode 吞吐漂移 |
| --- | ---: | ---: | ---: |
| Direct 事件 | +1.2623% | +1.3644% | +0.7552% |
| Direct 轮询 | -0.7366% | -0.0962% | +0.2949% |

## 边界与后续

控制漂移是观测，不是置信区间。L0 只有一个模型进程；轮询两侧虽有重复，仍没有跨日重复性或锁频控制。
比较复用了相同 Host 对象，候选之间的差异定位到 native completion 的事件/主动等待；
结果不能进一步拆成内核调度、驱动唤醒、CPU 核迁移或缓存一致性的独立耗时。
本轮不采集 CPU 占用或功耗，因此不能声称轮询的系统效率更高；默认事件等待保持不变。
若值得继续，应再测有限自旋后转事件等待，检验延迟和 CPU 成本，而不是直接默认无限轮询。
本轮没有新增长上下文、PPL、D2D/加锁普查或 Linux GPU 数据。
此前完成证明的独立百分比不与本轮相加；全部失败和成功证据遵循独立目录保留规则。

## 复查

```powershell
python -X utf8 build-orca-slm-async-20261104/build_replay_prefix_poll.py --verify
python -X utf8 build-orca-slm-async-20261104/replay_prefix_poll_short.py r1 --verify
python -X utf8 build-orca-slm-async-20261104/publish_replay_prefix_wait.py --check
```

新计量使用 `replay_prefix_poll_short.py r2` 等新编号，不覆盖成功证据。
档案保留候选 runtime 源码、构建与检查命令、依赖、运行配置、原始协议/日志与哈希；
未变 Host 对象的源码和构建见[源码接入](orca-source-adoption-2026-11-04.md)及前述完成证明档案。
数学 pack 与二进制记录哈希，不分发模型或二进制。

精确 JSON (`direct-replay-prefix-wait-a770-windows-2026-11-04.json`, raw capture retained locally) ·
原始证据 (`direct-replay-prefix-wait-a770-windows-2026-11-04.json.gz`, raw capture retained locally)
