**DFlash2 首轮原生试验，2026-10-29**

短输入初测没有提速：现有 MTP 为 **26.14 token/s**，DFlash2 为 **23.97 token/s**，吞吐下降 **8.31%**。前后基线的解码耗时漂移为 −0.47%。2K 长输入的原 MTP 基线预填反复失败，未获得有效长输入对照。

硬件为 Intel Arc A770 16 GB、Xeon E5-2696 v4（22 核 / 44 线程）、约 256 GiB RAM，Windows，oneAPI 2025.3.2。主模型为本机 Qwen3.8-Flash-Next IQ2_XS。对照使用冻结的 `build-sycl-a770-compact-device-counter-20261029` 引擎及现有 full-vocab MTP；正式服务、EXE 和配置均未修改。

本次实现使用[匹配的官方检查点](https://huggingface.co/incoai/Qwen3.8-Flash-Next-DFlash2)，包含 81 个张量、603,065,600 个参数。原文件 SHA256 为 `d6723f2a359eaf33ec0c95f22ae8edda896774020c552296a85e1913b4d3186f`。复用现有 Q8_0 矩阵内核及主模型 embedding/head，新增五层草稿计算和第 5/15/25/35/45 层完成输出的四流均值采集。矩阵为 Q8_0、激活为 Q8_1，候选选择器的两个 BF16 码本留在 CPU。DFlash2 设备分配为 699,877,904 字节，约 667.5 MiB。

试验限于单会话、贪心解码、block=4（一个已知 token 加三个草稿）、2048 草稿注意力窗口。原 MTP 使用 0.8 概率门槛；DFlash2 固定提供三个候选，由原验证器决定接受结果。适配层填入的概率 1.0 仅用于固定窗口，不能用于采样。采样、会话恢复和并发均未验证。

两组固定 6297 个专家缓存槽（`--expert-cache 6000`）、8192 上下文、2048 预填块、1024 MiB 显存预留、FP16 KV / packed V4、graph、prompt-cache=0、suffix-draft=0。保留相同自适应缓存策略，关闭诊断导出和分析器。短输入为 23 个输入 token，每次生成 32 个 token；A1/B/A2 各使用新进程，一次预热、两次保留测量。

| 组 | 两次保留速度，token/s | 解码耗时中位数，ms | 预填耗时中位数，ms |
|---|---:|---:|---:|
| A1，原 MTP | 24.23 / 28.24 | 1226.85 | 872.90 |
| B，DFlash2 | 22.41 / 25.77 | 1334.90 | 875.90 |
| A2，原 MTP | 24.39 / 28.32 | 1221.10 | 873.80 |

用 A1/A2 耗时中位数的几何平均作为基线，得到 26.1444 → 23.9718 token/s，−8.3099%。整次请求的墙钟耗时约 2.098 → 2.211 秒，也没有改善。保留样本仍随缓存变热而加速，因此这些数字表示上述相同请求历史的初测；两次对应的 DFlash2 样本均慢于两侧基线。

每条保留请求，原 MTP 接受 15/16 个提议（93.75%），DFlash2 接受 20/33（60.61%）。DFlash2 接受的草稿总数更多，但本实现的整体开销抵消了收益。未做算子耗时分解，不能据此把回退归因于某一个内核或 CPU 选择器。

全部九条短输入请求的 32 个输出 token 完全相同，三个进程均 exit 0，缓存槽数相同，启动后空闲显存均大于 1 GiB。独立 CPU 检查完成 150 项比较，覆盖上下文 K/V、所有草稿矩阵投影及归一化、卷积/注意力/SILU/残差连接，以及候选选择路径。逐算子检查使用 GPU 实际输入独立重算，避免把中间舍入差异在后续 Q8 量化边界放大；这不是 BF16 官方实现的端到端逐位一致性验证。

2K 测速保留了三次失败：混合短/长输入顺序在原基线 layer 19 预填失败；独立长输入组在 layer 27 失败；补齐已有测试的 Short16 预热后，在 layer 42 失败。扫描确认专家 Down 及上游 H 存在大量非有限值。这三次均未开始 DFlash2 长输入组，不能给出 2K 代码、文本的性能结论。没有修改原引擎的预填实现来绕过检查。

本轮结论仅支持：这个原生 Q8、block=4 试验在所测短输入上没有收益，暂不替换现有 MTP。DFlash2 其他精度、块大小及优化后的实现仍需另测。

本机材料均保留在隔离目录：

- [短输入完整结果、参数、输入 token 和来源哈希](../../../build-sycl-a770-dflash2-20261029/screen-short/results.json)。同目录保留 A1/B/A2 的逐 token 协议、stderr 和逐请求数据。
- [短输入实际运行脚本](../../../build-sycl-a770-dflash2-20261029/run-short.py)：SHA256 与结果中测量时的 `run.py` 完全一致；[最终长输入脚本](../../../build-sycl-a770-dflash2-20261029/run-long.py)。输出目录拒绝覆盖既有记录。
- [原生草稿实现](../../../build-sycl-a770-dflash2-20261029/dflash.cpp)、[适配及构建脚本](../../../build-sycl-a770-dflash2-20261029/build.py)、[构建与 EXE 哈希](../../../build-sycl-a770-dflash2-20261029/build.json)、[检查点准备记录](../../../build-sycl-a770-dflash2-20261029/preparation.json)。
- [独立 CPU 检查入口](../../../build-sycl-a770-dflash2-20261029/check.py)、[150 项检查结果](../../../build-sycl-a770-dflash2-20261029/smoke/trace/cpu-check.json)、[短输入校验汇总](../../../build-sycl-a770-dflash2-20261029/smoke/results.json)。
- [首次预填失败](../../../build-sycl-a770-dflash2-20261029/screen-prefill-failure/results.json)、[未预热的长输入失败](../../../build-sycl-a770-dflash2-20261029/screen-long-unprimed-failure/results.json)、[Short16 预热后的长输入失败](../../../build-sycl-a770-dflash2-20261029/screen-long/results.json)。各目录保留原始协议和 stderr。
