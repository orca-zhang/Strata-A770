# 当前 U4／HC：事件等待与持续 CPU 活动的隔离对照

2026-11-06，Windows10，Intel Arc A77016GB，Xeon E5-2696v4（22核／44逻辑处理器），
驱动32.0.101.9034，oneAPI2025.3.2。Orca L0是对照；本组没有原生产SYCL或Linux GPU新测量。

同一已验收栈数组Direct EXE、事件等待、内核pack和模型保持不变。
仅在READY后启动自己的辅助子进程：idle阻塞等待，busy在一个逻辑CPU上持续执行有界整数循环。
所有辅助进程固定CPU43、BELOW_NORMAL优先级，180秒上限，stdin停止／父管道EOF均可退出；
每臂finally停止自己的辅助进程和引擎，未更改Windows电源计划、驱动、系统或生产配置。
引擎亲和性不变且可以使用CPU43；没有排除逻辑核／同核线程竞争。
这个测试只比较活动的净影响，没有测量C-state、uncore频率、内存控制器、功率或能耗；
即使提速也不能单凭它确认某一机制。

固定实际5771专家槽、reserve1536MiB、U4/min16/M16、read256、adapt32/decay0.95、
MTP4、step／last min-p0.5、cumulative/suffix/cost-window/prompt-cache0；Direct BCS1，ISA及线程内存device。
两个顺序预先固定：idle→busy→L0→busy→idle，再busy→idle→L0→idle→busy。
每个新进程15次相同Short（23输入token、最多128输出、自然EOS70）；丢前十次、保留五次中位数。
每顺序内重复同模式的两个进程取几何均值，L0每顺序仅一个进程；不混合两顺序来选更有利数字。
辅助活动在首请求前就绪，覆盖固定预热和测量。无内层profiler、GPU时间戳或额外GPU回读。
全部150/150完整tokens、草稿采纳／提供、结束和reuse/resume协议匹配。
净decode排除准备、初始化和prefill；TTFT、首token后交付速度与直接测得的整请求分别报告，
不能把TTFT和存在首窗口重叠的引擎decode相加。

| 臂 | 模式 | 净decode tok/s | TTFT ms | 整请求 ms | READY ms |
| --- | --- | ---: | ---: | ---: | ---: |
| 0-idle | idle | 49.7159 | 755.932 | 2125.374 | 30270.112 |
| 1-busy | busy | 50.0572 | 754.368 | 2112.312 | 30302.611 |
| 2-l0 | l0 | 51.3761 | 750.589 | 2070.354 | 26397.424 |
| 3-busy | busy | 49.9322 | 754.475 | 2117.115 | 30066.660 |
| 4-idle | idle | 49.6454 | 756.365 | 2125.443 | 29870.483 |
| 5-busy | busy | 49.9109 | 750.847 | 2113.748 | 30267.634 |
| 6-idle | idle | 49.6912 | 759.997 | 2128.293 | 30067.699 |
| 7-l0 | l0 | 51.4819 | 748.971 | 2073.565 | 26321.194 |
| 8-idle | idle | 49.6208 | 755.855 | 2125.630 | 29856.869 |
| 9-busy | busy | 49.7760 | 751.572 | 2120.971 | 29733.579 |

| 顺序 | busy/idle decode变化 | idle漂移 | busy漂移 | busy/idle TTFT变化 | busy/idle整请求变化 | busy/L0 decode变化 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 0 | +0.632% | -0.142% | -0.250% | -0.228% | -0.503% | -2.689% |
| 1 | +0.377% | -0.142% | -0.270% | -0.886% | -0.452% | -3.183% |

预设门要求两个顺序的decode均至少提升1%，所有报告指标的同模式漂移绝对值不超过3%，TTFT不恶化超过3%。
结果：`{"control_drift": true, "quality": true, "short_decode_gain": false, "ttft_nonregression": true}`。漂移不是置信区间；不追加重复来取得通过。

| 臂 | 引擎 kernel+user CPU ms／完整请求 | 辅助进程 kernel+user CPU ms／完整请求 |
| --- | ---: | ---: |
| 0-idle | 56.250 | 0.000 |
| 1-busy | 65.625 | 2121.875 |
| 2-l0 | 2109.375 | 0.000 |
| 3-busy | 93.750 | 2125.000 |
| 4-idle | 59.375 | 0.000 |
| 5-busy | 90.625 | 2131.250 |
| 6-idle | 59.375 | 0.000 |
| 7-l0 | 2115.625 | 0.000 |
| 8-idle | 56.250 | 0.000 |
| 9-busy | 40.625 | 2140.625 |

CPU用时来自GetProcessTimes：在五条保留完整请求之前／之后各采样一次，差值除以五。
它包含prefill＋decode及请求间小间隙，排除初始化和十条预热；不是decode-only CPU或能耗。
FILETIME的100ns表示单位不等于实际CPU计量精度；保留原kernel／user计数及采样边界。
辅助进程CPU成本单列，不能把引擎低CPU用时当作整个方案低CPU。

本窗口未通过全部预设门；没有足够证据采用持续CPU活动来修复decode差距。
保持现有事件等待及默认配置，不把这个有限阴性结果外推为所有CPU／内存服务因素均被排除。

TTFT和decode不是必然此消彼长：同后端HC128→HC256的历史对照已同时改善Short decode与TTFT。
跨后端重复2K的TTFT改善而decode较慢，不能单凭两个指标方向就归因为初始化后移或prefill损害decode。
本组只覆盖Short新进程历史，不能替代2K／8K、广泛PPL或Linux GPU验证；原Direct8K间歇故障未关闭。
所有输出协议一致不等于验证全部中间张量或广泛PPL。Orca L0仍是推荐路径，Direct仍为WIP。

资产使用已标注stat-cached SHA256（文件身份／大小／时间戳），未重哈希全部模型权重。
运行、原始依赖校验和归档持共享GPU锁；数学pack、引擎和Host对象沿用已归档来源。
本组归档包含本轮脚本、预声明计划、完整原始协议／日志／结果；父构建源码与数学pack来源见下方历史归档。
保持同一硬件／驱动／编译器、冻结EXE／pack／资产、参数和请求历史，才是同条件复测。

离线审计现有记录，不重新运行GPU：

```powershell
python -X utf8 build-orca-cpu-activity-20261106/publish.py --check
```

新测量必须使用新的计划和输出目录，拒绝覆盖已有结果；本轮未为通过门追加复测。
下面仅记录新目录复现入口，没有为获得通过而执行：

```powershell
python -X utf8 -c "import sys; sys.path.insert(0,'build-orca-cpu-activity-20261106'); import run as m; m.PLAN=m.OUT/'prepared-repro-r1.json'; m.STAGE=m.OUT/'model-repro-r1'; m.prepare(); m.run()"
```

[原栈数组Host／长上下文对照](orca-direct-argument-array-2026-11-06.md) ·
[同后端HC256与TTFT／decode同时改善](orca-hc1-production-contexts-2026-11-05.md) ·
[当前GPU窗口诊断](orca-direct-current-window-2026-11-06.md)

JSON (`direct-cpu-activity-a770-windows-2026-11-06.json`, raw capture retained locally) · 本轮完整原始文本 (`direct-cpu-activity-a770-windows-2026-11-06.json.gz`, raw capture retained locally)
