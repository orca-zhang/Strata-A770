# Short：原生完成等待与GPU空隙

2026-11-08，Windows10，Arc A77016GB，驱动32.0.101.9034，oneAPI2025.3.2。

上一轮真实Owner追踪把部分GPU空隙定位在同步交接之外。本次只重编原生运行时一个TU，给completion observer、运行时sequence wait及底层state fence wait增加进入／退出事件，并包围D3DKMT注册与WaitForSingleObject。原超时、取消、TLS事件复用、fence读回及等待策略保持，89个Host对象与数学ZEBIN复用。独立CPU事件自检覆盖四个scope、嵌套及异常退出。

固定Owner追踪Direct→Owner＋wait追踪Direct→Owner追踪Direct，各进程Code2K×2→Short×10，仅最后Short捕获。两端参数／环境／lib_dirs相同，仅EXE不同；实际5243槽／7201MiB、reserve1536MiB、cache预算5000、HC128、MTP4、min-p0.5、短提示热度关闭。这是独立诊断，不能替代5771槽锚定、当前spec6生产数据或L0对照。

全部36请求／2868输出及八个协议字段匹配、退出0。追踪零事件／缓冲丢失，GPU队列／DMA和wait事件完整配对。阶段标记仍在原decode timer两侧，误差门为1ms。

| Direct窗口 | 原decode ms | 阶段标记 ms | 无DMA且无队列包 ms | Owner之外的这类空隙 ms |
| --- | ---: | ---: | ---: | ---: |
| 0（仅Owner追踪） | 1406.000 | 1406.0523 | 14.6601 | 8.5108 |
| 1（增加wait追踪） | 1417.700 | 1417.6865 | 22.7185 | 17.0860 |
| 2（仅Owner追踪） | 1459.400 | 1459.4155 | 16.9428 | 10.4403 |

所有数字来自同一个ETW时钟。OS等待时长包含尚未结束的GPU工作，不能直接称为唤醒耗时；嵌套scope用时间并集避免重复。

| wait类别 | scope数 | 注册数 | OS调用数 | OS超时数 | scope并集 ms | OS等待并集 ms | 注册并集 ms | OS等待与Owner之外空隙重叠 ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| completion | 128 | 99 | 101 | 2 | 1395.4340 | 1392.9250 | 1.6220 | 9.4342 |
| sequence | 0 | 0 | 0 | 0 | 0.0000 | 0.0000 | 0.0000 | 0.0000 |
| state | 0 | 0 | 0 | 0 | 0.0000 | 0.0000 | 0.0000 | 0.0000 |
| all | 128 | 99 | 101 | 2 | 1395.4340 | 1392.9250 | 1.6220 | 9.4342 |

带wait追踪decode的Owner之外空隙共17.0860ms，可不重复地分成：OS wait内部9.4342ms、注册且不在OS wait内0.0000ms、其它wait scope部分0.3256ms、全部已标记wait scope之外7.3262ms。

这些交集定位了CPU所处的调用区间，不证明某个fence目标与某个DMA尾部完全对应，也不是IRQ到线程唤醒的精确延迟。其它线程／进程、驱动依赖或GPU semaphore仍可能影响观察。Scope只记录decode开始后进入的等待，跨边界区间裁剪，不能把未覆盖的等待解释为零。注册、事件字符串及重编后的Host布局会扰动执行；这不是正常无追踪提速或可回收预算。

生产运行时、默认策略及部署EXE未改，Orca L0仍推荐，Linux GPU待用户实测。完整ETL／XML只本机保存；公开归档为引擎自身事件、汇总健康信息、完整源／diff／命令／协议和原始文件哈希。

[上一轮真实Owner交接](orca-direct-owner-handoff-2026-11-08.md) · [原decode阶段L0／Direct拆分](orca-direct-etw-phases-2026-11-08.md)

[JSON](../../../sycl/orca/direct-wait-trace-a770-windows-2026-11-08.json) · 完整证据 (`direct-wait-trace-a770-windows-2026-11-08.json.gz`, raw capture retained locally)

离线复核：`python -X utf8 build-orca-wait-trace-20261108/publish.py check`。新采样使用新目录与冻结资产身份缓存，不重哈希整份模型。
