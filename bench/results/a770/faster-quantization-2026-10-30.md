# A770：更快的量化方式及两版激活 LUT 结果

2026-10-30。对象为本机 Arc A770 16 GiB、Xeon E5-2696 v4、256 GiB RAM、PCIe 3.0、Windows 上的 IQ2_XS pack。本报告将“更快”理解为推理更快，同时单列离线量化速度。此次续查只读取已有测量和外部一手源码，没有新增 GPU 计时、安装依赖或改变生产引擎、权重、配置。

**当前最有测量依据的方向是：低位存储/传输，高行数 prefill 临时转 INT8 计算。若目标是进一步压缩带宽，优先校准现有 IQ 格式的混合精度；若要研究新格式，优先看 QTIP/EXL3。尚未找到在本机现有 decode 路径上通过完整模型验收的更快量化替代。**

## 选择顺序

| 顺序 | 方案 | 主要作用 | 本机证据和边界 |
| --- | --- | --- | --- |
| 1 | 保留 IQ2/IQ1/Q2 权重，按实际行数转 INT8、用 XMX 计算 | 减少长提示词处理的计算时间；压缩载荷不变 | 已有完整专家链独立确认，含 H2D、转换、GU、激活、Down；尚无模型 TTFT/质量验收 |
| 2 | 对低敏感层/专家评估 IQ2_S→IQ2_XS/IQ2_XXS，必要时 IQ1_M | 减少权重载荷、提高缓存容纳量 | 现有引擎已有相应格式；尚无新的安全替换名单或 PCIe 流量实测 |
| 3 | QTIP/EXL3 trellis 编码，配合 Hadamard 和分阶段内核 | 在低位精度下兼顾解码成本与误差 | 已有 Intel ESIMD 源码，但作者验证的是 B70/Xe2，不能直接套用 A770/DG2 |
| 4 | HQQ 的均匀 W2/W4，参考 GemLite 的形状分派及 RevSplit-K | 简单线性解码、离线转换快；部分形状减少元数据加载 | 没有本机质量/性能结果；其现成运行栈不能直接替换 Strata SYCL |
| 5 | T-MAC 在 CPU 计算部分未驻留专家 | 可能用少量激活/输出传输替代专家权重传输 | 只有外部 CPU 证据；本机老 Xeon 的算力、RAM 带宽和并行调度仍须测量 |

当前 pack 并非统一的 2-bit：34 层 GU 为 IQ2_S（2.5625 bpw），11 层为 IQ2_XXS（2.0625 bpw），3 层为 IQ1_M（1.75 bpw）；Down 均为 Q2_0（2.25 bpw）。专家载荷总计 35,454,976,000 B。详见[实际格式和低位误差筛查](lowbit-lut-2026-10-30.md)。减少理论乘法数、提高 XMX 峰值或降低名义 bit 数，均不能代替完整链计时。

## 1. 已有收益：低位传输，INT8 计算

复用仓库已有的[整数 prefill 完整链测量](xpu-optimization-2026-10-28.md)，无需另建量化后端。计时保留原低位专家，每次计入 pinned H2D、权重/输入转换、GU、激活和 Down。以下是独立确认轮的吞吐变化，参考为该实验的 FP16/XMX prefill 链。

| GU 格式 / 临时 INT8 布局 | 每专家实际行数 | 完整链吞吐变化 |
| --- | ---: | ---: |
| IQ2_XXS / column | 512 / 1024 / 2048 | +20.228% / +33.034% / +40.202% |
| IQ2_S / column | 512 / 1024 / 2048 | +20.518% / +32.895% / +40.180% |
| IQ1_M / VNNI | 512 / 1024 / 2048 | +46.358% / +70.507% / +86.828% |

九个场景均在两轮独立进程中通过 ≥1% 收益、双侧漂移 ≤3%，各轮均有 8/8 稳定 ABBA 周期。原始结果 SHA256：`de5b707fe24a0e4228684269fdab3683aa87edf8e17a6212ee76e2cb0b40369d`。中间行数 T16/40/64 的 54 个格式/布局/驻留组合中，另有 35 个独立确认，增幅 +2.546%～+46.825%。这是代表性真实权重、合成普通/离群激活的算子证据。

T 是单个专家收到的行数，不能拿整个 prompt 长度替代。旧真实热点有 349/510/668/978/1971/1995 行，尾块补测及真实输入/模型验收仍需完成；截至本次读取，相关六形状补测已排队。本报告不启动重复队列，也不把离散校准点推广为任意连续阈值。

这条路径增加临时 INT8 数据，却保持 PCIe 上传输的低位权重不变，适合权重被多行复用的 prefill。现有[逐专家 INT8 与原生 grouped GEMV 的 decode 对照](int8-native-grouped-2026-10-29.md)出现瞬时输出错误，且没有正向筛选信号；不能将上述收益用于小窗口 decode。此前原始 BF16→U4 完整专家的载荷增加 83.05%，高 T 虽可能更快，也不适合作为降低带宽的默认方案，见[请求收益缺口](u4-request-gap-2026-10-27.md)。

## 2. 带宽候选：先复用当前 IQ 格式

比另写二值/三值内核更小的试验，是从原 BF16 权重对低敏感范围重新量化，继续用当前引擎支持的格式。下表只计算一个原 IQ2_S 专家的 GU，Down 保持 Q2_0，不是误差、流量或速度测量。

| 新 GU 格式 | GU 实际 bpw | 完整专家字节 | 相对原 1,510,400 B 减少 | 假设全部 34 个 IQ2_S 层转换时，减少全部专家载荷 |
| --- | ---: | ---: | ---: | ---: |
| IQ2_XS | 2.3125 | 1,408,000 | 6.780% | 1.6602 GiB / 5.028% |
| IQ2_XXS | 2.0625 | 1,305,600 | 13.559% | 3.3203 GiB / 10.055% |
| IQ1_M | 1.75 | 1,177,600 | 22.034% | 5.3955 GiB / 16.340% |

全 34 层转换只是容量预算，不建议直接实施。应先在固定校准输入上比较 router 加权专家输出扰动，再比较固定目标 NLL；选择依据还应包括缓存 miss 字节。低调用频率、低 router 排名都不足以确定可降位，更不能把载荷减少比例当作请求提速比例。

现有 `NativeExpertLayout` 的 GU/Down 类型由层级描述；同层逐专家或 gate/up 分别混格式仍需分组和布局支持。最小试验可以先按层离线校准，不能认为换一个类型号就能完成任意逐专家混精度。现有 IQ2 权重已经使用码本，QTIP/AQLM 也不能仅凭“采用码本”就认定优于它。

## 3. 新格式：QTIP/EXL3 优先，YAQA 可作为质量方向

[QTIP][qtip] 用 Hadamard 等处理使权重更适合压缩，再用 trellis 编码。其参考实现提供 2/3/4-bit 矩阵向量内核；[EXL3][exl3] 是基于 QTIP 的工程实现。最相关的来源是 [exl3xpu][exl3xpu]：已有原生 ESIMD 的 trellis 解码、GEMV、DPAS 和大 M reconstruct→GEMM 路线。其设计以少量整数运算生成码本值，值得比较与当前 IQ 表加载的实际成本。

限制是其 tile、DPAS 和公开速度来自 B70/Xe2、另一模型及约 4 bpw checkpoint；A770 是 DG2。还需要验证本模型、目标 bitrate、Hadamard 变换成本、低位精度和主存专家流式访问。不能直接读取现有 IQ2 blob，也没有现成可部署的本机低位资产。2-bit 新格式对已经 1.75 bpw 的 IQ1_M GU 未必节省容量，必须把符号/尺度等元数据计入。

[YAQA][yaqa] 使用与模型输出相关的近似 Hessian 来改进量化舍入；QTIP 当前 README 推荐它作为后续方法。这是帮助相同格式保留质量、进而探索更低位的方向，尚无本机证据证明它降低运行时计算量，也不能自动把 YAQA 的量化结果当作 EXL3 格式兼容。

## 4. 其他来源怎样使用

| 方法 | 读到的实际能力 | 对当前机器的判断 |
| --- | --- | --- |
| [HQQ][hqq] + [GemLite][gemlite] | HQQ 支持 1/2/3/4/8-bit、无需校准数据的离线量化；GemLite 有 GEMV、GEMM、Split-K、RevSplit-K，支持 1/2/4/8-bit 权重 | 离线量化快不保证推理快。HQQ 的 fast runtime 对分组轴有限制；GemLite 当前主要证据/代码是 CUDA、HIP/Triton，未见本机 A770 验收。可借鉴小 M 减少 scale/zero 重复加载的方法 |
| [AQLM][aqlm] | 多码本相加，提供 CUDA/Triton GPU 及 Numba CPU 内核；不同码本配置的速度差异明显 | 极低位质量候选，缺少本模型和 A770 高效内核；不优先于已有 Intel trellis 路线 |
| [QuIP#][quip] | 2-bit E8 向量码本、快速 CUDA 推理；仓库推荐后续 QTIP | 保留为原理/质量对照；其 README 明确 3-bit 缺相应优化内核而更慢，进一步说明少 bit 不自动更快 |
| [FLUTE][flute] / HIGGS | 非均匀 LUT 量化，支持向量解码及 Hadamard；2-bit 需要源码构建 | 公布支持的 GPU 是 NVIDIA；这是权重解码 LUT，与本次激活 LUT 不同，不能把本次失败外推给 FLUTE |
| [Marlin][marlin]、[BitBLAS][bitblas]、MXFP4/NVFP4 | 硬件相关的低位权重矩阵内核；Marlin 明确要求 NVIDIA CC≥8.0，BitBLAS 的公开测试/安装以 CUDA 为主 | 可参考布局、分块和流水线，不能直接接入 A770；Blackwell FP4 的数字不作为本机依据 |
| [T-MAC][tmac] | CPU 上 1/2/3/4-bit 混合精度激活 LUT；支持 ARM/Intel CPU | 有 CPU 分担未驻留专家的研究价值，仍需验证旧 Xeon、实际专家形状及 GPU 等待；作者明确提醒旧 x86 未保证加速 |

若用户指的是“把 BF16 转成低位权重的过程更快”，HQQ 值得先比较；本报告优先的 INT8 阶段分派解决的是推理时间。BitNet 的二值/三值训练结果也不能直接作为当前模型的免训练替换依据。

## 5. 本次两版激活 LUT 的归档

两版均保持原 IQ2_S GU、SwiGLU、Q8_1 激活、Q2_0 权重字节/尺度和输出映射，只替换 Down 的计算。Q2 编码满足 `c−1 = bit0 + 2×bit1 − 1`。第一版对四个激活构造 16 项 int16 子集表，按两个 bitplane 查表并补偿；第二版对两个激活直接构造 16 项 Q2 点积表。它们没有继续量化权重。

- subset4：每个权重字节三次表读取；CAP64 额外 scratch 为 320 KiB。
- direct2：每个权重字节两次表读取；CAP64 额外 scratch 为 640 KiB。
- 每次调用都重新构表，构表时间计入完整专家链；使用全局内存表，没有省去权重传输。

沿用冻结原始 `iq.obj`，另设重建原 helper 控制，顺序为原始/控制/候选/控制/原始。240 个真实第一层专家、8 个物理 bank，N2560/F640，验证窗口 T1/4/6；这里 T 是输入 token 数，与上文 prefill 的每专家行数不同：T1 为 10 专家组×1 条目，T4/6 为 20/30 组×2 条目。驻留分纯 VRAM、每八个专家一个主存映射、全主存映射。它不是全模型 48 层采样。graph 每次包含 16 条链、预热 4 次，每臂保留 5 批×2 replay。

以下为两侧基线漂移均 ≤3% 的全部场景。吞吐变化分别相对原始对象/重建控制；需同时达到 +1% 才进入独立确认。没有场景进入确认。

| 版本 | 驻留 | T | 相对原始 / 控制 | 最大绝对漂移 |
| --- | --- | ---: | ---: | ---: |
| subset4 | 混合 | 6 | −10.461% / −11.429% | 0.621% |
| subset4 | 全主存映射 | 1 | +0.468% / +0.809% | 1.864% |
| subset4 | 全主存映射 | 4 | −0.028% / −0.235% | 0.952% |
| subset4 | 全主存映射 | 6 | +0.735% / −0.365% | 1.091% |
| direct2 | 混合 | 4 | −9.250% / −9.661% | 2.944% |
| direct2 | 混合 | 6 | −7.115% / −9.255% | 1.087% |
| direct2 | 全主存映射 | 1 | +0.711% / −0.807% | 1.126% |
| direct2 | 全主存映射 | 4 | −0.756% / −0.577% | 1.390% |
| direct2 | 全主存映射 | 6 | +0.400% / −0.049% | 0.615% |

VRAM 六项和剩余混合三项均未通过双基线漂移要求；全部原样保留在 [450 批原始计时及门槛结果](q2-activation-lut-2026-10-30.json)，不据此报告确定的 VRAM 退化幅度。漂移合格的混合场景回退，全主存映射场景没有一致达到门槛的收益。因此不接入生产，也不追加模型计时；此结论仅限这两种全局内存表实现。

两版共通过 2016 个 eager 完整链 bank/形状检查、90 个 graph 输出检查，以及全部 256 个 Q2 编码字节的 CPU 边界自检；完整输出、Q8 字节、guard、未用输出和空调用要求均通过。检查器还核对实际路径见证，避免回退原路径被记成候选。源码见[候选内核片段](q2-activation-lut-2026-10-30.hpp)，复核入口如下，读取已有产物、不运行 GPU：

```powershell
python -X utf8 bench/results/a770/check_q2_activation_lut_20261030.py --check
python -X utf8 bench/results/a770/check_q2_activation_lut_20261030.py --check --pair
```

另一项已完成的[正确 U2×S8 DPAS 完整链](u2s8-layout-2026-10-29.md)也没有独立确认的提速：它证明 A770 原生 U2 指令和正确打包可用，但不支持“原生 INT2 必然更快”。继续工作的优先级应由完整链和模型收益决定。

## 来源版本

外部来源于 2026-10-30 读取。以下链接固定到实际读取的提交；本地快照及 SHA256 保存在隔离研究目录 `build-quantization-research-20261030/sources.json`。外部作者的 NVIDIA/B70/ARM 速度均未计为 A770 实测。

[qtip]: https://github.com/Cornell-RelaxML/qtip/blob/e90c6688c8dfae326a3a81b5eb032db7c6680ec0/README.md
[exl3]: https://github.com/turboderp-org/exllamav3/blob/cdc785db89e263e65e5849cb53e1ada156411414/README.md
[exl3xpu]: https://github.com/sybil-solutions/exl3xpu/blob/15ded2f3add148c4db3c900cba7de878238f53bc/docs/DESIGN.md
[yaqa]: https://github.com/Cornell-RelaxML/yaqa-quantization/blob/f9508723251ad839f0162326569f17fe70486fcc/README.md
[hqq]: https://github.com/mobiusml/hqq/blob/d88a488ec8aa2d58362ef2038a52bca862db2e74/Readme.md
[gemlite]: https://github.com/mobiusml/gemlite/blob/f649e80e3014bc7ca8195573683c2c440f3bd94e/README.md
[aqlm]: https://github.com/Vahe1994/AQLM/blob/e79a896ed6656fe4ed06193d42d004e7d0bbdbb2/README.md
[quip]: https://github.com/Cornell-RelaxML/quip-sharp/blob/1d8f873e9a2a8b86b12bb1064c312c5689b77d98/README.md
[flute]: https://github.com/HanGuo97/flute/blob/9eb83a12d56949bbe7fe9c836ba97a67bd1e3761/README.md
[marlin]: https://github.com/IST-DASLab/marlin/blob/1f25790bdd49fba53106164a24666dade68d7c90/README.md
[bitblas]: https://github.com/microsoft/BitBLAS/blob/0c51e34ac52747aa3f41f8157a253c2d489d2b11/README.md
[tmac]: https://github.com/microsoft/T-MAC/blob/7042f8f73330bd083bc1e4bc5ccb3f88a4904aee/README.md
