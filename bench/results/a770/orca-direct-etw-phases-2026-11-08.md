# Short：按原始decode计时边界切分GPU调度追踪

2026-11-08，Windows10／Arc A77016GB／驱动32.0.101.9034／oneAPI2025.3.2。

上一轮只追踪完整Short请求，不能把其中的空隙全部归入decode。本次在两个冻结Host入口的原始d0之前、decode_ms计算之后各加入一个ETW字符串事件，仅重编／链接Host入口；数学ZEBIN、Verifier／shared对象、合格运行时和其它Host对象复用。原计时语句、循环、采样和GPU同步语义不改。

保持独立诊断条件：实际5243槽／7201MiB、cache预算5000、reserve1536MiB、HC128／MTP4／min-p0.5、两端短提示热度关闭。固定matched-L0→Direct→matched-L0；每进程Code2K×2、Short×10，仅最后Short启用DxgKrnl和私有阶段provider。没有独立反序或正常性能／采用门，不替代原5771槽或当前spec6生产成绩。

全部36请求／2868输出、8协议字段及退出码匹配；97个实际编译工作区头文件与预先固定身份匹配。三份追踪零事件／缓冲丢失，原始XML进程归属、完整队列／DMA配对均通过。每份只有一对本进程阶段标记，其间隔与原始decode_ms相差≤1ms。

| 顺序／后端 | 原decode_ms | 标记间隔 ms | 差值 ms |
| --- | ---: | ---: | ---: |
| 0／matched | 1374.200 | 1374.2281 | +0.0281 |
| 1／native | 1400.000 | 1399.9627 | -0.0373 |
| 2／matched | 1374.000 | 1374.0508 | +0.0508 |

pre_decode从本次追踪第一个已归属提交开始，到decode.begin为止；包括已观察到的准备／prefill，未包含首次提交前的CPU准备，不能直接当作TTFT。decode按两个Host标记切分；post_decode保留其后的异步提交收尾。跨边界DMA区间按时间裁切，数量记录重叠段，不能把跨界段数相加当作新派发次数。

| 顺序／后端 | 阶段 | node | DMA重叠段 | DMA并集 ms | 无本进程DMA ms | 有未完成队列包但无DMA ms | 无DMA且无未完成队列包 ms |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 0／matched | pre_decode | 5 | 112 | 733.605 | 4.530 | 2.524 | 2.006 |
| 0／matched | decode | 5 | 128 | 1367.161 | 7.067 | 4.199 | 2.867 |
| 1／native | pre_decode | 5 | 25 | 735.410 | 3.726 | 0.892 | 2.834 |
| 1／native | decode | 5 | 128 | 1381.817 | 18.146 | 4.057 | 14.089 |
| 2／matched | pre_decode | 5 | 112 | 735.097 | 4.850 | 2.729 | 2.122 |
| 2／matched | decode | 5 | 128 | 1367.370 | 6.681 | 4.198 | 2.483 |

仅为可相加的诊断分解，取两端L0区间的算术均值：Direct的decode标记区间多25.8232ms，其中DMA区间并集多14.5511ms、无本进程DMA空隙多11.2722ms。后者中，有未完成队列包的部分变化-0.1417ms，无未完成队列包的部分多11.4139ms。它定位了下一步要查的两类区间，没有证明特定Host函数、锁、context切换或kernel为根因，也不等于可直接回收的加速预算。

“有未完成队列包”覆盖全部所选工作／同步包，从入队到完成；不等于依赖已就绪。“无未完成队列包”也不直接等于CPU计算：Host等待、其它进程或驱动交互都可能造成它。DMA区间包含内部信号量和依赖等待，不能称为EU活跃或纯kernel时间。所有时间均来自ETW的同一时钟，不用CPU时钟减GPU时间戳。追踪及rundown会扰动调度，这些是定位数据，不是正常无追踪速度。

CPU-provider首轮用xperf更新多provider失败，独立会话已在finally停止，0模型请求；改用logman原生provider列表文件后自检通过。首次准备继承清单没有生成文件哈希项而停止，未编译／运行GPU；随后用原冻结源码及冻结转换脚本重建文本核对，保留空build-r1、失败源码和记录，在build-r2继续。没有丢弃或补跑模型样本。

生产源码、部署、推荐L0及Linux GPU待测状态保留。原始全部进程ETL／XML仅本机留存；公开归档只含本测试进程的调度事件、两个原始阶段事件、追踪健康信息、源码／编译依赖、命令与协议，完整原始文件摘要在审计中。

[完整请求追踪](orca-direct-etw-scheduler-2026-11-08.md) · [无追踪准入对照](orca-direct-policy-parity-2026-11-08.md)

[JSON](../../../sycl/orca/direct-etw-phases-a770-windows-2026-11-08.json) · 精确文本与原始进程事件 (`direct-etw-phases-a770-windows-2026-11-08.json.gz`, raw capture retained locally)

离线复核：`python -X utf8 build-orca-etw-phases-20261108/publish.py check`。新采样使用新目录，沿用资产身份缓存；原样本、失败与预设条件保留。
