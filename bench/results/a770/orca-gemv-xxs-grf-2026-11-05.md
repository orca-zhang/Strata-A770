# Orca L0：IQ2_XXS GEMV 定向 256 GRF 对照

A770 16 GB、Xeon E5-2696 v4、Windows、驱动 `32.0.101.9034`、oneAPI 2025.3，2026-11-05。
沿用当前 U4N16／四路加载／M16／min_rows16 主配置，同 EXE、权重、缓存5771槽和 MTP4，只重新编译一个原生 GEMV image 的寄存器选项。
原 SPIR-V、kernel ID、参数 ABI、SG32／WG256、整数点积和 FP32 累加顺序保持。
当前部署内核为128 GRF，候选256 GRF；两侧元数据均未报告 spill，不能沿用旧 SYCL 对象的576字节溢出结论。

480条 eager 完整链逐字节核对全部输出和 scratch，包含 guards、未用输出、1–7条目、显存／映射主存／混合驻留及空组；另有72次图重放最终数据检查。
这以当前部署 image 为实现参考，不是新的独立 CPU 数学或广泛质量结论。

每个新进程先 Short23/16，再交错运行三轮 Code／Prose2K/full128；丢弃首轮，对后两轮取中位数。
两侧控制取几何均值；预设每场景 decode 吞吐增幅≥1%、外侧 decode 漂移绝对值≤3%，通过才启动独立反序。
全部请求核对完整 token、生成数、MTP accepted/offered、reused、resume 和停止原因。Decode 使用引擎内部时间，排除加载、初始化和 prefill；未开启内部剖析。

| 顺序 | 场景 | 原版 decode ms | 候选 ms | 原版 tok/s | 候选 tok/s | 吞吐变化 | 外侧耗时漂移 |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| performance-forward-r1 | Code2K | 4195.524 | 4205.300 | 30.509 | 30.438 | -0.232% | +0.168% |
| performance-forward-r1 | Prose2K | 4238.924 | 4250.300 | 30.196 | 30.116 | -0.268% | -0.124% |

**没有确认当前 decode 提速，候选未采用，主配置和 U4N16 支持保留。**
首轮 build-r1 在组件执行前误把参考输出作为权重输入，报 bulk size；失败目录完整保留，build-r2 改用原始专家权重，没有覆盖失败或替换计时样本。
编译、GPU执行和资源哈希均持共享实验锁。归档保留源码、命令、原始协议、日志和哈希，不含权重、EXE、OBJ、SPIR-V 或输出二进制。

```powershell
python -X utf8 build-orca-gemv-xxs-grf-20261105/run.py self-check
python -X utf8 build-orca-gemv-xxs-grf-20261105/publish.py --check
```

[数字](../../../sycl/orca/gemv-xxs-grf-a770-windows-2026-11-05.json) · 文本证据 (`gemv-xxs-grf-a770-windows-2026-11-05.json.gz`, raw capture retained locally)
