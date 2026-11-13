# 融合 HC Q8：仅匹配 indirect payload length

2026-11-06，Windows 10，Arc A770 16 GB，驱动32.0.101.9034，oneAPI2025.3.2。
旧版本的全局32字节长度试验保持原结果；本次仅对当前SG16／GRF128融合HC Q8隔离一个命令字段。
选择器依据内核元数据与形状，不依据模型HC1阶段；满足条件的HC0与HC1融合Q8调用都可能受到影响。
HC检查器中的HC1.*是历史诊断标签，不能据此将本轮结果归因于独立HC1阶段。
原始实际命令中，Direct目标walker的indirect length为832，Orca L0为800字节，各80次；
当前声明cross-thread832字节，前32字节inline。候选只将目标walker长度改成800，
保留832字节后备分配、参数与零填充、指针、SBA窗口、首选SLM、数学映像、依赖和其它命令。
没有测量实际内存总线流量，不能把命令长度差直接当成DRAM流量下降。

另复核同一批已有上下文日志：两后端的计算提交均在node5，元数据名称为Compute。
这组捕获不支持“选到了不同计算节点”的解释；它不是完整模型的引擎利用率测量，没有重新查询硬件。

候选默认关闭，限校准C0、SIMD16／128GRF、一barrier、硬件local-ID、80×256形状、
cross832／inline32的已抓取元数据组合。没有改生产源码、部署、推荐L0路径或Linux策略。
两种设置各通过14项CTest、72项完整CPU命令比较，以及64项完整HC eager/replay输出／CPU／源／guard检查。
归一化唯一长度字段后，144项CPU命令逐字节相同；后备字节与填充另核对。
真实抓取80次目标walker／设置，核对832→800，所有其它已解码字段、非目标长度及命令计数一致；抓取失败0。
非法环境设置按预期拒绝。CPU边界包含79／80／81组、800／832／864字节cross、128／256GRF、
硬件ID开关及两种排序方式，用于拒绝范围外的误选择。

完整模型固定off→on→off，再on→off→on；每个新进程15次Short，丢前十次、保留后五次中位数。
外侧同策略取几何均值。90次完整输出、草稿采纳／提供、结束、reuse／resume均匹配冻结参考。
实际5771槽、reserve1536MiB、U4/min16/M16、read256、adapt32/decay0.95、MTP4、step／last min-p0.5，
cumulative/suffix/cost-window/prompt-cache0，BCS1，ISA／线程内存device；同一EXE及数学包，仅策略开关不同。
profiler、GPU时间戳与额外回读关闭；净decode排除准备／初始化／prefill。
TTFT与首decode窗口重叠，整请求直接测量，不能相加。READY另列；资产前后用stat-cached SHA256身份核对。

| 进程 | 净decode tok/s | TTFT ms | 整请求 ms | READY ms |
| --- | ---: | ---: | ---: | ---: |
| 0-direct-off | 50.1433 | 758.072 | 2118.581 | 32421.670 |
| 1-direct-on | 48.4128 | 779.474 | 2187.648 | 32150.635 |
| 2-direct-off | 50.1181 | 757.901 | 2111.747 | 33190.157 |
| 3-direct-on | 50.1002 | 770.701 | 2126.098 | 32783.933 |
| 4-direct-off | 50.0071 | 754.741 | 2117.030 | 33274.290 |
| 5-direct-on | 49.9216 | 751.973 | 2123.130 | 30750.025 |

| 顺序 | 开／关净decode变化 | decode控制漂移 | TTFT变化 | TTFT控制漂移 | 整请求变化 |
| --- | ---: | ---: | ---: | ---: | ---: |
| off→on→off | -3.427% | -0.050% | +2.835% | -0.023% | +3.427% |
| on→off→on | +0.007% | -0.357% | +0.866% | -2.430% | +0.358% |

原门保持：双向decode各至少+1%、全部报告指标漂移不超过3%、TTFT／整请求不回退超过3%、完整协议通过。
用户已接受小幅正收益；原始门和失败记录仍保留，不追加挽救重复。漂移不是置信区间。
实际门：`{"by_backend": {"direct": {"control_drift": true, "request_nonregression": false, "short_decode_gain": false, "ttft_nonregression": true}}, "eligible_for_long_context": [], "quality": true}`。

本轮仅为同一Direct后端内的参数长度试验，没有新的L0／原生产SYCL吞吐、长上下文、PPL或Linux GPU结论。
两种顺序未建立一致收益，且正序整请求非回退门失败；候选维持隔离和默认关闭。
不能将正序单次下降视为同等幅度的稳定退化，也不能用反序近似持平将原失败门改成通过。
既有Direct8K间歇故障仍开放。源码默认仍Direct128／L0256。

复核完整记录，不运行GPU：

```powershell
python -X utf8 build-orca-hc-indirect-20261106/publish.py --check
```

同条件复现需相同硬件／驱动／编译器、冻结EXE／pack／模型资产、完整env／argv／输入及请求历史。
首次缓存节点日志自检因未归一化Windows换行而拒绝，原脚本片段及原因保留；修正只影响离线归档核对。
每次使用新输出目录，归档保留脚本、源码、原始命令、完整协议／日志与摘要，不分发模型或二进制。
编译、GPU执行、重型哈希及归档遵守共享实验锁。源生成器复用旧试验，并保留父脚本SHA256。

[HC1区间定位](orca-direct-hc-route-split-2026-11-06.md) · [SLM单字段试验](orca-direct-hc-slm-2026-11-06.md)

JSON (`direct-hc-indirect-a770-windows-2026-11-06.json`, raw capture retained locally) · 原始证据归档 (`direct-hc-indirect-a770-windows-2026-11-06.json.gz`, raw capture retained locally)
