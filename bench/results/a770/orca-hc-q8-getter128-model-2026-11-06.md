# Direct：generic128／getter128的无探针Short模型对照

2026-11-06，Windows10，Arc A77016GB，驱动32.0.101.9034，oneAPI2025.3.2。
两种配置使用同一Direct EXE，融合Q8均为128 GRF；仅比较generic行索引与getter改写。
此前getter256的组件／长历史失败不由本轮覆盖；本轮无L0或原生产SYCL性能样本。

预先固定generic→getter→generic，再getter→generic→getter；各新进程15次相同Short，
丢前十次，保留后五次中位数，外侧同配置取几何均值；共90次完整协议，不追加重复取通过。
输入23token、最多128输出、自然EOS70，tokens／草稿采纳提供／结束／reuse／resume全部匹配。
实际5771专家槽、reserve1536MiB、U4/min16/M16、read256、adapt32/decay0.95、MTP4、
step／last min-p0.5、cumulative/suffix/cost-window/prompt-cache0，BCS1，ISA／线程内存device。
没有内层profiler、GPU时间戳或额外回读；净decode排除准备、初始化、prefill。TTFT与decode首窗口重叠，
整请求直接计时，不能相加。首次READY另列；不从请求TTFT推导初始化提前后的总体收益。
源清单继承的旧mixed sequence/order仅为来源上下文；实际执行以active_groups/active_sequence为准。

| 进程 | 净decode tok/s | TTFT ms | 整请求 ms | READY ms |
| --- | ---: | ---: | ---: | ---: |
| 0-direct-generic128 | 49.7725 | 762.073 | 2127.074 | 30727.433 |
| 1-direct-getter128 | 49.5295 | 762.384 | 2136.245 | 31081.894 |
| 2-direct-generic128 | 49.6489 | 764.294 | 2132.493 | 30515.632 |
| 3-direct-getter128 | 49.5575 | 764.447 | 2136.170 | 31116.243 |
| 4-direct-generic128 | 50.0608 | 756.964 | 2115.578 | 30992.083 |
| 5-direct-getter128 | 47.4319 | 810.088 | 2243.419 | 31135.977 |

| 顺序 | getter／generic净decode变化 | decode控制漂移 | TTFT变化 | TTFT控制漂移 | 整请求变化 |
| --- | ---: | ---: | ---: | ---: | ---: |
| generic128→getter128→generic128 | -0.364% | -0.248% | -0.105% | +0.291% | +0.303% |
| getter128→generic128→getter128 | -3.152% | -4.289% | +3.960% | +5.970% | +3.477% |

原门槛保持：两个顺序净decode均至少+1%，全部报告指标控制漂移绝对值不超过3%，
TTFT及整请求不回退超过3%，且完整协议通过。控制漂移不是置信区间。
实际门结果：`{"by_backend": {"direct": {"control_drift": false, "request_nonregression": false, "short_decode_gain": false, "ttft_nonregression": false}}, "eligible_for_long_context": [], "quality": true}`。

第一个顺序控制稳定，净decode变化−0.364%，TTFT−0.105%，整请求+0.303%。
反向顺序净decode变化−3.152%，但getter两侧decode漂移−4.289%、TTFT漂移+5.970%，
不能据此认定同等幅度的因果回退。原漂移、收益、TTFT及整请求门均保留失败；本轮未建立正收益。
getter128保持隔离，不改源码默认或部署，也不追加重复／长上下文取通过。

用户已允许采用小幅正收益；原门与实测结果仍保留，本报告本身不改源码默认或部署。
本组仅为同后端、同寄存器策略的数学实现比较，不能将收益记为去除Level Zero的API收益。
亦无新长上下文、PPL、Linux GPU或长期稳定性结论，历史Direct8K间歇故障仍开放。
模型资产前后使用stat-cached SHA256身份核对，没有重新哈希89.8 GiB资产；构建／包／源另核对摘要。
首个CPU准备包装器因文本计数把==128也计入==12而拒绝；没有生成计划或执行GPU，失败源码／原因与修正均保留。

独立复核已有记录，不运行GPU：

```powershell
python -X utf8 build-orca-hc-route-split-20261106/publish_model.py --check
```

同条件复现须保留相同设备／驱动／编译器、冻结EXE／pack／资产、完整env／argv／输入及请求历史，
并使用新输出目录保存每次尝试。归档保留源、计划、完整协议与日志及摘要，不分发模型或二进制。

[编译资源与256项算子验证](orca-hc-q8-getter128-2026-11-06.md) · [HC1／路由定位](orca-direct-hc-route-split-2026-11-06.md)

JSON (`hc-q8-getter128-model-a770-windows-2026-11-06.json`, raw capture retained locally) · 精确文本归档 (`hc-q8-getter128-model-a770-windows-2026-11-06.json.gz`, raw capture retained locally)
