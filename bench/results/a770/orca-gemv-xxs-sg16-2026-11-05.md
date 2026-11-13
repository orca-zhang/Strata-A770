# Orca L0：IQ2_XXS GEMV 物理 SG16 对照

A770 16 GB、Xeon E5-2696 v4、Windows、驱动 `32.0.101.9034`、oneAPI 2025.3，2026-11-05。
沿用当前 U4N16／四路加载／M16／min_rows16 主配置，同 EXE、权重、实际缓存5771槽和 MTP4。
只在冻结数值源码的 `launch_gu_port` 中，对 TG16／LN16 将物理子组32改为16；WG256、GRF128、点积、累加和归约顺序保持。
源码检查覆盖全部1024个 XOR 邻居，映射相同；其他模板实例的子组属性保持。
原生元数据确认原版SIMD32、候选SIMD16，两侧均未报告 spill。为复用原 EXE，导出查找键保留旧模板名中的SG32字样；实际物理大小以元数据为准。
编译器 ABI 头及原生 Host payload 完全一致；只替换部署 pack 的一个 image 和相应 manifest，其他 image 逐一核对摘要。

480条 eager 完整链逐字节核对全部输出和 scratch，包含 guards、未用输出、1–7条目、显存／映射主存／混合驻留和空组；另有72次图重放最终数据检查。
参考是当前部署实现，不据此宣称新的独立 CPU 数学或广泛质量验收。

每个新进程先 Short23/16，再交错三轮 Code／Prose2K/full128；丢首轮，后两轮中位数，外侧控制取几何均值。
预设每场景 decode 吞吐增幅≥1%、外侧 decode 耗时漂移绝对值≤3%，通过才做独立反序。
全部完整输出、MTP accepted/offered、生成数、reused、resume、停止原因与当前 min16 参考一致。Decode 排除加载、初始化及 prefill，无内部剖析；TTFT/request 原数保留。

| 顺序 | 场景 | 原版 decode ms | 候选 ms | 原版 tok/s | 候选 tok/s | 吞吐变化 | 外侧耗时漂移 |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| performance-forward-r1 | Code2K | 4197.025 | 4206.350 | 30.498 | 30.430 | -0.222% | +0.025% |
| performance-forward-r1 | Prose2K | 4243.647 | 4239.300 | 30.163 | 30.194 | +0.103% | +0.248% |

**没有确认当前 decode 提速；候选未采用，U4N16 主配置保留。**
首轮 build-r1 的派生脚本误改旧编译命令目录，机器码已成功生成，但组件编译前报路径不存在。失败快照保留；run-r2.py 只修正命令和本试验输出目录，复用同一份通过来源校验的 SPIR-V，没有重编译候选数值源码。

编译、GPU工作、重型哈希与发布检查持共享实验锁。归档保留源码、精确派生步骤、命令、完整原始协议与日志；不含权重、EXE、OBJ、SPIR-V、输入或输出二进制。

```powershell
python -X utf8 build-orca-gemv-xxs-sg16-20261105/check_source.py
python -X utf8 build-orca-gemv-xxs-sg16-20261105/publish.py --check
```

[数字](../../../sycl/orca/gemv-xxs-sg16-a770-windows-2026-11-05.json) · 文本证据 (`gemv-xxs-sg16-a770-windows-2026-11-05.json.gz`, raw capture retained locally)
