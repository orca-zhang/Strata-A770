# Short：真实原生Owner同步交接与GPU空隙

2026-11-08，Windows10，Arc A77016GB，驱动32.0.101.9034，oneAPI2025.3.2。

这是Direct内部的诊断，不是L0对照或正常性能成绩。固定无Owner追踪Direct→带Owner追踪Direct→无Owner追踪Direct；三者都保留原始decode计时两侧的两个Host阶段标记。每个新进程Code2K×2→Short×10，只追踪最后一个Short。5243槽／7201MiB、cache预算5000、reserve1536MiB、HC128、MTP4、min-p0.5、短提示热度关闭；与5771槽锚定及当前spec6生产数据分开。

Owner只添加publish／begin／end／return四个事件及每种调用点一次的函数签名。嵌套owner调用直接执行，不计为跨线程交接；原MPSC发布、FIFO执行、TLS完成信号、等待、异常与返回语义保留。89个Host单元及四个库统一重编，数学ZEBIN和已验证的原生运行时对象复用。实际CPU追踪检查17次同步调用、嵌套绕过、返回值和异常，并要求完整四事件配对及零丢失。

全部36请求／2868输出token及八个协议字段匹配、退出0，GPU队列／DMA事件完整配对，追踪零事件／缓冲丢失。原始失败保留：准备阶段路径拼接错误，0编译／GPU；首次编译Windows min/max宏冲突，0模型请求；修正仅诊断头的Windows宏保护后重新隔离构建。89项编译通过后，依赖门遇到src/../include别名；原失败未改，独立finish.py规范化后逐个重审实际头文件、保留对象哈希再链接，没有重编这89项或补跑GPU。
独立CPU probe首次icx单步构建把链接库当作额外源文件而退出1；原记录保留，随后只用此前已验证的MSVC驱动构建这个小probe。oneAPI引擎、89个已编译对象及链接完成的EXE哈希均未改变。
首次模型启动误把Direct基线EXE搭配继承的L0环境，缺少原生memory模块变量，READY之前退出，0次GEN。该失败完整保留；独立run-r2对两端使用逐项一致的原生argv／env／lib_dirs，仅EXE不同。没有弃掉或补跑成功的模型样本。

| Direct窗口 | 原decode_ms | 阶段标记 ms | 无DMA ms | 无DMA且无未完成队列包 ms |
| --- | ---: | ---: | ---: | ---: |
| 0（无Owner追踪） | 1411.300 | 1411.2663 | 18.9629 | 14.1663 |
| 1（Owner追踪） | 1414.800 | 1414.8637 | 24.3502 | 17.9024 |
| 2（无Owner追踪） | 1415.500 | 1415.4803 | 19.4814 | 15.2785 |

本轮三个Direct窗口在decode内的活动context数为[2, 2, 2]。相邻DMA段的context归属改变数保留在JSON；它不等于实际硬件context切换次数，也没有证明切换成本。

带追踪decode记录128次完整同步交接，涉及1个Owner线程、1种调用点。下面的区间均来自同一个ETW时钟，单次延迟用µs，累计／并集用ms。并发调用的累计时间可以重叠，不能直接相加当作decode开销。

| 区间 | 样本 | 中位 µs | p95 µs | 最大 µs | 累计 ms | 时间并集 ms | 与无DMA且无队列包相交 ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| publish_to_begin | 128 | 48.100 | 68.100 | 87.300 | 5.6346 | 5.6346 | 4.8969 |
| body | 128 | 36.600 | 54.600 | 196.300 | 4.9464 | 4.9464 | 2.1761 |
| end_to_return | 128 | 23.050 | 58.900 | 76.100 | 3.7682 | 3.7682 | 0.0000 |
| total | 128 | 109.950 | 162.500 | 277.800 | 14.3492 | 14.3492 | 7.0730 |
| handoff_delay | 256 | 34.550 | 64.600 | 87.300 | 9.4028 | 9.4028 | 4.8969 |

无DMA且无未完成队列包共17.9024ms；其中4.8969ms落在发布到开始／执行结束到返回区间，另2.1761ms落在Owner任务本体但未被前者覆盖，剩余10.8294ms在所有已标记Owner区间之外。这是互不重复的观察分区，不是移除线程交接后必然回收的时间。

publish→begin包含发布事件本身的尾部、MPSC入队、等待其它任务和线程调度；end→return包含结束事件尾部、完成发布／唤醒及调用方调度。任务本体还可能等待GPU／依赖，不能叫纯CPU计算。首次调用点签名事件在publish之前，有额外追踪成本；ETW事件写入、重编后的Host代码布局也会扰动执行。不能把这些带追踪延迟替代正常无追踪性能，或据此断言锁、线程唤醒是之前25.8ms差距的唯一根因。

完整调用点、Owner／调用线程ID、逐次四事件时刻、与DMA空隙的交集均在JSON及归档。完整ETL／XML留在本机，公开归档只保留本测试引擎事件、追踪健康摘要及原始文件哈希，不发布其它桌面进程。推荐Orca L0、生产默认和Linux GPU待测状态保持。

[原始decode阶段对照](orca-direct-etw-phases-2026-11-08.md) · [CPU空闲微测试](orca-direct-owner-idle-2026-11-08.md) · [此前CallerOwner整模型测试](orca-direct-caller-owner-2026-11-05.md)

[JSON](../../../sycl/orca/direct-owner-handoff-a770-windows-2026-11-08.json) · 完整诊断源／命令／原始事件 (`direct-owner-handoff-a770-windows-2026-11-08.json.gz`, raw capture retained locally)

离线复核：`python -X utf8 build-orca-owner-handoff-20261108/publish.py check`。重新采样使用新目录，沿用模型资产身份缓存，不重哈希整份模型。
