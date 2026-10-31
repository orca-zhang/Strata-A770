# A770：单算子倍率为什么没有变成请求收益

机器：A770 16 GiB、E5-2696 v4、256 GiB RAM、PCIe 3.0 x16、Windows、oneAPI 2025.3。目前没有通过质量和独立重复测试的显著 U4 请求收益。

HC-up 的约 4 倍来自局部热态算子测试。真实 T 和完整上下文下，同步诊断的 HC-up 是 281→104 ms；原阶段只占该同步诊断 prefill 的 1.1%。省下 177 ms，约为该诊断 25.45 s 的 0.70%。同步改变了计算与复制的重叠，这些占比不能当作无插桩请求的精确占比。即使用 1.1% 作为算术示例，局部 4 倍、没有新增开销时，整体倍率也只有 `1 / (0.989 + 0.011/4) = 1.0083`。

同一同步诊断中，完整专家 GU→SwiGLU→down 占 48.8%，专家反量化占 12.7%，等待权重复制占 6.3%。这些才是扩大 prefill 收益需要覆盖的主要阶段；同步时间不能直接当作异步执行的关键路径时间。在没有新增开销的算术模型中，局部 4 倍要换来整体吞吐 +10%，原先至少需要覆盖全程 12.12% 的耗时。仅优化占 1.1% 的 HC-up 无法达到这个量级。

专家层还有批次被路由拆散的问题。以下来自已通过完整输出参考检查的 48 层路由采样，按非零专家调用计数：

| 2K 请求 | 专家调用数 | 每次行数中位数 | T≥128 调用数 | T≥128 调用占比 | T≥128 路由行占比 |
| --- | ---: | ---: | ---: | ---: | ---: |
| Code | 18,317 | 18 | 2,134 | 11.65% | 55.69% |
| Prose | 17,177 | 16 | 2,133 | 12.42% | 60.41% |

当前 U4 接入要求实际 T≥128 且资产存在。即使完成全部 3234 个所选资产，按原路由统计仍有约 88% 的调用走原路径。调用占比和路由行占比都不是耗时占比；候选数值变化也可能改变路由。高 T 热点倍率不能代表所有专家。六个真实热点的串行 H2D+完整链回放中，仅 Prose T1995 在两轮共同通过稳定门槛，约 2.8 倍。

此外，每专家 GU+down 的单 U4 payload 为 2,764,800 B，当前 IQ2_S/Q2_0 为 1,510,400 B，传输增加 83.05%。实际链还要计入 activation pack、SwiGLU、队列提交和槽位等待。当前 U4 仅用于 prefill；decode 沿用原实现。候选生成不同 token 还会改变 decode 和草稿接受工作量。

1220 资产版本仅完成 A1/B1 性能段，观察到 prefill 吞吐变化约 +1.41% / +1.46%。A2 原路径返回全零，整组作废；这些不是验收结果。固定目标的 256-token 局部质量预检通过，聚合 PPL +0.579%，也不能代替全部资产的质量验收。

本轮实验把最多 8 个专家合为 4 次计算 kernel 提交，使用两组 8 槽权重缓冲重叠搬运。48 次调用、六份真实输入的完整 GU/down 已与原 U4 链逐位一致，间隙/guard、尾行、128/256 裁剪及非有限输入后的恢复通过。模型质量和无插桩请求收益仍需验证。若高 T 合并收益仍小，应继续测低 T 固定开销，而不能仅扩大高 T 热点的算子倍率。

合并版六专家异步模型检查也已通过：Code2K/Prose2K 的完整 128-token 输出与逐专家 U4 版本相同。但扩大到原 1220 资产固定目标检查时，第一层停滞超过 60 秒，被引擎 watchdog 结束；尚无任何评分结果。原始日志及线程转储哈希保存在 `build-sycl-a770-u4s4-grouped-model-20261027/consistency/failure.json`。根因尚未确认，不能把组件通过视为模型可用。全部 3234 个原始 BF16→U4 资产已转换完成，模型验收仍待解决该调度问题。

后续保持相同 1220 资产和 256 个固定目标，仅打开 `STRATA_PREFILL_SYNC=1` 的诊断已通过，全部评分与逐专家 U4 预检完全相同。Code 的 773 次专家调用合为 103 组，Prose 的 737 次合为 100 组。这证明该同步诊断下的数值一致性，尚未修复异步停滞，也不提供速度证据。完整 8 专家组件检查另通过 64 次调用、完整 GU/down 逐位比对与双缓冲复用。

质量验收还暴露了原路径问题：原 9 案例/758 目标质量组在 U4 候选启动前，就因基线 NaN 失败。随后对已检查的 HC-context EXE 做三轮独立进程诊断，保持同一输入顺序、实际 5771 专家槽、min_p=0.5 和 reserve=1536 MiB，U4 全程关闭，探针 OFF/ON/OFF。三轮均正常退出，但非有限原始 logprob 行数分别为 577/402/577；完整结果未通过有限值检查，不接受为可用基线。

探针轮第二个 8K 输入 T=7993，最早采到的非有限阶段为第 16 层 GDN 的 `y`、`y_half` 和 `state_after`。下一层 AB 投影的输入已非有限，权重仍有限。此前多数阶段只采样尾部 10240 个元素，不能据此断言所有递推输入都有限，也不能把 AB 或 U4 当作已确认根因。

随后已完成第 16 层的完整输入/状态抓取，先由 GPU 自检核对全部 9 组缓冲区的读回顺序和数据，再保留同一 9 案例/758 目标顺序运行模型。两个 8K 快照（T7964/7993）的 h、gate、beta、z、gamma、初始/最终状态和 FP32/FP16 输出全部有限；key 平方范数最大约 1.00000021，gate 均非正，beta 均在 [0,1]。用模型链接的同一个递推对象回放，原列内核与 Xe 内核的全部 48 个 head、最终状态、FP32/FP16 输出逐位一致，也逐位匹配模型快照。独立 CPU64 对 head0 的最大绝对误差不超过 3.84e-9，NRMSE 约 2.03e-6；CPU64 没有覆盖其他 47 个 head。

这次模型仍有 268 行非有限原始 logprob，第二个 8K 输入最早采到的异常移至第 45 层 `mixed`，当时 residual 输入仍仅查尾部。因此上述回放只证明这两份第 16 层数据的正确性，不能认定故障固定在该层、已被修复，或确认 HC mixed 是最初来源。

随后全 28 阶段的全元素检查已构建并完成同一 9 案例/758 目标，36 项 FP32/FP16/BF16 全量/尾部/guard 计数检查通过，模型诊断账目和文件哈希复核通过。U4 仍关闭，模型正常退出，但原始 logprob 有 536 行非有限值。第一个 8K 前缀 T7964 的 1344 个检查点全部有限；第二个前缀 T7993，首个失败检查点为第 21 层 `projection_moe`：61440 个非有限值，相当于 24×2560；随后 `residual_moe` 为 24×10240。该轮此前已检查的注意力半层全部有限。新增 HC 中间探针仍只覆盖注意力半层，尚未检查 HC FFN、路由、共享专家及独立专家链，因而只能将起点缩至这一 MoE 区间；“一个 24 行专家”仍是假设。没有新增质量或性能验收结论。

再将检查扩为 49 阶段，补齐 HC FFN、路由权重/分数、共享专家和独立专家 gather/GU/SwiGLU/down，原有数学运算及计数 reduction 保持相同，不增加显式逐阶段等待。该隔离 EXE 的同一 9 案例/758 目标全部获得有限分数，六个前缀记录的阶段计数均未报告异常，原始分数与文件哈希复核通过。这只说明此次开探针诊断未复现异常；探针增加 GPU 工作并改变调度，不能据此确认修复或接受性能。随后用 `build-sycl-a770-prefill-moe-stage-20261027/control.py` 复用相同冻结协议，检查两个独立进程关闭探针后的有限性及分数一致性。

关闭探针的两个独立进程已完成，同样的九案例中分别有 402/309 行原始 logprob 非有限，完整账目与文件哈希复核通过，未获得可用的重复基线。进一步只关闭 `STRATA_PREFILL_XMX_EXPERT_BATCH`、保留原专家计算链，两个独立进程均完成全部九案例/758 目标：原始分数全部有限，两轮每个分数完全相同，也与上述开探针轮的全部分数一致；完整账目与文件哈希复核通过。这提供一个可重复的调度诊断对照，但增加了等待频率，只用于定位批处理与调度问题，不接受为已证明的根因、无等待的修复或 U4 性能收益。结果在 `build-sycl-a770-prefill-moe-stage-20261027/batchoff/results.json`。

专家分组新增了[共享 token 打包原型](u4-shared-group-2026-10-27.md)，64 次真实输入专家调用的完整 GU/down 与逐专家 U4 逐位一致。按已有资产和真实路由核对，activation pack 行数可减少约 5.64/6.11 倍，计算 kernel 提交数可由 1156/1152 降为 915/912。这是工作量核对，没有计入新增索引 gather 的时间，尚无整模型提速结论。排序实验保留全部选中专家，另提供按行数或累计路由权重安排执行优先级。

证据：[HC 归因](hc-u4-attribution-2026-10-27.md)、[完整专家链](u4-expert-chain-2026-10-27.md)、[真实热点回放](u4-expert-replay-2026-10-27.md)。路由原始记录在 `build-sycl-a770-u4s4-route-survey-20261027/model/`，失败性能组在 `build-sycl-a770-u4s4-wide-preflight-20261027/performance/failure.json`，合并检查在 `build-sycl-a770-u4s4-grouped-20261027/`。

```powershell
python -X utf8 build-sycl-a770-u4s4-grouped-20261027/check.py --check
python -X utf8 build-sycl-a770-u4s4-route-survey-20261027/run.py --check
python -X utf8 build-sycl-a770-u4s4-grouped-model-20261027/context_quality.py --check
python -X utf8 build-sycl-a770-gdn-recurrence-capture-20261027/run.py --self-check
python -X utf8 build-sycl-a770-gdn-recurrence-capture-20261027/run.py --check
python -X utf8 build-sycl-a770-prefill-full-stage-20261027/run.py --check
python -X utf8 build-sycl-a770-prefill-moe-stage-20261027/run.py --check
python -X utf8 build-sycl-a770-prefill-moe-stage-20261027/control.py --check
python -X utf8 build-sycl-a770-prefill-moe-stage-20261027/batchoff.py --check
```

后续诊断证据：`build-sycl-a770-u4s4-grouped-model-20261027/diagnostic-sync/results.json`、`build-sycl-a770-u4s4-grouped-model-20261027/context-quality/results.json`，完整 8 专家组件证据：`build-sycl-a770-u4s4-grouped-full8-20261027/check-results.json`。这些诊断均不替代完整质量与无插桩性能验收。

完整递推抓取与回放证据：`build-sycl-a770-gdn-recurrence-capture-20261027/model/results.json`、`build-sycl-a770-gdn-recurrence-capture-20261027/replay/results.json`。其中回放程序输出的单次计时不用于速度结论。
