# 当前 HC1：单独匹配首选 SLM 分配策略

2026-11-06，Windows 10，Arc A770 16 GB，驱动 32.0.101.9034，oneAPI 2025.3.2。
本组 L0 指 Orca L0；命令诊断两边使用相同的 SG16／GRF128 数学包，不是原生产 SYCL 的新对照。
较早的全局 v5 SLM 试验及 M4 大投影试验保持原结果；本轮单独检查当前融合 HC Q8。

命令观察器从初始化开始记录有边界的 CPU 映射。两后端共128项完整HC输出／CPU参考／源与guard检查通过，
160次目标walker准确覆盖T1…8；每后端80次，含预热、eager和replay，所有抓取失败数为0。
每次目标派发均为80组、256个work-item、SIMD16、16个硬件线程、一个barrier；实际SLM一致。
参数、几何、掩码、local-ID、denorm及已解码派发字段一致，indirect length另有832／800字节差异；本轮不改它。

| T | 实际 SLM 字节 | 向上取整 | Direct 首选 | L0 首选 | 每后端 walker 数 |
| --- | ---: | ---: | --- | --- | ---: |
| 1 | 1792 | 2 KiB | MAX | 16 KiB | 10 |
| 2 | 3584 | 4 KiB | MAX | 32 KiB | 10 |
| 3／4 | 5376／7168 | 8 KiB | MAX | 64 KiB | 20 |
| 5…8 | 8960…14336 | 16 KiB | MAX | 128 KiB | 40 |

候选复用已保留的分桶计算：取整后的实际SLM乘ceil(128／每组硬件线程数)，上限128 KiB。
128是与当前已安装L0命令匹配的校准参数，不是新测硬件拓扑；首选字段也不代表已测实际占用率。
候选默认关闭，仅校准C0、GRF128／SIMD16、一barrier、80×256形状且名称匹配融合HC Q8时启用。
数学、实际SLM、参数、复制、依赖、状态及其它内核策略保持；没有新增线程或等批。
两种设置各通过14项CTest和36项完整命令逐字节对照；归一化首选字段后命令完全相同。
真实命令另核对160次候选目标walker，以及其它HC字段、非HC首选分配和全部命令计数不变。
候选两种设置另共128项完整HC输出／CPU／源／guard eager/replay检查通过；非法标志按预期拒绝。

无探针模型试验固定off→on→off，再on→off→on；同一候选EXE／数学包，各新进程15次Short，
丢前十次，取后五次中位数，外侧同设置取几何均值。90次完整协议与冻结参考全部匹配。
实际5771槽、reserve1536、U4/min16/M16、read256、adapt32/decay0.95、MTP4、step／last min-p0.5，
cumulative/suffix/cost-window/prompt-cache0、BCS1、ISA／线程内存device。未复制其它线程的新缓存或MTP策略。
profiler、GPU时间戳与额外回读均关闭；净decode排除准备／初始化／prefill。TTFT与首decode窗口重叠，
整请求直接测量，不能相加；READY另列。旧mixed sequence只作来源上下文，以active_groups/sequence为准。

| 进程 | 净 decode tok/s | TTFT ms | 整请求 ms | READY ms |
| --- | ---: | ---: | ---: | ---: |
| 0-direct-off | 49.9216 | 754.131 | 2116.704 | 30931.095 |
| 1-direct-on | 50.0143 | 756.126 | 2114.875 | 30591.238 |
| 2-direct-off | 50.0501 | 753.388 | 2111.253 | 30065.853 |
| 3-direct-on | 49.8433 | 755.627 | 2117.500 | 31461.844 |
| 4-direct-off | 49.9572 | 755.295 | 2115.021 | 30044.504 |
| 5-direct-on | 50.1361 | 755.350 | 2111.088 | 29944.388 |

| 顺序 | 开／关净decode变化 | decode控制漂移 | TTFT变化 | TTFT控制漂移 | 整请求变化 |
| --- | ---: | ---: | ---: | ---: | ---: |
| off→on→off | +0.057% | +0.257% | +0.314% | -0.098% | +0.042% |
| on→off→on | +0.065% | +0.587% | +0.026% | -0.037% | -0.034% |

原门保持：两个顺序净decode各至少+1%、所有报告指标漂移不超过3%、TTFT／整请求不回退超过3%、完整协议通过。
控制漂移不是置信区间；用户已接受小幅正收益，原门及失败记录仍保留，不追加重复取通过。
实际门：`{"by_backend": {"direct": {"control_drift": true, "request_nonregression": true, "short_decode_gain": false, "ttft_nonregression": true}}, "eligible_for_long_context": [], "quality": true}`。

本报告不改变源码默认、部署或推荐后端；模型阶段没有新的L0／原生产SYCL吞吐样本。
没有新长上下文、PPL或Linux GPU结论，历史Direct 8K间歇故障仍开放。模型资产前后使用stat-cached SHA256身份核对。
最初准备解码器时错误假定旧M4选择器的文本形式，CPU自检在编译／GPU前拒绝；失败源码与修正均保留。

独立复核已有记录，不运行GPU：

```powershell
python -X utf8 build-orca-hc-slm-20261106/publish.py --check
```

同条件复现需保留相同设备／驱动／编译器、冻结EXE／内核包／模型资产、完整env／argv／输入及请求历史。
新运行使用新目录保留每次尝试，不能覆盖冻结记录。归档保存源、计划、完整协议／日志、原始命令和摘要，
不分发模型或可执行／数学二进制；既有依赖来源见原试验归档。编译、GPU、重型哈希及归档持共享锁。

[HC1／路由定位](orca-direct-hc-route-split-2026-11-06.md) · [getter128模型试验](orca-hc-q8-getter128-model-2026-11-06.md)

JSON (`direct-hc-slm-a770-windows-2026-11-06.json`, raw capture retained locally) · 原始证据归档 (`direct-hc-slm-a770-windows-2026-11-06.json.gz`, raw capture retained locally)
