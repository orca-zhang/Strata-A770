# HC 核间顺序：逐条核对实际等待

2026-11-08，Windows 10／Intel Arc A770 16GB／驱动32.0.101.9034。复用11月6日同数学包HC原始命令抓取；本轮只编译CPU解码器，没有运行GPU、修改运行时或新增性能采样。

每条命令均核对原始字节、连续边界和原始索引。分别识别160条norm及80条融合Q8 up，追踪到同缓冲区的下一条walker或捕获边界。完整签名保留在JSON。

| 后端／目标 | 目标数 | 同缓冲区有下一walker | 到捕获边界 | 后续包含CS stall | 后续PIPE_CONTROL条数分布 |
| --- | ---: | ---: | ---: | ---: | --- |
| direct／norm | 160 | 160 | 0 | 160 | {"1": 160} |
| direct／up | 80 | 0 | 80 | 80 | {"3": 80} |
| l0／norm | 160 | 64 | 96 | 64 | {"0": 96, "1": 64} |
| l0／up | 80 | 0 | 80 | 0 | {"0": 80} |

边界后的命令没有推定为不存在；这里不跨batch跳转、提交或缓冲区建立完整依赖图，也不将独立HC计数外推为每token模型计数。多条PIPE_CONTROL可能承担状态切换、Host完成或其他不同责任，不能只按数量删除。

此前完整walker核对已确认L0把dataport刷新和完成计数放入walker，Direct使用后续屏障及列表完成；此前walker-flush候选保留CS stall，Short两顺序未胜出。本次进一步核对后续实际命令，不能把walker内刷新直接等同于省去所有核间等待。

参考代码缓存固定在NEO提交`9e866e5f9b8b3281cb6d8217a925594caa67cf76`，不是已证明的9034驱动源码。`cmdlist_hw.inl`的`isResolveIoqDependencyWithBarrier`和`appendWaitOnInOrderDependency`显示同引擎隐式依赖可走PIPE_CONTROL；存在列表交错时可选semaphore。保留原错误路径的404记录，正确通用编码器另行缓存；重复检查不联网。源码、来源URL和SHA随证据归档，许可一并保留。

本轮没有新tok/s、TTFT、PPL、Linux GPU或同步因果结论。Orca L0仍为推荐后端，Direct仍为WIP。原5771槽／MTP4／min-p0.5性能锚点不与并行会话的6509槽／spec6／0.6结果混用。

[此前walker控制核对](orca-direct-hc-walker-audit-2026-11-06.md) · [此前walker内刷新实验](orca-direct-walker-flush-2026-11-06.md)

[JSON](../../../sycl/orca/direct-hc-ordering-a770-windows-2026-11-08.json) · 解码源码、命令及原始包归档 (`direct-hc-ordering-a770-windows-2026-11-08.json.gz`, raw capture retained locally)

本机离线复核：`python -X utf8 build-orca-hc-ordering-20261108/publish.py check`。编译、检查和发布均持共享实验锁；没有重复旧GPU资格或测速。
