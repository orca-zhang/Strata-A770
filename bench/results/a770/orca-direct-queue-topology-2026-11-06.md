# Direct／L0：2026-11-06冻结完整引擎的实际提交 context

2026-11-06，Windows10／Arc A77016GB／驱动32.0.101.9034／oneAPI2025.3.2。
保留2026-11-06冻结完整构建的数学包、库与策略，仅重编两个主机入口以添加阶段标记，链接进程内IAT观察器。Direct沿用已验证的全局显存记账Runtime；不是其它并行会话后续源码的重新构建。
固定实际5771专家槽／7922MiB缓存、1536MiB预留、MTP4、step／last min-p0.5、U4/min16/M16、BCS1；资产采用原stat-cached SHA256身份核验。
每后端一个新进程，依次2K→2K→8K→8K→Prose2K→Short→Short。14条完整输出／草稿／结束／复用协议均须匹配冻结参考。
prefill标记从r0到prompt_ms，包含该段缓存准备；净decode标记从d0到decode_ms，不包含其后最终wait_commit。准备／初始化及READY不计入净decode。
诊断日志和SRW锁会干扰调度，不报告吞吐或context切换延迟；协议原始耗时只作原始证据保留。没有逐调用QPC、GPU时间戳或额外设备回读。

| 后端 | 全进程创建context数量 | node5计算context | node2复制context |
| --- | ---: | ---: | ---: |
| l0 | 4 | 2 | 2 |
| direct | 9 | 6 | 3 |

| 后端 | 请求 | 阶段 | 提交次数 | 计算context数 | 复制context数 | L0 API handle数 | 等待／查询触发提交 |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: |
| l0 | 1/Code2K | prefill | 121941 | 1 | 1 | 11 | 1399 |
| l0 | 1/Code2K | decode | 2156 | 1 | 1 | 6 | 0 |
| l0 | 2/Code2K | prefill | 122611 | 1 | 1 | 11 | 1399 |
| l0 | 2/Code2K | decode | 2058 | 1 | 1 | 6 | 0 |
| l0 | 3/Long8K | prefill | 148291 | 1 | 1 | 11 | 1516 |
| l0 | 3/Long8K | decode | 2043 | 1 | 1 | 6 | 0 |
| l0 | 4/Long8K | prefill | 148520 | 1 | 1 | 11 | 1516 |
| l0 | 4/Long8K | decode | 1923 | 1 | 1 | 6 | 0 |
| l0 | 5/Prose2K | prefill | 116928 | 1 | 1 | 11 | 1355 |
| l0 | 5/Prose2K | decode | 2387 | 1 | 1 | 6 | 0 |
| l0 | 6/Short | prefill | 114 | 1 | 0 | 7 | 1 |
| l0 | 6/Short | decode | 1141 | 1 | 1 | 6 | 0 |
| l0 | 7/Short | prefill | 114 | 1 | 0 | 7 | 1 |
| l0 | 7/Short | decode | 1106 | 1 | 1 | 6 | 0 |
| direct | 1/Code2K | prefill | 29048 | 3 | 2 | 0 | 0 |
| direct | 1/Code2K | decode | 344 | 3 | 1 | 0 | 0 |
| direct | 2/Code2K | prefill | 29106 | 2 | 2 | 0 | 0 |
| direct | 2/Code2K | decode | 344 | 3 | 1 | 0 | 0 |
| direct | 3/Long8K | prefill | 34398 | 3 | 2 | 0 | 0 |
| direct | 3/Long8K | decode | 329 | 3 | 1 | 0 | 0 |
| direct | 4/Long8K | prefill | 34436 | 2 | 2 | 0 | 0 |
| direct | 4/Long8K | decode | 329 | 3 | 1 | 0 | 0 |
| direct | 5/Prose2K | prefill | 27814 | 2 | 2 | 0 | 0 |
| direct | 5/Prose2K | decode | 407 | 3 | 1 | 0 | 0 |
| direct | 6/Short | prefill | 30 | 5 | 0 | 0 | 0 |
| direct | 6/Short | decode | 185 | 3 | 1 | 0 | 0 |
| direct | 7/Short | prefill | 26 | 4 | 0 | 0 | 0 |
| direct | 7/Short | decode | 185 | 3 | 1 | 0 | 0 |

API调用身份来自所选应用导入的执行、append、同步、查询和释放调用，在调用栈中以TLS传递；普通录制列表不在append阶段提交，等待调用也可能触发延迟提交。
表中L0 handle是实际触发提交的API首参数，可为队列、列表、fence或event；不是应用Stream或物理队列数量。handle可被销毁和复用，只按当前阶段的实际调用身份统计。Direct没有Level Zero handle，以实际D3DKMT context统计。
每个成功D3DKMTSubmitCommand的BroadcastContext必须唯一且来自已观察到的活context；提交总数必须与阶段计数相等，HwQueue提交必须为零。创建／销毁记录、node及原始context值均保留在JSON与stderr。
原始bytes是D3DKMT CommandLength，即提交命令流长度，不是拷贝数据量、显存流量或PCIe流量。
观察器只覆盖本进程的已选应用导入和已选Intel驱动动态KMT入口；未知context、漏计数或未归属L0调用栈的提交会使本次校验失败。私有调用、其它进程与真实硬件context切换成本不在该计数中。
队列／context拓扑差异只能提出下一步隔离假设，不能证明它是吞吐差距的原因，也不能把context数直接换算百分比。
多个context落在同一个node，不代表新增独立物理EU／BCS引擎或已经测得并行利用率。
本轮不共享或合并Direct context：跨队列等待、HostGate前进性、每队列scratch／完成序号及释放所有权仍须分别验证。没有修改运行时默认、生产程序、采样策略或Linux交付。
此前Direct8K间歇故障和首次大缓存驻留失败仍保留；14条诊断通过不能关闭间歇问题，也不增加Linux GPU或PPL结论。

首轮观察器编译因Windows SDK的D3DKMT_CREATEDEVICEFLAGS没有Value字段而失败，未执行GPU；r2只删除这一不必要的设备flags输出，保留context flags。失败源码／日志不覆盖。
首次L0长短历史的7条完整协议通过，context／阶段计数也可解析，但原API身份覆盖断言失败；r3将TLS作用域扩展到所选同步／查询及reset／destroy入口，仅重新链接观察器，保留相同主机对象、数学包及原失败。该轮不完整API归属不能当作覆盖通过，未采集性能成绩。
归档含执行脚本、精确生成源码、命令、依赖、全部协议／日志及失败证据；仅记录二进制摘要。基础完整构建和数学包沿用[整版本对照](orca-direct-full-current-2026-11-06.md)，Direct Runtime沿用[全局显存检查](orca-direct-global-memory-2026-11-06.md)，重现须保留其冻结归档及相同环境／资产／初始profile。

本机复核，不新增GPU：

```powershell
python -X utf8 build-orca-queue-topology-20261106/publish.py check
```

[JSON](../../../sycl/orca/direct-queue-topology-a770-windows-2026-11-06.json) · 精确文本归档 (`direct-queue-topology-a770-windows-2026-11-06.json.gz`, raw capture retained locally)
