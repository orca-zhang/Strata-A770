# A770：2K decode 的 GPU kernel 归属与 graph 采集覆盖

A770 16GB / PCIe 3.0 / Windows / driver 32.0.101.9034 / DPC++ 2025.3.2 / VTune 2026.4。**本轮取得 eager 解码的 kernel 排序，没有新增性能收益或生产变更。** 当前 2K 验收基线执行的 IQ2_S GU 是 WG256；不能按工作区源码中的 WG128 路径推断它的行为。

当前基准包含实际 6297 槽显存专家缓存及 pinned-host 缺失镜像，是混合驻留路径。沿用原始 Code/Prose 2048 输入、完整 128 输出、cache6000、reserve1024、context8192、实验 min-p0.5、K FP16/V packed4、专家链开启/批量等待关闭及原始 Down FP32 重算基线。生产 min-p0.8 保持。只从精确归档源重编主程序，加入 decode 边界的 ITT resume/pause；其他对象全部沿用冻结链接配方并记录摘要。没有添加逐层等待或改变数学。

## 先检查分析器是否覆盖实际工作

首次正常 graph 采集完成，但 computing-task 表只有显式拷贝和 barrier，**没有计算 kernel**。不能据此给 GU/Down 排序。微型对照对 1048576 个元素执行 21 次加一，并检查完整向量等于21；三个应用均 exit0。

| 微型模式 | 实际执行次数 | 应采集次数 | 报告中的加一 kernel 次数 |
| --- | ---: | ---: | ---: |
| eager，第一次后 resume | 21 | 20 | 20 |
| graph，第一次后 resume | 21 | 20 | 1 |
| graph，从启动就采集 | 21 | 21 | 1 |

这确认本机这套分析路径没有正确统计 graph 的逐次 kernel 重放；不暂停也没有解决。硬件采样和显式 memcpy 记录不能补齐缺失的逐 kernel 时间。原 graph 总 GPU Time 为10.738秒，而计算任务表只归属约0.687秒拷贝/barrier，未覆盖的部分不能解释为主机耗时。这里也不推断所有设备、驱动或工具版本都有相同限制。

因此第二组复用同一 EXE，只设置现有 `STRATA_VERIFY_EAGER=1`，使 verifier 和 MTP 都直接提交。阶段等待和其他诊断关闭。eager 调度及分析器都改变性能，以下是**采集期间累计 kernel 时间的比例**，不是正常 graph 的时间占比或可兑现的 tok/s 增幅。

## 采集范围与输出核对

每组依次运行 Short23/16、Code2K warm128、Prose2K warm128、Code2K collected128、Prose2K collected128。初始化、prefill、两次长预热保持暂停，仅第4、5次 decode resume。两组共10次完整输出均逐项等于冻结参考，引擎 exit0；READY 空闲 graph2192/eager2211MiB。每组四次长 prefill 的48层 Down扫描全部完成、重算数均为0。Short走 verifier 提示路径，不产生长 prefill 重算摘要。

eager 采集的 Code/Prose 分别有55/57条窗口换入日志；GU、SwiGLU、Down 各5376次，等于 `(55+57)×48`。IQ2_S GU3808次、其他GU1568次之和也等于5376。该交叉检查支持只覆盖所选两次解码。全部计算 kernel 共260498次、累计9.572915秒。

| kernel 组 | 累计秒 | 累计 kernel 时间比例 | 调用次数 |
| --- | ---: | ---: | ---: |
| IQ2_S GU | 1.700125 | 17.759742% | 3808 |
| 其他格式 GU | 0.615318 | 6.427697% | 1568 |
| 专家 Down | 1.084449 | 11.328305% | 5376 |
| 专家 SwiGLU/Q8 | 0.040734 | 0.425513% | 5376 |
| native MMVQ wide | 1.474196 | 15.399656% | 24743 |
| GR up/down-slice/down-reduce/norm 四项 | 1.305024 | 13.632462% | 45715 |
| GDN step/norm | 0.492454 | 5.144243% | 9789 |
| 其他计算 kernel | 2.860615 | 29.882382% | 164123 |

分类按导出的 kernel 名称分组，没有将多个队列的累计时间当作串行关键路径。专家完整链合计3.440626秒、35.941257%。IQ2_S GU 五种工作规模的 local size 均为 `256 x 1`，GRF128、无spill；其 stalled 从41.3%到80.3%，不能直接等同远程内存等待。

## 对下一项优化的约束

1. 优先对**当前冻结对象**做 GU WG256/128、相邻行读取和布局的单变量对照。没有证据证明工作区中的 WG128 已用于这份整模型基线，也没有由本轮数据证明 WG128 更快。
2. 专家 GU 共占24.187439%，高于 Down 的11.328305%。独立 SwiGLU/Q8 仅0.425513%；单纯省去这部分 GPU 工作的空间很小，融合仍须计入启动/读写变化并实测。
3. dense MMVQ 和 GR 仍值得独立检查；这些是实际 decode 调用，不能沿用大 T prefill INT8 原型的倍数。
4. 保留缓存和搬运证据：[A/B/C 双缓冲对照](memory-path-2026-10-27.md)确实链接本轮相同 `iq.obj`；四种分块未通过重复收益门槛。另一次[硬件计数对照](host-device-metrics-2026-10-28.md)使用的是 WG128 算子对象，SHA不同；它支持主存/显存路径诊断，不能替代当前 WG256 的停顿或吞吐测量。

当前冻结 `iq.obj` SHA256为 `17cceb618ebf7707f4582747f206c1db5bdb25b8c92ae65ef25536130ca783bb`；硬件算子组 `baseline.obj` 为 `25cf6cbd9d51082c93e29f3d4d154637eb91b3ec33a8dfc6721f17663a4d45ff`。因此本轮首先补足版本/归属证据，不全局启用新内核或缓存策略。之后仍须关闭采集，在正常 graph 的原始 Code/Prose 2K/full128 上满足两场景≥1%、基线漂移≤3%、独立确认及质量/显存检查；小于0.3%的吞吐变化按噪音解释。35/40 tok/s目标均未因本轮而通过。

## 失败保留与检查

原 graph 后检查误将四次长 prefill 摘要要求为五次；eager 后检查错误要求不存在的 eager 日志文案。两次采集均正常退出，失败脚本、协议和结果保留；新增独立审计从实际输出、四次摘要、阶段边界、kernel 次数和74项摘要重建结论。微型探针初次头文件路径错误、后续stdout路径错误也保留，完成的恢复组用应用exit0证明向量断言通过，没有替换失败数据。

[完整数字、分组和来源摘要](decode-gpu-profile-2026-10-28.json)。离线检查：

```powershell
python bench/results/a770/check_decode_gpu_profile_20261028.py
python bench/results/a770/check_decode_gpu_profile_20261028.py --local
python build-sycl-a770-decode-gpu-eager-profile-20261028/audit.py --self-check
python build-sycl-a770-decode-gpu-eager-profile-20261028/audit.py --check
```

本机原始目录：`build-sycl-a770-decode-gpu-profile-20261028/`、`build-sycl-a770-decode-gpu-eager-profile-20261028/`、`build-sycl-a770-graph-profiler-check-v2-20261028/`。采集全程持GPU/编译共享独占锁；后续报告和摘要核对只读、不启动GPU。原始 `run.py --check` 保留失败，使用上述独立审计核对完成结果。
