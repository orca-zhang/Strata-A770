# HC 融合 Q8：T=1／T=4 专门化与参数复制

两后端均未通过完整Short采用门，候选保持隔离。

2026-11-06，Windows10，Intel Arc A77016GB，驱动32.0.101.9034，oneAPI2025.3.2。
本组L0指Orca L0；Direct绕过Level Zero。没有原生产SYCL的新测量。

从冻结的当前SG16／256GRF融合HC Q8出发，只在同一个kernel内为T1／T4选择固定循环边界；
T2／T3／T5..8保留通用路径，不增加派发、队列、拷贝、凑批等待或改变归约顺序。
原窗口诊断每个保留进程145窗口：T1有35、T4有80、T3有20、T2有10；
T4约占GPU窗口跨度64%，T1约18%。这是已有窗口诊断的覆盖分析，不是HC独占时间或新吞吐。

首版按值传递helper参数，编译声明的private空间翻倍；其256项检查通过，但未进入模型采样。
另立修正版改为只读引用，将private降至接近基线；首版、原计划和记录均保留，不覆盖失败或劣化证据。
部署的融合Q8与基线payload／per-thread-payload ABI完全一致。首版前端只改变images_3.spv；
修正版把通用helper改为引用，也改变普通up的images_2／9.spv，其余22份前端映像逐字节不变。
普通up的前端变化未部署：原生pack中的image_2／9与基线逐字节相同。
部署pack只更换image_3及其native manifest，其他原生数学映像保持。

| 版本 | SIMD／GRF | private字节／硬件线程 | spill字节 | 代码段字节 | 静态指令数（含padding） |
| --- | --- | ---: | ---: | ---: | ---: |
| 基线 | 16／256 | 9728 | 0 | 27840 | 2075 |
| 首版按值 | 16／256 | 19456 | 0 | 93376 | 7028 |
| 修正版只读引用 | 16／256 | 9856 | 0 | 90880 | 6853 |

实际IGA汇编、完整opcode统计和原生元数据均保留；解码器来自已缓存的本机驱动，不查询GPU。
静态计数包含所有专门化／通用分支及NOP padding，不代表实际执行指令数。
private声明不等于物理显存流量、峰值或占用率；代码增长也不能单独证明端到端退化。
两版各通过两后端×基线／候选的256项完整HC检查，共512项，全部64个输出哈希一致。
覆盖T1..8、两个改变输入、eager／replay、完整输出／源／guard、CPU FP64和独立Q8参考。

整模型只测修正版，计划在测量前固定：Direct基线→候选→基线，L0同序，再L0和Direct反序。
每新进程15次相同Short，丢前十、留后五中位数；两侧同模式取几何均值，共180次请求，不追加重复取通过。
每请求输入23token、最多128输出、自然EOS70；完整tokens、草稿采纳／提供、结束及reuse/resume逐项比较。
固定实际5771专家槽、reserve1536MiB、U4/min16/M16、read256、adapt32/decay0.95、MTP4，
step／last min-p0.5、cumulative/suffix/cost-window/prompt-cache0；Direct BCS1、ISA／线程内存device。
同一后端使用同一EXE，仅切换冻结的数学pack；不从当前生产JSON取策略。
没有内层profiler、GPU时间戳或额外GPU回读；净decode排除准备／初始化／prefill。
TTFT与整请求分别计时；TTFT和引擎decode首窗口重叠，不能相加。

| 臂 | 净decode tok/s | TTFT ms | 整请求 ms | READY ms |
| --- | ---: | ---: | ---: | ---: |
| 0-direct-baseline | 49.5225 | 758.148 | 2130.451 | 30564.060 |
| 1-direct-candidate | 49.5050 | 759.509 | 2131.432 | 30333.852 |
| 2-direct-baseline | 49.4560 | 757.232 | 2131.882 | 30234.259 |
| 3-l0-baseline | 51.2670 | 752.215 | 2077.177 | 26362.950 |
| 4-l0-candidate | 50.8795 | 759.414 | 2093.394 | 25896.245 |
| 5-l0-baseline | 51.4026 | 751.609 | 2073.716 | 26160.894 |
| 6-l0-candidate | 50.9721 | 756.490 | 2089.858 | 26180.404 |
| 7-l0-baseline | 51.2445 | 750.919 | 2078.150 | 26480.683 |
| 8-l0-candidate | 51.0651 | 757.465 | 2092.722 | 26187.767 |
| 9-direct-candidate | 49.5014 | 758.331 | 2133.253 | 30784.939 |
| 10-direct-baseline | 49.6454 | 759.860 | 2128.986 | 30393.255 |
| 11-direct-candidate | 49.3792 | 757.766 | 2134.616 | 30460.517 |

| 后端／顺序 | 净decode变化 | decode控制漂移 | TTFT变化 | 整请求变化 |
| --- | ---: | ---: | ---: | ---: |
| direct／基线→候选→基线 | +0.032% | -0.134% | +0.240% | +0.012% |
| l0／基线→候选→基线 | -0.887% | +0.264% | +0.998% | +0.865% |
| l0／候选→基线→候选 | -0.441% | +0.182% | +0.807% | +0.632% |
| direct／候选→基线→候选 | -0.413% | -0.247% | -0.238% | +0.232% |

| 顺序窗口 | 基线Direct／L0净decode | 候选Direct／L0净decode |
| --- | ---: | ---: |
| 0 | -3.595% | -2.702% |
| 1 | -3.121% | -3.094% |

后端组依次执行；后端表使用各自包围对照的代表值，控制漂移不是跨后端置信区间。
每后端要求两个顺序decode至少+1%，所有报告指标控制漂移绝对值≤3%，TTFT／整请求退步≤3%，完整质量通过。
结果：`{"by_backend": {"direct": {"control_drift": true, "request_nonregression": true, "short_decode_gain": false, "ttft_nonregression": true}, "l0": {"control_drift": true, "request_nonregression": true, "short_decode_gain": false, "ttft_nonregression": true}}, "eligible_for_long_context": [], "quality": true}`。

编译与数值优化可能让两后端共同受益，不能把它计为移除API层的独立收益。
本组没有新长上下文、广泛PPL、Linux GPU或长期稳定性结论；历史Direct8K间歇故障仍未关闭。
源码默认、部署EXE与启动配置未改变；Orca L0仍是推荐路径，Direct仍为WIP。
资产使用明确标注的stat-cached SHA256身份检查；编译、GPU、重型哈希和归档持共享锁。
归档包含原计划、两版源码／编译命令／资源／汇编、完整原始HC检查及模型协议，不分发权重或二进制。

首次发布检查错误要求修正版也仅改变一份前端SPIR-V，在写入公开结果前失败；失败源码及记录保留。
修正后分别核对两版实际前端变化，并独立核对部署pack；未重跑GPU、改写原模型记录或放宽性能门。

独立复核已有记录，不重新运行GPU：

```powershell
python -X utf8 build-orca-hc-fixed-t-20261106/publish.py --check
```

同条件复测需相同硬件／驱动／编译器、冻结EXE／pack／资产、参数和请求历史，并使用新输出目录。
[全Host与长上下文对照](orca-direct-argument-array-2026-11-06.md) · [SG16寄存器对照](orca-hc-sg16-grf-2026-11-06.md)

JSON (`hc-fixed-t-a770-windows-2026-11-06.json`, raw capture retained locally) · 精确文本归档 (`hc-fixed-t-a770-windows-2026-11-06.json.gz`, retained locally for privacy)
