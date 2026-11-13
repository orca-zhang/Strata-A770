# Short：按Host阶段归属完成等待与GPU空隙

2026-11-08，Windows10，Arc A77016GB，驱动32.0.101.9034，oneAPI2025.3.2。

只重编请求入口generate.cpp一个Host单元，在每个decode窗口加入八个阶段边界，复用原有Owner／completion等待追踪、其余88个Host对象、运行时与数学ZEBIN。没有增加GPU时间戳、回读或同步，也没有调整轮询、线程优先级、队列与采样策略。

固定wait＋Owner追踪Direct→再加阶段追踪Direct→wait＋Owner追踪Direct，各进程Code2K×2→Short×10，只捕获最后Short。两端参数／环境／lib_dirs相同，仅EXE不同；实际5243槽／7201MiB、reserve1536MiB、cache预算5000、HC128、MTP4、min-p0.5、短提示热度关闭。独立诊断，不替代5771槽锚定、当前spec6生产数据或L0对照。

全部36请求／2868输出及八个协议字段匹配，三个引擎退出0；零追踪事件／缓冲丢失。GPU队列／DMA、Owner、wait以及29个完整窗口的阶段序列均通过配对。解析器负控拒绝缺失、重复、错线程及错窗口边界。

| Direct窗口 | 原decode ms | ETW decode ms | 无DMA且无未完成队列包 ms |
| --- | ---: | ---: | ---: |
| 0（仅wait＋Owner追踪） | 1404.000 | 1404.0526 | 17.3650 |
| 1（增加阶段追踪） | 1404.000 | 1403.9672 | 15.4467 |
| 2（仅wait＋Owner追踪） | 1409.000 | 1408.9926 | 16.2715 |

两端控制decode耗时变化+0.356%。这些是带追踪诊断耗时，不作为正常吞吐比较、优化收益或采用门。阶段标记与原decode计时的最大误差须小于1ms。

| Host阶段 | 区间数 | 时长 ms | DMA并集 ms | 空队列／DMA ms | Owner之外空隙 ms | 其中OS wait内 ms | 其中全部wait外 ms | wait进入数 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| before_loop | 1 | 0.0197 | 0.0000 | 0.0197 | 0.0197 | 0.0000 | 0.0197 | 0 |
| prepare | 29 | 0.0787 | 0.0000 | 0.0787 | 0.0787 | 0.0000 | 0.0787 | 0 |
| verify | 29 | 1208.5450 | 1203.3398 | 4.0633 | 2.5132 | 0.7847 | 1.6574 | 29 |
| post_verify | 29 | 3.4603 | 0.0000 | 3.4603 | 3.4603 | 0.0000 | 3.4603 | 0 |
| commit | 29 | 3.7877 | 0.2792 | 2.4077 | 0.2399 | 0.0000 | 0.2399 | 0 |
| emit | 29 | 0.9527 | 0.9331 | 0.0000 | 0.0000 | 0.0000 | 0.0000 | 0 |
| draft | 29 | 185.1123 | 179.2826 | 4.2532 | 2.0341 | 1.4916 | 0.4375 | 70 |
| finalize | 29 | 1.9706 | 0.8439 | 1.1236 | 1.1236 | 0.0000 | 1.0842 | 29 |
| tail | 29 | 0.0389 | 0.0000 | 0.0389 | 0.0389 | 0.0000 | 0.0389 | 0 |
| after_loop | 1 | 0.0013 | 0.0000 | 0.0013 | 0.0013 | 0.0000 | 0.0013 | 0 |

`prepare`含窗口／输入准备及apply_pending；`verify`包围ver.run；`post_verify`含路由计数、接受判断和适应线程准备；`commit`含ver.commit；`emit`含输出、历史及PLE行预读；`draft`包围mtp.draft条件与调用；`finalize`含计时记账、适应线程join及wait_commit；`tail`含循环尾部。before／after_loop覆盖计时区间两端，所有阶段时长和分区精确相加。wait进入数按scope进入时刻归属，跨阶段时长则按区间裁剪。

| 活动计算context | node | verify内DMA ms | draft内DMA ms | 其它阶段内DMA ms |
| --- | ---: | ---: | ---: | ---: |
| 0 | 5 | 1203.3398 | 23.2535 | 2.0562 |
| 1 | 5 | 0.0000 | 177.3588 | 0.0000 |

表中context按捕获的opaque handle排序，仅作本次识别。两个活动context都位于node5计算节点；阶段内执行不等于该阶段创建／提交的全部工作。commit等可异步跨越Host边界，DMA区间包含依赖与调度，不是纯kernel时间；没有精确测量硬件context切换次数或成本。

OS等待含尚未结束的GPU工作；无DMA／无队列包与调用区间的交集不等于IRQ唤醒延迟、因果根因或可回收时间。不能把本轮分区与上一轮数值相减来估计收益。生产运行时、部署EXE和默认策略未改，Orca L0仍推荐，Linux GPU待用户实测。完整ETL／XML含其它桌面活动，只保留本机；公开归档仅含引擎事件、汇总健康信息、源码／命令／协议与原始文件哈希。

[前一轮完成等待](orca-direct-wait-trace-2026-11-08.md) · [原decode阶段L0／Direct拆分](orca-direct-etw-phases-2026-11-08.md)

[JSON](../../../sycl/orca/direct-decode-stages-a770-windows-2026-11-08.json) · 完整证据 (`direct-decode-stages-a770-windows-2026-11-08.json.gz`, raw capture retained locally)

离线复核：`python -X utf8 build-orca-decode-stages-20261108/publish.py check`。新采样使用新目录、相同历史与冻结资产身份缓存，不重哈希89.8GiB模型。
