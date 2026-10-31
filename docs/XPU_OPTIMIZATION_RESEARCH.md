# Intel XPU 优化调研：对当前 A770 分支的价值

调研日期：2026-10-28，执行状态更新至10月29日。阅读了用户提供的六个入口、相关 PR、内核源码和本地实验记录，随后在隔离构建中测量候选；本轮未修改生产推理实现或部署候选。下文外部数字均为项目作者报告，本地测量与限制见[执行记录](../bench/results/a770/xpu-optimization-2026-10-28.md)。

当前目标是 A770 16 GB、Windows、PCIe 3.0、IQ2_XS pack 的 Code/Prose 2K 输入和完整 128 输出。该 pack 的专家实际使用 IQ2_S / IQ2_XXS / IQ1_M GU、Q2_0 Down，不能按统一 INT4 或 FP8 模型分析。测量边界见 [A770_PERFORMANCE.md](A770_PERFORMANCE.md)。

## 当前续接：先验收标准 FP16，再决定 V4 的用途

当前目标明确采用单场景规则：吞吐变化绝对值≤0.3%为噪声，单场景稳定提升至少1%即有价值，不要求联合通过。其他场景的回退、配置与输出差异全部保留；正确性、质量、相邻基线耗时漂移≤3%、独立复测及观测空闲显存≥1024MiB仍需检查。上一轮续接误将旧联合门槛恢复为当前要求，现予纠正，不以几何平均、三分之二或2%回退上限否决单场景收益。

已重跑FP16/V4的80次原始响应审计与live缓存功能审计，沿用现有逐场景独立检查。这是CPU复核，没有新增GPU性能或质量测量。三场景为Short、Code2K128、Prose2K128，旧漂移超限配对仍排除。

| 已有完整模型对照 | 单场景初筛 / 独立确认吞吐变化 | 其他场景 | 当前判定 |
| --- | ---: | --- | --- |
| 标准FP16相对V4，Short | +7.2041% / +4.3369% | Code −0.5455% / −0.4713% | Short性能确认；实际KV质量仍待测 |
| 标准FP16相对V4，Prose2K128 | +16.2135% / +12.5355% | 同上 | Prose性能确认，不需要第二组联合矩阵；实际KV质量仍待测 |
| Q4草稿相对原草稿，V4下Code2K128 | +4.1854% / +4.0257% | V4 Short/Prose回退；标准FP16三个场景均回退，Code −2.6130% / −2.4471% | 仅确认V4下Code的贪心收益；不采用为FP16性能优化，采样范围仍单列 |
| 共享gate/up合并相对原路径，V4及FP16下 | 两种KV配置均无场景独立达到1% | FP16 Short +1.7556% / +0.0810%，Code +0.3371% / +0.4834%，Prose +0.1551% / +0.4976% | 不采用为整模型性能优化；算子22%–66%的证据保留 |
| 双方prompt-cache6的交替多会话，FP16+RAM对V4 live | TTFT延迟降低97.1245% / 97.2112% | V4实际均miss，FP16恢复1950/1952 tokens | 五引擎35次请求及原始日志审计通过；是完整缓存策略对照，非decode或KV内核收益 |

FP16最新完整矩阵的Short/Code/Prose分别为+4.3369%/−0.4713%/+12.5355%，最大相邻基线漂移0.2346%。同格式完整输出重复，但不同KV格式的输出不同；不能据此直接判断质量优劣，或将整请求耗时差归因于单个KV内核。FP16/V4方向均尚无独立确认的TTFT收益。当前6297槽、spec4/min-p0.5和访问顺序也不代替其他历史或生产配置的验收。

V4目前有保留为容量选项的实测依据。8K/32K分配下两种格式均保留6297专家槽；64K分配下FP16实际降至6000槽、READY空闲899MiB，因低于1024MiB门槛未发请求，V4仍保留6297槽、READY空闲1097MiB并完成Short16。六个独立引擎及原始协议/布局/来源哈希审计通过；五次实际Short输出均匹配各自8K格式参考。K+V布局差额为8K的77.1875MiB、32K的308.75MiB、64K的617.5MiB，不等于进程峰值节省。FP16布局来自几何计算，V4来自13份实际分配日志；专家槽数和READY来自各次初始化。这不是64K长请求质量/速度或公平槽数的性能对照。原有V4对V8的约2%收益不能证明V4优于标准FP16。

决定是否改变默认仍缺实际KV续写评分及当前热历史质量，以及完整长请求的质量/速度。已有368目标评分继续排队，双方prompt-cache6的公平多轮对照已完成并审计通过，支持已测交替多会话下使用FP16+RAM；8K/32K/64K分配探测已完成，但它只发Short16，不能代替完整长请求。标准FP16共享合并的完整对照已结束，无场景独立达到1%；高M整数专家链九格均已独立确认，下一步补六个非对齐热点行数。生产默认未变，V4保留为容量选项。

## 结论与顺序

| 顺序 | 值得做的事 | 最小验证 | 当前判断 |
| --- | --- | --- | --- |
| P0 | 对照上游的图重放、跨队列所有权和状态边界修复，继续定位间歇性非有限值 | 复用发生失败的精确构建和已有诊断，补查实际缓冲依赖；不能靠增加等待后的通过宣称修复 | 正确性前提，尚未发现与上游相同的根因 |
| P1 | 将标准FP16 KV作为候选优先基线，评估V4是否只需保留为容量选项 | 核验已测贪心吞吐，再补实际KV质量、公平缓存和更大分配容量；分别记录专家槽数与内存 | FP16的Short/Prose解码独立确认+4.337%/+12.535%，Code回退0.471%；TTFT无确认收益，尚不改生产默认 |
| P1 | 合并共享专家的 gate/up 提交 | 相同权重、Q8 输入、点积次序和两个输出，仅合并 launch；先测真实 2560→640、T1–8 | 19个算子形状独立确认+22.002%至+66.142%；V4与FP16两组整模型对照均无场景独立达到1%，不采用 |
| P1 | 继续做 prefill 专家完整链的阶段分派 | 先验证已有 XMX chain / batch 能力，再决定是否需要新的整数计算路径；计入 H2D、转换和临时空间 | T16/40/64中35/54格独立确认+2.546%至+46.825%，其余回退/不足门槛保留；原T128/256三格及新增高M九格也已确认，尚无模型TTFT结论 |
| P2 | 根据实际行数和驻留方式选择小 M 内核 | 统计每个专家的真实 token 数，分别测 VRAM、pinned-host、混合路径，保留现有 IQ 格式 | GEMV完整链的T1有8格确认，T8九格回退；FP16整模型20次功能请求已审计，候选重复但2K输出改变，须先过续写质量再计时 |
| P2 | 降低 MTP 草稿成本，联合评估接受率与轮次时间 | 先使用已有词表裁剪、Q8 专家和成本窗口能力；仅对仍昂贵的草稿投影尝试更低位权重 | 不新增第二套自适应控制器；有损草稿须验证采样语义 |
| P3 | 优化多轮会话的 RAM 前缀恢复与预算 | 先测现有 conversation cache 的命中、恢复字节数、TTFT 和显存占用 | FP16缓存对FP16重算降低97.18%；对V4重算降低97.12%，均已独立确认；单轮及KV质量继续测 |

## 1. routed/shared 融合：借鉴执行方式，先缩小范围

[vLLM #61163](https://github.com/vllm-project/vllm/pull/61163) 和配套 [kernels #670](https://github.com/vllm-project/vllm-xpu-kernels/pull/670) 截至调研时均未合并。#670 源自 [#628](https://github.com/vllm-project/vllm-xpu-kernels/pull/628)，不是三份相互独立的收益。

它将 routed experts 和一个 sigmoid-gated shared expert 放进同一算子，仍提交六个内核：tile map、gather、合并 GU、激活/共享门控、合并 Down、最终 combine。持久 workspace 由调用方提供，router 和 TP reduction 保持在外部。**一个算子不等于一个 GPU kernel。**

作者的历史配对结果：B70 / Xe2、TP2、Qwen3.6-35B-A3B、在线 per-tensor FP8 权重、32K 输入 / 256 输出，FP16、B1、compile+graph 的 TPOT 从 9.605 降至 7.665 ms，降低 20.2%。最新修订的 7.777 ms 是独立 smoke，不能与旧基线组成新收益。完整历史矩阵未在最新修订重跑。

限制与本地差异：

- vLLM pass 针对 H=2048、256 experts、top-k=8，以及特定 routed/shared 中间维度；本地主要为 H=2560、512 experts、top-k=10，量化格式也不同。
- 配套内核已将 H/I 对齐约束收紧至 32 的倍数，并验证 workspace、设备、别名和对齐；不应按旧 #628 的较宽约束移植。
- 本地验证器先执行 shared expert，再等待专家计划并执行 routed expert。整块合并可能推迟 shared 工作，减少它覆盖计划等待的机会，须将调度变化一起计时。
- 本地已有共享/路由输入 Q8 去重、SwiGLU/Q8 融合和批量 combine，重复实施这些功能不构成新优化。

最小候选在 [shared_expert.dp.cpp](../sycl/src/kernels/cuda/shared_expert.dp.cpp)：`shared_expert_multi` 对相同输入连续调用两次 `native_mmvq`，分别算 gate 和 up。相同格式、相同形状时，可以让同一 grid 的不同区段读取两份原始权重，并写回原来两个输出，不必先重排全部模型权重。只在这一对调用确有重复收益后，再考虑共享门控、Down 或 routed/shared 的更大融合。

接入边界在 [verify.cpp](../sycl/src/core/verify.cpp) 的 shared/pre 与 grouped/post 两段。**当前没有证据支持本地约 20% 的收益预期。**

## 2. small M：按专家实际行数分派

[四卡 B70 项目](https://github.com/imryanpurdy/Qwen3.8-27B-4x-Intel-B70s) 的 [`vxk_worktree.patch`](https://github.com/imryanpurdy/Qwen3.8-27B-4x-Intel-B70s/blob/main/kernel/vxk_worktree.patch) 提供 block-FP8 W8A16 的 small-M 内核：M≤40、K/N 128 对齐时专门分派，并跳过其原先 W8A8 路径的动态输入量化。

其 144.2 tok/s 来自四卡 TP4、Qwen3.8-27B、MTP5 和图执行的组合。README 的跨日期 eager/graph 表格不能证明 small-M 内核单项收益，也不能与本地 125B MoE 比速度。

更接近 MoE 的来源是尚未合并的 [kernels #644](https://github.com/vllm-project/vllm-xpu-kernels/pull/644)：针对 INT4 grouped-GEMM 的小行数改走 GEMV。作者报告 B70、Qwen3.6-35B-A3B-GPTQ-Int4、无 MTP、4K context、graph 下，Code/Prose 从 75.8/76.3 到 86.6/87.2 tok/s，约 +14%。它的当前分派上限是 grouped GEMM 输入的 8 行，不应理解成任意八个并发请求。

对本地有价值的是分派依据：同一个验证窗口中，不同专家收到的 token 数不同，重用同一权重的机会也不同。应记录每个 group 的真实行数、矩阵形状、格式、实际驻留位置，再选择原生 dot/GEMV 或适合复用权重的小矩阵路径。

本地 [iq_kernels.dp.cpp](../sycl/src/kernels/cuda/iq_kernels.dp.cpp) 已有 grouped GU/Down 和 XMX 量化读取路径，[native_mmvq.hpp](../sycl/include/strata/kernels/native_mmvq.hpp) 已支持 1–8 列。因此这里只存在进一步校准和覆盖特定形状的空间。不能把“用 GEMV 替代通用 GEMM”当作尚未实现的功能，也不能关闭当前整数 dot 所必需的 Q8 激活量化。

## 3. exl3xpu：最有价值的是分阶段计算和完整链计时

[exl3xpu](https://github.com/sybil-solutions/exl3xpu) 的 [设计](https://github.com/sybil-solutions/exl3xpu/blob/main/docs/DESIGN.md) 区分三条路径：小 M 向量 GEMV、中 M 解码后直接供给 DPAS、大 M 重建权重后使用 oneDNN。EXL3 tile 与 DPAS 操作数布局匹配，避免小 M 路径把全部解量化权重写到显存再读回来。

其 INT8 prefill 在 Hadamard 域进行，将激活量化融合到输入 Hadamard，将权重量化融合到 reconstruct。作者报告 B70、Qwen3.8-27B EXL3 4bpw、4096 tokens 下全部 linears 从 1748 降至 926 ms；teacher-forced NLL 从 1.6439 到 1.6467，约 +0.17%，top-1 一致率 97.2%。这些是该格式和输入分布的测量，不能作为本地 IQ2/Q2 的精度结论。

本地最值得继续的是 **prefill 的专家完整链**：

1. 先复用 [prefill.cpp](../sycl/src/prefill/prefill.cpp) 已有 `STRATA_PREFILL_XMX_EXPERT_CHAIN` / `STRATA_PREFILL_XMX_EXPERT_BATCH`，核实真实分派和完整请求效果。开关存在不代表当前冻结构建已启用或通过采用门槛。
2. 若考虑新整数路径，应按专家实际 token 数决定是否值得转换，并计入 H2D、GU、激活、Down、转换、冷启动和对专家缓存的挤占。
3. 对原始 BF16→整数路径使用已有真实热点激活和质量检查；对 IQ2/Q2 路径单独验证，不能将低位 GGUF 再量化后的结果当作原始 BF16 量化的结果。

本地已经有 [阶段拆分与原型](../bench/results/a770/phase-split-2026-10-27.md)、[真实热点完整链重放](../bench/results/a770/u4-expert-replay-2026-10-27.md)。后者 Prose T1995 的串行含搬运重放约 2.8 倍，仍没有整模型收益。本轮的价值是确认这些后续工作值得优先于再写一个通用后端。

局部 HC-up 原型的反例也应保留：[归属测量](../bench/results/a770/hc-u4-attribution-2026-10-27.md) 中，该阶段仅占原阶段计时约 1.1%；单算子约四倍不能解决整个请求。EXL3 的 trellis、码本、Hadamard 和 Xe2 DPAS 布局均不能直接用于当前 IQ2_S；本地 DG2/A770 的 XMX tile 也与 Xe2 不同。

## 4. MTP：先降低草稿成本，复用现有策略

[vllm-xpu-arc](https://github.com/arcitech-psp/vllm-xpu-arc) 的工作路线保留 BF16 模块边界，对 draft LM head 和部分 draft linears 使用 W4A16。其 [LM head helper](https://github.com/arcitech-psp/vllm-xpu-arc/blob/main/patches/vllm_xpu_draft_lmhead_int4.py) 和 [MTP helper](https://github.com/arcitech-psp/vllm-xpu-arc/blob/main/patches/vllm_xpu_draft_mtp_int4.py) 展示了目标模型与草稿权重分离的做法。

作者同夜同 wrapper 的组合路线为单流 132.7→144.4 tok/s（+8.8%）、四流合计 373.1→380.4（+2.0%）。这是 fixed-K3、draft INT4 等变动的组合，不能单独归因于草稿量化。README 另列的 +27% 含测量方法变化，不用于预测收益。

本地已有：

- [mtp.cpp](../sycl/src/core/mtp.cpp) 的 `draft_vocab.bin` 词表裁剪及 `STRATA_MTP_Q8_EXPERTS`。
- [draft_policy.cpp](../src/spec/draft_policy.cpp) 的实测轮次成本、接受前缀和探测机制。
- [DEEPINFER_OPTIMIZATIONS.md](DEEPINFER_OPTIMIZATIONS.md) 中已有默认关闭的成本窗口和连续草稿链实验。

优先测这些现有能力的单变量效果。如果 draft 投影仍明显昂贵，再尝试更低位权重。验收指标应为 `实际提交 token 数 / 整轮耗时`，同时保留分位置接受率；单看接受率或单看 draft 算子时间都可能选错。

已审计的FP16/V4直接初测提供了具体例子：有效Prose配对中，FP16的草稿accepted/offered为68/89（76.404%），V4为71/93（76.344%），接受率几乎相同，而相邻FP16基线几何平均decode为3934.525ms、V4为4572.450ms。V4接受数量更多并不等于接受率更高；两格式完整输出也不同，不能把耗时差归因于某个KV内核。原始计数及来源哈希另存`decode-draft-accounting.json`；这仍是既有初测，不代替新的独立确认。

有损草稿可能降低接受率，是否保持目标分布还取决于实际 verifier/sampler，不能仅因“只改 draft”就声称不影响输出。源 helper 的零 scale、非整组维度等输入边界也需按本地模型重新处理，不直接照抄。该项目自适应控制器标为 experimental，本地已有策略，无需再引入一套。

本地10月29日续测：Q4草稿投影在6297槽、物理V4、原spec4/min-p0.5的同EXE OFF/ON对照中，Code2K128贪心decode独立确认+4.185%/+4.026%，两侧基线漂移均小于0.06%；50次完整请求的原始协议、退出码、哈希、配对及显存审计通过。Short78/EOS回退2.751%/3.355%，Prose2K128回退0.876%/1.022%，均保留，不以联合门槛否决Code场景。Q4额外37.266MiB、目标head不变。另用同一已核对FP32采样器隔离重链，五引擎25次功能请求和原始协议审计通过，15次贪心及10次Short固定种子48-token采样均匹配原始，两个独立候选重复一致；这没有验证Code/Prose采样，也不是新链接下的速度测量。标准FP16 KV的五引擎50请求复核现已完成并审计通过：Short回退1.134%/0.813%，Code回退2.613%/2.447%，Prose回退1.451%/1.251%，全部配对漂移有效、输出匹配自身FP16参考，完整请求观测空闲至少1413.879MiB。因此FP16下不采用Q4草稿，V4下Code的独立收益保留；两组收益不能相乘以推断最佳跨格式组合。另确认原始LM head已为IQ4_XS且全词表草稿直接复用，避免再增加一份Q4_0 head。当前冻结EXE不含成本窗口/连续提交开关，相关旧源码能力不能通过设置环境变量直接生效；最小隔离接入及完整验证仍在继续，详见执行记录。

## 5. Nix 项目：RAM 前缀缓存的状态与预算比打包更相关

[vllm-xpu-nix](https://github.com/jasonboukheir/vllm-xpu-nix) 的 [RAM prefix 文档](https://github.com/jasonboukheir/vllm-xpu-nix/blob/main/docs/kvarn-prefix-cache.md) 把压缩 attention pages 与对应 recurrent checkpoints 一起存入有上限的 pinned RAM arena，按实际字节数收费；attention KV 命中还必须有同一边界的递推状态。

其配置测得 262015-token、两图像请求的 TTFT 为冷 383.389 s、RAM 命中 2.690/1.749 s，每次恢复约 3.90 GB，生成 token 相同。该结果证明多轮复用的价值，不能解读成计算 kernel 或 decode 的相同倍数。同步恢复也可能阻塞其他活跃请求。

本地 [generate.cpp](../sycl/src/program/generate.cpp) 已有 `--conversation-cache-mib`，并区分 positional 与 running state；[conversation_state.cpp](../sycl/src/core/conversation_state.cpp) 已处理快照保存、校验和恢复。先测这些能力，再决定是否缺少共享前缀粒度、压缩页保存或预算核算。没有必要为此引入 Nix 或另一套缓存框架。

K4V2 等更低位 KV 可以另列为长上下文实验，但当前有 K FP16/V packed4 路径，更低位 K/V 的精度、实际节省空间和可换来的专家驻留量均未验证，不列为当前 2K decode 的优先项。

## 6. 上游修复是排查线索，不能当成本地根因

| 来源与调研时状态 | 可借鉴的边界 | 本地适用限制 |
| --- | --- | --- |
| [#457](https://github.com/vllm-project/vllm-xpu-kernels/pull/457)，open | 将 grouped GEMM 内核中的全局 reset/atomic work stealing 改为确定性 grid-stride，避免图重放竞态 | 本地不是该 grouped-GEMM 实现，尚未发现相同计数器问题 |
| [#658](https://github.com/vllm-project/vllm-xpu-kernels/pull/658)，已合并 | 状态页号×stride 在乘法前提升至 64 位；以大偏移和守卫页验证 | 本地多处已用 size_t；上游复现不证明本地仍有同样溢出 |
| [#660](https://github.com/vllm-project/vllm-xpu-kernels/pull/660)，open | 图 padding 不能写入共享 null state，否则会传播 NaN | NULL_BLOCK_ID=0 是 vLLM 约定；本地合法状态 0 不能因此被跳过 |
| [#662](https://github.com/vllm-project/vllm-xpu-kernels/pull/662)，open | 已 pinned 且保持存活的源可直接 H2D，避免不必要 snapshot；必须明确 stream-order 和重用时机 | 本地 ExpertCache 已直接 queue.memcpy，不能假定也有该额外 snapshot；重点是源的生命周期 |
| [#647](https://github.com/vllm-project/vllm-xpu-kernels/pull/647)，open | 可变验证长度、空请求、padding 用真实请求边界生成 causal KV 元数据 | 更相关于可变长度/并发验证，尚无本地收益测量 |

Nix 项目的 [fork delta](https://github.com/jasonboukheir/vllm-xpu-nix/blob/main/docs/fork-delta.md) 另保留 fresh-state GDN 跨 subgroup 读取前的 barrier、untiled prefill 和 mixed-batch 算术分派。它们可用于检查 producer/consumer 边界，不能直接替换本地 native GDN。该项目自己的 [barrier 实验](https://github.com/jasonboukheir/vllm-xpu-nix/blob/main/docs/d256-barriers-dispatch-experiment.md) 还显示删 barrier 会回退；减少同步语句不代表更快。

本地已有 [大偏移 GEMM 复现与修复边界](DEEPINFER_OPTIMIZATIONS.md)、[Down witness](../bench/results/a770/down-witness-2026-10-27.md) 和非有限值追踪。后续应在这些证据上补齐缺失依赖，不重复建立一套宽泛诊断框架。

## 本轮排除的重复工作与验收口径

- GU WG128、整行预取、坐标交换、SG16 协作读取、同字节字段重排已有本地负结果；见 [性能索引](A770_PERFORMANCE.md)，不作为新方向重新推荐。
- 主存直读与显存驻留已有计数对照，双缓冲四种分块没有重复收益；[内存路径记录](A770_MEMORY_PATH.md) 支持研究驻留成本，不支持直接启用新 staging。只看纯 VRAM 内核会漏掉当前混合驻留的主要限制。
- 缓存裁剪、降低 MTP min-p、已提交前缀热度都有历史测试记录。最新门槛允许单场景确认；引用正值时仍须满足漂移与独立复测要求，并列出回退和旧基线的适用范围。
- 独立 SwiGLU/Q8 仅占本地 eager 累计 kernel 时间约 0.426%；其大倍数不会自动变成整模型大收益。[同一采集](../bench/results/a770/decode-gpu-profile-2026-10-28.md) 中专家 GU 约 24.187%、Down 约 11.328%、dense MMVQ 约 15.400%，这些是采集时累计 kernel 时间，不能当作正常 graph 的串行关键路径占比。
- 当前分析器对 graph 的逐次 kernel 重放覆盖不足，调度后的正常 graph 请求需关闭采集独立计时；不能把缺失 kernel 归成主机时间。

后续最小第一组是**共享专家 gate/up 合并提交**：保留原始权重与运算边界，先做真实形状和完整数组对照，再跑正常 graph 的原始 Code/Prose 2K/full128。固定精确链接对象、实际专家缓存槽数、KV、MTP 和预热；计时期间不编译。用户最新更新执行门槛：吞吐变化绝对值≤0.3%为噪声，**单场景稳定提升至少1%也算有效，不需要跨场景联合通过**。逐场景记录全部回退和适用范围，不再用此前几何平均/三分之二规则阻止单场景确认。相邻基线耗时漂移≤3%、独立确认和正确性/质量/显存检查仍须通过；35 tok/s目标单独验收。逐项实验状态见[执行记录](../bench/results/a770/xpu-optimization-2026-10-28.md)，不将旧研究中的外部数字当作本地验收。

10月29日补充：标准FP16下的现有RAM缓存已独立确认约2K多轮TTFT由20.3s降至0.572s，当前物理V4路径不支持该RAM快照。完整策略对照也已确认：相同6297槽、spec2/max-t1条件下，V4关闭缓存重算约19.5s，FP16缓存恢复约0.563s，两轮TTFT延迟均降低97.12%，原始35次请求审计通过。这证明该策略对照中的FP16方案有实际价值，不能归因于单个KV内核，也不能单独决定移除V4。

仍驻留显存的prompt checkpoint只保存递推/indexer状态，不复制V池。新的FP16/V4自身冷读、重复和返回旧会话检查已完整结束，12次完整128输出与原始日志审计通过：两种格式均在立即重复时复用2041/2048 tokens且保持自身冷读输出；返回旧Code分支均安全miss、重算输出不变。V4 Prose与旧OFF的冷读差异不能算作命中恢复失败。双方prompt-cache6的公平交替多会话对照现也完成：允许V4实际命中，但本次A/B交替的21次V4请求均miss；FP16两引擎分别恢复1950/1952 tokens，TTFT约19.8s降至0.570/0.551s，独立两轮延迟降低97.1245%/97.2112%。五引擎35次完整响应及原始协议、恢复/预算、哈希和配对审计通过；READY空闲2063–2145MiB、最多parked277.843MiB。此结论适用该交替多会话缓存策略；一token请求、READY-only显存记录不证明完整生成速度、峰值或所有会话历史。

FP16/V4固定槽单轮对照及三个新原始进程的独立确认现已结束，80次请求的两份原始协议审计通过。FP16的Short16解码吞吐初筛/确认分别提升7.204%/4.337%，Prose2K128提升16.214%/12.535%；Code2K128回退0.545%/0.471%，未达1%。新确认各指标基线漂移绝对值均≤0.379%，原漂移超限配对仍排除。TTFT没有任一方向独立确认的≥1%收益。三个场景各自同格式完整输出重复、跨格式输出不同，不能把差异归因于单个KV内核或直接判质量优劣。当前8K分配、6297槽及已测贪心请求更支持以FP16为优先基线；实际KV质量、公平缓存对照和长上下文容量尚未收齐，暂不删除V4或改变生产默认。8K下两者K+V布局相差77.1875MiB，这不是进程峰值节省；旧V4对V8的约2%结果不用于判断FP16/V4优劣。详见执行记录。

标准FP16 KV下的Q4草稿投影复核现已完成，沿用五引擎计时器及独立原始协议审计，三个贪心场景均回退0.813%至2.613%，因此不能转用V4下Code约4%的收益。实际KV评分仍待GPU，独立原始分数审计CPU自检通过。取舍继续区分当前2K请求收益与长上下文容量价值；8K/32K/64K分配探测已完成，完整长提示请求的质量/速度仍未测。
