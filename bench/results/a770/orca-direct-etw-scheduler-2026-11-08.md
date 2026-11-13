# Short 请求的 Windows GPU 调度追踪

2026-11-08，Windows10／Arc A77016GB／驱动32.0.101.9034／oneAPI2025.3.2。

复用相同Q2准入诊断的matched-L0／Direct／matched-L0 EXE，数学包、HC128／MTP4／min-p0.5、5000缓存预算、1536MiB预留及关闭短提示热度的条件保留，实际均5243槽／7201MiB。完整Verifier和shared-expert源码文本一致，其它适配层和全部动态工作尚未证明一致。这与原5771槽锚定及当前spec6生产数据分开。

每个新进程依次Code2K×2、Short×10，仅第10条Short启用独立DxgKrnl ETW会话；显式capturestate取得device／context／node映射。Base／Resource／GPUScheduler掩码0x8041，级别5，无堆栈、逐kernel探针或新增GPU时间戳。只停止本脚本创建的会话，已有系统追踪保留。

完整请求36条／输出2868个，全部token／8协议字段和退出码通过。三份追踪均零事件／缓冲丢失，独立重读原始XML核对进程归属；所有所选队列／DMA起止及序列配对完整。

以下是完整Short请求的调度观察，包含准备／prefill／decode，不能标作净decode或正常无追踪速度。ETW开关和rundown可能扰动执行。DMA起止之间可能包含GPU内部依赖、信号量等待和其他停顿，不是EU活跃时间或纯kernel耗时。

| 顺序／后端 | node | 活跃context | work DMA段 | 所有队列包 | DMA区间并集 ms | 区间首尾内无本进程DMA ms | 已入队且本进程无DMA ms | DMA抢占标记 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 0／matched | 5 | 1 | 240 | 324 | 2110.155 | 10.299 | 5.454 | 0 |
| 1／native | 5 | 3 | 153 | 153 | 2115.911 | 22.478 | 4.868 | 0 |
| 2／matched | 5 | 1 | 240 | 322 | 2102.603 | 11.316 | 5.236 | 0 |

“已入队且本进程无DMA”是提交到首次DMA的等待区间并集扣除本进程DMA区间；依赖未满足、Host准备和其它进程调度都可能影响它，不能直接称作驱动开销。各包等待之和会重复计算重叠等待，不作为关键路径损失。数值仅在同一node和完整配对范围解释；context数不代表独立物理EU引擎。

| 顺序／后端 | node | 提交到首次DMA 中位数 µs | p95 µs | 最大 µs |
| --- | ---: | ---: | ---: | ---: |
| 0／matched | 5 | 71.550 | 2354.300 | 2775.800 |
| 1／native | 5 | 35.200 | 68.200 | 210.900 |
| 2／matched | 5 | 64.150 | 1824.300 | 2156.900 |

成功DMA结束上的抢占标记合计0；AttemptPreemption事件数分别为[299, 154, 308]，尝试事件不等同实际抢占。非零地址／flags的DMA故障事件数分别为[0, 0, 0]。

分析脚本检查区间并集／交集、提交与完成配对和缺失完成拒绝。首版离线自检对缺失全部DMA完成时的预期形式写错：实现已拒绝，但测试预期返回状态；修正预期后通过，首版脚本保留。另保留首次浮点求和版本及首份离线输出；最终使用固定顺序和fsum复算，不重跑或替换GPU请求。空闲追踪的首轮没有显式rundown，未取得context；第二轮补capturestate后验证成功，均无模型请求。

没有修改生产代码、部署、推荐L0或Linux GPU待测状态。原始ETL／全部进程XML保留本机，不公开其它桌面进程事件；公开归档包含仅本测试进程的事件、追踪健康信息、原始模型协议、命令和分析源码。原始完整文件摘要保留在审计中。

[无追踪的相同准入对照](orca-direct-policy-parity-2026-11-08.md) · [先前context调用审计](orca-direct-queue-topology-2026-11-06.md)

[JSON](../../../sycl/orca/direct-etw-scheduler-a770-windows-2026-11-08.json) · 进程事件与精确文本归档 (`direct-etw-scheduler-a770-windows-2026-11-08.json.gz`, raw capture retained locally)

离线复核：`python -X utf8 build-orca-etw-scheduler-20261108/publish.py check`。复测须在新目录保留全部条件和失败；资产沿用身份缓存，无完整模型重复哈希。
