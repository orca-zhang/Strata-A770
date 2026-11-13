# 短提示路由热度复用：独立对照

2026-11-07，Windows 10／Arc A770 16GB／驱动32.0.101.9034／oneAPI2025.3.2。
固定 HC128 数学包、MTP4、step／last min-p0.5、实际5771专家槽、1536MiB预留；
Direct 保留已采用的 Q2_0 shared-down 并发扩展。这不是另一会话的spec6／校准MTP配置。

候选仅重编generate Host单元：每个短提示窗口完成后，读取已有mapped路由ID；
纯短提示按现有长预填充的0.125权重计入usage，并复用最多四批LFU准备。
混合／长预填充仍用原routing_tail；无新GPU数学内核、读回或路由采集同步。
准备仍使用原有换入与等待，全部留在prompt／TTFT及直接计时的整请求中。
实验开关STRATA_SHORT_ROUTE_HEAT只在隔离EXE中，默认关闭；生产源码、EXE和配置未改。

预先固定八个新进程：L0、Direct关闭／开启／关闭、Direct开启／关闭／开启、L0。
每进程15次Short，首请求为冷态，最后5次中位数为暖态，其余全部保留。
同一个Direct EXE切换开关；两侧同模式控制取几何均值。所有120次完整输出及草稿计数准确匹配固定参考。
净decode排除初始化、准备和prefill；TTFT与decode重叠，不把两者相加。无GPU时间戳／profiler。

| 状态／方向 | 关闭／开启 tok/s | 吞吐变化 | TTFT耗时变化 | 整请求耗时变化 | decode控制漂移 |
| --- | ---: | ---: | ---: | ---: | ---: |
| cold／forward | 22.761／23.121 | +1.584% | +0.455% | -1.051% | +1.184% |
| cold／reverse | 22.859／23.093 | +1.026% | +0.881% | -0.542% | +1.145% |
| warm／forward | 50.434／50.942 | +1.008% | -13.497% | -5.068% | +0.137% |
| warm／reverse | 50.201／50.974 | +1.540% | -13.450% | -6.279% | -0.196% |

| 状态 | 全指标控制漂移≤3% | 两方向decode提升 | prompt／TTFT／整请求回退≤3% | decode收益超过对应漂移 |
| --- | --- | --- | --- | --- |
| cold | True | True | True | False |
| warm | True | True | True | True |

预先冻结的门槛（全指标控制漂移≤3%、时间回退≤3%、暖态两方向decode提升）通过：True。
冷态反序吞吐变化+1.026%，对应控制漂移+1.145%；该描述性检查不通过，不据此宣称冷启动加速。
首个冷请求每进程只有一个样本，控制漂移不等于置信区间。所有冷态与过渡样本都在JSON和原协议中。
本实验只判断这个固定Short场景的缓存策略效果；不能据此声称完整Direct快于L0、PPL改善或长上下文故障修复。
原源码审计发现短提示没有计入热度，属于可测遗漏；没有计数本身不证明加入后一定更快。
build-r1 compile rejected count_routing with one argument;explicit nullable residency argument fixed in build-r2 before any GPU/model run. Original driver and diagnostics retained.

不运行GPU的本机复核：

```powershell
python -X utf8 build-orca-short-route-heat-20261107/experiment.py selfcheck
python -X utf8 build-orca-short-route-heat-20261107/report.py check
```

同条件复现需固定原数学包、Host库、模型／初始profile、输入、参数和完整进程／请求顺序；新输出目录运行experiment.py run。
查询不变的大模型身份复用此前SHA256＋文件标识／大小／时间戳缓存，不重新扫描89.8GiB权重。
编译、GPU、重型哈希和归档持共享实验锁；精确字节归档含原失败、源码差异、命令和协议，不含权重及二进制构建产物。
未运行Linux GPU，不更新README主成绩。

[路由与槽位诊断](orca-direct-q2-residency-2026-11-07.md)
[JSON](../../../sycl/orca/direct-short-route-heat-a770-windows-2026-11-07.json) · 证据归档 (`direct-short-route-heat-a770-windows-2026-11-07.json.gz`, raw capture retained locally)
