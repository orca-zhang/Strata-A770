# 当前 U4／HC：仅 GRF 切换的状态命令精简

2026-11-06，Windows10／Intel Arc A77016GB，驱动32.0.101.9034，oneAPI2025.3.2。
精简版Short净decode在两个预定顺序中分别变化-0.007%／-0.053%；未通过两窗口均至少+1%的收益门，保持隔离。
这是当前U4/min16/M16/HC Q8 SG16及已验收49个栈数组Host对象的独立运行库试验，未重测原生产SYCL。
数值内核、引擎核心、Host对象和模型资产冻结；Orca L0是同数学包的对照。

候选仅在已校准且只有GRF字段变化时，省去内容未变的MI_LOAD_REGISTER_IMM、PIPELINE_SELECT和CFE_STATE，
保留STATE_COMPUTE_MODE、原依赖屏障及其它状态改变时的完整编程。进程缓存开关ORCA_DG2_COMPACT_GRF=0/1，缺省0。

独立统计版在每进程启动及两次Short的捕获中均记录14,879次kernel追加，
其中1,202次仅GRF改变。对应追加从57降到47 DWORD，每次少3条命令／40字节，
合计少48,080字节。两版按kernel名称、前后状态、mask及字数核对完整直方图，
除这10个DWORD外一致；4/4完整模型协议匹配。计数含初始化／录图／请求准备，未按GPU重放次数加权，不能称为每token计数。
例如融合HC Q8 kernel占384次纯GRF切换；这仍不把GPU窗口差距全部归到状态命令。

无统计版库不含ORCA_GRF_APPEND字符串；吞吐使用该独立构建，不插入GPU时间戳、额外回读或逐算子日志。
两策略各14个原生CTest和64项HC完整输出／源／guard／CPU参考检查通过；非法开关预期退出1。
同源Linux/WSL构建的5项CPU检查（编码器两策略各一次）及4份依赖审计通过，未执行Linux GPU。

固定实际5771专家槽、reserve1536MiB、U4/min16/M16、read256、adapt32/decay0.95、MTP4，
step／last min-p0.5、cumulative/suffix/cost-window/prompt-cache0，Direct BCS1、ISA及线程内存device。
两种顺序在采样前固定：full→compact→L0→compact→full，再compact→full→L0→full→compact。
Direct两策略使用同一EXE；每个新进程15次Short，丢弃前十次、保留五次中位数；每策略两进程取几何均值。
每请求23输入token、最多输出128、自然结束于70；全部150/150完整token／草稿／结束及复用状态匹配。
净decode排除准备、初始化和prefill；TTFT、READY与直接测得的整请求时间单列，不将TTFT和decode相加。

| 顺序 | 策略 | 净decode tok/s | TTFT ms | 整请求 ms | READY ms |
| --- | --- | ---: | ---: | ---: | ---: |
| 0 | full | 49.6489 | 756.395 | 2123.367 | 30462.244 |
| 1 | compact | 49.5260 | 755.812 | 2129.111 | 30338.562 |
| 2 | l0 | 51.3422 | 749.298 | 2072.482 | 26243.159 |
| 3 | compact | 49.7194 | 756.742 | 2124.920 | 29903.659 |
| 4 | full | 49.6032 | 756.258 | 2129.222 | 29908.040 |
| 5 | compact | 49.7230 | 756.260 | 2121.899 | 30738.308 |
| 6 | full | 49.5435 | 757.591 | 2129.328 | 30134.059 |
| 7 | l0 | 51.5312 | 753.342 | 2072.706 | 26393.464 |
| 8 | full | 49.7371 | 756.403 | 2122.640 | 30245.088 |
| 9 | compact | 49.5050 | 755.629 | 2129.052 | 30106.780 |

| 顺序窗口 | compact/full净decode变化 | full漂移 | compact漂移 | compact/L0净decode变化 | compact/full TTFT变化 |
| --- | ---: | ---: | ---: | ---: | ---: |
| 0 | -0.007% | -0.092% | +0.391% | -3.349% | -0.006% |
| 1 | -0.053% | +0.391% | -0.438% | -3.721% | -0.139% |

预设门：两窗口Short净decode均至少+1%，所有报告指标的同策略外侧漂移均不超过3%，TTFT不恶化超过3%。
各门结果：`{"control_drift": true, "quality": true, "short_decode_gain": false, "ttft_nonregression": true}`。
同策略漂移不是置信区间；每个窗口L0仅一个进程。不跨窗口混成一个有利数字，也不追加重复来取得通过。
候选保持隔离，生产源码、默认、部署EXE与配置未改；命令减少不自动建立整模型提速。
本组无新长上下文、PPL或Linux GPU结果，不关闭原Direct8K间歇故障。既有长上下文差距见下方全量Host报告。

准备build-r1因过期测试模块路径在排队时主动取消，未开始编译／GPU；原脚本与取消记录保留。
build-r2无统计版已构建，但统计版覆盖CMAKE_CXX_FLAGS使默认/EHsc丢失，编译失败，未执行模型。
build-r3从原CMake选项追加统计宏，保留上述失败、所有编译／链接命令、原始协议、统计和源码。

资产按明确标注的stat-cached SHA256身份核验，不重新哈希全部模型权重。归档只含文本及精确原字节编码，不分发权重或二进制。
编译、GPU、重型哈希及归档均持共享实验锁。保持硬件／驱动／编译器、冻结源／EXE／数学包、资产、参数和请求历史才是同条件复测。

本机独立复查，不重新执行GPU：

```powershell
python -X utf8 build-orca-current-grf-20261106/publish.py --check
```

[当前全量Host及长上下文对照](orca-direct-argument-array-2026-11-06.md) · [当前HC1→路由区间](orca-direct-current-route-phase-2026-11-06.md)
精确JSON (`direct-current-grf-u4-a770-windows-2026-11-06.json`, raw capture retained locally) · 原始文本归档 (`direct-current-grf-u4-a770-windows-2026-11-06.json.gz`, raw capture retained locally)
