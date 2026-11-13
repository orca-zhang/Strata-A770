# 当前 U4／HC：相邻 GRF 切换屏障合并

2026-11-06，Windows10，Intel Arc A77016GB，驱动32.0.101.9034，oneAPI2025.3.2。
Short净decode在两个预定顺序中变化-0.050%／+0.160%；未通过两顺序均至少+1%的收益门，候选保持隔离。
本组L0指Orca L0；没有新测原生产SYCL。引擎核心、49个已验收栈数组Host、数学pack及模型资产保持冻结。

原纯GRF切换先写一条PIPE_CONTROL，若调用者紧邻之前已经写了同一依赖屏障，就形成两条连续相同命令。
候选用内部barrier_end记录已知命令边界，只在已校准的纯GRF转换、末尾确实是一条已跟踪屏障、
且全部24字节与所需屏障完全相同时省去第二条。GRF模式切换及其余状态命令保持原样。
单纯比较末尾字节不够：inline参数也可能碰巧等于PIPE_CONTROL，未跟踪字节不能合并。
不同可见性、带完成信号／时间戳、初始／无效状态、耦合状态改变和保守回退仍保留原命令。
进程缓存开关ORCA_DG2_DEDUP_GRF_BARRIER=0/1，默认0；未新增派发线程、复制、队列或凑批等待。

两种策略各通过14个原生CTest及64项HC完整输出／源／guard／CPU参考，共28项CTest和128项HC检查。
CPU检查逐字核对纯GRF序列，覆盖隐式尾屏障及两种可见性下的空尾、相同、不同、信号、时间戳和未跟踪尾字节。
非法开关按预期退出1；无统计版库不含ORCA_GRF_APPEND字符串。
同源Linux/WSL通过五项CPU检查（编码器两策略）和四份依赖审计，没有执行Linux GPU。

独立统计版两种策略各录到14,879次kernel追加，其中1,097处满足相邻字节匹配。
这些追加从57降为51 DWORD，每处省一条PIPE_CONTROL／24字节，合计26,328字节。
完整queue／kernel／状态／mask／字数直方图在补回6个DWORD后相同；4/4完整统计模型协议一致。
此计数包含启动／录图／请求准备，不按GPU重放次数加权，不能称为每token屏障数量或实际物理流量。

干净计时按预先固定的full→dedup→L0→dedup→full，再dedup→full→L0→full→dedup执行。
Direct两策略用同一个无统计EXE；每个新进程15次Short，丢前十次、保留后五次中位数，重复同策略取几何均值。
固定实际5771槽、reserve1536MiB、U4/min16/M16、read256、adapt32/decay0.95、MTP4，
step／last min-p0.5、cumulative/suffix/cost-window/prompt-cache0；Direct BCS1、ISA及线程内存device。
每请求23输入token、最多128输出、自然EOS70；全部150/150完整token／草稿／结束和reuse/resume协议匹配。
无内层profiler、GPU时间戳、统计或额外GPU回读；净decode排除准备、初始化和prefill。
TTFT和整请求分别测量，不把与首窗口重叠的引擎decode加到TTFT上。

| 臂 | 策略 | 净decode tok/s | TTFT ms | 整请求 ms | READY ms |
| --- | --- | ---: | ---: | ---: | ---: |
| 0-full | full | 49.6489 | 756.034 | 2124.009 | 31427.613 |
| 1-dedup | dedup | 49.7336 | 757.924 | 2123.288 | 29956.712 |
| 2-l0 | l0 | 51.4365 | 753.653 | 2074.404 | 26316.851 |
| 3-dedup | dedup | 49.5680 | 757.393 | 2128.289 | 30109.905 |
| 4-full | full | 49.7018 | 758.763 | 2127.804 | 29807.787 |
| 5-dedup | dedup | 49.7512 | 758.823 | 2126.273 | 29705.873 |
| 6-full | full | 49.6665 | 759.108 | 2127.313 | 29780.064 |
| 7-l0 | l0 | 51.4819 | 749.703 | 2069.440 | 26379.596 |
| 8-full | full | 49.4910 | 755.991 | 2128.411 | 29822.471 |
| 9-dedup | dedup | 49.5645 | 756.594 | 2127.826 | 29673.268 |

| 顺序 | dedup/full净decode变化 | full漂移 | dedup漂移 | dedup/full TTFT变化 | dedup/L0净decode变化 |
| --- | ---: | ---: | ---: | ---: | ---: |
| 0 | -0.050% | +0.107% | -0.333% | +0.035% | -3.472% |
| 1 | +0.160% | -0.354% | -0.375% | +0.021% | -3.543% |

预设门：两窗口decode均至少+1%，所有报告指标同策略控制漂移不超过3%，TTFT不恶化超过3%。
结果：`{"control_drift": true, "quality": true, "short_decode_gain": false, "ttft_nonregression": true}`。漂移不是置信区间，不为通过追加重复。
本组是隔离候选，运行库生产源码、默认配置和部署EXE未改；性能是否采用须按完整门判定。
初版build-r1的13项关闭策略GPU检查通过，但新增CPU断言把带flush标志的命令头与默认头比较而失败。
build-r2只将断言改为逐字比较完整配置的屏障／抢占命令，实际运行库encoding／recording源码逐字节相同。
原失败源、构建／检查日志与修正版均保留；首版没有执行模型请求或性能采样。
没有新的长上下文、广泛PPL或Linux GPU成绩；原Direct8K间歇故障未关闭，Orca L0仍为推荐路径。
资产按明确标注的stat-cached SHA256校验，未重新哈希全部权重。编译、GPU、重型哈希和归档均持共享锁。
归档保留补丁生成器、精确源、编译／链接命令、完整协议、原始统计与模型计划，不分发权重或二进制。

首次归档因WSL的UTF-16LE通知日志不符合UTF-8而在写公开文件前退出；原发布器与失败记录保留。
修正版为非UTF-8文件保存base64及原字节SHA256，并逐项回译核对，没有改采样或重跑GPU。

本机独立复查现有记录，不重新执行GPU：

```powershell
python -X utf8 build-orca-grf-barrier-20261106/publish.py --check
```

同条件复测需保持硬件／驱动／编译器、冻结源码／EXE／数学pack、资产、参数与请求历史，并使用新输出目录。

[当前窗口定位](orca-direct-current-window-2026-11-06.md) ·
[上一项不改变屏障的状态命令精简](orca-direct-current-grf-u4-2026-11-06.md) ·
[全Host与长上下文对照](orca-direct-argument-array-2026-11-06.md)

精确JSON (`direct-grf-barrier-a770-windows-2026-11-06.json`, raw capture retained locally) · 本轮原始文本归档 (`direct-grf-barrier-a770-windows-2026-11-06.json.gz`, retained locally for privacy)
