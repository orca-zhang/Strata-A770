# HC 连续算子链：无 GPU 时间戳，固定总计算量

2026-11-07，Windows 10，Arc A770 16GB，驱动 32.0.101.9034，Xeon E5-2696v4，oneAPI 2025.3.2，VTune 2026.4。
L0 是 Orca L0，Direct 仍为 WIP。

使用同一 full-current 冻结头文件、运行库及 HC128 原生包，Direct 沿用驻留诊断 runtime 对象。
直接链接已核对 SHA 的 strata_orca_kernels.lib，调用真实模型的 Host 算子包装；没有重编 SPIR-V/ZEBIN 数学内核。
固定 T4、残差开、Q8 输出和 seeded 合成输入，只有一套热 BF16 权重、同一组输入及输出地址。
每次 HC read 连续执行 norm→down-sliced→reduce→up/Q8；录制后逐图断言实际 kernel 节点数为 192／768。
它覆盖算子交错，不复现 48 层权重工作集、真实中间张量或跨层输入递归，不能与此前 norm-only 绝对耗时相减。

复用独立 CPU FP64 norm/down/up/inject/mixed 参考和独立 Q8 量化器对照。
每个 RUN 后比较完整输出与 guard、回读所有输入和权重检查未改写；清零、回读、验证及打印均在 Host 计时外。
无 GPU 时间戳命令；Host steady-clock 只包围 replay 与每列表后的等待。
48 次 HC read/list 重放 512 次，192 次/list 重放 128 次；各样本均为 24,576 次 HC read、98,304 次原生 kernel 派发。
每臂新进程，L0→Direct→L0；每尺寸预热 49,152 次 HC read，采集前／中／后各三次并交错列表长度，采集中采用反向交错顺序。
各尺寸六个不带 profiler 的样本取中位数，两侧 L0 中位数取几何均值。
保留全部 60 条工作协议、1,622,016 次 HC read、6,488,064 次原生派发。

| 每列表 HC read | 每列表 kernel | L0 Host µs/read | Direct Host µs/read | Direct 耗时变化 | 末／首 L0 漂移 |
| --- | ---: | ---: | ---: | ---: | ---: |
| 48 | 192 | 56.334375 | 56.284660 | -0.088% | +0.225% |
| 192 | 768 | 53.991564 | 52.131150 | -3.446% | -0.383% |

| 臂 | 48 Host µs/read | 192 Host µs/read | 长列表耗时变化 |
| --- | ---: | ---: | ---: |
| 0-l0 | 56.271145 | 54.095258 | -3.867% |
| 1-direct | 56.284660 | 52.131150 | -7.379% |
| 2-l0 | 56.397677 | 53.888070 | -4.450% |

| 臂 | EU active | EU stall | 显存读 GB | 显存写 GB | 平均 MHz |
| --- | ---: | ---: | ---: | ---: | ---: |
| 0-l0 | 19.713% | 42.227% | 67.143 | 13.052 | 2193.5 |
| 1-direct | 19.568% | 41.215% | 94.261 | 18.456 | 无效 |
| 2-l0 | 19.486% | 41.785% | 70.392 | 13.925 | 2200.3 |

外部 device-wide 计数每臂覆盖每尺寸 294,912 次原生派发，共 589,824 次，以及验证、控制边界和桌面活动。
它不能隔离单个尺寸或算子，也不能作为 PCIe／D2D API 复制量。
API 拦截、二进制抓取和 CPU 带宽采集关闭；请求 gpu-sampling-interval=1，时间轴导出设置 bin-count=200，以实际导出 bin 边界解释。
EU 比例按 core clocks 加权；GPU timer 与 bin 宽相差超过 2ms 则该臂频率无效，保留原值和异常；XVE Busy 布尔值不作利用率。

全部协议、图 kernel 数、输出 hash、编译实际头、Host 库、ABI、原生包及源码身份逐项核对。
归档按原字节保存源码、命令、原始协议及导出计数并回读校验，不包含系统 capture trace 或数学二进制。
编译、GPU、重型哈希及归档使用共享实验锁；同条件复测需要相同设备／驱动／编译器、冻结依赖、输入、预热与顺序，并使用新目录。
这不是整模型吞吐／TTFT／PPL 结果，不建立单一根因；未改生产默认或部署 EXE，没有 Linux GPU 结论，历史 Direct 8K/nonfinite/驻留故障仍未关闭。

```powershell
python -X utf8 build-orca-hc-chain-20261107/run.py selfcheck
python -X utf8 build-orca-hc-chain-20261107/report_r2.py check
```

[norm-only 列表长度对照](orca-direct-hc-batching-2026-11-07.md) ·
[JSON](../../../sycl/orca/direct-hc-chain-a770-windows-2026-11-07.json) · 原字节文本证据 (`direct-hc-chain-a770-windows-2026-11-07.json.gz`, raw capture retained locally)

离线报告首版因计数网格不一致而停止；保留原报告脚本及失败记录，未重跑 GPU。修订版要求 clocks／timer／frequency／EU 的网格完全一致，内存与 busy 按各自原始导出保留全部 bin，不补零、不删 bin。下列指标覆盖不完整；表中对应值只是已导出部分的总和，不能比较后端读量变化：

- 1-direct: GPUXVE_BUSYMetricSource, missing=[177], extra=[], changed=[]
- 1-direct: GPUGtiReadThroughputMetricSource, missing=[177], extra=[], changed=[]
