# HC 同步补充：追踪 batch 跳转之后的屏障

2026-11-08，Windows 10／Intel Arc A770 16GB／驱动32.0.101.9034。复用同数学包原始命令；本轮没有运行GPU、重编运行时或新增性能样本。

此前[缓冲区内核对](orca-direct-hc-ordering-2026-11-08.md)明确止于捕获边界。现在按每个提交的根batch、GPU目标地址、深度、原始DWORD及索引，继续追踪所有无条件一级跳转直到提交内batch结束。Direct的778个提交／778个缓冲区、L0的1960个提交／2089个缓冲区全部覆盖，无遗漏、重复或未解析跳转；L0共129次链跳转。提交数量属于独立HC检查全过程，不能当成每token次数或物理引擎数。

| 目标／路径 | Direct | Orca L0 | 可支持的结论 |
| --- | --- | --- | --- |
| norm后同提交有下一walker | 160次，HDC＋untyped flush＋CS stall | 64次，CS stall；flush在walker内 | 两边均保留核间等待 |
| norm位于单walker提交末尾 | 无对应目标 | 96次，walker后直接结束 | 不能跨提交推定没有依赖 |
| up位于多walker提交末尾 | 80次，后面三条PIPE_CONTROL | 32次，跳转后仍有两条PIPE_CONTROL | 边界前零条并不代表提交内零条 |
| up位于单walker提交末尾 | 无对应目标 | 48次，walker后直接结束 | 与多walker路径分开列出 |

L0的32次up尾部实际顺序为：`MI_BATCH_BUFFER_START → PIPE_CONTROL → MI_STORE_DATA_IMM → MI_NOOP → PIPE_CONTROL → MI_BATCH_BUFFER_END`。前一条包含HDC／untyped刷新和CS stall；后一条还包含Host可见DC flush与立即数完成写入。Direct三条均有CS stall／HDC／untyped，其中最后一条还有DC flush。它们承担的职责不同，条数不是GPU独占时间。

这补齐了一个容易误判的地方：原捕获边界后的屏障确实存在。没有把跨提交的CPU等待、事件及KMD fence顺序拼成未经验证的全局执行图。

Direct适配层在finish前添加普通屏障，Windows close又添加完整Host可见屏障；这一尾部重复此前已单独测试。11月3日旧数学／源码快照的100条Short协议全部匹配，但候选decode观察值−0.343%，控制漂移+0.301%，没有建立收益，因此未采用。本轮不重复该候选，也不把旧速度移植为当前HC128结果。[旧试验与精确证据](../../../orca-l0/demo/dg2-tail-barrier-windows-2026-11-03.json)。

CPU检查覆盖地址损坏、secondary、predicated、地址循环、深度错配、缺失缓冲区及提前终止七个负控；不支持的路径明确拒绝。全部真实命令仍由原解码检查核对字节、边界与计数。本次没有发现足以支持新的屏障删除方案的证据，Short根因仍未证明；推荐Orca L0、Direct为WIP的结论保留。

复核：`python -X utf8 build-orca-hc-ordering-20261108/publish_paths.py check`，持共享锁、不联网、不运行GPU。归档保存本次检查源码、全部路径结果和原抓取器源码，原始命令证据以摘要链接此前归档，不修改已完成记录。

没有新增tok/s、TTFT、PPL、Linux GPU或默认采用结论。

[JSON](../../../sycl/orca/direct-hc-ordering-paths-a770-windows-2026-11-08.json) · 补充精确归档 (`direct-hc-ordering-paths-a770-windows-2026-11-08.json.gz`, raw capture retained locally) · 此前原始命令包 (`direct-hc-ordering-a770-windows-2026-11-08.json.gz`, raw capture retained locally)
