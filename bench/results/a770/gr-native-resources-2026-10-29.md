# 当前 GR 内核的原生资源

四个实际链接的 GR 内核已合并为一次构建、一次模块提取，未运行模型或计算内核。独立 `python -X utf8 build-sycl-a770-gr-native-20261029/run.py --check` 已通过对象身份、精确 kernel ID、全部产物哈希及资源检查。

对象为 `build-sycl-a770-hc-up-fixed-streams/hc-streams.obj`，SHA256 `d65c0e0be0de9dfe9bcc0ab6b57f85ad398efad41a42138ebc757096a763d44a`。来源身份沿用已核验的冻结链接、源码清单及模型主机入口唯一匹配。本次没有重新编译这个对象；wrapper 使用本机 A770、驱动 32.0.101.9034 和 DPC++ 2025.3.2 提取默认 native 模块。

| 阶段 | GRF / SIMD | metadata private_size | 每工作项 global private_space | 静态 SLM | 代码字节 |
| --- | --- | ---: | ---: | ---: | ---: |
| up | 128 / 16 | 20096 | 1256 B | 12288 B | 44672 |
| down-sliced | 128 / 16 | 未报告 | 未报告 | 未报告 | 17664 |
| down-reduce | 128 / 16 | 20096 | 1256 B | 未报告 | 32064 |
| norm-split | 128 / 32 | 40192 | 1256 B | 64 B | 22080 |

四个内核均未报告 spill。up、reduce、norm 的 `per_thread_memory_buffers` 却明确包含 `type: global`、`usage: private_space`、`size: 1256`、`is_simt_thread: true`；同时出现 `private_base_stateless`。1256 B 恰好等于 metadata 中 by-value `__generated_GrMulti` 的参数大小。**没有寄存器 spill，仍可能存在全局 private 访存。** 分配大小不能直接换算成实际搬运字节数或可删除耗时。

down 使用 caller 提供的 local 参数，静态 metadata 不包含其全部动态 SLM；冻结调用分配 `8*128*sizeof(float)=4096 B`。up 的实际静态 SLM 已确认是 12288 B，但不能仅据此推算实际占用率或吞吐。

对应第一层的新候选是把三处内核中的动态 `m.a[token]` 选择改为八个固定索引分支，让编译器有机会只选择被使用的字段。浮点算术、归约顺序、工作组、子组、精度和输出保持原实现。是否消除 private 分配必须由新机器码验证，不能从源码写法推断。

`build-sycl-a770-gr-arg-select-20261029/run.py --self-check` 已通过可逆的三内核修改、冻结头文件/输入身份及构建生成。后续有限批次共用一个 GPU/编译 lease：重建同源控制和候选，提取八个模块；控制机器码须与上述四份原始机器码一致，候选消除 private 且无 spill 后，才运行现有三路径的六项 parity/null 检查。null 的 1..8 token、sink 和 changed-input 32 个见证还须与冻结原对象日志逐项一致。启动身份在该目录 `launch.json`；排队和启动不代表资源门槛或数值已通过。这一批尚未计时，也不修改生产代码。

条件满足后，`build-sycl-a770-gr-arg-select-speed-20261029/run.py` 才执行完整 HC 命令图筛选，资源门槛未过则直接记录跳过。复用冻结 worker 的输入/运算和六种 T 的原对象输出校验值，仅把 warmup/计时循环改为整个四阶段的 graph 回放。两轮独立进程分别正序/倒序 T1..6，每个进程每种 T 保留三次5000次回放的样本，每次之前预热1秒；共用 `原对象O-同源重建A-候选B-A-O`，每轮五进程。候选分别对两侧A及两端O比较，两轮同一T都须≥1%，两种基线漂移都须≤3%。测速期间复用现有编译进程启动监视器，所有原始样本保留。可逆graph修改、18样本完整性、错误数据拒绝和两轮漂移门槛的CPU自检已通过；PID19124已绑定资源任务PID16784及创建时间1793255660506ms等待。当前没有计时结果；该小链图筛选仍不能替代整模型decode和质量验收。

另一个测量限制：当前 SYCL `gpu_stamp_kernel` 的非 HIP 实现写入常数 0；现有 graph 阶段开关本身不足以提供有效的 GPU 阶段耗时。既有 [时钟验证记录](../../../docs/A770_PERFORMANCE.md) 也已记录 ESIMD `rdtsc()` 读数39029→35778，未通过跨 kernel 递增检查；本轮不重复这项原样探针。下一步应验证计时机制再关联正常 graph 的阶段成本，不能将 eager 累计时间或空表当成正常 graph 的关键路径比例。

最终目标仍是同一冻结负载下 decode 至少提升 20%，先 Windows 后 Linux；本报告只提供资源归因和候选前置证据。

## 离线 ISA 核查：已有实际 private 写入

四份冻结机器码已用本机 `iga64.dll`（版本 `1.1.0-int-8da3662fc62+`）一次离线反汇编，并通过独立检查。此批只调用 DLL 的 CPU 解码接口，没有启动编译器、GPU 或模型。静态计数止于每份代码唯一的 EOT，不计尾部填充或数据。

| 阶段 | 静态指令 | UGM / SLM / DC0 / GTWY send | EOT PC |
| --- | ---: | --- | --- |
| down-sliced | 1315 | 13 / 66 / 15 / 2 | `0x4410` |
| up | 3265 | 83 / 35 / 15 / 3 | `0xad90` |
| down-reduce | 2437 | 102 / 0 / 16 / 1 | `0x7c48` |
| norm-split | 1565 | 51 / 7 / 15 / 3 | `0x5540` |

入口处还核实了三个实际写入见证：metadata 的 `private_base_stateless` 参数经线程步长和低高位地址计算，传给下表的 UGM store。检查包含 ABI 偏移到 GRF 的映射、入口 payload load、地址形成和 store；入口到这些 store 之间没有分支。

| 阶段 | private base 的 payload 偏移 / 低位寄存器 | 线程步长 | store PC |
| --- | --- | ---: | --- |
| up | 1608 / `r54.2` | 20096 B | `0x1128` |
| down-reduce | 1632 / `r55.0` | 20096 B | `0x11a0` |
| norm-split | 1608 / `r57.2` | 40192 B | `0x1138` |

三处都是执行宽度 `(1\|M0)` 的 `store.ugm.d32x64t.a64.wb.wb`。这把证据从“分配了 private 空间”推进到“入口确有 private 全局写入”，支持继续测试固定索引候选。它们是 scalar/transpose 消息，不能把 metadata 的 1256 B/工作项直接乘 SIMD 或工作组人数作为实际搬运量；上表 send 数也不是动态执行数、private 总流量或可删除时间。是否减少指令和访存，仍以候选机器码及完整 graph/模型对照为准。

原始结果及逐条 PC/寄存器见证在 `build-sycl-a770-gr-private-isa-20261029/results.json` 和 `private-witness.json`。以下独立检查重新解码四份机器码，并复核三条见证及全部输入/输出哈希：

```powershell
python -X utf8 build-sycl-a770-gr-private-isa-20261029/private-witness.py --check
```

## 固定索引候选：资源与六项数值检查通过

`build-sycl-a770-gr-arg-select-20261029/run.py` 已完成。独立 `--check` exit 0：同源重建控制的四份机器码逐字节等于原对象；候选三内核的 private 分配消失、没有报告 spill，四者仍为128 GRF，SIMD、SLM与原对象一致，down机器码不变。default/direct/norm-fallback三路径的parity及null六个进程均exit0；各null日志的32个T1..8/sink/changed checksum与冻结原对象逐项一致。这是算子数值与资源结论，尚非整模型质量或速度结论。

候选四份机器码已用同一IGA DLL离线解码；独立 `build-sycl-a770-gr-arg-select-isa-20261029/run.py --check` 重新解码并核对所有原始/候选输入、输出哈希及资源/数值前置门槛，exit 0。

| 阶段 | 代码字节：原→候选 | 静态指令：原→候选 | UGM send：原→候选 | scalar transpose d32 store：原→候选 |
| --- | ---: | ---: | ---: | ---: |
| down-sliced | 17664→17664 | 1315→1315 | 13→13 | 0→0 |
| up | 44672→16832 | 3265→1213 | 83→17 | 12→0 |
| down-reduce | 32064→23168 | 2437→1860 | 102→82 | 15→0 |
| norm-split | 22080→15168 | 1565→1090 | 51→26 | 13→1 |

静态计数止于唯一EOT，最后一列只按确切消息拼写计数，未将所有消息归因为private。候选metadata中已无`private_base_stateless`及`usage: private_space`。尤其up的指令与消息大量减少，支持继续测试，但计数变化没有包含动态执行权重、缓存行为、调度和模型关键路径，不能作为同等比例的性能收益。

条件完整HC graph筛选进程19124已通过前置门槛，持锁完成worker构建后开始`round1-0-original`，模型或生产配置尚未修改。结果仍须等待原两轮双基线和漂移检查。

条件模型后续已准备并启动等待：`build-sycl-a770-gr-arg-select-model-20261029/model/run.py`，PID21856、创建时间1793260644096ms，绑定图筛选19124的实际进程对象及创建时间。两轮图筛选没有合格T时，不链接或运行模型；前置结果不完整也不能进入模型。通过才在一个共享lease内重链接、执行原四进程9场景/758目标精确质量，再调用原完整输出、显存/频率、相邻对照和独立确认测速器。链接仅将原HC对象替换为`hc-select.obj`，不组合GU、append或局部MMVQ；先链接，后启动编译事件监视器，质量和测速期间不再编译。

CPU自检已覆盖可逆的单对象链接变更、主机入口唯一归属与错误archive/重复入口拒绝、原质量接受/拒绝测试以及测速代码生成。原counter来源清单遗漏的唯一链接输入`ze_loader.lib`，用已检查GU模型链接清单中保存的同路径摘要补齐；其他原链接输入必须匹配counter冻结摘要，所有输入在真正构建前重新核对。该准备与启动不代表图筛选、模型质量或速度已通过；20%及Linux目标仍未证明。

### 图筛选第一轮：没有合格T，等待原第二轮结束

第一轮五个进程均exit0、每进程18条样本完整，原checksum均匹配。以下从`partial.json`的原始保留样本中位数重算；只列第一轮，不能写成两轮确认或模型结果。

| T | 对同源控制吞吐变化 | 对原对象吞吐变化 | 同源控制耗时漂移 | 原对象耗时漂移 |
| --- | ---: | ---: | ---: | ---: |
| 1 | +2.500% | −2.480% | −5.332% | −7.087% |
| 2 | −0.667% | −13.582% | +5.173% | +8.583% |
| 3 | −8.306% | −15.546% | −2.641% | +11.346% |
| 4 | −3.474% | −5.116% | −1.998% | −7.356% |
| 5 | −10.913% | −11.620% | +7.067% | −12.174% |
| 6 | −12.156% | −7.339% | +0.309% | −3.989% |

没有T满足原双基线≥1%/漂移≤3%的第一轮门槛，private消除尚未形成合格速度信号。超限漂移不能用于确认退化幅度。当前完整批次仍执行第二轮，未改变其脚本、门槛或原始记录，条件模型任务继续按原依赖等待。第一轮每次单HC图回放的wall/进程CPU计量约为毫秒级；进程CPU可能包括等待，不能仅据二者接近就判定host提交占比。下一步需验证小图实际节点/输出更新，并将提交循环与最终等待分开计量；现有checksum也包含prime留下输出的可能性，不能单独充当图确实执行的充分证据。这些限制不会被静态指令数减少所替代。

### 批量图执行诊断：准备记录与完成结果

`build-sycl-a770-gr-graph-diagnostic-20261029/run.py`复用冻结worker、构建参数和原/候选对象。两种图长度使用相同EXE/环境/输入，已从四进程合为两进程，每臂在同一进程内执行chain1/96各T1/T6三样本，共12样本。每样本重新捕获1或96次完整HC，须有4或384个kernel节点及一个root，保留原生DOT；finalize后先将整个out填为`0xff`并读回有效区域，逐word确认`0xffffffff`，warmup及回放后的全部输出checksum必须与原对象一致。这排除了prime遗留输出造成通过的可能。

chain1每样本5000次回放、chain96为64次。计量提交循环结束与最终wait结束的时间，wall须与submit+tail-wait包络一致，另记进程CPU。submit可能包含GPU backpressure，tail-wait不是全部GPU时间；重复同层权重/输入也不代表模型层工作集，因此该诊断不做配对收益接受或模型外推。CPU自检覆盖可逆源码修改、合批顺序、节点/poison/checksum/包络错误、NaN和缺样本拒绝，exit0。启动PID17480、创建1793261737899ms，经CIM核对后绑定19124实际Windows进程句柄等待，共用一个编译/GPU锁，尚无运行结果；启动记录在其`launch.json`。

18:03完成：原对象与固定索引对象两个进程均exit0，24条样本、全部节点数/单root、输出poison读回及checksum通过；编译启动guard完整且零事件。随后原 `run.py --check` 独立exit0，冻结输入、原始DOT及产物摘要检查通过。下表为每格三次样本的中位wall，单位µs；chain96的每HC耗时仅为wall除以96。

| 对象 | T | chain1每图wall | chain96每图wall | chain96每HC摊销wall |
| --- | ---: | ---: | ---: | ---: |
| 原对象 | 1 | 2452.967 | 5358.303 | 55.816 |
| 原对象 | 6 | 2186.512 | 10092.853 | 105.134 |
| 固定索引 | 1 | 2346.548 | 3219.664 | 33.538 |
| 固定索引 | 6 | 2344.155 | 6182.741 | 64.404 |

chain1尾wait中位约0.54–0.68µs，chain96约46.51–147.17µs，大部分wall位于提交循环内；结合下文已观察的逐回放host等待，这不代表大部分耗时都可从CPU删除。图内批量摊销明显不同，支持后续按正常模型图粒度分析；原模型本就跨层捕获大图，不能把这里chain1→96的差异当作模型新增机会。两臂只按原/候选顺序各执行一次，没有相邻基线和独立轮次，不将两者差值认作已确认的候选收益。诊断仍为 `model_speed_acceptance=false`，不覆盖有效A3/B2/A4模型确认。

```powershell
python -X utf8 build-sycl-a770-gr-graph-diagnostic-20261029/run.py --check
```

### 两轮图筛选终态：无合格T，条件模型已跳过

16:21独立`build-sycl-a770-gr-arg-select-speed-20261029/run.py --check` exit0：10个进程、180条原始样本完整，checksum及冻结输入/产物哈希通过；编译监视器`complete=true, passed=true, events=[]`。第二轮重算如下；两轮合并的`passed_tokens=[]`。

| T | 对同源控制吞吐变化 | 对原对象吞吐变化 | 同源控制耗时漂移 | 原对象耗时漂移 |
| --- | ---: | ---: | ---: | ---: |
| 1 | +6.518% | +9.502% | +2.826% | +17.859% |
| 2 | +1.880% | +14.719% | +1.897% | +2.914% |
| 3 | +0.216% | +17.138% | −6.875% | +8.975% |
| 4 | +1.603% | +11.485% | +5.258% | +5.281% |
| 5 | −3.602% | +11.897% | +10.032% | +9.455% |
| 6 | +3.200% | +7.286% | +4.015% | +0.565% |

第二轮T2独自满足原门槛，但第一轮没有通过，不能选取这一轮作为确认。其余超限漂移或不足门槛的数值也不作确定收益/退化结论。条件模型21856已正常结束，独立`model/run.py check` exit0，state记录`complete=true, target_achieved=false, skipped=No T passed both graph rounds against both controls`；没有链接或执行该条件模型。资源、数值通过仍有效，速度没有通过。

诊断17480在19124实际句柄结束后继续等待共享锁；CIM核对锁持有者20500正执行其他既有任务的编译，没有重排或并发启动诊断。下一步仍是核对节点、poison和提交/尾wait，先解释测量范围再决定后续实验。

### 重复回放中的原生等待：源码与既有API日志

对已冻结公开UR提交`5e422374f5f6710714d970f66c71658182d448d7`的检查发现，[waitForOngoingExecution:688](https://github.com/oneapi-src/unified-runtime/blob/5e422374f5f6710714d970f66c71658182d448d7/source/adapters/level_zero/command_buffer.cpp#L688)在存在上一轮完成事件时调用`zeEventHostSynchronize(..., UINT64_MAX)`，再释放并清空事件。[urEnqueueCommandBufferExp:1945](https://github.com/oneapi-src/unified-runtime/blob/5e422374f5f6710714d970f66c71658182d448d7/source/adapters/level_zero/command_buffer.cpp#L1945)在选择regular/immediate分支之前执行这项等待；两个分支均保存新的完成事件。`build-sycl-a770-gr-graph-replay-wait-20261029/audit.py --check`已复核公开源码的逐字节来源摘要、函数边界/顺序、冻结5000次worker及删除等待的拒绝检查，exit0。该公开源码不是本机DLL的完整身份或调用路径证明。

随后复用了已通过完整向量/产物摘要检查的`build-sycl-a770-runtime-append-trace-20261029/`四份原生API日志，无需再占用GPU。原probe先`run().wait_and_throw()`，再连续20次`run()`，最后统一wait。四臂default/regular/immediate/append各有21个回放标记；排除含显式prime等待的第一个间隔后，余下19个循环内相邻回放间隔**每个均恰好调用一次`zeEventHostSynchronize`**，没有普通kernel launch插入。append臂仍有这些等待。`trace.py --check`独立重读原日志、EXE/DLL及probe源码摘要、完整21次向量输出和删除等待的拒绝检查，exit0。

这把“回放之间确有原生host等待”从公开源码推断推进到现存小probe的实际调用证据，仍不证明等待时长、HC或整模型的完整内部路径，更不能将这些等待全部当作可删除时间。因此旧5000次循环不能仅因最后才调用queue wait就称为5000张图同时在途，也不能把submit计量视为纯CPU成本。正常模型窗口读取结果的依赖仍须满足；先解释小图与大图的测量范围，再做候选配对或运行时变更。

以上两项无GPU检查已加入固定CPU批量入口，当前七项一次执行全部exit0：

```powershell
python -X utf8 bench/results/a770/check_queue_batch_20261029.py
```

### 独立直接模型任务的首轮信号，尚待确认和质量

另一会话的`build-sycl-a770-gr-arg-select-model-20261029/run.py`直接以原生/算子数值检查为前置，未使用小图速度门槛；与本会话已跳过的`model/run.py`条件链分开。17:14只读重读其保存的A1/B1/A2三臂：均exit0、每臂8条完整2K/full128请求，输出逐条等于冻结参考；从三次保留样本中位数重算如下。仍是保存样本的首轮核对，完整原始协议、六进程、监视器、显存和质量终审尚未完成。

| 场景 | A1 / B1 / A2中位耗时ms | 首轮吞吐变化 | 基线耗时漂移 |
| --- | --- | ---: | ---: |
| Code2K | 4474.3 / 4363.1 / 4502.1 | +2.866734% | +0.621326% |
| Prose2K | 4899.8 / 4741.5 / 4844.3 | +2.751681% | −1.132699% |

Short初始化输出也相同。A1/A2的8条历史含接受/提议计数全部相同；Code的三条保留请求在原/候选均为73/92。Prose计数却不同：原版为72/95、71/96、71/98，候选为71/97、71/98、71/93，不能把该场景的端到端数字全归为相同工作量下的kernel加速。源码没有改变浮点运算表达式和归约写法，不足以证明所有实输入的评分或控制路径不变；六项算子fixture通过也不替代完整758目标及匹配热历史质量。

模型任务9776仍持锁执行原独立A3/B2/A4，图诊断17480经CIM确认存活等待，没有改动两个在运行脚本。首轮模型正信号同时说明，小图门槛失败不能单独作为固定索引候选的最终淘汰依据；后续以完整模型确认及质量为准，图诊断解释测量范围。尚不计为模型确认、采用或20%完成。

### 其他热点的全局 private 排查

2026-10-29 的 CPU 离线复核按精确核名读取既有十份原生元数据，并核对元数据与原资源记录的 SHA-256。GR up/reduce/norm 作为正对照，均检测到 `private_size`、`private_base_stateless` 和每 SIMT 工作项 1256B 的 `global/private_space`；GR down 三项均未报告。其余六个既有热点也未报告这些全局 private 项，报告的空间如下：

| 既有核标记 | SIMD | spill_size B | scratch single_space B |
| --- | ---: | ---: | ---: |
| gu16 | 32 | 576 | 1024 |
| mmvq21_nc4 | 32 | 1504 | 2048 |
| gdn_step16 | 16 | 992 | 1024 |
| gdn_ab | 32 | 1440 | 2048 |
| iq4xs_nc3 | 32 | 416 | 512 |
| iq4xs_nc4 | 32 | 608 | 1024 |

本项只排除了这六个已提取核上的同类全局 private 元数据征象；不表示没有 scratch 访存，也不覆盖全部模型内核。没有重新提取机器码、检查当前 EXE/大型归档身份或运行 GPU，因此没有新增性能结论。不据此重复已经关闭的全局 GRF256 扫测。GR 六进程完整测速及显存失败见 [独立模型报告](gr-arg-select-model-2026-10-29.md)，完整质量仍待。

[精确核名与资源记录](check_private_metadata_20261029.json)可用以下命令重算；脚本自检包含同名 misc 区段、缺失/重复核、重复字段及 SIMT private buffer：

```powershell
python -X utf8 bench/results/a770/check_private_metadata_20261029.py --check
```
