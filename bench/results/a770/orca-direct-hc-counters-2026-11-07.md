# HC norm：无 GPU 时间戳的外部硬件计数

2026-11-07，Windows10，Arc A77016GB，驱动32.0.101.9034，Xeon E5-2696v4，
VTune2026.4，oneAPI2025.3.2。L0指Orca L0；Direct仍为WIP。

本轮沿用 full-current 的冻结头文件和运行库，以及 Direct 驻留诊断 runtime 对象；
两侧加载同一原生包与392字节 norm ABI，没有重编数学内核。
复用独立CPU FP64参考、完整输出/输入/guard检查；固定T4、残差开、48个独立缓冲组、seeded合成输入。
没有保存或声称使用真实模型中间张量，也没有down/up或其余流水线算子。

列表内192次norm轮流覆盖48组缓冲；每次RUN重放2048个列表，每列表后等待完成。
GPU时间戳命令数为0。Host计时仅包围重放与最终等待；清零、输出/源回读、校验及打印在计时外。
每臂新进程，固定L0→Direct→L0；预热4096个列表，之后采集前、采集中、采集后各3次RUN。
保留全部30次协议，共12,976,128次norm派发；三臂CPU检查通过，完整输出hash相同。
不带profiler的六次RUN/臂取中位数；两侧L0中位数取几何均值。

| 臂 | Host µs/norm | 采集时耗时变化 | EU active | EU stall | 显存读 GB | 显存写 GB | 平均MHz |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 0-l0 | 7.910713 | +2.257% | 1.404% | 16.333% | 277.753 | 186.846 | 2188.5 |
| 1-direct | 8.237302 | +0.967% | 1.350% | 16.022% | 274.356 | 183.587 | 2201.3 |
| 2-l0 | 7.915508 | +2.656% | 1.395% | 16.212% | 276.657 | 186.105 | 2199.8 |

Direct的Host耗时变化为+4.097%，末臂/首臂L0耗时漂移为+0.061%。
这不是纯GPU kernel时长或模型tok/s；不能与旧的首尾GPU标记时长直接相减。

外部计数覆盖每臂1,179,648次norm，以及验证回读、控制调用边界和桌面活动。
它是device-wide，未拦截API，不抓二进制，不启用CPU内存带宽；EU比值按core clocks加权。
显存字节是设备本地内存计数，不能称为PCIe或D2D复制量。XVE Busy布尔值不作利用率。
异常GPU elapsed timer原值保留，若与bin时间宽度相差超过2ms，则该臂平均频率无效。

相对两侧控制，Direct平均EU活跃core cycles变化-0.697%，
停顿core cycles变化+1.368%，
显存读量-1.028%，写量-1.549%。
这个独立工作集用于区分norm本身的行为与模型流水线影响，不能将单项差异外推为整体收益或单一根因。
静态PE导入无SYCL/UR/OpenCL，Direct也无Level Zero导入；该检查不枚举动态/间接加载。

源码、实际编译头、运行库、ABI、原生包与原始协议逐项核对；重型哈希、编译、GPU和归档使用共享锁。
未改默认参数或部署，没有新增PPL、长上下文或Linux GPU结论；仍保留历史Direct8K/nonfinite/驻留故障。
同条件复测须保留设备/驱动/编译器、冻结EXE/头/运行库/ZEBIN、完整输入与命令顺序，另用新目录。
归档只包含本实验文本、协议及选定计数，不公开系统trace或数学二进制。

```powershell
python -X utf8 build-orca-hc-counter-20261107/run.py selfcheck
python -X utf8 build-orca-hc-counter-20261107/report.py check
```

[同包完整Short计数](orca-direct-gpu-counters-2026-11-07.md) ·
[JSON](../../../sycl/orca/direct-hc-counters-a770-windows-2026-11-07.json) · 文本证据 (`direct-hc-counters-a770-windows-2026-11-07.json.gz`, raw capture retained locally)
