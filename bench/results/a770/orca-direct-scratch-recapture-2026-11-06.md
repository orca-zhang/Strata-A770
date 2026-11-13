# Direct：scratch池扩容后重录启动窗口图

2026-11-06，Windows10／Intel Arc A77016GB／驱动32.0.101.9034／oneAPI2025.3.2。
由当前Short真实命令普查中的28次scratch描述符切换提出候选；没有继续修改已经排除的不变SBA／systolic冗余。
仅重编一个Verifier Host TU。首次录制全部窗口及commit图后，保留已经扩容的scratch池，再重录窗口图。
STRATA_ORCA_RECAPTURE_WARM=0／1在启动时读取并缓存，默认0；其它值返回错误。候选只在独立构建内，不改生产源码或默认。
旧图经既有forget／delete路径释放；数学内核、Runtime、缓冲所有权、未完成依赖、实际SLM、寄存器及模型参数保持。

固定实际5771槽／7922MiB缓存、reserve1536MiB、MTP4、step／last min-p0.5、cumulative／suffix／cost-window／prompt-cache0、U4/min16/M16、BCS1。
单独诊断EXE使用相同Verifier对象及保留的命令日志Runtime，先关后开，各新进程两次Short；完整tokens／草稿／结束／复用计数均匹配冻结参考。

| 诊断设置 | 请求 | 实际walker | scratch基址转换 | 参数基址转换 | SBA包 |
| --- | ---: | ---: | ---: | ---: | ---: |
| 0 | 0 | 67271 | 28 | 50 | 524 |
| 0 | 1 | 67271 | 28 | 50 | 520 |
| 1 | 0 | 67271 | 7 | 0 | 382 |
| 1 | 1 | 67271 | 7 | 0 | 378 |

本组重录还观察到参数基址转换减少；录图地址布局也随之改变，不能把后续组合收益单独归因于scratch或假设每次启动都得到相同虚拟地址布局。
逐次解析真实追加DWORD、列表身份／关闭、队列序号及成功提交后的重放权重。日志范围覆盖完整请求，不是净decode计数；诊断耗时不作为性能成绩。
随后切换到无命令日志Runtime的同一EXE，顺序在采样前固定为关→开→关、开→关→开。
每进程15次Short，丢弃前10次，保留后5次中位数；外侧同设置取几何均值。全部90条完整协议必须匹配。
净decode排除准备、加载、初始化与prefill；TTFT与整请求独立测量，不能相加。READY单列，不混入净decode或主控制漂移门。
没有逐算子计时、GPU时间戳、额外回读或性能遥测；协议中的各原始耗时全部保留。

| 设置 | 净decode tok/s | TTFT ms | 整请求 ms | READY ms |
| --- | ---: | ---: | ---: | ---: |
| 0／0 | 49.0712 | 774.084 | 2160.003 | 15358.735 |
| 1／1 | 48.6145 | 767.840 | 2176.900 | 14215.575 |
| 2／0 | 48.5033 | 789.043 | 2190.953 | 13906.834 |
| 3／1 | 48.4094 | 775.751 | 2178.936 | 14885.400 |
| 4／0 | 48.8588 | 774.410 | 2167.115 | 13999.735 |
| 5／1 | 48.7397 | 780.776 | 2180.635 | 14539.961 |

| 顺序 | 开／关净decode变化 | decode控制漂移 | TTFT变化 | 整请求变化 | READY变化 |
| --- | ---: | ---: | ---: | ---: | ---: |
| 0 | -0.352% | -1.157% | -1.751% | +0.068% | -2.731% |
| 1 | -0.582% | +0.682% | +0.497% | +0.585% | +5.085% |

原门保持：两个顺序净decode均至少+1%、decode／prefill／TTFT／整请求控制漂移不超过3%、TTFT及整请求不退步超过3%，完整协议一致。
用户允许采用小幅正收益；原1%门失败仍保留，不把控制漂移当置信区间，不追加重复取得通过。
实际门：`{"control_drift": true, "original_decode_gain": false, "quality": true, "ttft_request_nonregression": true}`。
本轮两个顺序净decode均为负变化，TTFT改善方向也不一致；原收益门失败，不能按小幅正收益采用。保留全部样本，不追加重复取通过。
本报告保留候选为隔离实验，不自动改默认或部署；组件命令减少不能直接换算模型吞吐。
没有新的L0／原生产SYCL对照、长上下文、PPL或Linux GPU结论；此前Direct8K间歇及首次驻留问题仍开放。

重现需保留相同设备／驱动／编译器、冻结源码／EXE／数学包／资产、完整env／argv／输入、初始profile与进程顺序，并使用新输出目录。
编译、GPU、哈希及归档持共享实验锁；资产使用stat-cached SHA256身份核验，未全量重哈希模型。
归档保留精确源码、编译依赖、命令、配置及全部原始协议／日志；只保存二进制摘要，不分发模型、EXE、obj或zebin。

完整基础构建及数学包沿用[冻结整版对照](orca-direct-full-current-2026-11-06.md)，无日志Runtime沿用[全局显存记账检查](orca-direct-global-memory-2026-11-06.md)，诊断Runtime沿用下方状态普查；需一并保留各自冻结归档。

本机复核，无新增GPU：

```powershell
python -X utf8 build-orca-scratch-recapture-20261106/publish.py check
```

[原始状态普查](orca-direct-state-domains-2026-11-06.md) · [其补充审计](orca-direct-state-domains-2026-11-06-audit.md)
[JSON](../../../sycl/orca/direct-scratch-recapture-a770-windows-2026-11-06.json) · 精确文本归档 (`direct-scratch-recapture-a770-windows-2026-11-06.json.gz`, raw capture retained locally)
