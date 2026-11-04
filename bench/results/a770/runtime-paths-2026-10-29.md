# A770：compute-runtime / UR 优化空间

本机为 Windows、A770 16GB、驱动32.0.101.9034，oneAPI DLL FileVersion 2025.3.0、UR loader 0.12.0、Level Zero API 1.17。本轮提交路径组合已通过两轮整模型测速与显存复核：独立复测Code吞吐+2.668767%、Prose+2.901650%。四轮固定目标、四轮完整热历史、真实模型API及服务器引擎/实际DLL验证均已通过；独立可选`strata-iq2_xs-runtime-append.json`已完成验收，默认配置未改。可迁移的经验与启动方法见[优化经验](knowhow-2026-10-29.md)。

调用链为 `Strata → SYCL → Unified Runtime 的 Level Zero adapter → Level Zero loader → Intel 用户态驱动 / compute-runtime 对应层 → Windows GPU 调度`。`UR_L0_*` 改的是 adapter 的选择或调用路径；GRF、指令调度和 U4×S4 内核属于编译器或算子优化。公开 compute-runtime 源码不能直接当成本机 Windows 驱动二进制的同一源码证明。

| 优先项 | 可能减少的工作 | 当前证据与限制 |
| --- | --- | --- |
| immediate command lists、driver in-order、counter events | 小 kernel 提交、事件管理及同步 | 默认regular、强制immediate改变队列类型；小图有 `inOrder: 1` 和 `Counter: 1`，append组合下事件复位调用为0。完整组合已确认下述整模型吞吐变化，尚未分拆各开关贡献。 |
| graph immediate append | 重放已有命令列表的提交开销 | 四个小图进程完整向量通过，append组合调用21次append、传统提交为0；完整模型诊断为514次append、queue-execute及event-reset-append均0。日志不计时。完整组合另已通过两轮整模型测速、显存、冷/热质量及服务器引擎/实际DLL验收。 |
| UR Level Zero v2 adapter | adapter 内部提交和对象管理 | 本机有 v1/v2 两套 DLL，可用进程局部 `UR_ADAPTERS_FORCE_LOAD` 对照；强制 v2 的两次小图检查通过，native 队列为 regular。加载过两个 DLL 不能说明默认计算用了哪套，也没有证明 v2 更快。 |
| 驻留、拷贝和缓存策略 | 权重访问、迁移、无效拷贝与等待 | 必须观察 decode 的实际命令流。已有 graph、device arena 和 pinned host 权重，不能再把这些已有措施列成新增收益。DG2 专用一致性或栅栏 workaround 须有正确性证据才能改变。 |

公开 UR 源码已冻结到 commit `5e422374f5f6710714d970f66c71658182d448d7`，相关摘录及完整源文件 SHA256 保存在 `build-sycl-a770-runtime-dispatch-20261029/source-audit.json`：

- [device.cpp:1998](https://github.com/oneapi-src/unified-runtime/blob/5e422374f5f6710714d970f66c71658182d448d7/source/adapters/level_zero/common/device.cpp#L1998)：Windows DG2 因驱动限制默认关闭 immediate command lists。
- [platform.cpp:846](https://github.com/oneapi-src/unified-runtime/blob/5e422374f5f6710714d970f66c71658182d448d7/source/adapters/level_zero/common/platform.cpp#L846)：driver in-order 的开关与驱动版本条件；不能将未设置理解为关闭。
- [command_buffer.cpp:80](https://github.com/oneapi-src/unified-runtime/blob/5e422374f5f6710714d970f66c71658182d448d7/source/adapters/level_zero/command_buffer.cpp#L80)：immediate append 前提检查、默认关闭原因，以及 graph counter event 的组合条件。
- [command_buffer.cpp:807](https://github.com/oneapi-src/unified-runtime/blob/5e422374f5f6710714d970f66c71658182d448d7/source/adapters/level_zero/command_buffer.cpp#L807)：DG2 的 graph copy-engine 路径有专用限制。通用开关名不代表这条路径可以启用。

这些是公开源码证据，未证明已安装的2025.3二进制具有完全相同的分派。安装的 DLL、开关字符串、驱动扩展及既有测试环境的只读审计见 `build-sycl-a770-down-scan-context-20261028/runtime-audit.json`。

graph自身是否按in-order创建也是必要条件。上述冻结源文件的 [canBeInOrder](https://github.com/oneapi-src/unified-runtime/blob/5e422374f5f6710714d970f66c71658182d448d7/source/adapters/level_zero/command_buffer.cpp#L708) 同时检查显式driver in-order开关和command-buffer描述的 `isInOrder`；[graph counter事件选择](https://github.com/oneapi-src/unified-runtime/blob/5e422374f5f6710714d970f66c71658182d448d7/source/adapters/level_zero/command_buffer.cpp#L752) 还要求 `IsInOrder && ImmediateAppendPath`。本轮小图日志已取得in-order/counter及实际append调用证据；该证据不自动证明完整模型graph也具有相同描述和调用次数。

decode 已使用 graph。运行时优化主要能省去验证窗口之间的提交、事件与同步开销；专家读权重、CPU 专家计算和 PCIe 传输仍须各自处理。当前 graph 逐 kernel 采集没有通过覆盖检查，eager 累计 kernel 时间也不是 graph 关键路径份额，因此没有依据预测运行时整体收益百分比。

这条提交路径有具体的源码依据。同一冻结commit的 [enqueueWaitEventPath:1851](https://github.com/oneapi-src/unified-runtime/blob/5e422374f5f6710714d970f66c71658182d448d7/source/adapters/level_zero/command_buffer.cpp#L1851) 分别用 `zeCommandQueueExecuteCommandLists` 提交事件复位列表和计算列表，再追加完成通知与事件复位；有copy列表时另行提交。它不会因为应用queue改成immediate就自动改用graph immediate append。[enqueueImmediateAppendPath:1768](https://github.com/oneapi-src/unified-runtime/blob/5e422374f5f6710714d970f66c71658182d448d7/source/adapters/level_zero/command_buffer.cpp#L1768) 使用另一API提交计算列表，但若 `ZeCommandListResetEvents` 仍存在，也会追加复位列表。因此不能把“强制immediate”直接解释为删除了全部复位、屏障或事件开销。

更明确的待验证目标是 [创建逻辑:840](https://github.com/oneapi-src/unified-runtime/blob/5e422374f5f6710714d970f66c71658182d448d7/source/adapters/level_zero/command_buffer.cpp#L840)：只有 `UseCounterBasedEvents` 为真时，才不创建这条复位列表及其同步事件。也就是需同时核实immediate append、driver in-order、command-buffer的 `isInOrder` 和counter支持，不能只看应用queue或单一环境变量。这能说明要消除哪项工作，尚不能说明这项工作在本机decode关键路径上占多少。

本轮把该完整源文件保留在 `build-sycl-a770-runtime-source-20261029/command_buffer.cpp.txt`，SHA256 `e2cdb461533a8e8484545ef93971a4738bbb4b996de6085b551028d8a0c249e7`，与原审计一致。后续日志需核对成功调用的API、列表句柄和创建flags；仅统计函数名字符串不足以区分初始化、回放、复位和扩展查询。这仍是公开源码的调用链，不是已安装Windows驱动的热点测量。

当前模型的冻结验证器还已启用device plan、no-host和descriptor cache；其 `verify.cpp:1414` 回放graph，逐层主机握手在 `verify.cpp:1446` 受 `!no_host` 控制。不能再把删除逐层CPU握手列为新增优化。首次正常graph CPU采样collect退出1；修正后的软件采样已成功，结果如下。CPU采样只能定位活跃CPU成本，不能测出全部阻塞等待或GPU空隙。

首次失败原因已从原日志定位：`samplingMode=sw` 但 `PMU` 为空，默认开启的 `enableCharacterizationInsights` 增加硬件计数，生成了缺值的 `--pmu-type`，采集器在启动前拒绝该参数。新任务 `build-sycl-a770-runtime-graph-cpu-sw-20261029/run.py` 只增加 `-knob enable-characterization-insights=false`，保持相同ITT-only EXE和正常graph。新配置确认没有PMU参数；collect、summary、functions、modules、processes全部exit0，五条请求528个输出token、6297槽、READY1969MiB、四组48层扫描均核对通过。

原计划的 `-report tasks` 在本版VTune不受支持，故runner最终exit1、`results.json complete=false`原样保留。未重跑模型，只用同一原始采集导出 `hotspots -group-by task` 和 `top-down`，两者exit0，记录在 `export-results.json`。离线 `audit.py` 核对两份manifest、全部输出、软件采样配置和任务边界：仅Code2K.Decode/Prose2K.Decode各一次，任务耗时4.449106/4.792025秒，与原生decode标记分别相差0.032/0.029毫秒；另有0.078727秒CPU样本在任务外，不能归入decode。

采集CPU时间9.082244秒，其中工具分类Spin Time为4.749826秒。函数自身CPU时间中，`NtYieldExecution` 为4.674389秒、`RtlQueryPerformanceCounter` 为1.206200秒；top-down的两条 `urQueueFinish` 调用链合计包含93.459599%的CPU样本，经实际 `ur_adapter_level_zero.dll` 进入Intel驱动等待链。这确认该采集进程使用legacy adapter，并把下一步定位到同步路径；**等待中的CPU活跃时间可与GPU执行重叠，93.46%不是可移除的decode耗时**。驱动大量函数缺少符号，尚未定位到本机NEO内部的具名实现。复核无需重跑模型：

```powershell
python -X utf8 build-sycl-a770-runtime-graph-cpu-sw-20261029/audit.py
```

等待策略另有一项只读核对：已安装legacy/v2 DLL都含 `UR_L0_QUEUE_SYNCHRONIZE_NON_BLOCKING` 字符串。同一冻结公开源码的 [queue.cpp:1805](https://github.com/oneapi-src/unified-runtime/blob/5e422374f5f6710714d970f66c71658182d448d7/source/adapters/level_zero/queue.cpp#L1805) 和 [1857](https://github.com/oneapi-src/unified-runtime/blob/5e422374f5f6710714d970f66c71658182d448d7/source/adapters/level_zero/queue.cpp#L1857) 在该分支只释放/重新取得queue互斥锁，仍调用native host同步；开关名不能证明调用会异步返回。现有采样显示驱动等待，尚无同一queue锁竞争证据，因此不新增这一开关的模型测速。原文、来源hash及DLL字符串见 `build-sycl-a770-runtime-source-20261029/wait-policy-audit.json`；这仍是公开源码路由核对，不是本机DLL同源证明。当前已排队的append对照保持不变。

最小 native queue 检查已完成。它复用已有的完整向量图探针，仅在最后等待结束后调用 `sycl::get_native`，区分 native queue 与 immediate command list。每个新进程执行21次完整向量更新并检查全部 `1<<20` 个元素；五组合分别运行eager/graph，共10个进程，全部exit0且完整向量检查通过。

| 进程局部配置 | eager观察 | graph观察 | 完整向量检查 |
| --- | --- | --- | --- |
| 默认环境 | regular queue | regular queue | 2/2通过 |
| 强制v1、immediate=0 | regular queue | regular queue | 2/2通过 |
| 强制v1、immediate=1 | immediate command list | immediate command list | 2/2通过 |
| 强制v1、immediate+in-order+counter+append | immediate command list | immediate command list | 2/2通过 |
| 强制v2，其他开关保持默认 | regular queue | regular queue | 2/2通过 |

因此，当前安装版本的默认队列确实为regular，强制immediate也确实改变native队列类型。此前只有queue类型时不能证明graph immediate append；本轮新增API日志已补齐该小图的实际调用证据。小图正确性通过仍不替代完整模型质量和性能检查。

完整记录、二进制和源文件哈希在 `build-sycl-a770-runtime-dispatch-20261029/results.json`，离线检查与CPU自检均通过：

```powershell
python -X utf8 build-sycl-a770-runtime-dispatch-20261029/run.py --self-check
python -X utf8 build-sycl-a770-runtime-dispatch-20261029/run.py --check
```

新增诊断 `build-sycl-a770-runtime-append-trace-20261029/run.py` 已结束，四个新graph进程均exit0，每进程21次更新的全部向量元素核对通过。日志明确显示 `Validation Layer Enabled`，不计时。只统计 `[trace] API(...)` 入口，排除错误复述和扩展查询字符串，得到以下进程全程计数，包含初始化和清理，不当作每次模型回放次数：

| 小图配置 | graph immediate append | 传统队列提交 | 事件复位append |
| --- | ---: | ---: | ---: |
| 默认 | 0 | 64 | 44 |
| 强制regular | 0 | 64 | 44 |
| immediate+in-order+counter、append关闭 | 0 | 42 | 43 |
| 同组合、append开启 | 21 | 0 | 0 |

最后一组同时记录 `inOrder: 1`、`Counter: 1`，没有append调用错误。原始日志、来源哈希、`run.py --check` 及实际入口计数检查均保留：

```powershell
python -X utf8 build-sycl-a770-runtime-append-trace-20261029/run.py --check
python -X utf8 build-sycl-a770-runtime-append-trace-20261029/audit.py
```

完整模型对照 `build-sycl-a770-runtime-append-model-20261029/run.py` 的六个引擎均正常退出。两臂使用同一device-counter EXE、原始验证器及模型；固定6297槽、tail64、Code/Prose各2048输入与完整128输出、min-p0.5。仅候选使用2025.3 v1 immediate+driver in-order+counter+graph append，基线为默认提交环境；共同保留原有relaxed-allocation开关。测速没有API日志、验证层或profiler。每进程Short16及Code/Prose预热后，保留三轮交错请求；同一场景须两轮吞吐≥1%、基线耗时漂移绝对值≤3%，<0.2%为本组噪声。

| 场景 | A1/B1/A2初筛吞吐变化 | 初筛基线耗时漂移 | A3/B2/A4独立吞吐变化 | 独立基线耗时漂移 |
| --- | ---: | ---: | ---: | ---: |
| Code2K | +2.765941% | -0.235563% | +2.668767% | -0.022900% |
| Prose2K | +2.454723% | -0.135487% | +2.901650% | -0.162451% |

全部6240个输出token、每进程八组48层扫描、tail64实际直方图、来源及三个运行时DLL哈希通过。48条长请求均有完整请求区间内的显存采样和decode频率采样；本次1567条完整显存采样全部落在monitor运行边界内。六臂最低采样空闲显存依次为1597.504、1642.238、1605.645、1608.887、1620.652、1608.141MiB，均≥1024；decode采样均为2400MHz、未观察到降频标志。1Hz采样不证明瞬时峰值或没有分页。

原runner收尾exit1：继承的`memory_coverage`仍从父模块的旧tail256目录读日志，时间戳边界检查拒绝了旧样本。原`performance/results.json`保持`complete=false`。新增`audit.py`离线重现错误目录读取，再只在本次审计模块内绑定正确目录，重算显存和频率并执行原全部输出、协议、扫描、配对与哈希检查；没有重跑GPU或编译器。结果另存`performance/recovered-results.json`，生成及随后独立`--check`均exit0。原`run.py --check`针对保留的未完成记录；本次恢复结果的复核命令为：

```powershell
python -X utf8 build-sycl-a770-runtime-append-model-20261029/audit.py --check
```

固定目标与热历史任务`quality.py`已完成，原独立`--check`通过。四个全新固定目标进程的9场景/758目标精确等于冻结原版，必需概率有限，聚合PPL变化0；另四个热历史进程保留测速的cache0/shortread1024、Short16+预热+三轮请求，每进程完整1040输出token匹配参考，最后两条请求的256条评分在四进程间精确一致。冷评分的cache1/shortread256不替代热历史。两臂仍为同一EXE，仅候选增加本次运行时设置；质量诊断不提供速度结论。后述实际模型API及服务器引擎/实际DLL也已通过。

落地配置的只读核对发现，当前默认`strata-iq2_xs.json`与本轮两臂共同条件有以下差异；上述吞吐收益限定于本轮共同条件加候选运行时设置。服务器`serve/server.py`已有独立配置入口，`engine_args`读取该配置参数，`child_env`合并其环境变量并保留父进程环境。现已生成独立可选配置，并通过完整启动参数、有效环境和实际子进程DLL校验；当前默认配置摘要与冻结记录一致。

| 条目 | 当前默认配置 | 本轮两臂共同条件 |
| --- | --- | --- |
| EXE | prefill-cache-tail-production-v3 | down-device-counter/baseline |
| STRATA_VERIFY_EAGER | 1 | 不存在，使用graph |
| STRATA_PREFILL_XMX_EXPERT_BATCH | 1 | 0 |
| --expert-cache | auto | 6000，实际6297槽 |
| --spec-min-p | 0.8 | 0.5 |
| --vram-reserve-mib | 1536 | 1024 |

冻结的验证器依据`STRATA_VERIFY_EAGER`是否存在选择路径，字符串0仍表示该变量存在。独立graph配置应移除这个键，并在启动前核对父环境；不能把本组结果直接归到默认eager参数。本机本次核对没有继承该变量。此核对没有运行新模型请求或更改服务器源码、默认配置。

真实模型API诊断脚本`build-sycl-a770-runtime-model-trace-20261029/run.py`已完整运行并通过独立`--check`。两臂使用同一个已确认EXE及Short16、Code2K128、Prose2K128，额外开启`UR_L0_DEBUG=1`；完整输出、扫描及tail64直方图通过。排除扩展查询字符串与错误复述后，默认/候选全程append计数为0/514、queue-execute为192944/0、event-reset-append为11533/0，候选in-order/counter见证均为True。计数覆盖初始化、prefill、decode及清理，不外推GPU内部全部同步操作。日志模式开启验证层，全部时间值仅作原始诊断记录，不用于测速。

```powershell
python -X utf8 build-sycl-a770-runtime-model-trace-20261029/run.py --check
```

工作区另有运行时与IGC候选组。`build-sycl-a770-neo-igc-20261029/run.py` 首组在A0原生模块属性解析断言中退出，尚未产生模型请求或比较；失败记录保留。后续v3的 `deferred-for-hotspots.json` 记录了按用户先分析热点的要求、在取得锁和测量前延后整模型扫测。

原生资源探针的新证据保存在 `build-sycl-a770-igc-spill-native-20261029/resource-recovery.json`。原runner有15项长路径汇编文件检查失败，原记录保留；恢复工具仅从已生成的native二进制提取准确代码节与元数据，没有重跑GPU或编译器。18组记录及来源哈希核对通过，六个基线内核的spill分别复现为GU16 576、MMVQ21/NC4 1504、GDN step16 992、GDN AB 1440、IQ4XS/NC3 416、IQ4XS/NC4 608字节。auto模式六组代码与基线逐字节相同；强制256 GRF使六组代码改变且元数据不再列出spill_size，按该元数据解析为0。这是编译器资源选择证据；尚未执行数值或性能验收，增加GRF也可能降低并发，不能据此推断decode收益。本轮只读核对该探针，没有修改其脚本、全局设置或当前排队的append对照。复核无需模型请求：

```powershell
python -X utf8 build-sycl-a770-igc-spill-native-20261029/recover.py --check
```

另一个已结束的检查是[host USM 原子绕法](host-usm-atomics-2026-10-29.md)：device counter 的边界检查通过，但同一扫描器下的裁剪候选评分未重复，流水线没有启动测速。不能把本轮所有必需概率有限当作旧 NaN 已修复，或据此将剩余差异归因于 compute-runtime。

2026-10-29补充collector路径核对：当前append runner只更改导入模块的`m.H`，但继承的`memory_coverage`函数使用其定义模块`m.base.H`。隔离导入实测确认，两者分别指向append本次目录和旧tail256目录；只更改`m.H`不会改变读取器的路径。六个模型进程均已结束，冻结脚本和原始失败记录保持；补审计只使用本次`monitor.stdout`，不能将模板目录的旧样本用于显存验收。新[换入上限32→16对照](adapt16-current-2026-10-29.md)已绑定两处目录，并通过实际临时文件与缺失区间拒绝自检。

只读补审计`build-sycl-a770-runtime-append-model-20261029/audit-memory-path.py`现已exit0，原生进程句柄及创建时间确认原collector结束后，在共享锁内先重现旧目录读取失败，再绑定本次目录。原完整输出、协议、配对、来源及DLL哈希、显存/频率覆盖检查均通过，结果另存`memory-path-audit.json`，原`results.json complete=false`不改写。未重跑模型或编译器。自检使用两个目录的不同时间戳文件，并拒绝缺失覆盖和没有原目录缺陷的“修复”。该补审计和上面的`audit.py`复核同一组保留数据，不是新的独立性能队列；固定目标、完整热历史质量和生产采用仍分别判定。

```powershell
python -X utf8 build-sycl-a770-runtime-append-model-20261029/audit-memory-path.py --self-check
# 复核已结束的补审计，不重跑模型；须等待共享锁：
python -X utf8 build-sycl-a770-runtime-append-model-20261029/audit-memory-path.py --check
```

独立候选中位数为Code30.09711、Prose27.81097 tok/s；两轮候选最低保留值分别29.96255和27.49554 tok/s，所有保留请求均未达到35。原协议的0.2%噪声措辞保留，当前统一按小于0.3%解释；两轮收益均超过两者。完整来源和采样显存终审已通过，固定目标及完整热历史质量仍待验收，尚未采用。

本次已关闭的`monitor.stdout`为686988字节。全程1567个完整显存样本中，48条完整请求区间覆盖1295个样本；A1/B1/A2/A3/B2/A4分别覆盖206/237/206/205/238/203个，最低空闲1597.50391MiB。48条请求区间内共214个decode频率样本，均为2400MHz，无采样限频。上述样本数分别表示全程和请求区间，不能混为一谈；1Hz采样不能证明瞬时最低值或没有分页。

可选配置接入脚本`build-sycl-a770-runtime-profile-20261029/run.py`已通过CPU自检，尚未运行模型或生成配置。它复用服务器实际的`engine_args`、`child_env`和`StrataEngine`，要求参数与本轮测量一致，拒绝继承的eager开关，包括字符串0。固定目标、完整热历史与实际模型API诊断全部通过后，才在共享锁内启动同一EXE，检查Short16及原始Code/Prose2K完整128输出、6297槽、READY显存、扫描、tail64和正常退出；通过后生成独立的`strata-iq2_xs-runtime-append.json`。实际启动验证不用于推导速度；默认配置保持原样。后续步骤已串联到现有质量进程结束之后，每步失败即停止，没有另起质量任务。

## 更低一层：NEO / Windows 驻留与提交

继续优化有源码入口，但上述约3%是UR提交组合的整模型结果，尚未分离NEO内部成本。本轮只读核对公开compute-runtime commit `3c7d20d781da468dc60a8204f0735004918e0940`，没有修改驱动、启动新GPU任务或改变已有排队脚本；公开实现不代表本机Windows DLL具有同一实现。

| 可调查项 | 公开源码证据 | 进入优化前要测什么 |
| --- | --- | --- |
| 驻留集合遍历、间接USM收集与锁 | [cmdqueue.cpp:500](https://github.com/intel/compute-runtime/blob/3c7d20d781da468dc60a8204f0735004918e0940/level_zero/core/source/cmdqueue/cmdqueue.cpp#L500)先尝试按pack提交间接分配；失败分支才持锁收集，再逐allocation准备host指针并登记驻留 | 当前graph是否实际走该分支、集合大小及关键路径耗时；专家权重经指针表间接访问，不能仅为省遍历而取消必要的可见性 |
| WDDM驻留处理及paging fence | [wddm_residency_controller.cpp:131](https://github.com/intel/compute-runtime/blob/3c7d20d781da468dc60a8204f0735004918e0940/shared/source/os_interface/windows/wddm_residency_controller.cpp#L131)过滤已经驻留的allocation；[64行起](https://github.com/intel/compute-runtime/blob/3c7d20d781da468dc60a8204f0735004918e0940/shared/source/os_interface/windows/wddm_residency_controller.cpp#L64)只有非空handle集合才调用makeResident，失败才进入trim/重试 | 实际MakeResident/Evict、预算压力及paging fence等待；不能从函数名或1Hz显存余量推断每轮搬运或分页 |
| 主机等待与下一次提交之间的空隙 | [wddm.cpp:1192](https://github.com/intel/compute-runtime/blob/3c7d20d781da468dc60a8204f0735004918e0940/shared/source/os_interface/windows/wddm/wddm.cpp#L1192)含条件忙等、monitor fence和KMD等待；[提交:1077](https://github.com/intel/compute-runtime/blob/3c7d20d781da468dc60a8204f0735004918e0940/shared/source/os_interface/windows/wddm/wddm.cpp#L1077)还检查未完成的paging fence | 对齐模型窗口、原生API起止和GPU队列时间线，区分GPU仍执行、提交不及时与唤醒延迟；CPU等待采样不能代替这项测量 |

另需区分CSR的逻辑驻留标记与WDDM物理驱逐：[makeSurfacePackNonResident](https://github.com/intel/compute-runtime/blob/3c7d20d781da468dc60a8204f0735004918e0940/shared/source/command_stream/command_stream_receiver.cpp#L238)更新任务/可驱逐状态，公开Windows CSR的[processEviction](https://github.com/intel/compute-runtime/blob/3c7d20d781da468dc60a8204f0735004918e0940/shared/source/os_interface/windows/wddm_device_command_stream.inl#L155)为空；仅看NonResident调用不能认定每轮物理驱逐。实际预算trim仍须单独观察。

进一步核对同一冻结commit，间接USM的pack路径已经增量处理分配。[makeIndirectAllocationsResident:1313](https://github.com/intel/compute-runtime/blob/3c7d20d781da468dc60a8204f0735004918e0940/shared/source/memory_manager/unified_memory_manager.cpp#L1313)按CSR保存`latestResidentObjectId`及`latestSentTaskCount`，首次处理全部分配；之后仅在`allocationsCounter`增长时遍历新增ID，没有新增分配时跳过该遍历。入口仍取得容器写锁并更新task count。[submitIndirectAllocationsAsPack:1532](https://github.com/intel/compute-runtime/blob/3c7d20d781da468dc60a8204f0735004918e0940/shared/source/memory_manager/unified_memory_manager.cpp#L1532)检查memory-manager许可、TBX模式及debug覆盖；[MemoryManager:395](https://github.com/intel/compute-runtime/blob/3c7d20d781da468dc60a8204f0735004918e0940/shared/source/memory_manager/memory_manager.h#L395)默认许可，公开[WddmMemoryManager声明](https://github.com/intel/compute-runtime/blob/3c7d20d781da468dc60a8204f0735004918e0940/shared/source/os_interface/windows/wddm_memory_manager.h#L28)直接继承它且未覆盖该方法。因此不能先假定Windows每次都走全量收集回退，再把通用集合缓存作为新增优化。

这也不表示所有驻留工作已经消失。[immediate路径:532](https://github.com/intel/compute-runtime/blob/3c7d20d781da468dc60a8204f0735004918e0940/level_zero/core/source/cmdlist/cmdlist_hw_immediate.inl#L532)在允许间接访问时调用同一间接驻留入口，随后仍处理command-list自己的资源容器；regular路径也保留逐command-list资源处理。当前将“另建通用间接USM集合缓存”降级，先测实际pack/回退路径、分配ID变化、锁成本和资源容器成本。间接分配释放还依赖已记录的task count，不能仅跳过登记。上述结论限于公开源码，未证明已安装DLL同源或模型重放走该分支，没有新增GPU实测或吞吐收益。

因此当前顺序为完成已有append质量与模型API验证，再用已排队的graph时间线诊断定位空隙；只有实际关键路径成本成立，才考虑NEO补丁或新的运行时对照。本轮重新执行`audit.py --check`通过，复核的是上面同一批保留数据，不增加独立测速次数。没有证据支持仅靠运行时达到35/40 tok/s，也没有新增收益百分比。

09:59补充只读核对：裸GUID小图的原生build/start/probe/stop/export/stats均成功，21次完整向量更新通过。原关联断言失败的具体原因是末尾两包的Queue完成晚于用户态结束标记16/31µs，以及首次重放3包、后19次各4包；79包的端点顺序均正确。独立恢复报告显式保留原失败、不均匀计数和跨结束边界的完成通知，`recover.py --check`通过；它只确认观察事件与20个源码调用的一致性，`independent_kernel_coverage=false`，不能据此把包络外时间计为GPU空闲或新增速度收益。整模型ETW进程10888已在现有共享锁队列，未重复启动采集。

同次进程核对中，原append质量进程19012及后续控制进程已结束，尚无本组冷/热质量记录、整模型API记录或可选配置；此前“等待质量进程”的叙述不代表它们仍在运行。只读重查确认当前评分入口、已确认测速、共同基线及参考评分前置条件通过，合并来源哈希无不符，但这不说明原进程的退出原因，也不替代质量验收。本轮未重新排入质量任务、修改冻结脚本或默认配置。

10:04续跑：再次确认没有本组存活质量进程或既有质量目录后，使用原`quality.py`重新排入唯一质量进程6892，记录创建时间并保留`resume-quality-1004.log/json`。脚本未修改，冷/热两阶段仍在同一个现有GPU lease内执行。后续控制只等待这个进程句柄；要求退出0、冷`fixed_targets_passed`、热`complete && passed && !skipped`以及排队时冻结的评分/trace/profile/server来源哈希全部通过，才依次复核质量、执行及复核真实模型API诊断、执行及复核实际服务器引擎和可选配置发布。任一步失败即停止；默认配置不变。本次仅恢复已终止的采用链，没有重跑已确认测速或复制现存质量队列，尚无新质量结果。

11:47补充：Windows系统事件12/13及启动时间确认机器在本地10:52、11:01重启，6892质量进程、28356后续控制及只读DLL观察进程均已不存在，相关工具会话也已丢失；本组冷/热、模型API及服务器验证结果仍不存在。日志只保留等待共享锁记录，不能写成质量已执行或失败。本轮没有清理其他任务的lease；重启后已有新ETW进程11612及MMVQ质量进程12220，保持其队列。

重排前只读复核原冻结评分入口及原采用链来源、已确认测速、497项合并来源/DLL哈希、共同环境和质量目录不存在条件均通过，A770驱动仍为32.0.101.9034。新`resume-post-reboot.py`已启动控制进程23316（UTC03:47:06.8569520），仅排入缺失质量进程15632；它仍通过原`quality.py`取得一个共享lease依次进行冷/热质量。`resume-post-reboot.json`冻结控制进程身份、当前启动时间、来源和原续跑记录，独立state/log记录每步PID与退出码。冷/热通过原严格布尔条件后，才运行原质量复核、原模型API诊断及复核、原服务器smoke及复核。已有测速不重跑，默认配置不改。

实际DLL观察补足了原profile脚本只哈希预期文件、未枚举实际加载模块的缺口。`dll-witness.py`通过本次新记录绑定控制进程、profile父子关系、创建时间、实际EXE/完整命令行，以及三个实际加载DLL的路径与哈希；原来绑定28356的观察协议保留，本次使用独立`dll-witness-protocol-post-reboot.json`和`dll-witness-post-reboot.json`。观察者只在真实服务器smoke前启动，不另发模型请求；最终`--check`还要求profile完整通过。原profile成功本身不替代这份实际模块见证，新链任一步失败仍停止采用。

```powershell
python -X utf8 build-sycl-a770-runtime-profile-20261029/dll-witness.py --resume build-sycl-a770-runtime-append-model-20261029/resume-post-reboot.json --self-check
# 仅在上述实际链完成后复核，不启动模型：
python -X utf8 build-sycl-a770-runtime-profile-20261029/dll-witness.py --resume build-sycl-a770-runtime-append-model-20261029/resume-post-reboot.json --check
```

map后续排查已汇总到[MAP_OPTIMIZATIONS.md](../../../docs/MAP_OPTIMIZATIONS.md#follow-up-all-map-sites-and-the-a770-runtime)：覆盖工程各后端容器、Python缓存和选定UR/NEO提交/内存索引。公开NEO的主USM索引已是排序向量，map用于延迟释放等其他用途；当前SYCL verifier已用描述符缓存与图数组。待定位实际关键路径后，优先考虑UR重复查询合并及带设备校验的少条目快捷路径，再评估NEO偏移查询、分配ID间隙遍历。保留节点地址的间接分配记录、GPU异步写入的时间戳值不能直接迁到移动元素的flat容器。19份公开来源及LF/CRLF对应关系经远端及本地检查通过；该源码排查没有新增内核或整模型吞吐结论。

12:00续记：原23316控制及15632质量进程在同一次系统启动中已确认不存在，工具会话也不可用，退出原因未知；state停在排队，冷/热质量目录仍不存在。原record/state/log保留，此次不能归因为再次重启，也没有重跑已确认测速。工作区已有独立任务恢复已停止ETL的导出，以及MMVQ质量进程23192；本链未更改其lease或重复启动它们。

新增14行`resume-durable.py`复用原控制器，仅选择新的record/state并冻结自身来源；自检通过后用Windows `Start-Process -WindowStyle Hidden`启动，stdout/stderr单独保留。控制进程20756（UTC04:00:01.3149200）及质量子进程19128在启动shell退出后由原生进程查询确认存活，当前等待23192持有的共享lease。`resume-durable-interruption.json`记录本次同boot中断，`resume-durable.json`记录新控制身份；它继承的旧重启字段描述原10:04链，不作为本次退出原因。原质量、模型API、服务器及DLL检查全部复用，任何一步失败即停；目前没有新增质量通过结论。

后续实际DLL见证的最终复核应绑定新的控制记录：

```powershell
python -X utf8 build-sycl-a770-runtime-profile-20261029/dll-witness.py --resume build-sycl-a770-runtime-append-model-20261029/resume-durable.json --check
```

上述命令仅在实际服务器步骤完成后才应通过，不启动新模型请求。当前任务按Code或Prose任一场景独立≥1%、相邻基线漂移≤3%及原质量/历史/显存/实际配置条件验收；旧文档中的≥20%、35/40 tok/s及Linux方向不增加本次完成门槛。

12:23续记：20756控制及19128质量进程已退出，`resume-durable-state.json`记录原质量入口exit1。它拿到lease后、创建质量目录或模型进程前，在来源哈希循环触发`UnboundLocalError`：`main()`的自检分支给局部变量`b`赋值，遮蔽了正常分支使用的模块`b`。该失败有明确脚本原因，不是新重启、模型质量失败或GPU超时。原记录和日志保留；冷/热结果仍不存在。

停止后的原`quality.py`逐字节保留为`quality-before-shadowfix.py`，哈希仍为`4b42e9e7f9026d3c74f72094581ff25b5d7d5a3caaa70ed779fd6ed4a8baa08c`。修复只重命名自检的环境字典，并在该分支执行同一来源哈希循环，回归自检通过。`resume-shadowfix.py`验证旧哈希和这四处精确文本变化，复用原控制器全部冷/热、模型API、服务器及DLL门槛；新record/state冻结修复源码、旧源码和已结束的失败记录。模型、运算、评分参考和验收条件均未改变，没有重跑已确认测速。

新控制13268（UTC04:20:57.0979230）、质量子进程9928（UTC04:20:58.6495330）在启动shell退出后仍由原生查询确认存活，等待21088持有的共享lease；未清理或修改它的锁。新`resume-shadowfix.stderr`为空，旧错误仍留在共用追加日志`resume-post-reboot.log`，不能把旧尾部回溯误认作新进程再次失败。最终DLL复核改为绑定最新记录：

```powershell
python -X utf8 build-sycl-a770-runtime-profile-20261029/dll-witness.py --resume build-sycl-a770-runtime-append-model-20261029/resume-shadowfix.json --check
```

ETW离线恢复导出和统计已通过，丢事件/缓冲均为0；但原全记录关联审计exit1，失败文件`audit-recovered-failure.txt`保留。context `0xffffc303ee542010`的queue sequence1446在11944843–11945951µs出现抢占及恢复，两次DMA ID为36871/36874；它早于decode标记148764967µs。独立`audit-decode-windows.py`通过CPU自检后，仅排除完整时间包络与两个decode窗口都不相交的context/sequence历史，再调用未修改的原关联器；跨窗口、整个窗口的包络及重复端点均保留。真实专项检查仍exit1：Prose区间175130921–180014372µs内，sequence61786的DMA ID98393在179735408µs以`bPreempted=true`结束，随后ID98396恢复并正常完成，原“一次Queue对应一次DMA执行”假设不成立。专项stderr保留，未生成通过结果；没有放宽断言、重新采集GPU或报告空闲时间/重写runtime收益。后续若继续关联，应按实际submission/completion ID区分抢占后的多个执行片段。

另一路合法device-counter MMVQ的四个冷评分进程已完成，`quality/results.json complete=true && summary.fixed_targets_passed=true`，全部9场景/758目标与原参考精确一致、必需概率有限。这只补齐该候选的冷评分门槛，不替代本runtime链；该MMVQ的新独立测速进程22820已排队，完整热历史和实际配置仍待验证。

12:41续记：MMVQ的22820测速现已正常结束，三进程完整记录及独立`performance.py --check`通过，但候选未通过初筛：Code−0.782504%且基线耗时漂移−4.125093%，Prose−1.787474%且漂移−1.437148%；无合格场景。三个进程最低采样空闲显存1004.680/1002.078/975.578MiB也低于1024，不能进入确认或采用。详见[合法MMVQ初筛](neo-igc-hotspots-2026-10-29.md#合法device-counter-mmvq的新基线初筛不采用)。其共同环境额外含`SYCL_CACHE_PERSISTENT=0`，不与本runtime cohort混算或转移结论。

本runtime控制13268/质量9928在UTC04:40原生查询仍存活，新stderr为空，当前等待21136的down-prefix-repeat任务；该任务有实际模型子进程22092，未修改它的lease。只读AST比较确认修复前后质量脚本除自检分支外完全一致；已确认测速的来源清单也不包含后来新增的质量wrapper，旧性能证据未被本次修复改写。热历史先调用`cold.load_speed()`重建恢复证据并绑定正确cohort日志，再执行原`m.check(speed)`，无需再次修改该路径。默认及可选配置均未切换，本组冷/热、模型API、服务器和实际加载DLL的完成证据仍待产生。

13:19补充：[按真实执行ID处理抢占/恢复的离线ETW审计](graph-etw-preemptions-2026-10-29.md)已在同一保留trace上exit0，独立端点扫描重算也通过，原失败记录保持。Code为1015个队列/1015个DMA执行段；Prose为1057个队列/1061个执行段，4次恢复间隔合计390µs。Prose另有81.964ms未被所选进程执行段并集覆盖的突发区间，尚未定位原因。它们是调度事件区间，不能当作GPU空闲、可删除驱动耗时或runtime改写收益上限。本项不增加模型采用门槛。

原生查询确认13268控制、9928质量以及后置的signed-grid实验19300仍存活；质量尚未创建结果目录，当前等待21464持有的共享lease及外部编译完成，新控制器stderr为空。此前21784的FP16值存储候选在独立确认阶段因检测到外部编译而退出，不能将其初筛信号记为独立通过，也不替代本runtime证据；未重启、修改或清理其他任务。runtime冷/热、模型API、服务器、实际DLL见证和可选配置仍待完成。

13:58续记：13268/9928已结束，原state记录UTC05:31:36质量exit1；失败在创建质量目录及模型启动前。494项原性能文件中493项不变，唯一变化为基线`model.map`，另一候选的链接命令更换OUT却沿用了基线/MAP路径。实测EXE仍严格匹配SHA256 `54f124e2da6a142ed845d686feadbdbd06c1b52c47c563c48575b8d47f633fbe`；全部原DLL检查通过。原历史完整清单现在不匹配，未改写该清单或旧性能记录。

`audit-runtime-inputs.py`仅将这一个已知非执行链接输出单列，记录原摘要、覆盖摘要、归档副本和事件说明，其余输入、DLL、完整输出、计时、显存/频率及原错误目录复现仍执行原检查。新执行输入报告`runtime-inputs-recovered-results.json`及独立`--check`通过，复核的仍是原Code+2.668767%/Prose+2.901650%、最低采样空闲1597.504MiB，未新增测速或质量结论。

排队前进一步检查发现，冷评分会从旧build及quality清单再次继承该map条目；现用同一严格单项分离函数处理合并后的清单，并逐项预检通过。修复前源码保留为`quality-before-runtime-inputs.py`，摘要等于旧控制记录冻结值。`resume-runtime-inputs.py`验证这三处精确来源改动，复用原冷/热、API、服务器及DLL步骤，并使用独立日志、record/state；新字段明确此次为debug-map覆盖恢复，不是再次重启。

新控制22812（UTC05:56:46.7759370）、质量子进程6348（UTC05:57:02.809）在启动shell退出后经原生查询确认存活，当前等待22572的现有lease。冷/热质量仍未产生，通过后才运行后续门槛；可选配置尚不存在。最终DLL复核必须绑定新记录：

```powershell
python -X utf8 build-sycl-a770-runtime-profile-20261029/dll-witness.py --resume build-sycl-a770-runtime-append-model-20261029/resume-runtime-inputs.json --check
```

14:18续记：上述22812/6348已结束，第一次实际A1仅有4015槽（READY空闲3071MiB），在评分前被固定6297槽断言拒绝；模型exit0，原文件逐字节移至`quality-capacity-failed-4015/`。不能把较少缓存下的记录替代测速cohort。只读系统计数器另观察到桌面及浏览器占用，以及公共`native_grouped_parity.exe`与其他模型并行；它们不能单独证明第一次容量差异的确切原因。

`quality-capacity-guard.py`保留原来源、评分和容量条件，额外检查工作区公共EXE。第二次21472/22064的A1已达到6297槽、READY空闲1375MiB，Short16完整输出与原参考一致；正式9场景评分前外部icx/clang开始，原busy检查抛错，链停止，模型正常exit0。文件另存`quality-compiler-failed/`，两次失败及来源记录均保留，没有质量通过结论。

对不接受任何测速结果的质量阶段，新增`quality-wait.py`在既有busy检查点等待GPU/编译空闲，再发下一个请求；不改变分数、槽数、显存或协议门槛，不吞掉其他错误。保留已加载模型可避免每次干扰都重载33GiB专家。模拟忙一次后成功、参数转发和非busy错误传播自检通过。它不为计时阶段接受编译重叠，亦不保证能阻止不使用共享lease的外部进程启动。

新控制21488（UTC06:17:18.0430920）、质量子进程23852（UTC06:17:35.765）在启动shell退出后经原生查询确认存活，目前等待3216持有的共享lease。完整冷/热、API、服务器及DLL门槛继续原链；最终见证应绑定`resume-quality-wait.json`，历史旧命令不再代表本次控制身份：

```powershell
python -X utf8 build-sycl-a770-runtime-profile-20261029/dll-witness.py --resume build-sycl-a770-runtime-append-model-20261029/resume-quality-wait.json --check
```

14:36质量进展：23852已持有共享lease，A1模型20048正常exit0，实际6297槽、READY空闲1333MiB。9场景/758目标已跑完，无非有限评分，Short16与冻结原输出一致；独立只读调用原`q.parse`重解析已关闭的`A1.logpos.tsv`，去除Initializer后，整个评分记录与`score_reference`精确相等。此时`partial.json`在finally落盘，尚未保存随后计算的`quality`字段，不能将该字段暂缺误读为评分丢失。完整四进程最终审计仍由原入口执行。

原生查询确认控制21488、质量23852及候选B1模型24268仍存活，进入runtime候选的评分；没有重跑已确认测速。IQ2_XXS两条复用20500继续等待本质量lease，尚未编译或产生GPU结论。A1的单臂通过不替代B1/B2/A2、热历史、模型API、服务器或DLL门槛。

14:42续记：B1模型24268已正常exit0，9场景/758目标的原LP文件再次用原解析器独立重读，与冻结原评分记录精确相等，必需概率有限、Short16一致、6297槽及READY≥1024均满足。B1的`quality`字段在进入B2首场景保存前同样暂缺，原LP重解析已确认不是评分不等；最终仍等待原四进程审计。B2模型20164已启动，6297槽、READY空闲1298MiB，控制21488/质量23852继续存活。

预检后续DLL观察代码时发现，原观察器把profile的所有子进程都当作模型候选；profile取得lease前后的临时PowerShell查询也可能进入此分支。保留原冻结观察器不变，新增只读`dll-witness-engine-only.py`，仅在原进程查询中加实测EXE绝对路径筛选，保留profile父进程、原生创建时间、完整命令行、三个DLL路径/摘要及最终profile通过条件。实际PowerShell谓词自检保留父进程/目标EXE、排除shell和其他EXE，原错误路径/摘要/进程祖先等自检全部通过。

只读观察8472（UTC06:40:58.8525650）已在启动shell退出后确认存活，绑定当前21488控制记录，使用独立protocol/result路径；不启动模型、不占GPU lease或改写原控制器来源。尚未捕获服务器模型，不将准备工作记为DLL通过。此观察结果只能在profile完成后通过以下检查：

```powershell
python -X utf8 build-sycl-a770-runtime-profile-20261029/dll-witness-engine-only.py --check
```

15:15续记：本次冷质量四轮全部9场景/758目标精确等于冻结参考、PPL变化0；热历史四轮均正常exit0，每进程1040完整输出token匹配参考，最终256条评分四轮精确一致。冷READY最低1269MiB，热最低1273MiB，均6297槽。原`quality.py --check`独立复核通过。

原控制21488在质量子进程exit0后，因直接读取成功记录省略的可选`hot['skipped']`而exit1；`resume-quality-wait-state.json`与stderr原样保留。它没有进入API/服务器阶段；原只读观察8472随后因控制进程结束而退出，没有DLL通过结论。新`resume-quality-done.py`只接受这个已记录失败及已经通过的冷/热记录，将字段读取改为`get('skipped',False)`，显式True仍拒绝；保留原质量复核、API、服务器及DLL门槛，不再运行八个模型质量进程。CPU检查通过后，续接24104（UTC07:14:29.0341710）已在启动shell结束后经原生身份核对存活，当前等待既有lease。

后续DLL观察复用已检查的engine-only筛选，绑定新控制记录，独立保存在`dll-witness-quality-done*.json`；原错误路径/摘要/祖先检查及实际PowerShell筛选自检均通过。原观察及控制记录不改写。最终复核使用：

```powershell
python -X utf8 build-sycl-a770-runtime-profile-20261029/dll-witness-quality-done.py --resume build-sycl-a770-runtime-append-model-20261029/resume-quality-done.json --check
```

15:45续记：真实模型API诊断已完整exit0，随后原`run.py --check`独立exit0。两进程均6297槽，READY空闲1337/1414MiB，Short16及原Code/Prose2K各完整128输出全部匹配。全程实际API入口计数：默认append0/queue-execute192944/event-reset-append11533，候选514/0/0；候选in-order及counter见证均为True。计数含初始化、prefill、decode和清理，不是单次decode次数；`UR_L0_DEBUG=1`的诊断时间不用于任何速度结论。A/B原始stderr分别862635362/618968142字节，协议、输出和来源哈希由原检查覆盖。

服务器profile4768（创建UTC07:36:19.940）、只读DLL观察23616（07:36:19.805）及控制24104经原生身份复核仍存活；当前等待独立HC算子测速19124的共享lease。没有启动第二组服务器、重复质量验证或改动该任务的锁。可选配置尚未生成，服务器输出及实际DLL加载仍待验收。

18:01续记：原服务器profile4768已exit0，`runtime-profile-20261029/results.json complete=true, passed=true`，实际6297槽、READY空闲1829MiB。通过实际server接口的Short16、Code2K128、Prose2K128输出全部匹配，原入口已生成独立可选`strata-iq2_xs-runtime-append.json`；默认配置不变。本项是服务器验证，不是新测速。

DLL观察23616始终未保存实际子进程见证；控制器在profile完成后等待60秒超时，已自动结束自己的观察器并记录失败，`resume-quality-done-state.json complete=false`。具体发现失败原因尚未定位，不能用服务器输出通过替代DLL见证，也不把整个runtime采用链标为完成。原日志、profile及可选配置均保留。

新`build-sycl-a770-runtime-profile-direct-20261029/run.py`保留原server验证及全部输出/6297槽/READY门槛，在实际server构造函数返回后直接使用其持有的`Popen`子进程PID查询DLL。记录父/子原生创建毫秒、完整命令行、查询前后子进程身份，以及三个实测runtime DLL路径和SHA-256；不再通过独立轮询发现profile。新结果和配置副本写入独立目录，必须与原可选配置完全相等；不覆盖旧结果或重跑冷/热质量、API诊断和测速。

原生查询及错误父子关系、命令行、PID重用、缺失/重复/错误路径/错误摘要DLL的CPU自检通过。隐藏worker13868、创建1793268074841ms，已在启动shell退出后确认存活；精确等待GR复测18296/1793267129481，然后持共享锁完成所有大型摘要和服务器采集。尚无新服务器/DLL通过结果；本会话35 tok/s目标仍未达成。

```powershell
python -X utf8 build-sycl-a770-runtime-profile-direct-20261029/run.py --self-check
# 仅在新服务器/DLL任务完整结束后复核，自动持共享锁：
python -X utf8 build-sycl-a770-runtime-profile-direct-20261029/run.py --check
```

18:15接入验收：本项的`build-sycl-a770-runtime-profile-20261029/server-dll-recovery.py`已完成同一可选配置的服务器补验，随后独立`--check`也exit0。它复用原服务器参数/环境及完整请求检查，保留已经通过的测速、冷/热质量和实际模型API记录；只补跑一次服务器引擎来取得同一实际子进程的DLL证据，不把补验时间用作新测速。

已在冻结观察器的编译后字符串中复现漏捕获原因：把Windows路径直接插入Python源码后，`\build`、`\baseline`、`\bin`中的`\b`变为退格字符`0x08`，实际过滤条件永远不能匹配正确EXE。旧自检执行的是未编译的独立谓词，漏掉了这一层。旧观察器、控制器超时、服务器通过记录及已生成配置全部保留；新的CPU检查覆盖编译后的错误字符串。第一次补验启动还发现Windows命令行尾随空格，已用实际启动记录加入检查，使用`rstrip()`后验证脚本路径；该启动失败发生在加载模型之前，源码及stderr另存保留。

补验worker23640（原生创建1793268674172ms，父进程4040）实际正常exit0。在`server.StrataEngine`构造完成、实际READY之后，同步查询其持有的`Popen`子进程15968：核对原生父子关系、完整引擎命令行、查询前后创建时间，以及`sycl8.dll`、`ur_loader.dll`、`ur_adapter_level_zero.dll`三个2025.3路径和SHA256；全部通过，避免异步发现进程的竞态及路径插值。随后Short16、Code2K128、Prose2K128完整输出匹配，实际6297槽、READY空闲1856MiB、引擎exit0。记录位于`server-dll-recovery-v2/protocol.json`、`dll.json`、`results.json`及同名stdout/stderr；来源和配置摘要由最终检查覆盖。

原`strata-iq2_xs-runtime-append.json`内容保持一致，默认`strata-iq2_xs.json`摘要仍为`3d01156599ac679d0685d823bd18ed7b0392bd118be89baf7c0c641a392b1645`。本项已经满足单场景独立≥1%的优化验收及实际可选配置接入；另一工作流的`runtime-profile-direct`与35 tok/s目标继续单独记录。最新接入复核命令为：

```powershell
python -X utf8 build-sycl-a770-runtime-profile-20261029/server-dll-recovery.py --check
```

18:18续记：共享工作区另一条`server-dll-recovery.py`接续已采用相同的同步拥有进程查询，worker23640/1793268674172已持锁运行。因此先核对原生身份、等待状态、来源摘要、没有GPU请求和对方真实lease后，仅撤下本轮重复worker13868；其源码和原waiting记录保留，取消原因见`runtime-profile-direct-20261029/superseded.json`。不再恢复这一重复服务器任务。

现有接续的`server-dll-recovery-v2/results.json complete=true, passed=true, exit_code=0`：6297槽、READY空闲1856MiB，原三组服务器输出全部匹配；`dll.json`实际记录子进程15968的sycl8.dll、ur_loader.dll、ur_adapter_level_zero.dll。原入口末尾检查已打印`PASS actual server outputs/config and synchronous native ancestry/command/all3 DLL checks; no speed inference`。本次只读核对小型终态和日志，未在另一个任务计时期间重算大型摘要；既有测速、质量和目标结论没有被这次服务器补验扩大。
