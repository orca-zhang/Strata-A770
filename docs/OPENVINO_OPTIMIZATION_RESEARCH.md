# OpenVINO 优化调研：Strata 可以借鉴什么

2026-10-31。核对 OpenVINO 官方源码及仓库内文档，固定版本为 `4964038cf75b6b4273b2bc57a60e95f5f2995b26`；OpenVINO GenAI 为 `49237dab013899bc4cd40bf436aca8cf1bb18337`，两者提交日期均为 2026-10-30。这是开发分支快照，不代表所有能力已随正式版发布。初始调研仅做源码与既有测量分析，没有运行 OpenVINO/Strata 对照测速或转换模型；后续 FP16 源码接入与复验另见下方进展。

后续范围更新：用户已排除 INT8 优化路线。[临时 INT8 实验](../bench/results/a770/openvino-prefill-2026-10-31.md)的接入与未完成测试已撤回，保留历史记录；接下来优先考虑保持现有精度和 IQ 存储的 FP16 分组调度、融合及访存优化。以下 INT8 内容作为调研证据保留，不再作为实施建议。

FP16 后续进展见[双专家调度验证](../bench/results/a770/openvino-fp16-pair-2026-10-31.md)：隔离模型验证后，冻结当时源码的完整SYCL模型复验也已完成。Code预填充吞吐两轮提高21.299%/21.500%，Prose提高20.412%/19.476%，整请求耗时减少约13.7%–14.9%；完整构建、1004项公开检查、完整输出、固定目标评分、交错历史及采样显存检查通过。独立可选配置`strata-iq2_xs-fp16-pair.json`已通过实际服务器引擎、完整输出及所加载运行时DLL检查，可用`python -m serve.server --engine strata --config strata-iq2_xs-fp16-pair.json`启动。该冻结SYCL产物和配置作为历史对照保留；生成吞吐变化为噪声，35 tok/s未达到。

[Orca L0 移植与完整模型验收](../bench/results/a770/orca-fp16-pair-2026-10-31.md)也已完成：保留原生 M4/NSG8、batch8、IQ 存储和传输格式，在 A770 上 Code2K 预填充吞吐两轮提高16.384%/15.978%，Prose2K提高15.729%/15.706%，整请求耗时减少约11.8%–12.6%。22个功能请求、49个计时请求、368个配对固定目标、6项CTest和原生模块审计通过，未确认生成速度收益。首轮Prose因3.404%基线漂移被拒绝，追加完整一轮后按原门槛通过，原记录保留。两侧均使用校准后的3072MiB预留，最低1Hz显存空闲1337MiB；自动缓存实际容量有变化，不能视为相同驻留容量下的纯算子收益，也不代表相对原1536MiB配置的收益。主配置已由并行任务将min-p改为0.5，本轮实测为0.8，因此保留新的主配置。独立原生配置`strata-iq2_xs-orca-fp16-pair.json`已通过实际服务器引擎的完整Short16/Code128/Prose128输出及原生模块审计，可用`python -m serve.server --engine strata --config strata-iq2_xs-orca-fp16-pair.json`启动；源码开关缺省关闭，该配置显式开启。

生成侧另对现有MTP成本窗口做了同EXE开/关诊断：544个生成token完整输出相同，Code/Prose窗口直方图及接受/提议总数也相同，未提供减少验证工作的证据，未推进正式性能组。其无预热冷请求读数不用于更新上述生成速度。

FP16预填充优化已完成[逐项交付核对](../bench/results/a770/openvino-fp16-pair-2026-10-31.md#本项交付核对)：1945份来源/结果文件及各阶段日志、当前实现与实测快照、独立收益、质量、显存和可选配置全部通过。生成35 tok/s另行记录，仍未达到；原性能审计和失败标志不改写。

主要对照本工作区的 A770 16 GiB、Xeon E5-2696 v4、PCIe 3.0、Windows 路径。当前 IQ2_XS pack 的 GU 实际包含 IQ2_S / IQ2_XXS / IQ1_M，Down 为 Q2_0。工作区源码、历史冻结 EXE、Orca 候选分别判断，源码中存在某个开关不表示当前运行产物已经采用。

**最值得借鉴的是按计算规模选择执行路径，并把专家路由、权重格式和完整 MLP 一起优化。就现有证据而言，prefill 比继续微调 decode 小算子更有机会取得较大收益。若目标专指生成 tok/s，则优先减少主存专家访问，不能拿 prefill 的倍数回答。**

| 优先顺序 | 建议 | 收益作用范围 | 当前证据与判断 |
| --- | --- | --- | --- |
| 已排除 | 保留低位载荷，高行数专家临时转 INT8/XMX，按实际行数分派 | prefill、TTFT | 用户决定不采用此路线；历史算子与质量证据保留，无完整模型性能验收 |
| 1 | 对不同专家权重做真正的 grouped GEMM | prefill、TTFT | OpenVINO 默认启用；本地 FP16 双专家在冻结 SYCL 模型确认约19.5%–21.5%，原生 Orca 安全配置确认约15.7%–16.4%的预填充吞吐收益；更大分组尚未验收 |
| 2 | 按执行成本管理专家驻留，核对本轮工作集能否同时容纳 | decode | 主存访问限制有本地计数支持；OpenVINO 的容量检查值得参考，直接改 LRU 没有收益证据 |
| 3 | 融合较大的 MoE 边界，包括共享专家与结果合并 | decode，部分 prefill | OpenVINO 有实际实现；本地小融合多数收益不足，优先级低于前两项 |
| 按场景 | 统一预算下复用 KV、递归状态及 MTP 状态 | 重复前缀、多轮 TTFT、并发吞吐 | 原理相关，本地已有会话缓存；先找缺口，避免重建 |
| 长上下文再评估 | 分别选择 K/V 的量化轴与精度 | 长上下文容量、decode | OpenVINO 有 INT4 KV 实现与外部数据；不能直接推出本地 2K 更快 |

**OpenVINO 重点做了哪些功课**

它的优化分布在两层。Runtime/GPU plugin 负责把图变成适合硬件的操作：融合、权重布局转换、中间内存复用、压缩矩阵计算、多内核选择与执行参数缓存。GenAI 再处理请求调度：PagedAttention、连续批处理、分块 prefill、前缀复用和推测解码。前者主要减少一次计算的成本，后者还会改善并发吞吐、重复工作和显存容量，不能混成一个加速数字。[图优化][ov-graph]、[内核选择][ov-kernels]、[连续批处理][genai-cb]都有对应实现说明。

对于当前 Strata，最有参考价值的是下面这些具体实现。

**1. Prefill 和 decode 分开，进一步按实际工作规模分派**

[MoE 实现][ov-moe]明确保留四条路径：`batched_gemv`、`micro_gemm`、`grouped_gemm`、`per_expert_onednn`。配置中 `moe_batched_gemv_threshold` 默认为 32，按送入 MoE 的总 token 数选择 decode/prefill 路径；`moe_use_grouped_gemm_prefill` 默认 true，并优先于 micro-GEMM。[默认配置][ov-options]与实际选择逻辑一致。

这里有两个限制：

- 32 是 OpenVINO 的 MoE 输入 token 阈值，**不是每专家行数阈值**。Strata 的专家 `ne`、MTP 窗口 T、整段 prompt 长度是三个不同量，不能直接移植这个数字。
- 该 batched GEMV 支持列表是 u4/i4/u8/i8/f16；u3 转 grouped 路径。micro-GEMM 的选择条件为 Xe2+ 和 u4/i4，A770/DG2 不满足此条件。不能由“Intel GPU 支持”推导本机能走最快的实现。

对 Strata 最直接的借鉴，是按每专家实际行数、格式和驻留方式，选择已有压缩 GEMV 或 FP16 XMX，并评估不同专家间的分组调度。保持 IQ 权重在 RAM/PCIe 上的低位格式，进入设备后再选择计算形式；没有必要先把整个模型改成 INT4。

类似的存储与计算分工，也见于它的 dense linear：[动态量化变换][ov-dq]为压缩 FC 插入激活量化，并按 group size、权重类型和 zero point 决定补偿数据；[FC 内核][ov-fc]有压缩权重解包、scale/zero-point 处理与分块复用。变换还明确避免对 sub-byte 权重计算内核不用的预归约。这些都是减少实际数据转换和重复工作的细节，不等于当前 IQ 专家可以直接使用其 INT4 内核。

本地[已有完整链测量](../bench/results/a770/faster-quantization-2026-10-30.md)给出如下独立确认结果，基线为该实验的 FP16/XMX 链，计入 pinned H2D、权重/激活转换、GU、激活和 Down：

| GU 格式 / 临时布局 | 每专家行数 512 / 1024 / 2048 | 完整链吞吐提升 |
| --- | --- | --- |
| IQ2_S / column | 512 / 1024 / 2048 | 20.518% / 32.895% / 40.180% |
| IQ2_XXS / column | 512 / 1024 / 2048 | 20.228% / 33.034% / 40.202% |
| IQ1_M / VNNI | 512 / 1024 / 2048 | 46.358% / 70.507% / 86.828% |

这些是本机既有算子结果，不是 OpenVINO 的性能，也不是模型 TTFT 收益。旧[热点采集](../bench/results/a770/u4-expert-replay-2026-10-27.md)的六个最忙专家有 349/510/668/978/1971/1995 行，说明真实输入中确实有值得研究的高行数专家，但只采了三个层的最忙专家，不能代表全模型覆盖率。

最小下一步是让已有完整链覆盖真实行数和输入，并确认真实 prefill 分派，不先另写低位格式或通用后端。小窗口 decode 保留原压缩 grouped GEMV：本地[INT8 对原生 grouped GEMV](../bench/results/a770/int8-native-grouped-2026-10-29.md)出现瞬时输出错误，原型已拒绝，不能把上述 prefill 数字推广到 decode。

**2. 真正把不同专家的 GEMM 放进一次调用**

OpenVINO 的 grouped 路径先按专家 gather token，用 row offsets 描述每个专家的输入范围，权重按 `[E,K,N]` 描述，随后对全部专家分别做 up、gate、down 的 grouped matmul，再按 row lookup 和 router 权重 scatter/reduce。gate matmul 的 post-ops 包含激活和与 up 的乘法，省掉独立 SwiGLU kernel。[对应实现][ov-grouped]还传入实际最大每专家行数 `DNNL_ARG_HINT_MAX_GROUP_SIZE`，避免每个专家都按整个 prompt 的规模分派，并缓存地址未变化时的执行参数。

这与本地三个名字相近的能力需要分清：

- [SYCL `f16_expert`](../sycl/src/prefill/gemm.dp.cpp)保留单专家 GU → SwiGLU → Down 回退；`STRATA_PREFILL_XMX_EXPERT_BATCH` 控制每八位专家才等待。上层预填充分派现在可用 `STRATA_PREFILL_FP16_PAIR=1` 将两个不同权重专家合入同一 GU/Down 网格。
- [Orca `ProductBatch`](../sycl/orca/gemm.cpp)仍要求 `first.w == r.w`，适用于同权重的多个输入；新增原生 `fp16_pair` 路径单独处理不同权重，保留 Orca 的 M4 布局和 batch8 完成边界。
- [原生 `native_expert_grouped`](../sycl/src/kernels/cuda/iq_kernels.dp.cpp)已经在 decode 对多个专家和少量 token 分组；它不是上述大行数、异构行数的 XMX grouped GEMM。

因此这是有实际代码差距的新方向：在设备上已经就绪的少量专家间合并矩阵调度，减少小 GEMM 的提交与低效执行。借鉴 row offsets、按真实行数分派和地址稳定时复用参数即可，不必引入整个 OpenVINO。

已先用**两个专家**验证，复用现有两槽反量化 ring，完整结果见上方后续进展。按当前固定尺寸，FP16 GU+Down 每专家需要 `(1280×2560 + 2560×640)×2 = 9.375 MiB` 权重；同时展开八个就要 75 MiB，比两槽多 56.25 MiB，还未计激活与输出。全展开会挤压专家缓存，不能只计算 GEMM 时间。原子 scatter、求和顺序和 FP16 中间舍入也须核对；OpenVINO 的不同专家实现不保证复现本地数值边界。

OpenVINO 自己也保留了一个有用反例：GPU mask generation 虽省主机同步，但 host 看不到真实最大每专家行数，可能导致过量分派。源码注释记录某 Qwen3.6-35B-A3B/int3 的 25-token 场景为 452 对 290 ms；未注明完整硬件条件，不能拿该数字预测本机。它说明应比较总成本，不能把“全部留在 GPU”当作充分条件。

**3. 专家驻留：学工作集检查和计账，谨慎对待磁盘卸载**

[专家权重提供器][ov-provider]对一次计算先去重，检查所需唯一专家数是否超过 resident capacity；能放下时申请本轮槽位，放不下则回退逐专家执行。其 offload 版本使用 LRU；[常量上传逻辑][ov-partial]只对 routed experts 部分上传，共享专家保持驻留。[计数器][ov-offload]区分 GPU hit/miss、磁盘读取、转置、GPU copy 及 batched/grouped fallback。

有价值的是：**命中率、搬运、布局转换和因容量不足而失去批处理的成本一起计算**。不过 `offload_ratio` 在这里是磁盘卸载比例，缓存按 MoE 实例工作；Strata 已有完整 pinned-host 镜像和跨层专家槽位，不能把两个比例直接对应。

这份 OpenVINO offload 实现逐专家读取文件切片，复制调用使用 blocking 参数，执行路径还保留 `stream.finish()` / oneDNN wait。它证明能运行超显存模型，没有证明优于 Strata 已有 RAM 直读、异步 staging 或缓存。不能据源码中的接口名称宣称存在成熟的异步预取流水线。

本地[主存/显存计数](../bench/results/a770/host-device-metrics-2026-10-28.md)中，算子主存路径系统读取为 8.69–9.38 GB/s，驻留后读取和停顿显著降低；但这不是当前整模型的精确带宽上限。[四种双缓冲分块](A770_MEMORY_PATH.md)均没有重复收益，增加缓存槽数又曾因运行时显存余量不足而被拒绝，因此不建议重新盲扫相同 staging 或缓存容量。

更小的下一步是复用[实际窗口采集](../bench/results/a770/window-reuse-2026-10-29.md)，给**主存未命中专家**补齐真实格式、分块复用和驻留状态的成本归因，再比较驻留名单。Code/Prose 的总重复路由条目约 25.01%/24.43%，但旧记录没有逐条驻留状态，且权重字段为零占位；不能推出 miss 流量也能下降四分之一。旧完全去重热度策略已经变差，不能把“改成唯一专家计数”重新当作新方案。

按字节、复用与换入成本评估驻留收益，是本报告从两边实现提出的候选；**不是 OpenVINO 已实现、也不是本地已确认提速的策略**。对单用户 decode，这是比照搬磁盘 LRU 更贴近瓶颈的研究方向。

**4. 融合完整 MoE 边界，避免仅追求少一个 kernel**

OpenVINO 的 decode batched GEMV 把 gate/up 与激活乘法放进同一阶段，Down 内乘 router 权重，再做按 token 的专家归并。启用[共享专家融合][ov-shared]时，把共享专家作为额外 expert slot，和 routed experts 一起调度；相关数据通路可见[MLP 内核][ov-mlp]。它也有 router 的 softmax/TopK/归一化等图模式融合。

Strata 已有 grouped GU、SwiGLU/Q8、device plan、no-host、graph、descriptor cache 和共享专家专用路径。不能把这些已有能力重新计为 OpenVINO 可带来的新增收益。真正值得检查的是跨“共享专家—路由专家—合并”边界的数据复用和提交，而非再次独立融合 SwiGLU。

本地[eager 采集](../bench/results/a770/decode-gpu-profile-2026-10-28.md)中专家 GU 占累计 kernel 时间约 24.19%，Down 11.33%，SwiGLU/Q8 只有 0.426%。这些比例受 eager 和采集影响，不是正常 graph 的关键路径份额；仍足以提醒小心选错优化对象。近期[共享链与小组合记录](A770_PERFORMANCE.md)多数未过独立 1% 门槛。进一步融合是中等优先级，不能预报大收益。

同步策略也应按路径选择。OpenVINO 的[同步文档][ov-sync]与配置源码显示，使用 oneDNN 时通常设 in-order，非 profiling 模式可依赖队列顺序；profiling 则引入显式事件。它并非一律要求多流或 out-of-order。本地已有 graph 提交优化约 2.7%–2.9% 的独立结果，见[运行时记录](../bench/results/a770/runtime-paths-2026-10-29.md)；这项已存在的收益不能再算一次。

**5. 缓存不仅是 KV：混合注意力和 MTP 必须一起恢复**

GenAI 的[混合缓存管理][genai-hybrid]区分按 token 增长的 KV 和递归状态。前缀缓存开启时，对线性注意力做间隔 checkpoint；默认间隔会参考状态大小，并在一个预算内安排 KV 与线性状态。其[MTP 实现][genai-mtp]还要求 main/draft 的 prefix caching 设置一致，恢复到双方都可复用的同一个前缀位置。

这是针对 GDN/混合架构很有价值的工程细节：不能只恢复 KV，也不能只让 target 命中缓存。Strata 已有会话状态、快照、MTP 和 prompt cache，先核对是否存在状态恢复或预算缺口，再考虑调整 checkpoint 密度；本轮没有发现并复现新的本地缓存 bug。

连续批处理与 chunked prefill 主要面向多请求，前缀命中改善重复请求 TTFT。[推测解码][genai-spec]则要同时检查草稿成本、目标验证时间和每轮接受 token 数。Strata 已有 MTP/lookup，不能仅凭 OpenVINO 也支持这些功能就预报新增收益。若只测 cache=0 的单请求冷前缀，前缀缓存不会提高该测试的速度。

**6. 长上下文 KV 与量化校准，作为后续专项**

OpenVINO 的[INT4 KV 说明][ov-kv]把 PagedAttention 的 K 按 channel 量化、V 按 token 量化；SDPA 路径的 K 不具备相同按 channel 能力。其报告数据分别来自 B580/Linux 上 Llama-3-8B 的容量，以及 B390 集显/9600 MT/s DDR 上 Llama-3.1-8B 的延迟。例如后者 34K 输入下 FP16/INT8/INT4 decode 为 102.67/86.14/80.48 ms/token。这是其他硬件与模型的测量，不是 A770、2K、QSA/GDN 的依据。

本地已研究 K FP16/V packed4 及恢复 FP16 value 的对照，应优先看当前完整请求结果，不能按“4-bit 省容量”认定更快。更低位 KV 的间接收益取决于节省显存能否安全换成有效专家驻留，以及反量化、状态和精度成本。

[NNCF 权重量化文档][ov-quant]提供 group size、混合精度 ratio、scale estimation、AWQ、GPTQ 和敏感度选择。这些对质量校准有参考价值，但标准 INT4 不会自动胜过当前约 2-bit 的 IQ 专家。旧原 BF16→单 U4 专家方案的实际载荷曾增加 83.05%，见[完整链回放](../bench/results/a770/u4-expert-replay-2026-10-27.md)；换格式必须重新算传输和驻留预算。

GDN 也不能只抄 tile：[OpenVINO PagedGatedDeltaNet][ov-gdn]明确写出部分短序列阈值在 Panther Lake、特定头尺寸上测得；还记录 B70 上独立 general kernel 的优势没有兑现到模型，当前对超过 20 Xe cores 的设备使用 short stage。应学习按规模验证和保留回退的做法，不采用其阈值作为 A770 校准值。

**怎样验证“较大收益”**

按用户最新范围，已验证保持 FP16 数学的双专家 grouped 调度，不再推进 INT8 分派或组合。调度仍可能改变累加边界，因此独立检查了数值、质量和完整请求，见上方后续进展。没有为此次调研安装完整新推理栈。

性能报告分别列冷启动、热态 prefill/TTFT、decode 和总请求时间；完整链计入 gather、转换、H2D、激活、Down、scatter，以及额外内存是否损害专家驻留。质量覆盖完整输出、非有限值、固定目标评分和长短交错热历史。Orca 当前 M4 的回归范围与 SYCL 性能基线仍分别保留，见[本地原生后端记录](../sycl/orca/README.md)。

沿用现有[验收口径](A770_PERFORMANCE.md)：单场景独立两轮达到 1% 可保留，绝对变化小于 0.3% 按噪声，基线耗时漂移不超过 3%，运行时显存余量和质量检查须通过。若目标是较大收益，优先级还应看覆盖了多少请求耗时。公式 `总加速比 = 1 / (1 - f + f/s + h)` 中，f 是被优化部分的基线耗时份额，s 是该部分加速，h 是新增开销占原总耗时的比例；即使一半耗时加速 1.4 倍且无新增成本，整体吞吐也只增加约 16.7%。这只是算术示例，不能当成本轮性能预测。

本地[历史同步诊断](../bench/results/a770/hc-u4-attribution-2026-10-27.md)曾把专家链与反量化分别归属到阶段时间的 48.8% 与 12.7%，而 HC-up 仅 1.1%；等待改变了原有重叠，比例不能当成当前正常 graph 的精确占比。它与完整链既有结果共同支持把精力投向专家 prefill，而不是继续围绕小投影的峰值倍数优化。decode 的较大收益仍需从真实主存 miss 成本中建立证据，本次没有找到可以直接移植并承诺提速的 OpenVINO 代码。

[ov-graph]: https://github.com/openvinotoolkit/openvino/blob/4964038cf75b6b4273b2bc57a60e95f5f2995b26/src/plugins/intel_gpu/docs/graph_optimization_passes.md
[ov-kernels]: https://github.com/openvinotoolkit/openvino/blob/4964038cf75b6b4273b2bc57a60e95f5f2995b26/src/plugins/intel_gpu/docs/gpu_kernels.md
[ov-moe]: https://github.com/openvinotoolkit/openvino/blob/4964038cf75b6b4273b2bc57a60e95f5f2995b26/src/plugins/intel_gpu/src/graph/impls/ocl_v2/moe/moe_3gemm_swiglu_opt.cpp#L1031-L1149
[ov-options]: https://github.com/openvinotoolkit/openvino/blob/4964038cf75b6b4273b2bc57a60e95f5f2995b26/src/plugins/intel_gpu/include/intel_gpu/runtime/options.inl#L83-L87
[ov-dq]: https://github.com/openvinotoolkit/openvino/blob/4964038cf75b6b4273b2bc57a60e95f5f2995b26/src/plugins/intel_gpu/src/plugin/transformations/dynamic_quantize_fully_connected.cpp
[ov-fc]: https://github.com/openvinotoolkit/openvino/blob/4964038cf75b6b4273b2bc57a60e95f5f2995b26/src/plugins/intel_gpu/src/kernel_selector/cl_kernels/fully_connected_gpu_bf_tiled.cl
[ov-grouped]: https://github.com/openvinotoolkit/openvino/blob/4964038cf75b6b4273b2bc57a60e95f5f2995b26/src/plugins/intel_gpu/src/graph/impls/ocl_v2/moe/moe_3gemm_swiglu_opt.cpp#L2476-L2765
[ov-provider]: https://github.com/openvinotoolkit/openvino/blob/4964038cf75b6b4273b2bc57a60e95f5f2995b26/src/plugins/intel_gpu/src/graph/impls/ocl_v2/moe/expert_weight_providers.cpp
[ov-partial]: https://github.com/openvinotoolkit/openvino/blob/4964038cf75b6b4273b2bc57a60e95f5f2995b26/src/plugins/intel_gpu/src/plugin/ops/moe_offload_constant.cpp
[ov-offload]: https://github.com/openvinotoolkit/openvino/blob/4964038cf75b6b4273b2bc57a60e95f5f2995b26/src/plugins/intel_gpu/src/graph/impls/ocl_v2/moe/moe_otd_runtime.cpp
[ov-shared]: https://github.com/openvinotoolkit/openvino/blob/4964038cf75b6b4273b2bc57a60e95f5f2995b26/src/plugins/intel_gpu/src/plugin/transformations/fuse_moe_shared_expert.cpp
[ov-mlp]: https://github.com/openvinotoolkit/openvino/blob/4964038cf75b6b4273b2bc57a60e95f5f2995b26/src/plugins/intel_gpu/src/graph/impls/ocl_v2/moe_3gemm_swiglu_mlp.cl
[ov-sync]: https://github.com/openvinotoolkit/openvino/blob/4964038cf75b6b4273b2bc57a60e95f5f2995b26/src/plugins/intel_gpu/docs/gpu_kernel_synchronization.md
[ov-kv]: https://github.com/openvinotoolkit/openvino/blob/4964038cf75b6b4273b2bc57a60e95f5f2995b26/src/plugins/intel_gpu/docs/blog/int4_kv_cache_support_in_2026_2.md
[ov-quant]: https://github.com/openvinotoolkit/openvino/blob/4964038cf75b6b4273b2bc57a60e95f5f2995b26/docs/articles_en/openvino-workflow/model-optimization-guide/weight-compression/4-bit-weight-quantization.rst
[ov-gdn]: https://github.com/openvinotoolkit/openvino/blob/4964038cf75b6b4273b2bc57a60e95f5f2995b26/src/plugins/intel_gpu/src/graph/impls/ocl_v2/paged_gated_delta_net.cpp#L55-L99
[genai-cb]: https://github.com/openvinotoolkit/openvino.genai/blob/49237dab013899bc4cd40bf436aca8cf1bb18337/site/docs/concepts/optimization-techniques/continuous-batching.md
[genai-hybrid]: https://github.com/openvinotoolkit/openvino.genai/blob/49237dab013899bc4cd40bf436aca8cf1bb18337/site/docs/concepts/optimization-techniques/hybrid-attention-models-cache.md
[genai-spec]: https://github.com/openvinotoolkit/openvino.genai/blob/49237dab013899bc4cd40bf436aca8cf1bb18337/site/docs/concepts/optimization-techniques/speculative-decoding.mdx
[genai-mtp]: https://github.com/openvinotoolkit/openvino.genai/blob/49237dab013899bc4cd40bf436aca8cf1bb18337/src/cpp/src/speculative_decoding/continuous_batching/mtp_strategy.cpp#L179-L209
