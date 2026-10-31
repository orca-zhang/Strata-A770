# Orca：其他推理框架的优化线索

调研日期：2026-10-31。目标是 Intel Arc A770 16 GB，未来以 Linux 为主。
本机目前是 Windows，CPU 为 Xeon E5-2696 v4（22 核／44 线程）；AMX、AVX-512
方案不适用于这颗 CPU。没有调研 OpenVINO。

Reddit 的公开搜索接口返回 HTTP 403，未能读取帖子。下面依据项目的原始文档、
固定提交源码和作者基准，不把它们表述为已核实的 Reddit 跑分。
外部项目均未在本机运行；外部倍数不能当作 Orca 的预计收益。

## 最值得做的几件事

| 优先级 | 线索 | 对 Orca 的具体意义 | 当前边界 |
| --- | --- | --- | --- |
| 1 | ExLlamaV3 的按形状选择 GEMV／GEMM 和实测调优 | 按量化格式、N/K、T、权重驻留位置选择 SIMD／XMX、子组宽度、行数和预取深度 | Strata 已有多列 MMVQ、SIMD16/32、Q6_K 宽读取与预取路径；需要测剩余真实形状，不能重写一遍 |
| 1 | Mirage／Luce 的设备端融合、软件流水线 | 从一个固定算子链开始，让下一块权重更早读取，减少中间结果落显存和全局等待 | Orca 的命令列表已减少 CPU 提交；仍需区分 GPU 内部等待。不要直接把整个模型改成常驻 kernel |
| 1 | KTransformers／Luce 的专家分组和异构重叠 | 同一专家的已就绪行聚合，权重读一次服务多行；按 CPU、PCIe、GPU 的完成时间安排命中／未命中任务 | 已有专家缓存、grouped 路径和 DMA 分工。批处理 MTP 的正确性、吞吐及预分配槽位的单请求缓存代价均已测量；不同配置的收益不能相加 |
| 2 | SGLang 的提前一批准备元数据 | 把内存、描述符、缓存查找和下一批已知输入的准备藏在 GPU 计算后面，缩短 host/GPU 往返 | 不能提前知道依赖当前 logits 的真实下一 token；只能准备已知部分或用经过验证的草稿 |
| 2 | ik_llama 的 SIMD 友好权重重排 | 保持量化码和尺度，改变布局，降低错位读取、位拆解和指令数 | 本仓库已有对齐和布局实验；新增填充会影响 PCIe 字节量及专家缓存容量，必须计入 |
| 2 | Luce 的按深度与上下文建模 MTP 成本 | 优化实际提交 token／毫秒，进一步校准草稿长度，而不只提高接受率 | Strata 已有成本策略、前缀观测和定期探测；新增价值是按上下文与驻留状态检查成本是否失配 |

这里的优先级是实验顺序，不是已测收益排序。

## NVIDIA：命令图之后还有什么

[Hazy Research 的 No Bubbles](https://hazyresearch.stanford.edu/blog/2025-05-27-no-bubbles)
使用 Llama3.2-1B、BF16、batch=1、32-token prompt／128-token generation，
在 H100／B200 上比较已经启用 CUDA graphs 的框架。作者报告相对 SGLang 超过
1.5 倍，核心是设备内部的任务流水线，而非单纯消除 Python 或驱动 API。

它把归一化与投影、gate/up/SiLU、输出与残差融合，并在共享内存中回收不再使用的
页面，提前读取后续权重。中间激活分块发布，down projection 可以消费已完成的块，
不必等待整个上游投影。代价是任务计数器、同步和片上资源占用。

[Mirage MPK 固定源码](https://github.com/mirage-project/mirage/blob/2edbcf4035d61b66fc16a1d36ffab41c4c401549/include/mirage/persistent_kernel/persistent_kernel.cuh)
进一步提供设备端 worker／scheduler 队列。
这与 Disruptor 的“就绪序号驱动消费”有可类比之处，但 CPU SPSC 环和 GPU 任务队列
的内存序、可见性、进度保证不同，不能直接共享实现。

[Luce megakernel 源码](https://github.com/PixelML/luce-megakernel/blob/bab084443f14e6ab3ba76f8e8e3f721ad2e56144/kernel.cu)
把 Qwen3.5-0.8B 的 24 层放进一个 cooperative dispatch，使用全网格同步。
[作者结果](https://github.com/PixelML/luce-megakernel/blob/bab084443f14e6ab3ba76f8e8e3f721ad2e56144/RESULTS.md)
报告 RTX 3090 上 BF16 单流 decode 413 tok/s，对照 llama.cpp 267 tok/s。
这是 0.8B、batch=1 的特定实现；文档未固定 llama.cpp 基线提交。
检查脚本比较 prefill+decode 与它自身的逐 token decode，并非独立框架的完整数值对照；
脚本在不匹配时打印 FAIL，仍会继续计时。

**落地实验：**复用现有 shared-expert 单项检查，以一个固定 gate/up→SwiGLU→down
链比较当前录制命令列表与有限融合。记录完整链时间、GRF 使用、spill、SLM、设备等待，
并覆盖 T=1/4/8。数值过关后才进入整模型 A/B/A。
跨 work-group 自旋依赖必须有可证明的进度条件；不能假设所有组同时驻留。
先使用不需要全网格自旋的融合边界；常驻调度是后续独立实验。

本机 Level Zero SDK 1.33.1 已定义 cooperative-kernel 队列能力标志、
`zeKernelSuggestMaxCooperativeGroupCount` 和 `zeCommandListAppendLaunchCooperativeKernel`。
因此后续原型可先查原生能力和实际 kernel 的组数限制；API 存在不代表 A770 当前驱动支持。
Orca 现在使用普通 launch，尚未验证 cooperative 路径。现有
`ORCA_L0_KERNEL_RESOURCES=1` 可报告子组、SLM、private/spill 字节；GRF 分配还需要看
编译器输出／生成代码。查询和诊断放在初始化或单项测试，性能比较关闭诊断。

## CUDA／ROCm：小批量未必该用矩阵单元

[ExLlamaV3 的派发代码](https://github.com/turboderp-org/exllamav3/blob/cdc785db89e263e65e5849cb53e1ada156411414/exllamav3/exllamav3_ext/quant/exl3_gemm.cu)
区分小 M 的 GEMV、多行 GEMV 与较大 GEMM；RDNA 路径明确对 M=1..8 尝试多行
GEMV。GEMM 的调优键包含形状、位宽、设备、输出格式等，候选会检查资源与形状约束。
这支持“按实测分工”，不支持“标量一律改成大 GEMM”。

其 ROCm 路径还记录了实际设备驻留能力与 occupancy API 不一致的情况，并对使用
设备屏障的网格作保守限制。这对 Xe 的提醒很具体：更大 tile、更多寄存器、更深预取
都有可能降低并发度；需要检查生成代码和硬件运行结果。

**落地实验：**围绕真实 IQ4_XS／IQ3_S／Q6_K 和 shared-expert 形状建立小型离线
选择表，分别测试设备内存与映射主机内存。测量包括输入转换、gather/scatter、完整
专家链和消费者；保持现有数值顺序，避免只用一个热缓存 GEMV 判断整模型收益。
已有预取、repack 的失败／无显著收益结果应保留为基线，见
[A770 性能记录](A770_PERFORMANCE.md)。

## CPU／GPU 异构：把同一权重服务的行凑在一起

[KTransformers 专家调度说明](https://github.com/kvcache-ai/ktransformers/blob/c9873312643b615a717832946fba0346e69ab035/doc/en/kt-kernel/experts-sched-Tutorial.md)
比较 uniform、frequency 和动态专家放置。在其 4×4090、Xeon Gold 6454S、
Qwen3-Next-80B-A3B-FP8 配置下，10% GPU 专家占比的报告吞吐从 uniform 56.57
提升到动态策略 70.22 tok/s。这是该混合配置的结果。
[线程池源码](https://github.com/kvcache-ai/ktransformers/blob/c9873312643b615a717832946fba0346e69ab035/kt-kernel/cpu_backend/worker_pool.cpp)
使用 NUMA 绑定、线程本地 worker 编号及异步任务分发。

[Luce 的同模型说明](https://github.com/Luce-Org/lucebox/blob/502b094ac3cdb9ed8b9250f167f84b78602906f8/server/docs/QWEN4EXP.md)
值得单独跟踪：Qwen3.8-Flash-Next 的 10 专家、GDN／QSA、MTP 和多槽位均有说明。
在 R9700＋Strix Halo 上，prompt chunks 在 2..4 个 stream 中流水化，一批专家和另一批
注意力重叠；decode 则将热专家放到独显，其余留在大内存设备。
其 UD-Q4_K_XL、多设备、MTP 结果与本机 IQ2_XS、A770 不能直接横比。

另一个可测点是它的 MTP 只保留 64,000 个草稿词表项，主模型仍负责验证。
Strata 已支持 `draft_vocab.bin` 子集及多语言子集；应复用现有能力，比较草稿 head
成本、罕见 token 接受率和总时间。本轮模型基准使用完整草稿词表，不能把子集收益
算进已有报告。子集不会自动保证采样语义，仍需遵守现有 verifier 的输出合同。

**落地实验：**先统计一个 verify window 中每个专家得到几行、唯一专家数和权重实际读取量。
同一专家且相同布局的行可以 gather 成矩阵；不同权重的独立算子适合 grouped kernel
或分支调度，硬拼为含大量零值的巨型矩阵会增加工作。
Orca 默认已有兼容 FP16/BF16 产品聚合。新移植的 HTTP 连续批处理支持独立槽位与采样历史；
Windows A770 上的串行／并发请求在 greedy 和带惩罚的固定种子采样下均逐 token 一致，
详见 [HTTP 批处理检查](../sycl/orca/batch-http-a770-windows-2026-10-31.json)。
另有四请求、每请求 64 token 的普通批处理对照：包含准入和 prefill 后，三组 A/B/A 吞吐均比
串行 MTP 低 2.82%～2.97%，基线漂移低于 0.23%。配置为 `--expert-cache 4000` 槽位预算
（运行时为 4191 个驻留专家、5760 MiB）、2048 上下文、256-token prefill 分块、
固定放置和 CPU 精确模式，不能外推到其他部署配置，详见
[完整样本](../sycl/orca/batch-groups-a770-windows-2026-10-31.json)。
随后在相同配置下启用 `--batch 4 --batch-mtp`，与串行 MTP 的三组 A/B/A 对照提升
10.06%～10.40%（中位数 10.13%，聚合 14.69 tok/s），输出 token 相同，基线漂移低于 0.30%，
详见 [MTP 批处理样本](../sycl/orca/batch-groups-mtp-a770-windows-2026-10-31.json)。两侧均预分配
四个槽位，这个对照没有测量槽位本身的资源代价。
另一次自动专家缓存、8192 上下文、2048-token prefill 分块的两槽位 A/B/A 中，预分配槽位
挤占了 701 MiB 专家缓存（6184 个驻留专家降到 5670 个）；单请求 Short／Code2K 的 decode
吞吐分别下降 3.49%／5.06%，decode 基线漂移低于 0.45%。24 次生成、1536 个 token 及草稿工作量
全部一致，详见 [槽位资源代价](../sycl/orca/batch-memory-a770-windows-2026-10-31.json)。
这组测试没有并发 BGEN，且上下文与缓存配置不同，不能与四请求吞吐直接相加；服务批量默认值仍为 0。
没有就绪行时，不增加等待时间来凑批。

[SGLang overlap scheduler](https://lmsys.org/blog/2024-12-04-sglang-v0-4/)
报告通过提前一批准备元数据约提高 1.1 倍吞吐，收益在小模型或较大张量并行时更明显。
它使用 future tokens 和事件管理依赖。可复用的是隐藏 CPU 准备时间的原则；
文中 cache-aware router、多卡 MLA、结构化输出的倍数分别属于其他场景。

## SIMD 布局与 Metal 的经验

[ik_llama 的重排源码](https://github.com/ikawrakow/ik_llama.cpp/blob/13cffc006b82e03e4d89cbb8c7327f0255ae3d18/ggml/src/iqk/iqk_quantize.cpp)
包含将多行量化码、尺度交错排列的实现，例如 `repack_q8_0`，并由
[对应 SIMD 内核](https://github.com/ikawrakow/ik_llama.cpp/blob/13cffc006b82e03e4d89cbb8c7327f0255ae3d18/ggml/src/iqk/iqk_gemm_kquants.cpp)
消费。布局优化可以保持量化值；格式转换／重新量化要另行评估。
对 Orca 应测 Xe 读取和解包成本，不能直接沿用 CPU 的交错宽度。

[Flash-MoE 的 Metal 内核](https://github.com/danveloper/flash-moe/blob/3601d41882a346d85041f7955c66429d248e8cd9/metal_infer/shaders.metal)
在 affine 4-bit GEMV 中预计算 scale×x、bias×x，再使用 FMA。
这会改变浮点结合顺序，数学等价不保证逐位相同；本机已有整数 dot／非线性 IQ
格式，不能机械套用这条公式。

该项目的速度还有一个决定性条件：
[infer.m](https://github.com/danveloper/flash-moe/blob/3601d41882a346d85041f7955c66429d248e8cd9/metal_infer/infer.m#L11)
明确写着原来每 token 10 个专家，为速度使用 K=4，主程序默认值也是 4。
所以其 397B、4.36 tok/s 的作者报告不能作为完整 10 专家推理的速度对照。
其文档还记录了 LZ4 压缩、路由预测、预取及自旋等待变慢的实验，适合作为待复验
假设。Apple 统一内存的 SSD／GPU 争用结论不能直接套用到独显 PCIe。

[PowerInfer](https://github.com/SJTU-IPADS/PowerInfer/blob/8bd56d69906c9d2dba4d3bf6899763401e01a9a4/README.md)
还有一种更激进的思路：利用 ReLU 稀疏激活，将热神经元放 GPU、冷神经元放 CPU，
用预测器和稀疏算子减少工作。其 11 倍级展示使用 Falcon(ReLU)-40B 等配套模型；
文档明确说明普通 llama.cpp 权重的兼容路径没有这类收益。
Qwen 当前的 SwiGLU 不能直接假定具有相同的精确零值结构；这属于模型／算法一起
设计的方向，不作为当前 Orca 保持现有模型行为的默认优化。

## API 层继续怎么压

API 层仍可通过减少提交次数、消除不必要的等待和往返、复用参数与描述符改善性能。
设备端任务推进也减少了 host 参与，是更深入的一种控制方式。
仅更换一个 API 函数入口，与合并一批工作或删除同步点的收益来源不同。

本机已有 [API 单项报告](../orca-l0/demo/api-submission-a770-windows-2026-10-31.json)：
32 个独立列表一起提交的完整窗口，对 1 kernel/list 为 4.3..5.0 倍，对 16 kernels/list
为 1.83..2.15 倍；这是单项窗口，不是模型 tok/s。
400-kernel 窗口中 DDI 表与导出入口的差异小于 0.5%，不能据此推导 Linux 驱动收益。
当前 Strata 适配器仍逐图提交，新增批量 replay API 尚未自动变成整模型收益。

稳定查询也应移出重复路径：驱动版本、扩展清单、函数入口、内存模块和适配器身份
随 Runtime／引擎生命周期保存。可用显存、当前预算、事件和 fence 状态保持实时读取。
硬件或驱动重建时重新发现能力，避免把旧句柄带入新的上下文。

## 验证方式

每个候选先留一个可运行的完整链检查，再测模型。固定权重、prompt、上下文、warm-up、
输出预算和缓存状态，使用 A/B/A 或交错配对，保留全部样本与基线漂移。
同一调度工作的比较要求 tokens、accepted/offered、reused、finish 都相同。
MTP 策略允许 accepted/offered 改变，但完整 greedy tokens、generated、reused 和 finish
必须相同。分别报告 TTFT、单请求 decode、总吞吐和总耗时。

例如本机 `spec-min-p=0.8` 虽提高草稿接受率，Short／Code2K 的 decode 吞吐却分别
下降约 6.91%／4.03%，见 [完整结果](../sycl/orca/mtp-minp08-a770-windows-2026-10-31.json)。
Strata 已有 `DraftPolicy::choose_mtp` 成本策略；接下来应检查成本分桶与草稿深度，
不凭接受率改默认门槛。

本次八个仓库的固定提交、源码 SHA256 和访问结果保留在本地
`build-orca-native-20261029/cross-platform-research-1793380755804309300/`。
这些线索尚无新增 A770 提速结论，也没有改变生产数值路径。
