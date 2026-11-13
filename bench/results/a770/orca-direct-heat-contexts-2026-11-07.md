# 默认短提示热度复用后：L0／Direct 完整上下文对照

2026-11-07，Windows 10／Arc A770 16GB／驱动32.0.101.9034／oneAPI2025.3.2。
本轮固定使用同一 HC128 ZEBIN 数学包、MTP4、step／last min-p=0.5、实际5771专家槽及1536MiB预留。
Direct 使用已采用的 Q2_0 shared-down 并发扩展及短提示热度复用，L0 保留原准入与热度策略。
本轮重新编译Direct默认开启版本，包含无新提示行时的保护；两后端环境均不设置STRATA_SHORT_ROUTE_HEAT。
继续固定两后端相同HC128包；本轮差异包含已采用的Host缓存策略，不是纯API层消融；
也不是当前另一个 spec6／校准 MTP 配置的性能测量。生产 EXE、配置和 README 主成绩未替换。

固定六臂：L0→Direct→L0，Direct→L0→Direct。每臂新进程执行2K→2K→8K→8K→Prose2K→Short×15。
每个长请求位置单独报告；Short 去前10、保留后5中位数，两侧同后端控制取几何均值。
完整120次协议核对全部输出token、生成数、输入数、结束、草稿采纳／提供数及reuse/resume。
长请求相对历史输出参考只允许长度截断造成的末尾草稿计数改变；六臂之间草稿计数仍必须完全一致。
净 decode 使用引擎 decode_ms，排除准备、初始化和 prefill；READY、TTFT、整请求、首批交付后的速度另留原始数据。
没有 GPU timestamps／内部 profiler，没有删除漂移或负收益样本，没有追加复测筛选通过。

| 请求位置 | 正序 L0／Direct tok/s | Direct变化 | 反序 L0／Direct tok/s | Direct变化 |
| --- | ---: | ---: | ---: | ---: |
| Code2K/first | 29.323／28.841 | -1.645% | 29.402／29.280 | -0.415% |
| Code2K/repeat | 36.601／36.345 | -0.698% | 36.749／36.486 | -0.715% |
| Long8K/first | 29.840／29.731 | -0.366% | 30.011／29.822 | -0.630% |
| Long8K/repeat | 34.178／33.937 | -0.704% | 34.280／34.063 | -0.630% |
| Prose2K | 33.606／33.305 | -0.897% | 33.681／33.423 | -0.765% |
| Short | 52.717／51.577 | -2.163% | 52.950／51.503 | -2.733% |

| 请求位置 | 正序 TTFT L0／Direct秒 | 耗时变化 | 反序 TTFT L0／Direct秒 | 耗时变化 |
| --- | ---: | ---: | ---: | ---: |
| Code2K/first | 26.089／28.251 | +8.288% | 25.682／27.200 | +5.911% |
| Code2K/repeat | 15.335／12.638 | -17.586% | 14.905／12.643 | -15.176% |
| Long8K/first | 33.872／30.952 | -8.621% | 33.528／31.046 | -7.403% |
| Long8K/repeat | 33.889／30.968 | -8.619% | 33.539／30.935 | -7.764% |
| Prose2K | 14.789／12.219 | -17.380% | 14.716／12.262 | -16.678% |
| Short | 0.718／0.650 | -9.546% | 0.719／0.644 | -10.420% |

| 请求位置 | 正序控制漂移：decode／TTFT | 反序控制漂移：decode／TTFT |
| --- | ---: | ---: |
| Code2K/first | +0.556%／-1.308% | -0.181%／-2.031% |
| Code2K/repeat | +1.072%／-1.953% | -0.273%／+0.594% |
| Long8K/first | +0.152%／-0.569% | -0.147%／+0.286% |
| Long8K/repeat | +0.672%／-0.225% | -0.457%／+0.211% |
| Prose2K | +0.640%／-0.567% | -0.334%／+0.614% |
| Short | +0.173%／-0.799% | -0.565%／+0.259% |

上下文趋势：

- forward：Short 的 Direct／L0 为-2.163%，重复8K为-0.704%，长请求相对Short变化+1.458个百分点。
- reverse：Short 的 Direct／L0 为-2.733%，重复8K为-0.630%，长请求相对Short变化+2.102个百分点。

原始门结果：`{"all_cases_decode_ahead": false, "all_cases_ttft_ahead": false, "control_drift": true, "quality": true}`。
全部指标最大控制漂移为2.037%，预先固定上限为3%，它不是置信区间。
控制漂移门通过；上表仍只描述固定请求历史中的有限样本。
请求长度、草稿轨迹、专家缓存和访存负载同时变化，不能只凭长短请求的差距认定API、锁或缓存是根因。
跨后端的TTFT与decode变化也不构成单项优化的因果消融，不能据此认定两者必然取舍。

本轮只重编Direct的generate Host单元；数学包、其他Host库及Direct驻留修复对象均复用资格版本，不重新编译数值内核。
1GiB allocation-part 上限已经存在于该冻结源码与当前主源码；本轮没有重新实现这一修复。
89.8GiB模型／MTP／U4资产沿用已建立的SHA256与文件身份／大小／时间戳缓存，实验前后复核；
EXE、数学包、依赖源与脚本另核对SHA256，不把加载或准备计入decode。
历史8K间歇非有限值问题继续保留；未复现不等于根因已修复。没有新的PPL或Linux GPU结论。

本机复核原协议、对照计算、依赖与文本归档，不运行GPU：

```powershell
python -X utf8 build-orca-heat-contexts-20261107/experiment.py selfcheck
python -X utf8 build-orca-heat-contexts-20261107/report.py check
```

同条件复测须保留硬件／驱动、工具链、冻结EXE与包、模型与初始profile、参数、输入及完整请求顺序，使用新目录。
文本归档保留本轮源、计划、完整协议、日志与依赖证明；复用构建另见Q2_0资格归档。
不分发权重、私有fixture、EXE、OBJ、LIB或ZEBIN；所有GPU、重型哈希和归档持共享实验锁。

[Q2_0资格与安全检查](orca-direct-q2-fork-2026-11-07.md) · [旧HC配置不同的上下文对照](orca-direct-full-current-2026-11-06.md)
[JSON](../../../sycl/orca/direct-heat-contexts-a770-windows-2026-11-07.json) · 精确文本证据 (`direct-heat-contexts-a770-windows-2026-11-07.json.gz`, raw capture retained locally)

按Direct落后幅度比较Short与重复8K（领先时落后幅度记为0）：

| 顺序 | Short落后 | 重复8K落后 | 落后缩小 | 绝对差距缩小 |
| --- | ---: | ---: | ---: | ---: |
| forward | 2.163% | 0.704% | +1.458百分点 | +1.458百分点 |
| reverse | 2.733% | 0.630% | +2.102百分点 | +2.102百分点 |

有符号变化变好与绝对差距缩小是不同指标；漂移门失败时，不能用这些点估计确定趋势。
长请求每位置每进程只有一次；上下文、草稿轨迹、缓存及工作集同时变化，不能从该表单独定位API、锁或物理传输量。
[短提示热度的独立消融](orca-direct-short-route-heat-2026-11-07.md) · [源码采用证明](../../../sycl/orca/direct-short-route-heat-adoption-a770-windows-2026-11-07.json)
