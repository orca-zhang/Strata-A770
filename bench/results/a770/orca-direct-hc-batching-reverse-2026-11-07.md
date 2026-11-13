# HC norm：相同计算量，192／768 次派发列表，反向复测

2026-11-07，Windows 10，Arc A770 16GB，驱动 32.0.101.9034，Xeon E5-2696v4，
VTune 2026.4，oneAPI 2025.3.2。L0 是 Orca L0，Direct 仍为 WIP。

沿用冻结 full-current 头文件与运行库、Direct 驻留诊断对象和同一 HC128 原生包；未重编数学内核。
T4、残差开、48 个独立缓冲、seeded 合成输入；复用独立 CPU FP64 参考及完整输出、输入、guard 检查。
两种列表只改变重放与等待边界：192 次列表重放 1024 次，768 次列表重放 256 次，单样本均为 196,608 次 norm。
不合并算术内核，不改变数据依赖，也不等待额外任务凑批；这不是模型服务的动态批处理。

每臂新进程，Direct→L0→Direct；每尺寸预热 393,216 次派发。
各尺寸在采集前、中、后各测三次，并在进程内交错顺序；采集中采用反向交错顺序。
无 GPU 时间戳，Host steady-clock 仅包围 replay 与每列表后的等待；清零、回读、完整校验、打印在计时外。
各尺寸六个不带 profiler 的样本取中位数，外侧 Direct 中位数取几何均值。
全部 60 条工作协议、12,976,128 次派发保留；三臂独立 CPU 检查与完整输出 hash 一致。

| 每列表 norm 数 | L0 Host µs/norm | Direct Host µs/norm | Direct 耗时变化 | 末／首 Direct 漂移 |
| --- | ---: | ---: | ---: | ---: |
| 192 | 8.118146 | 8.406484 | +3.552% | -1.883% |
| 768 | 7.544690 | 7.437803 | -1.417% | +2.060% |

| 臂 | 192 Host µs/norm | 768 Host µs/norm | 768 相对 192 耗时变化 |
| --- | ---: | ---: | ---: |
| 0-direct | 8.486779 | 7.362369 | -13.249% |
| 1-l0 | 8.118146 | 7.544690 | -7.064% |
| 2-direct | 8.326949 | 7.514011 | -9.763% |

Host 耗时包含 CPU 提交、完成通知和等待，不能称为纯 GPU kernel 时长，也不是模型 tok/s。
改变列表长度同时改变 GPU 工作连续性及提交／等待次数，不能把两尺寸差值全部归因于 CPU 锁或某个 API。
此处也未复现 down/up、router 等模型周边工作集，不能外推完整 Short 或长上下文收益。

| 臂 | EU active | EU stall | 显存读 GB | 显存写 GB | 平均 MHz |
| --- | ---: | ---: | ---: | ---: | ---: |
| 0-direct | 1.443% | 16.817% | 282.100 | 189.165 | 无效 |
| 1-l0 | 1.636% | 17.643% | 308.494 | 238.161 | 2239.0 |
| 2-direct | 2.034% | 18.224% | 340.503 | 257.548 | 2268.5 |

硬件采集包含每尺寸每臂 589,824 次 norm，共 1,179,648 次派发，
以及验证回读、控制边界、桌面活动；为 device-wide 混合尺寸窗口，不能分离某个尺寸的硬件计数。
API 拦截、二进制抓取和 CPU 带宽采集均关闭，采样间隔 1ms。
EU 比例按 core clocks 加权；显存字节为设备本地内存计数，不能称为 PCIe 或 D2D API 复制量。
异常 GPU timer 原值保留，与 bin 宽相差超过 2ms 则平均频率无效；XVE Busy 布尔值不作利用率。
两侧 Direct 的几何均值相对中间 L0：平均 EU 活跃 core cycles +3.112%，
停顿 core cycles -2.281%，显存读 +0.465%，写 -7.322%。

全部原始样本、各尺寸 profiler 扰动与前后漂移、计数与时间轴校验保留在 JSON。
源码、编译实际头、运行库、ABI、原生包及协议均校验 SHA；归档逐文件原字节回读验证，不包含系统 capture trace 或数学二进制。
同条件复测须保留设备／驱动／编译器、冻结输入／头／运行库／ZEBIN，并使用新输出目录和相同交错顺序。
未改产品运行库、生产默认或部署 EXE；无新 PPL 或 Linux GPU 结论，历史 Direct 8K/nonfinite/驻留故障仍未关闭。

```powershell
python -X utf8 build-orca-hc-batching-20261107/run.py selfcheck
python -X utf8 build-orca-hc-batching-20261107/report_reverse.py check
```

[单一 192 次列表计数](orca-direct-hc-counters-2026-11-07.md) ·
[JSON](../../../sycl/orca/direct-hc-batching-reverse-a770-windows-2026-11-07.json) · 原字节文本证据 (`direct-hc-batching-reverse-a770-windows-2026-11-07.json.gz`, raw capture retained locally)

[正向对照](orca-direct-hc-batching-2026-11-07.md)。反向未重编 EXE，未与正向样本合并选优。
