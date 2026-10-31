# A770：按执行 ID 恢复 graph ETW 关联

同一份已停止的整模型 ETL 现在通过 decode 区间的队列/DMA 关联检查，未重跑 GPU 或模型。修复的是分析器对 WDDM 抢占的假设，没有新增吞吐收益，也没有证明全部 kernel 的独立覆盖。

机器为 Arc A770 16GB、PCIe 3.0、Windows、驱动 32.0.101.9034。原请求为 Code/Prose 2K 输入、完整 128 输出、正常 graph；已有原生 PID/QPC 标记、完整输出和来源校验。恢复导出的 ETW 丢失事件/缓冲均为 0。

## 原假设与修复

一个 QueuePacket 可以在被抢占后分成多个 DmaPacket 执行段。例如 Prose 的 context `0xffffc303ee542010`、queue sequence61786，先执行 submission98393，在179735408微秒以 `bPreempted=true` 停止；179735494微秒以 submission98396 恢复，随后正常完成。恢复间隔为86微秒。

新 `audit-preemptions.py` 复用原分析器的 PID、QPC、设备/context 归属及区间统计，只替换一队列一 DMA 的假设：

- 队列起止必须唯一、完整，且最终没有抢占或超时标志。
- 每个 DMA submission/completion ID 精确配对；中间段必须标记抢占，最后一段必须正常完成。
- 执行段时间不得重叠，必须在队列生命周期内；恢复间隔单独记录，不纳入执行段并集。
- 同时保留 decode 内有端点的记录，以及生命周期跨越整个 decode 的记录；缺少队列起点不能静默遗漏。
- 先按事件名和长度识别 schema，再选择有 PacketType 的记录。首轮对此筛选不完整的失败及修复前源码均保留。

自检覆盖原 PID/QPC/边界检查、跨完整窗口、合法多段恢复，以及缺失、重复、错 ID、重叠、未完成和超时拒绝。真实 trace 的新审计 exit0；另以端点扫描法从保存的执行段独立重算并集、计数、ID、恢复间隔和边界，全部一致。原全记录及窗口审计的失败没有被覆盖。

## 同一份 trace 的结果

| 项目 | Code2K | Prose2K |
| --- | ---: | ---: |
| 原生标记间隔 | 4505.045 ms | 4883.451 ms |
| 相交的完整 typed 队列生命周期 | 1015 | 1057 |
| DMA 执行段 | 1015 | 1061 |
| 抢占后恢复次数 | 0 | 4 |
| 恢复间隔之和 | 0 | 0.390 ms |
| 裁剪后的执行段并集 | 4455.923 ms | 4743.858 ms |
| 不在这些执行段并集内 | 49.122 ms（1.0904%） | 139.593 ms（2.8585%） |
| 最大未覆盖连续区间 | 0.597 ms | 81.964 ms |

Prose 的81.964毫秒区间为179474344–179556308微秒。区间中点没有该进程已关联的 typed 队列生命周期；后续执行段的队列入队事件发生在179556249微秒，距该执行段开始59微秒。它提示单独定位这一突发区间，而不是把它平均摊成每次 map 查找或提交的固定成本。尚未将它归因为应用工作、CPU调度、驱动或其他进程的GPU活动。

这些是 **WDDM 调度事件区间**，不是 GPU busy/idle 指标，也不是 runtime 重写收益或其上限。DMA 区间内部仍可能包含命令依赖和调度间隙；本组只有两个带诊断的请求，不能转成稳态性能验收。queue→首个DMA的延迟还能与其他执行重叠，其求和也不能当作可节省耗时。

当前仍优先完成已独立确认约3%收益的 runtime append 质量/接入链，并实测[有符号码本分解](signed-grid-2026-10-29.md)的 GEMV 候选。本项移除了后续时间线分析的一个错误假设，没有增加目标完成门槛。

本地证据位于 `build-sycl-a770-graph-etw-model-20261029/`：`audit-preemptions-results.json`、`preemption-intervals-independent.json`、`preemption-audit-schemafix.stdout/.stderr`；修复前失败为 `preemption-audit.stderr`、`preemption-audit-first-failure.json` 和 `audit-preemptions-before-schemafix.py`。

```powershell
python -X utf8 build-sycl-a770-graph-etw-model-20261029/audit-preemptions.py --self-check
# 只读重放同一原始导出和来源哈希，不启动GPU：
python -X utf8 build-sycl-a770-graph-etw-model-20261029/audit-preemptions.py --check
```
