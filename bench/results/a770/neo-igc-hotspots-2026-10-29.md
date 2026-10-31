# Windows NEO / IGC decode 热点与待优化项目

当前证据支持两条主线：NEO/UR 的 graph append 路径已独立确认约3%的整模型组件收益；IGC 全局256 GRF净收益不成立，转为按确有 spill 的 MMVQ、GDN 和其他格式 GU 做局部归因，再检查主要专家算子的访存与指令调度。运行时采用仍待质量、完整热历史和实际配置验收。

当前任务接受Code或Prose任一场景吞吐提升≥1%、相邻基线耗时漂移≤3%、同一场景独立复测通过；小于0.3%按噪声解释。质量、完整热历史、采样空闲显存≥1024MiB和实际配置接入仍须分别通过。不要求Code/Prose联合通过，也不必先组合多个候选；使用同一冻结负载实测，不能乘算组件收益。旧的≥20%、35/40 tok/s和后续Linux验证保留为历史方向，不增加本次“单场景过也算”的完成门槛。

热点分析复用已冻结的原始 GPU CSV，核验正常 graph 的 CPU 栈，并完成当前主要 GU 及六个有 spill 热点的原生代码生成核验。运行时 append 组合已完成无剖析整模型独立确认，Code/Prose 分别为 +2.67%/+2.90%；全局 GRF256 筛选为 +0.64%/−0.15%，不进入原样确认。机器为 Intel Arc A770 16GB、PCIe 3.0、Windows、驱动 32.0.101.9034、DPC++ 2025.3.2、VTune 2026.4。负载为 Code/Prose 各 2048 输入、完整 128 输出，实际 6297 个专家槽，实验 min-p 0.5；生产 min-p 0.8 的分布仍需单独采集。

## 已测得的计算热点

这些数字来自 **eager 诊断采集的累计 kernel 时间**，合计 9.572915 秒、260498 次 kernel。它们用于排优先级，不能视为正常 graph 的 decode 时间占比，也不能据此认定驱动提交了同样次数的命令。两次被采集 decode 共 112 个 verifier 窗口；完整输出和采集边界已有独立核对。

| 热点 | 累计 kernel 时间 | 比例 | kernel 次数 | 直接观察 |
| --- | ---: | ---: | ---: | --- |
| 专家 GU → SwiGLU/Q8 → Down | 3.440626 s | 35.94% | 16128 | GU 24.19%，Down 11.33%，SwiGLU/Q8 0.43% |
| 量化 MMVQ，合并所有变体 | 2.312748 s | 24.16% | 37079 | 原先只统计 wide 的 15.40%，遗漏了其他 MMVQ；部分变体有 spill |
| GR up/down-slice/down-reduce/norm | 1.305024 s | 13.63% | 45715 | 无 spill；平均约 28.5 µs/次，需检查图内调度与依赖 |
| 其余 kernel | 1.156355 s | 12.08% | 115109 | 平均约 10.0 µs/次；包含采样、规划、值追加等，不能作为单一算子优化 |
| GDN 全部变体 | 0.700587 s | 7.32% | 23610 | step/norm 与 ab 存在 spill |
| BF16 GEMV | 0.354458 s | 3.70% | 15831 | 当前行均无 spill |
| QSA block top-k | 0.155011 s | 1.62% | 1344 | 工具报告 spill 11840 bytes |
| Router | 0.148106 s | 1.55% | 5682 | 工具报告 spill 4512 bytes |

主要 IQ2_S GU 为 WG256、SG16、128 GRF、零 spill，各工作规模的 XVE stalled 为 41.3–80.3%。Down 同样是 128 GRF、零 spill。高 stalled 支持检查访存和依赖，但不能独自证明 PCIe 饱和或 TLB miss。

有 spill 的 kernel 合计占累计 kernel 时间的约 31.94%；**这不是 spill 开销占 31.94%**。优先检查的具体实例：

| kernel / 变体 | 累计时间 | 比例 | 工具报告 spill bytes |
| --- | ---: | ---: | ---: |
| native_gu_port `<16,16,32,false>` | 0.521268 s | 5.45% | 576 |
| gdn_step_norm_multi | 0.492454 s | 5.14% | 992 |
| native_mmvq_wide / WideIQ4XS / NC4 | 0.470221 s | 4.91% | 608 |
| native_mmvq_wide / WideIQ4XS / NC3 | 0.217085 s | 2.27% | 416 |
| launch_mmvq / type21 / NC4 | 0.198519 s | 2.07% | 1504 |
| qsa_block_topk | 0.155011 s | 1.62% | 11840 |
| gdn_ab_multi | 0.154382 s | 1.61% | 1440 |

## 已补齐的正常 graph CPU 证据

复用工作区已完成的 `build-sycl-a770-runtime-graph-cpu-sw-20261029/` 采集。本轮重新运行其离线 `audit.py`：核对相同 ITT-only EXE、五条完整输出、6297槽、四组48层扫描、软件采样配置、来源哈希和实际任务边界，全部通过。第4/5次 Code/Prose decode 的任务时间分别为4.449106/4.792025秒，与原生标记相差均不足0.04毫秒。

| CPU采样项目 | 结果 | 范围 |
| --- | ---: | --- |
| 总活跃CPU时间 | 9.082244 s | 包括任务外样本 |
| 两次decode任务内活跃CPU时间 | 9.003517 s | Code4.337204 + Prose4.666313 |
| 任务外活跃CPU时间 | 0.078727 s | 不归入decode |
| 工具分类的Spin Time | 4.749826 s | 总采样范围，不等于可节省wall time |
| `urQueueFinish` 两条调用链的inclusive CPU份额 | 93.459599% | 总活跃CPU样本；经实际legacy adapter进入驱动等待链 |
| `NtYieldExecution` / `RtlQueryPerformanceCounter` 自身CPU时间 | 4.674389 / 1.206200 s | 已导出函数，不能猜测匿名驱动内部实现 |

这使“当前使用哪套adapter、CPU活跃时间集中在哪里”有了直接证据。**CPU等待可与GPU执行重叠，93.46%不是可删除的decode耗时。** 尚需正常graph的API时延、调用次数和GPU空隙才能估算关键路径收益。`results.json complete=false` 是最后的 `-report tasks` 不受本版VTune支持；collect及其他四项报告均exit0。随后只从同一trace导出task热点与top-down，两项均exit0；独立审计同时检查原采集和恢复导出，未重跑模型或隐藏失败。

本轮进一步将应用侧匿名地址对应到冻结对象的函数符号。`build-sycl-a770-graph-wait-callers-20261029/check.py` 读取同一EXE、原始COFF对象和已核对的top-down报告；对每个地址比较128字节代码前缀，只忽略链接重定位覆盖的字节，每项仍比较108–128个有效字节，四项均唯一匹配。结果及输入哈希保存在同目录 `results.json`，不启动GPU：

| 栈中地址 | 对应冻结对象/函数 | 已采集的inclusive CPU份额 |
| --- | --- | ---: |
| `0x14011c0b0` → `0x140121be0` | `verify.obj` 的 `Verifier::run` → 其 `lambda_3` → queue wait | `urQueueFinish` 子链82.004299% |
| `0x140108b90` | `mtp-last-draft.obj` 的 `MtpDrafter::draft` → queue wait | `urQueueFinish` 子链11.455300% |
| `0x14005f5e0` | ITT-only `generate.obj` 的 `main` 内 `lambda_96` | 另一路event等待，未计入上述两项 |

这给出了函数级调用点，不能据此前缀匹配声称精确源码行。冻结 `Verifier::run` 源码的窗口末尾会等待GPU完成；后续读取、草稿决策和状态更新仍有数据依赖。第三层若要删除等待，须先证明该处结果可晚读、控制流可迁移或已有等价依赖，而不能把93.46%的等待样本整体视为重复流程。仅替换等待API也未必减少GPU计算时间。

另已重新核对 `build-sycl-a770-runtime-append-trace-20261029/audit.py`。其小图每进程21次完整向量更新均通过：默认配置全程64次传统队列提交、44次事件复位append；immediate/in-order/counter/graph-append组合为21次immediate append、零传统队列提交、零事件复位append。日志启用验证层，不计时，包含初始化与清理；这些不是完整模型的调用次数。对应完整模型对照 `build-sycl-a770-runtime-append-model-20261029/run.py` 已由现有任务执行，本线程不重复启动。该collector的1%筛选仍须同场景独立确认，并完成质量、热历史、显存与实际配置验收。

### 第二层首轮实测：graph immediate append

该任务的A1/B1/A2均已结束且exit0；本轮只复核已完成的这三个进程。两臂使用同一device-counter EXE（不同于GRF256筛选的冻结EXE），固定6297槽、tail64、min-p0.5、原始2048输入/完整128输出，无profiler、API日志或验证层。每进程Short16初始化后，两场景各预热一次、交错保留三条请求。候选只改变2025.3 legacy adapter的immediate/in-order/counter/graph-append组合。

| 场景 | 邻接基线几何均值 | 候选保留请求中位数 | 首轮吞吐变化 | 基线耗时漂移 |
| --- | ---: | ---: | ---: | ---: |
| Code2K | 4367.347 ms | 4249.800 ms | +2.765941% | -0.235563% |
| Prose2K | 4720.499 ms | 4607.400 ms | +2.454723% | -0.135487% |

27条完整原始输出、DONE计时字段、READY槽数/余量、24组48层预填扫描及来源哈希均已复核。复核器最初误将DONE限定为9字段，实际协议另含扩展字段；已改为读取必需字段并比较完整原始DONE行，自检覆盖扩展字段、缺失字段及计时篡改，真实三进程协议复核通过。该检查器错误不作为模型运行失败。

独立A3/B2/A4确认已完成，三个新进程均exit0；每场景每进程保留三条请求：

| 场景 | 独立确认邻接基线几何均值 | 候选保留请求中位数 | 独立确认吞吐变化 | 基线耗时漂移 |
| --- | ---: | ---: | ---: | ---: |
| Code2K | 4366.400 ms | 4252.900 ms | +2.668767% | −0.022900% |
| Prose2K | 4736.048 ms | 4602.500 ms | +2.901650% | −0.162451% |

六进程共54条完整原始输出、6240个输出token、48条完整128输出请求及48组48层预填扫描均通过复核；48条完整请求均有显存与decode频率记录。最小采样空闲显存1597.504MiB，decode频率样本均为2400MHz。采样不能证明瞬时峰值或无换页。

原collector的六个模型进程均成功，但最终显存导出失败：导入的 `memory_coverage` 函数仍引用另一个实验目录。原始 `performance/results.json complete=false` 和原 `run.py --check` 失败保留。`build-sycl-a770-runtime-append-review-20261029/recover.py` 仅在离线导出时临时绑定该函数到实际日志目录，随后恢复；不改解析器、时间边界、验收门槛或原报告，无GPU/编译器重跑。恢复报告 `results.json complete=true`，原完整检查和全部原始协议重放均通过；`recover.py --self-check` 还复现错误目录失败并验证绑定恢复。

这确认了约3%的运行时组件收益，**尚未生产采用**。质量/历史和实际配置仍须单独验证；后续若组合运行时与IGC，须在同一EXE上重新实测，不能直接相乘当作最终结果。

## 按 ezpp 首页的四个层次展开

依据 [ezpp 首页“性能优化的几个层次”](https://github.com/ez8-co/ezpp/blob/236b731286baf4978288f881e274b68857a61d56/README.md)：语法层；算法与流程层；架构层；硬件 scale-out/scale-up。原文共同原则是“基于真实的度量”。已核对远端 HEAD 为此提交，并在本地保留原文及 SHA256。以下按其含义组织工作；不是要求为每一层都新增代码，也不新增生产分析器依赖。

### 共同前提：先建立当前路径的真实度量

正常 graph 的 CPU 栈已按上节补齐；接下来补原生 API 时延与 GPU 队列空隙。采集复用已核对的 ITT-only EXE，初始化与预热暂停，仅采集 Code/Prose 的第4/5次 decode，完整输出、6297slots 和四次48层预填扫描均通过核对。

首轮 `build-sycl-a770-neo-graph-cpu-profile-20261029/run.py` 已失败，标准输出/错误为空；后续检查trace内部配置与 `perfrun` 日志确认：可选characterization添加了缺值的 `--pmu-type`，采集器在模型启动前拒绝参数。成功采集仅增加 `-knob enable-characterization-insights=false`，保留软件CPU栈和正常graph。此前准备的V2预检任务仍在排队、无子进程、未启动采集或模型时，已因这份经审计的等价证据取消；记录在 `build-sycl-a770-neo-graph-cpu-profile-v2-20261029/superseded-by-verified-graph-cpu.json`，不再重复占用GPU锁。

这一轮 CPU sampling 只能回答活跃 CPU 时间花在哪里；阻塞等待、API 调用延迟、WDDM/GPU 排队仍需其他证据。不能把全部未归因 wall time 算作 NEO 开销。已有 ezpp 适合主机大阶段，微秒级 kernel 与驱动调用使用 VTune、原生事件或 QPC。

已检查本机的历史 CPU 采集，不能用于填补当前 graph 的占比：

- `2026-10-14-a770-hc-cpu-runtime` 是单独 HC worker 的普通 in-order 提交，10ms CPU 采样。queued / wait-each 的驱动加 loader 分别占73.027% / 82.240%，但无整模型或 graph。`zeKernelSetArgumentValue`、`zeCommandListAppendLaunchKernel` 与 malloc/free 支持检查基础实现及批量流程，不能将这些百分比套到已捕获的 graph。
- `build-sycl-a770-recheck-20261023/baseline-stall-vtune` 是旧 EXE、cache4000、reserve1536 的超时诊断；原请求仅完成两条短输出，2K请求未完成。其 CPU 栈包含 oneMKL GEMM 与共享/主机内存分配，不能作为当前 warm decode 的热点证据。

当前采集已完成上述任务边界和调用链核对；历史“驱动很忙”的结论没有用于替代这次检查。

已核对本机 NEO DLL 的 SHA256 与 RSDS/PDB 身份。所查询的三个公开符号地址未返回匹配 PDB（Intel地址403，另外两处404）；这不证明所有符号服务均不存在该文件。后续可先按模块、导出 API 和调用者归因，不能把匿名函数猜成某个内部锁或等待实现。

| 层次 | 对应当前热点 | 首批工作 | 继续实施的依据 |
| --- | --- | --- | --- |
| 第一层：语法与基础实现 | 有 spill 的 MMVQ、其他格式 GU、GDN；NEO/UR 热路径中的拷贝、分配、临时对象 | 先检查实际机器码的寄存器活跃区间、spill、load 和指令依赖；CPU 栈确认热点后再检查临时容器与拷贝；逐 kernel 对照默认/自动/256 GRF | 源码或选项确实消除了工作，原生指令与资源相符，完整数值校验和无剖析实测通过。主要 IQ2_S GU/Down 无 spill，不全局增加 GRF |
| 第二层：算法与流程 | MMVQ 24.16%、专家链 35.94%、GR 高频调度，以及 graph 提交/事件路径 | 查批量提交、事件复用、依赖与屏障、host/device 数据访问顺序；用 legacy/V2、in-order/counter event、immediate append 做定位明确的单变量对照 | API 次数、CPU 时间或 GPU 空隙减少，正常 graph 的关键路径缩短；不以 eager 次数代替 host 提交次数，不重复已失败的 WG/双缓冲方案 |
| 第三层：架构，删除不必要流程 | 重复事件复位/完成通知、重复驻留/映射、重复描述符处理 | 先列出“仍执行且可以证明多余”的环节；与第二层区别是整条路径能否不再执行，而非只把它做快 | 调用链与资源生命周期证明被删工作不再必要，异步依赖、间接指针可见性和内存安全不变；完整模型独立确认收益 |
| 第四层：硬件扩展 | A770显存容量、PCIe3、主存读取、频率与功耗 | 测量当前平台的上限、显存余量与频率；硬件升级仅列为成本/上限参照 | 原文指出低成本设备场景未必适用加机器；Strata 本轮固定现有 PC，不采购设备，不把换硬件计入本轮软件收益 |

### 已开始的架构路径核对

- 冻结运行环境已启用 `STRATA_VERIFY_DEVICE_PLAN=1`、`STRATA_VERIFY_NO_HOST=1`、`STRATA_VERIFY_DESC_CACHE=1`。实际链接的 `build-sycl-a770-gu-q8-export-20261027/verify.cpp` 中，graph 路径在第1414行重放，逐层主机等待循环在第1446行受 `!no_host` 控制。因而不能再把“删除当前逐层主机握手”列成未做优化，也不能仅凭环境变量名认定存在重复描述符工作。
- 审计的上游 UR legacy `enqueueWaitEventPath` 分别提交 reset-event 列表和 compute 列表，再处理完成事件；`urEnqueueCommandBufferExp` 还执行 ongoing-execution 检查与事件关联。这是第二/三层的待测调用链，不是已测 NEO 热点；需要先确认本机实际 adapter 和 CPU 成本。
- 当前 GU 原生资源探针已完成，七项代码生成核验均有效，详见下一节。旧 WG128 探针在尚未取得锁、没有测量结果时取消，避免重复验证。探针仅提供选项/代码生成证据，不计性能收益。后续 GRF 优化应针对已有 spill 的具体热点扩展覆盖。

新增一个源码层面的待归因项：已核对哈希的冻结 `verify_kernels.dp.cpp` 在25处launch属性中使用 `use_root_sync`，文件内未发现root-group调用；本机2025.3的 `sycl/handler.hpp` 会据此执行 `setKernelIsCooperative(UsesRootSync)`。`build-sycl-a770-root-sync-path-20261029/run.py` 现已完成，一个128工作组的向量运算体分别在eager/graph及普通/root-sync属性下运行；编译和四个新进程均exit0，每进程21次更新的完整32768元素检查通过，独立`--check`也exit0。日志开启验证层，不计时。

| 小探针模式 | 普通kernel append | cooperative kernel append | 队列execute | event-reset append |
| --- | ---: | ---: | ---: | ---: |
| eager，普通属性 | 23 | 0 | 8 | 2 |
| eager，root-sync属性 | 2 | 21 | 8 | 2 |
| graph，普通属性 | 2 | 0 | 64 | 44 |
| graph，root-sync属性 | 2 | 0 | 64 | 44 |

以上为进程全程API入口计数，含初始化与清理。eager路径确有21次cooperative launch；graph两组没有该API，kernel append、队列execute与event-reset计数相同，另有少量host同步次数差异，未量时。这个结果不证明所有模型辅助函数都不需要root语义，也不证明两份native代码、调度约束或性能相同。当前没有依据把删除25处属性计作正常graph的提交收益，故不据此新增整模型删属性候选；eager实际模型和原生资源仍须另核对。未修改模型属性、生产配置或原始日志。

### 第二/三层补充：GR 的实际对象与重复激活计算

已完成无GPU来源复核：固定基线的链接脚本直接传入 `build-sycl-a770-hc-up-fixed-streams/hc-streams.obj`，其SHA256与原构建及本次冻结基线清单一致；对应源码是 `2026-10-13-a770-hc-up-fixed-streams/source/hc-streams.cpp`，不是最早的last-draft源码副本。`fused_gr_read_multi` 的前128字节无重定位，在基线EXE可执行段RVA `0x129370` 唯一匹配。这是主机函数和来源身份的证据，不是设备指令等价或运行时间证明。

该实际源码的up为SG16：160个工作组均从down partials重新计算各token的320项SiLU，并写相同的 `a[k].lo`；sliced reduce仅写原始和。因此源码中确有重复工作，但不能据此断言它在正常graph关键路径上的成本。现有up已融合残差、归一化、门控及mixed输出，不能把这些步骤再次列为尚未实施的融合。

核对历史后，本轮不新增原样的“SiLU只算一次”候选：

| 历史实现 | 已记录的完整HC算子吞吐变化 | 适用边界 |
| --- | ---: | --- |
| `2026-10-12-a770-hc-silu-once`：reduce计算激活，up按part2读取 | −5.109039% | 从summary的九组base/silu/base行重算，三组整体变化均为负；早期源码不同于当前链接对象，不能当作当前模型退化值 |
| `2026-10-14-a770-hc-up-lo-precompute-v4`：sliced路径特化，reduce导出lo | −0.503705% | 记录为screen不通过、未独立确认；含134条外部编译活动记录，且其基线已有padding/live-SLM等变化，不能当作干净的当前对象对照 |

`build-sycl-a770-gr-source-review-20261029/check.py --check` 已通过，保留上述来源哈希、唯一函数前缀匹配及历史判定。下一步GR工作需先补当前graph的阶段成本及原生访存/依赖证据，再决定新的数据布局或跨kernel复用；历史拒绝只用于降低相同思路的优先级，不证明所有重构均无效。

### 第一层已有结果：当前 IQ2_S GU 的 IGC 选项核验

探针链接完整模型实际使用的 `build-sycl-a770-vram-down-sg16/iq.obj`，SHA256 为 `17cceb618ebf7707f4582747f206c1db5bdb25b8c92ae65ef25536130ca783bb`，提取 `iq2s_gu_pair` 的执行块。汇编比较止于唯一 EOT，排除尾部数据；另将标签归一化，确认改变不只是标签编号。未执行数值或性能验证。

| 选项 | 实际生成结果 | 当前处理 |
| --- | --- | --- |
| 默认 | 128 GRF、SG16、无 spill | 对照 |
| 自动大 GRF | 仍为128 GRF、SG16；执行段哈希与默认完全一致 | 对此 GU 不构成独立性能候选，不能推广到其他 kernel |
| 强制256 GRF | 确实生成256 GRF、SG16，指令改变 | 保留定向测试资格，不能因静态指令较少就判定更快 |
| `forceBCR` / `enableBCR` / `enableAccSubBRA` | 仍为128 GRF，非标签指令确实改变 | 待数值及无剖析性能验证，不作默认开关 |
| `preSchedForAcc` | 新增96字节 spill | 此 GU 上降级，不能假定调度选项必然改善寄存器压力 |

结果在本地 `build-sycl-a770-neo-igc-native-current-20261029/results.json`。基线与自动模式的执行段 SHA256 均为 `90436aadb46fcc7462c775b76e2accba70ed6c42516319e786c6f2eaec4ed21a`。这验证了本机选项传递及代码生成，尚未证明任何 decode 收益。

### 第一层新增结果：六个有 spill 热点的原生资源

`build-sycl-a770-igc-spill-native-20261029/run.py` 已完成默认/自动/256 GRF 的18组原生模块提取。`verify_kernels.obj` 与 `native_mmvq.obj` 从冻结模型实际链接的 `strata_kernels.lib` 提取，并与原构建对象逐一核对哈希；IQ对象仍复用上述冻结对象。准确的运行时kernel ID与元数据一致，六个默认spill值全部复现原CSV。

| 实例 | 默认：GRF / SG / spill bytes | 自动大GRF | 强制256 GRF |
| --- | --- | --- | --- |
| native_gu_port `<16,16,32,false>` | 128 / 32 / 576 | 资源和代码段均与默认相同 | SG32，不再报告spill |
| launch_mmvq / type21 / NC4 | 128 / 32 / 1504 | 资源和代码段均与默认相同 | SG32，不再报告spill |
| gdn_step_norm_multi / SG16 | 128 / 16 / 992 | 资源和代码段均与默认相同 | SG16，不再报告spill；SLM仍为3136B |
| gdn_ab_multi | 128 / 32 / 1440 | 资源和代码段均与默认相同 | SG32，不再报告spill |
| WideIQ4XS / NC3 | 128 / 32 / 416 | 资源和代码段均与默认相同 | SG32，不再报告spill |
| WideIQ4XS / NC4 | 128 / 32 / 608 | 资源和代码段均与默认相同 | SG32，不再报告spill |

自动模式使用 `-cl-intel-enable-auto-large-GRF-mode`，强制模式使用 `-cl-intel-256-GRF-per-thread`；均保留原有 `-cl-fp32-correctly-rounded-divide-sqrt`。六个自动模式的**准确命名代码段SHA256**各自与默认一致，强制256模式则全部改变。强制模式元数据均为256 GRF且没有 `spill_size` 字段；“不再报告spill”按该元数据口径表述。

原始 `results.json` 保留 `all_cases_valid=false`：15组长kernel名的汇编文件检查失败，但18组原生模块均已成功提取。随后使用已安装的LLVM `llvm-objcopy.exe`，把准确命名的 `.text.<kernel ID>` 和 `.ze_info` 节导出到短文件名；无GPU或编译器重跑。恢复元数据与原ocloc导出一致，来源、模块、工具和代码段哈希均通过 `recover.py --check`，结果保存在 `resource-recovery.json`。该恢复只解决ocloc长输出路径问题，没有隐藏原失败，也没有提供数值执行或性能结果。

由此把自动GRF从这六个热点的性能候选中移除，转为验证**定向256 GRF的数值与动态耗时**。主要IQ2_S GU/Down仍是零spill；整进程强制256还会改变这些kernel，必须另测占用率与整模型退化，不能把六项spill消失当作全局开关的采用理由。若采用定向源码属性，须使用冻结源码并核对对象，不能混入当前工作区其他改动。

进一步离线检查主要IQ2_S GU的原生汇编：只统计唯一EOT之前的指令，默认3977条、256 GRF为3367条，其中mov为839/822条；两者均有107条 `send.ugm`、3条 `send.dc0` 和1条结束消息。`forceBCR`、`enableBCR`、`enableAccSubBRA` 的总数分别为3976/3977/3976，消息数相同。复核脚本 `build-sycl-a770-igc256-model-20261029/inspect_isa.py` 核对原执行段哈希，并包含最小解析自检；输出保存在同目录 `isa.json`。这些是没有循环/分支权重的静态出现次数，不是动态指令数量、带宽或性能收益；不同寄存器分配也可能改变性能而不改变指令数。

新增第一层定向线索：上述默认机器码包含464条`addc`和144条`mach`；源码的激活、权重行及输出索引包含`size_t`乘法。这还不能将这些指令全部归因为地址计算，更不能证明它们限制吞吐。`build-sycl-a770-gu-index32-20261029/prepare.py`复用已冻结的完整专家链探针，仅增加32位偏移对照：原对象mode0、同helper的64位偏移mode256、同helper的32位偏移mode32；后两者均保持WG256/SG16、原点积/累加运算体、64位基址、权重及Down不变。范围限定为探针N2560/F640/TOK8/CAP64，CPU自检遍历全部合法索引并覆盖4GiB以上基址；这不证明模型中任意设备索引均满足相同边界。`run.py`已在共享锁内完成四项构建、真实native提取和两项反汇编，全部exit0；随后独立`--check`复核资源、静态计数和全部来源/产物哈希通过。同helper64位/32位的EOT之前静态指令为4023/3244（−19.36%），`addc`464/269、`mach`144/80；两者均为128GRF/SG16/零spill，`send.ugm`107条及`dp4a`112条保持相同。4023是本次同helper对照，不能与上面的冻结原对象3977混作同一基线。本阶段未执行数值或性能验收；下一步仍须完整输出/Q8/guard和独立无剖析对照，不能将指令条数下降视为decode收益。

这为“零spill也可能受GRF模式影响”补充了具体代码证据，故使用冻结EXE完成一个整进程256 GRF筛选，测量spill改善、地址计算和占用率变化的净效果。`build-sycl-a770-igc256-model-20261029/run.py` 仅运行默认/256/default一组，复用当前主要GU与上述六项的准确原生证据，没有旧WG128探针或整组选项扫测。

| 场景 | 邻接基线几何均值 | GRF256保留请求 | 吞吐变化 | 基线耗时漂移 |
| --- | ---: | ---: | ---: | ---: |
| Code2K | 4387.799 ms | 4359.900 ms | +0.639899% | +0.136836% |
| Prose2K | 4738.750 ms | 4746.000 ms | −0.152768% | −0.078049% |

三个进程均exit0，全部15条Short16及Code/Prose完整128输出匹配冻结参考，READY为6297槽且余量≥1024MiB，12组48层预填扫描通过。离线 `audit.py` 已从原始协议重算计时、输出、实际配置和配对门槛，并核对来源哈希。256模式的Short初始化为10092.3ms，默认两次为1200.3/1203.5ms；它包含冷JIT等开销，不计作warm decode收益。

单次保留请求只用于筛选。两个场景均未达到≥1%，因此不启动已准备的原样独立确认器；**全局GRF256降级**。下一步用同一冻结ITT-only EXE做默认/256/default eager诊断，按准确kernel及形状判断收益与退化，再决定局部属性；剖析耗时不能作为正常graph的性能验收。后续沿用当前同场景独立≥1%门槛。

对应 `build-sycl-a770-igc256-eager-attribution-20261029/run.py` 已通过无GPU自检并启动等待共享锁，尚无新剖析结果。它复用原采集命令、冻结ITT-only EXE及原始五条完整请求，只采第4/5次decode；默认/256/default采用三个新进程，持久JIT缓存关闭。比较按完整kernel名、global/local形状、SIMD及调用数匹配，还要求两个采集请求的accepted/offered相同，避免把草稿工作量变化归于GRF。逐项记录实际GRF、spill、stalled和occupancy，并按邻接基线报告漂移。新worker检查器已对原已完成eager日志重放通过五条原始协议及四组48层扫描；这些准备工作不计性能结果。

该任务已完成A0/B256/A1三个模型进程及全部六项采集/导出，均exit0：15条完整原始输出/协议、1584个输出token、12组48层扫描、6297槽已复核。两个被采集请求的accepted/offered均为Code73/92、Prose72/98，与旧eager诊断相同。原严格比较器随后因逐项调用次数不同而exit1；原脚本、断言和 `results.json complete=false` 保留，不输出全工作量同条件加速比。每臂耗时约17–19分钟，其中模型约3分钟；未因观察等待而重启。该目录的 `source-provenance.json` 另核对原MMVQ/GDN源码清单、Ninja编译参数及两个冻结archive成员哈希，只证明源码/对象对应关系。

A0有323个kernel/形状项、260119次调用、累计9.309776秒，均报告128 GRF。相对旧诊断的323项/260498次，完整name/shape集合相同，但总调用数少379次；完整输出及总accepted/offered一致不足以证明全部计算工作相同。专家GU/SwiGLU/Down各5376次，主要GU3808次、其他GU1568次，均复现；GU16、MMVQ21/NC4、GDN ab、IQ4XS NC3/NC4的逐形状次数也与旧诊断一致。GDN step为9744/旧9789次，另一些单token路径不同，原因尚未确定。新旧默认精确匹配的252项覆盖A0累计时间74.58%，但这仍未证明全部标量参数或驻留相同，也不作新旧默认的加速结论。

B256有同样323项、260271次调用，均报告256 GRF；311项不再报告spill，12项仍有spill，不能称为全局消除了spill。后置默认A1为323项、259797次调用。`build-sycl-a770-igc256-cohort-review-20261029/review.py` 另存独立局部诊断：三臂name/global/local/SIMD/calls及默认资源精确匹配的251项，分别覆盖三臂累计kernel时间69.07%/67.79%/69.24%；72项明确排除。它不修复或替代原比较，也不计算全工作量加速比。原始协议、来源哈希和局部诊断 `--check` 均通过。

| 匹配的局部项 | 形状数 / 每臂调用数 | 256 GRF相对相邻默认的累计kernel吞吐变化 | 默认合计耗时漂移 |
| --- | ---: | ---: | ---: |
| WideIQ4XS / NC3 | 7 / 3600 | +27.299% | −0.221% |
| WideIQ4XS / NC4 | 7 / 7200 | +24.918% | +0.331% |
| type21 MMVQ / NC4 | 6 / 2640 | +36.315% | +0.161% |
| GDN ab | 1 / 4032 | +42.754% | −0.085% |
| 其他格式 GU / type16 | 5 / 1232 | +0.875% | +0.156% |
| 主要 IQ2_S GU | 5 / 3808 | −2.971% | −0.063% |
| Q2_0 Down | 5 / 5376 | −2.541% | −0.146% |

表内各项的每个匹配形状默认漂移也均≤3%。Q6_K a2的一/二/三/四/六列合计时间均改善，部分小形状的默认漂移超过3%，仅作为候选线索，不能称为各形状均通过。GDN step的三臂调用数为9744/9729/9744，整项排除。所有这些数字都来自eager profiler，不是正常graph或独立算子确认；相等的次数仍不足以证明标量实参及驻留相同。原生资源与动态变化共同支持局部选择，不支持全局256 GRF。

同一离线诊断还以SQLite只读方式查询每臂已完成的实例表、kernel元数据及暂停区间。按实例起点排除记录的暂停范围后，主要GU、GDN step、QSA append和采样器各形状的原始实例数均精确复现CSV。该检查排除了这些项的CSV计数解析错误，尚未证明采集完整性、ITT边界精确性或差异原因。内部时间单位没有转换成QPC；原生数据库哈希、自检和全部原始结果保留。

新增 `build-sycl-a770-igc-window-review-20261029/review.py` 按暂停区间的并集分开检查两个采集窗口，限定六个准确GDN/GR标记。两个窗口的顺序与已核对的request4/5源码和worker协议对应；这仍是按顺序推定的关联，不是独立GPU/ITT任务或原生QPC绑定。逐形状合计调用数精确复现CSV，内部duration按每CSV秒1e10 ticks换算后的差值均≤2µs；不把此时钟当作ETW/QPC。初次JSON重放因tuple/list表示不同失败，原脚本、原结果及失败说明保留；统一JSON表示后，所有测量值、cohort和排除项完全不变，生成、自检及`--check`均通过。

| 窗口内的局部项 | 每臂匹配调用数 | 256 GRF累计kernel吞吐变化 | 默认合计漂移 | 范围限制 |
| --- | ---: | ---: | ---: | --- |
| Code GDN step/norm | 4785 | +53.523% | +0.049% | 一个相同形状；新增定向测试依据，非整模型收益 |
| Code GDN ab | 1980 | +43.452% | +0.087% | 一个相同形状 |
| Prose GDN ab | 2052 | +42.093% | −0.251% | 一个相同形状 |
| Code GR down-reduce | 3879 | −10.974% | +0.250% | 仅三个匹配形状，另一个排除 |
| Prose GR down-reduce | 2134 | −12.237% | +0.154% | 仅三个匹配形状，另两个排除 |

Prose GDN step为4959/4944/4959次，继续整项排除；Code GDN step则三臂均为4785次。GR up/down-slice及部分norm/reduce形状在拆分窗口后仍有次数差异，不能借此恢复全工作量比较。相等次数仍不足以证明相同标量参数、驻留或完整采集。两个窗口内三臂的原生append API总数反而分别完全相同：Code的cooperative/普通launch为86153/36707次，Prose为89441/38104次。它们不含逐kernel/实参关联，尚不能解释GPU记录差异；也不能把eager的这些调用次数和CPU时间套到正常graph。

据Code窗口的GDN step线索，`build-sycl-a770-gdn-step-scoped-20261029/run.py`仅给SG16的既有launch增加GRF256，保留原运算体、屏障、精度和ABI，SG32作原生代码/资源对照。真实编译、四项原生资源记录及原/候选各156种T/keep/prefix完整状态与输出检查均已完成，两个数值进程均exit0、0失败；独立`run.py --check`再次通过。SG16由128GRF/spill992B变为256GRF/未报告spill，SG32资源和执行段哈希与原版相同。测试副本只修正`keep<=0`的断言条件为`keep==0`，因为`keep=-1`实际传空指针、按T更新所有token，不能要求状态不变；原核内`n_keep?*n_keep:T`语义及可逆属性修改也已自检。不提供计时或整模型收益结论，该任务不修改已排队的GDN ab候选。

后续 `build-sycl-a770-gdn-step-model-20261029/run.py` 已完成重链接及正常graph的A0/scoped/A1筛选，三个新模型进程均exit0；随后独立`run.py --check`通过原始协议、15条完整输出、6297槽、12组48层扫描和全部来源哈希。运行前强制核对冻结`STRATA_GDN_SG16=1`；链接输入集合只增加已通过原生/数值检查的候选verify-kernels对象，其余绝对对象/库不变，不叠加append或MMVQ。两臂各156项状态/输出检查和SG32原生对照均通过，其余24个kernel未逐项比较原生代码。

| 场景 | 邻接基线几何均值 | 候选保留请求 | 吞吐变化 | 基线耗时漂移 |
| --- | ---: | ---: | ---: | ---: |
| Code2K | 4374.747837ms | 4512.600ms | −3.054828% | −0.198671% |
| Prose2K | 4713.637281ms | 4812.400ms | −2.052255% | −0.463531% |

`passes_screen=false`，这项SG16 step-only GRF256属性不进入原样独立确认，不能把eager的+53.52%写成正常graph收益。每进程仍是Short16、Code/Prose各一次预热和一次保留完整128输出；候选Short16的3705.0ms及所有样本原样保留。筛选未采运行时显存/频率，故不能解释变慢原因或作独立退化确认；它只拒绝本候选进入采用。等待与退出使用30秒观察循环，没有因观察超时杀死或重启。GDN ab的另一个排队对照不受本结果替代，质量/热历史/生产和最终20%均未通过本项证明。

同目录`confirm.py`已通过CPU自检；由于上述初筛未通过，未启动，也未生成confirmation目录。它复用已完成MMVQ确认的三进程采集器，仅替换为GDN候选EXE、正确结果目录和GDN范围说明；保留Short16、每场景一次预热及三次交错保留请求、同一场景独立≥1%/漂移≤3%、完整原始输出/退出/八组48层扫描，以及本次目录1Hz显存/频率和采样空闲≥1024MiB检查。自检实际读取临时目录的显存样本并拒绝缺失覆盖，同时验证两个单场景方向、同场景交集、非有限值和漂移拒绝。运行前必须重查真实GDN原生/数值证据、完成的整模型初筛及至少一个合格场景；不会因准备完成就启动未合格候选。引擎协议与退出沿用30秒观察循环。独立确认仍是组件门槛，不代表最终20%、质量/历史或生产采用通过。

`build-sycl-a770-igc-scoped-mmvq-20261029/prepare.py` 已核对原冻结源码/头文件并生成局部候选材料：只在两个既有MMVQ launch处增加属性，Q6_K a2一/二/三/四/六列及IQ4_XS SG32三/四/六列使用256 GRF，运算体、精度和其他分派不变。所选八项的三臂合计时间改善≥1%、漂移≤3%；少数小形状的不稳定性仍按上段保留。

其 `run.py` 已通过CPU自检，并在共享GPU锁内完成候选编译、整模型链接及24份原生资源记录核对。八个目标实际使用256 GRF、原SIMD宽度保持；七个目标不再报告spill，Q6_K四列仍有1440字节；四个未选变体的资源与机器码逐字节一致。无profiler的默认/候选/默认整模型初筛已完成，保留原输入、完整128输出、6297槽、min-p0.5和相邻漂移≤3%。Code提升1.662680%，Prose提升0.158144%（小于0.3%的噪声）；下述独立确认也通过Code。质量、历史及实际接入尚未完成，也尚未叠加append组件。

以下为同一探针、相同编译环境的资源元数据，来自`build-sycl-a770-igc-scoped-mmvq-20261029/build-results.json`；未报告`spill_size`按0列出，不把资源变化直接计作运行时间收益：

| 变体 | 原GRF / spill字节 | 局部GRF / spill字节 |
| --- | ---: | ---: |
| Q6_K一列 | 128 / 1376 | 256 / 0 |
| Q6_K二列 | 128 / 3232 | 256 / 0 |
| Q6_K三列 | 128 / 5280 | 256 / 0 |
| Q6_K四列 | 128 / 3648 | 256 / 1440 |
| Q6_K六列 | 128 / 11520 | 256 / 0 |
| IQ4_XS三列 | 128 / 416 | 256 / 0 |
| IQ4_XS四列 | 128 / 608 | 256 / 0 |
| IQ4_XS六列 | 128 / 1120 | 256 / 0 |

本表的Q6_K目标是`native_mmvq_q6k_wide_a2`，不能与先前六热点探针中`launch_mmvq/type21/NC4`的1504字节混为同一变体。Q6_K五/八列和IQ4_XS一/二列均保持原资源与代码。两臂模型链接输入另经只读展开核对，候选只增加一个MMVQ对象覆盖，使用同一个原FP32-Down-retry prefill对象；没有将新device-counter prefill混入该组件对照。

| 场景 | 初筛前后基线几何均值 | 候选一次正式请求 | 吞吐变化 | 基线耗时漂移 |
| --- | ---: | ---: | ---: | ---: |
| Code2K | 4397.419 ms | 4325.500 ms | +1.662680% | −0.745373% |
| Prose2K | 4718.350 ms | 4710.900 ms | +0.158144%，噪声 | +0.006358% |

三个全新引擎均exit0，15条完整请求/1584个输出token及12组48层预填扫描全部通过，原生资源和完整来源检查也通过。每个场景每进程只有一次正式请求，此表只能用于初筛。首次Short16的decode记录为基线1247.5/1219.8毫秒、候选10255.6毫秒；原因尚未拆分，原样保留，不能把预热后的结果解释为首次请求提速。该初筛只有READY显存检查，不能替代运行期采样。

独立`confirm.py`现已完成，三个全新引擎均exit0；保留原FP32-retry基线和同一个局部MMVQ候选，每进程Short16后各场景一次预热及三次交错正式请求。Code邻接基线几何均值4354.598428ms、候选中位数4302.200ms，吞吐+1.217945%、基线漂移−0.169791%；与初筛为同一场景，`single_scene_confirmed=true`。Prose为+0.069103%（噪声）、漂移+0.061642%，不计收益。3120个完整输出token、原始协议、每进程8组48层扫描、来源哈希及本次目录的1Hz显存/频率覆盖均经独立离线复核，最低采样空闲1614.042969MiB。候选首条保留Prose耗时8336.6ms，原样保留；其decode窗口采样600–2400MHz，其余候选请求为2400MHz，未报告throttle标志。这是相关现象，不能确定低频是原因或结果，也不证明没有瞬时异常。候选Short16本轮1204.8ms，邻接基线1229.9/1213.4ms，初筛的10255.6ms异常没有在这一轮重复；不据此声称已定位原因。质量及实际接入尚待验收，未采用。

后续接入还须统一prefill基线：上面的MMVQ筛选/确认沿用原FP32-retry prefill，不能直接移作已修正host USM原子计数的device-counter组结果。`build-sycl-a770-igc-scoped-mmvq-device-counter-20261029/build.py`复用同一个局部MMVQ对象，在device-counter原始基线链接上仅增加该对象、重定向输出和map；重链接已exit0且EXE存在，但原map检查器把主机对象名限定为`native-HEX.obj`，拒绝了实际的`native_mmvq-HEX.obj`。原脚本及`build.json complete=false`保留。新增只读`recover-build.py`审计现有EXE/map：精确IQ4/Q6入口均唯一属于同一候选对象，错误archive、缺失、重复及混合归属仍拒绝；原链接计划、共同device-counter fixture和全部来源哈希均通过。恢复结果另存`recovered-build.json`，生成后独立`--check`通过，未重编译或重跑GPU。新组合仍须重新进行完整质量、同场景独立性能和热历史验收，旧1.66%/+1.22%不作为新组合收益。

同目录`quality.py`现已在原device-counter基线上完成A1/B1/B2/A2四个新进程，全部exit0；九场景/758固定目标的四进程评分与冻结原版精确一致，必需概率全部有限，`summary.fixed_targets_passed=true`、aggregate PPL变化0%、逐场景top1一致率1。候选和基线使用相同MMVQ初筛基础环境，未添加compact、pruning或append开关。实际6297槽，四臂READY空闲显存依次1633/1613/1634/1485MiB；完整原始协议、评分文件、原FP32-retry扫描标记和全部链接/来源哈希已由独立离线`audit`复核通过。原中断和等待记录保留；冷评分通过不替代新组合性能或完整热历史。

`performance.py`现已完成合法device-counter基线的A1/B1/A2，三个进程均exit0，独立`--check`通过。Code/Prose吞吐变化为−0.782504%/−1.787474%，基线耗时漂移−4.125093%/−1.437148%；Code漂移超限，不能确认退化幅度。`passes_screen=false`；最低采样空闲显存975.578125MiB，候选最低1002.078125MiB，也未通过1024MiB门槛。全部完整输出、每进程8组48层扫描、请求区间显存/频率覆盖及来源哈希均已核对，decode频率样本均为2400MHz。本组不启动原样独立确认、热历史或实际接入。详细结果见文末“合法device-counter MMVQ的新基线初筛”；旧FP32-retry收益不迁移到本组。

实际接入入口`profile.py`已改接恢复构建证明并通过CPU自检，尚未启动。只有本组完整冷评分、同场景独立速度、采样显存及非跳过的热历史全通过，才会再持共享锁运行真实`server.StrataEngine`。它使用本组EXE和测速参数/环境，检查实际加载的SYCL/UR DLL路径、来源哈希、6297槽、READY≥1024MiB以及Short16、Code/Prose2048/full128的272个完整输出；两次长请求的tail64直方图按实际计划校验，不复用八请求采集器的硬编码次数。自检实际覆盖质量/速度/显存/历史前置失败、服务器参数及环境函数、父环境`STRATA_VERIFY_EAGER=0/1`拒绝、错误直方图与请求次数拒绝。退出等待使用30秒观察循环，不因观察超时重启模型。仅在实际检查成功后才生成`strata-iq2_xs-scoped-mmvq.json`，当前默认配置保持原冻结输入；新配置及真实接口测试尚未执行，不计速度、质量或接入成功。以上1%为单组件筛选/确认门槛；本`/goal`最终≥20%仍须在同一EXE及相同负载上实测，不能乘算旧组件收益。

复核发现原排队脚本仍把两个场景的筛选结果取交集。确认原进程3756尚未取得锁、没有编译器或模型子进程、没有生成构建协议后，已停止这一个等待进程，并保留修改前脚本和`single-scene-transition.json`。只将筛选汇总从`all`改为`any`，补上Code单独通过、Prose单独通过及漂移超限拒绝的CPU自检；自检exit0。替代进程31932已重新进入同一共享锁队列，所有算术、原生资源及完整输出检查保持原条件。此次替换没有GPU测量结果，不将排队或自检计作收益。

只从已完成A0 trace尝试 `-group-by task,computing-task` 导出，exit2、`Empty request output`；命令、stderr及哈希保留于 `build-sycl-a770-igc256-eager-task-export-20261029/`。该查询没有取得可用的ITT任务/kernel关联，不用空结果补齐归因，也未重跑GPU。

正常graph的队列覆盖探针 `build-sycl-a770-graph-etw-probe-20261029/run.py` 已完成，start/probe/stop/export全部exit0，21次完整向量更新通过。它复用现有EXE及本机xperf/DxgKrnl，不编译或改变模型；使用含PID的私有ETW会话，只停止自己成功创建的会话。原检查及新增离线 `coverage.py --check` 均通过：tracestats报告PerfCounter时钟、丢失事件/缓冲均为0；四个新建context属于同一个被观察到的owner PID23996，关联QueuePacket start/stop各87条、DmaPacket start/stop各85条。

原采集器没有记录探针子PID，也没有在原生代码中标记graph重放区间，因此该owner尚无独立EXE/PID绑定，以上计数还包含初始化与清理，不能称为21次重放逐次完整覆盖或整模型利用率。CSV中同名事件存在多个schema版本，离线检查要求所用字段在各版本位置一致，不能随意选一行schema。后续全模型队列测量前仍须补实际子PID、原生QPC边界和相应范围内的覆盖检查；本次证明该ETW路径确有可用的context/packet事件，不提供速度结果。

随后仅对原CSV做有界离线关联：85个计算QueuePacket（PacketType0、DmaBufferSize>0）均可按context/SubmitSequence匹配唯一DmaPacket起止及QueuePacket完成，DMA submission/completion ID一致；另外两条队列同步包单列。原记录没有抢占或超时。这验证了关联字段，但仍不能补回原采集缺少的PID与源码区间。

最小后续探针 `build-sycl-a770-graph-etw-qpc-20261029/run.py` 已完成编译（exit0），但xperf启动返回2147943404，即`0x800703ec / ERROR_INVALID_FLAGS`，原始中文stderr为“无效标志”；尚未执行GPU探针或创建成功的私有会话，不生成事件覆盖结论。原失败和全部命令保留。独立`audit.py --self-check`验证的多版本schema、PID/device/context、QPC校准及20个调用与Queue→DMA→完成的关联，仍只是解析器自检。

独立入口`build-sycl-a770-graph-etw-qpc-guid-20261029/run.py`已完成，build/start/probe/stop/export/stats均exit0；仅将provider改为裸GUID，原向量、两处TraceLogging、20次异步graph及成功创建后才stop的清理不变。真实子PID31724与原生QPC一致，21次完整向量更新通过，PerfCounter时钟且ETW丢失事件/缓冲均为0。原始结果`complete=true`，但原关联审计拒绝了真实边界：首调用3个typed包、后19次各4个，最后两项Queue完成晚于结束标记16/31µs。原审计和失败保留，不能把采集成功当作原覆盖检查通过。

同目录`recover.py`只重放原CSV、原生记录及来源哈希，不重跑GPU或编译器；实际结果另存`audit-results-recovered.json`，生成后独立`--check`再次通过。它保留每个观察到的Queue→DMA→完成的唯一键、ID一致、端点顺序、无抢占/超时和全部20调用均有非零关联检查，显式报告3/4的不均匀计数及两个结束后完成，并将DMA包络并集裁剪到标记区间。CPU自检覆盖不均匀计数、结束后完成/裁剪，以及跨界包缺少完成仍拒绝；原PID/schema拒绝自检也通过。**这是79个观察包与20个源码调用的事件流一致性，不是每个kernel/命令均被独立证明完整覆盖**；不均匀计数的驱动含义尚未确定，不把后置完成通知说成GPU仍在计算。WDDM包络不是GPU忙碌周期或可消除空隙；本诊断不改变当前同场景独立吞吐≥1%、相邻基线漂移≤3%及质量、历史、显存和实际接入的验收条件；1%是组件门槛，本目标最终≥20%仍须在同一EXE实测。

整模型后续位于 `build-sycl-a770-graph-etw-model-20261029/`，`run.py --self-check`、`audit.py --self-check`及真实小图前置检查均已通过；采集入口已以进程10888启动等待共享锁，尚未编译或运行模型。它复用原冻结MAIN编译/链接流程及完整Short16、预热Code/Prose128、正式Code/Prose128；只增加第4/5次decode的四个TraceLogging写入和原生QPC边界，无额外GPU等待。检查覆盖真实子PID、请求顺序、QPC时序/时长、原始输出及48层扫描；离线包关联检查拒绝缺少完成、时钟不一致、抢占/超时及未分类的跨整次decode包络，并对重叠DMA区间取并集。构建前重查小图原始结果、上述有界关联及保留的恢复报告一致，要求20次源码调用均有非零关联，并保留`independent_kernel_coverage=false`的范围。CPU自检接受真实3/4计数、拒绝缺失/不完整/未保留的关联及误称独立kernel覆盖。后续模型事件仍须实际审计，准备和小图通过不证明整模型事件完整覆盖或收益；WDDM包络不能解释为kernel忙碌时间或可删除耗时。

历史结果也纳入选择，避免重做已有实验：

- [IQ4_XS GRF256](iq4xs-grf-2026-10-28.md) 已证明同helper的NC3/4由spill416/608B变为未报告spill，但独立算子收益仅通过2560→640的五/六列，约15%–22%。该小形状不能代表当前占时较大的NC3/4输出头/QKV，也未给出整模型收益。本次重点是原冻结对象的自动选择和资源复现。
- `gdn-grf-confirm-2026-10-25/check_results.py` 本轮重新通过：649项差分检查、1620批独立样本、72/72稳定配对；SG16无prefix的汇总算子收益为26.813%/26.873%。其候选同时含GRF256、局部屏障和跳过无用prefix输出计算；源码与冻结模型不同，函数签名还增加了xq参数，不能直接把该对象替换进当前模型。其 `adopt=true` 是当时的算子门槛，不能当作本目标的整模型验收。
- [L1缓存提示](weight-cache-hint-2026-10-25-v2.md) 与 [L1 bypass/host allocation](weight-memory-2026-10-25.md) 已有独立算子筛选且未确认收益，不原样重跑这些开关。当前高stalled仍须按具体指令和访问路径解释。

### 待优化项目及优先顺序

排队期间对GR的重复工作进一步做了纯CPU历史复核：`build-sycl-a770-gr-source-review-20261029/historical.py`及独立`--check`均通过，从9份原始算子记录重新计算486条样本的三组相邻A/B/A、完整输出checksum、各T的收益和最大基线漂移，并检查冻结候选源码的原manifest。没有启动GPU、修改生产或把旧cohort计作当前测量。

| GR历史候选 | 算子初筛几何平均变化 | 独立确认 | 关键限制 |
| --- | ---: | ---: | --- |
| global lo仅group0写回 | +0.6641% | 无 | 初筛最大漂移3.8253%，不合格 |
| global lo按token分配写者 | +0.1609% | +0.7051% | 两轮均不足1%；是完整HC算子对照 |
| 另一种仅写一次 | +0.4743% | 无 | 最大漂移7.0518%，不合格 |
| up复用两个stream输入 | −22.2124% | 无 | 漂移3.5751%，不能精确认定退化幅度，不重排原样方案 |
| 按活跃T分配up SLM | +0.6642% | +0.2792% | 独立值落入当前0.3%噪声范围；不能按SLM节省比例预测速度 |
| down-reduce二维索引 | +1.8518% | +1.8122% | 旧HC算子、旧cohort；尚无当前完整模型证据 |

旧记录的`summary.source_adopted=false`与稍后的`adoption.source_adopted=true`描述不同阶段；后者对live-SLM/reduce2d明确是“无最低收益门槛、仅源码采用”，`model_ab_tested=false`，不等同当前组件1%或最终20%验收。源码复核还确认，较晚历史up采用按T分配的local指针，当前冻结对象源码采用最大T的group-local数组；即使数学主体相同，历史时间不能当作当前代码的时间，也不能把“曾在源码采用”说成当前EXE已包含。后续GR优先补当前各阶段的动态成本、实际SLM和依赖证据；不重排上述无新依据的小项，二维reduce仅保留为待隔离的旧组件线索。

```powershell
python -X utf8 build-sycl-a770-gr-source-review-20261029/historical.py --check
```

下表的百分比均沿用上面的 eager 累计 kernel 时间，不能相加预测整模型加速比。P0 是归因前提；P1 先做已有直接证据的定向核验；P2 要求补齐路径证据后再实施。

| 优先级 | ezpp 层次 | 项目 | 下一步可检验的工作与通过条件 |
| --- | --- | --- | --- |
| P0 | 共同度量 | 正常 graph 的 NEO/UR 提交与同步归因 | CPU栈、legacy adapter及等待链已确认；WDDM MakeResident同步事件区间仅约占Code/Prose decode的0.25%/0.23%，不优先单独重写这一段。仍须绑定UR/API、异步paging fence与实际关键路径，不能由等待样本或调度包络推断全部运行时空间 |
| P2 | 第一层 | MMVQ 寄存器压力：全家族24.16%，其中 WideIQ4XS NC3/4 合计7.18%、type21 NC4 为2.07% | 24项原生资源及合法device-counter冷评分通过；新基线初筛Code/Prose为−0.78%/−1.79%，Code漂移超限，采样空闲显存也不足1024MiB。不原样启动确认、历史或接入；旧基线Code独立+1.22%不迁移、不与append叠加，type21另核对 |
| P2 | 第一层 | 其他格式 GU：`native_gu_port<16,16,32,false>` 占5.45%、spill576B | 256 GRF虽消除spill，稳定匹配项诊断仅+0.875%，先降级；不原样启动此属性的独立整模型对照。主要IQ2_S GU也不能继承它的资源结论 |
| P1 / P2 | 第一层 | GDN step/norm 5.14% 与 ab 1.61%，分别spill992/1440B | step数值/原生检查通过，但正常graph初筛Code/Prose为−3.05%/−2.05%，原样GRF属性降级、不启动确认；ab的另一单项对照仍排队。历史屏障/prefix收益不能冒充本项结果 |
| P1 | 第二层 | 主要 IQ2_S GU/Down 的访存与指令依赖 | 二者零spill；检查实际load、依赖、驻留与访问顺序。分别测设备/主机/混合驻留，避免把41.3–80.3%的stalled直接解释为PCIe饱和；不原样重复失败的WG/双缓冲方案 |
| P2 | 第二/三层 | GR 13.63% 与图内小kernel依赖 | 已核对当前hc-streams对象及SG16源码；重复SiLU的两种历史搬移均未通过筛选，原样重测降级。先补当前graph阶段成本、原生访存与依赖，再选择有新依据的数据复用或流程删除 |
| P1 | 第二层 | graph 的reset-event、compute提交及完成处理 | 正常graph CPU栈与小图实际API均已核对；完整模型独立确认Code/Prose为+2.67%/+2.90%，保留为组合候选。继续质量/历史及生产验证；不把小图调用次数当作模型次数 |
| P2 | 第三层 | 删除仍存在且可证明多余的事件/驻留/描述符流程 | 先量出重复调用，再核对资源生命周期和异步可见性；root-sync小图API对照已完成，graph未见其改变launch/提交计数，先降级。现有device-plan/no-host/desc-cache已启用，不能把已删除的逐层主机握手再次计作机会 |
| P2 | 第一/二层 | QSA top-k 1.62%、Router 1.55%的大spill | 保留后续定向项目；按动态成本排在上述热点之后，不能只按spill字节数排序 |
| 持续参照 | 第四层 | 当前PC的容量、带宽与频率上限 | 记录显存余量、频率、主存/设备读带宽和PCIe路径；本轮固定A770，不把硬件升级计入本轮软件收益 |

当前顺序为：第一层在全局GRF256净收益不成立后，补逐kernel及形状的动态归因，再选择局部属性；第二层保留已独立确认的append约3%组件，并继续补正常graph的API/GPU空隙证据；第三层只删除经调用链和生命周期证明多余的路径；第四层持续提供硬件上限参照。共同度量贯穿各层，每次改动保留独立对照，不再启动无热点依据的整组选项扫测。

## 矩阵映射与本轮中断恢复

关于用户询问“重写compute-runtime能获得多少收益”：目前能引用的运行时整模型证据仍是上述UR提交组合的Code/Prose独立+2.668767%/+2.901650%，它没有测量整库重写的收益或上限。若只删除关键路径耗时且其余工作不变，删除总耗时比例p的理想吞吐提升为`1/(1-p)-1`；p为5%/10%/16.67%时，分别为约5.26%/11.11%/20%。CPU等待样本93.46%不能代入p，完整WDDM包络也不等于可删除的driver时间。当前不能承诺单靠重写达到20%；优先用真实提交/驻留/同步的关键路径证据选择局部修改。

重写范围须明确：应用改为直接调用Level Zero，仍通过Intel用户态驱动和WDDM；修改NEO本身也不会自动减少GPU算子运算量或跨PCIe的必要权重读取。当前已有graph回放、device-plan/no-host及描述符缓存。若只把占总耗时p的路径加速到r倍、其余关键路径不变，理想吞吐提升为`1/(1-p+p/r)-1`；路径快一倍时，要实现整模型+20%，该路径原先须占总耗时至少三分之一。减少CPU忙等还可能主要节省CPU占用，须另测是否缩短decode。

已定位用户所述SwapAB的高度吻合来源：[DeepGEMM PR192](https://github.com/deepseek-ai/DeepGEMM/pull/192)，H20严格对齐M32的两组报告257→154µs、127→84µs，按耗时重算吞吐+66.883%/+51.190%。单组other case为349→304µs（+14.803%），且1024是随机组长度的期望值。交换将问题M放到WGMMA灵活N侧，并同时改变tile、warpgroup和写回；不能归因于分组数或仅减少尾块。本机主热点是按真实4/2/1项处理的标量dp4a，不存在同一固定64行补齐。旧XMX的变体2是8×8方形交换，未独立确认；它不排除其他矩形映射。固定源码、上游数字、本机路径及ezpp层次见[专项核对](matmul-order-2026-10-29.md)，`review.py --check`已通过，不计本机速度收益。

Windows系统日志确认10:51关机/10:52启动及11:00关机/11:01启动；旧ETW10888、MMVQ8284、GU screen32496进程均已不存在。先保存`build-sycl-a770-matmul-order-review-20261029/reboot-recovery.json`，再只清理本任务32496的遗留锁；未删除外部锁或终止外部任务。ETW/MMVQ没有生成试验协议或质量目录，原CPU自检重新通过后，已恢复为ETW11612（正在构建）及MMVQ12220（等待同一锁）。此次重启不计采集或质量完成，后续仍须检查实际退出和结果。

GU index32已有六个边界进程和24个完整计时进程，A3-mixed4只留下T1–3、缺少PASS，其余A3未执行，原试验保持`complete=false`。新增只读`recover-screen.py`审计原始协议、全部来源哈希、完整输出/Q8/guard见证与每批样本，生成后独立`--check`通过；C4-vram的exit0和完整日志被保留恢复，未改写旧partial。两轮相邻同helper64→32对照在mixed4/mixed8均已齐全，但没有T同时满足两种混合布局、两轮≥1%及漂移≤3%。例如mixed8/T4两轮+1.648%/+2.203%，mixed4对应−2.021%/−0.088%；不能只采前者。纯显存部分漂移达21.903%，也不能补齐混合验收。原对象第二轮因A3缺失无法评价，未补造基线、未原样重跑整屏或进入模型；静态指令减少19.36%仍不等于decode收益。

后续离线恢复已完成：新的`events-recovered.csv`为4416872772字节，export/stats均exit0；`export-recovered-results.json complete=true`，独立`recover-export.py --check`通过全部模型输出、退出、来源与产物哈希，ETW报告零丢失事件/缓冲区，未重跑GPU。实际`audit-recovered.py`仍被原断言拒绝：context `0xffffc303ee542010`、序号1446有两个DMA完成。只读核对确认它是11944800–11945955µs的一次队列提交，先以`bPreempted=true`结束DMA36871，再以36874恢复并完成；两段均早于第4/5次decode的148764967/175130921µs起点。`audit-scoped.py`只单独记录完整且严格在两段decode之外的分段包，原窗口内检查随后仍拒绝Prose内序号61786的两段DMA98393/98396。原检查、两次失败及对应源码快照保留，没有伪造原审计通过。

另存的`fragment-review.py`显式审阅抢占/恢复，生成后独立`--check`通过：原生PID/QPC、设备/context归属、唯一队列起止、DMA ID匹配、顺序、每个非最终段的抢占标志及最终无超时完成均核对；保留跨边界包。结果`strict_original_audit_passed=false`、`independent_kernel_coverage=false`，不用于速度验收。

| 请求 | 原生decode时长 | 相关type0队列包 / DMA段 | 分段队列包 | 观察到的DMA调度段并集 | 调度段并集之外 |
| --- | ---: | ---: | ---: | ---: | ---: |
| Code | 4505.0418ms | 1015 / 1015 | 0 | 4455.923ms | 49.122ms，1.0904% |
| Prose | 4883.4474ms | 1057 / 1061 | 2 | 4743.858ms | 139.593ms，2.8585% |

Code/Prose各有3/4个跨边界队列包；Prose抢占后恢复的区间并集为313µs。另有311组不含type0的相关事件单独保留，未当作GPU空闲。表格是WDDM调度观察，不是GPU忙碌时间、完整kernel覆盖、可删除耗时或NEO收益上限；运行时成本也可能在调度包络内，不能将表中1.09%/2.86%代入重写收益公式。后续仍须将实际UR/API提交、驻留和同步与这条时间线绑定，才能作因果归因。

### 同一trace的WDDM驻留与分页事件

`residency-review.py`复用上述完整CSV，未重新运行GPU。流式投影保留全部schema及驻留相关事件，原CSV的27914254行和SHA256匹配恢复报告；独立`--check`重新读取原CSV，验证原文件及完整投影的逐字节哈希，再重建全部结果，通过。模型PID22356、设备、四个context及原生QPC/decode边界沿用原验证；MakeResident按同PID/TID的非嵌套起止配对，分页以实际所属DxgDevice和提交PID绑定，再按queue/packet/sequence核对唯一Start/Info/Stop、顺序及边界。全记录25602组模型MakeResident均已配对；原事件保留在投影中，结果文件逐项保留与两个decode相交的调用和分页包。

| 请求 | MakeResident次数 | 同步事件区间合计／并集 | 占原生decode时长 | 所属分页包 | 分页Info→Stop区间并集 |
| --- | ---: | ---: | ---: | ---: | ---: |
| Code4 | 1002 | 11.187ms | 0.248322% | 341 | 10.518ms |
| Prose5 | 1080 | 11.449ms | 0.234445% | 498 | 37.513ms |

两场景MakeResident累计NumAllocations为7642/7880，NumBytesToTrim均为0；这不证明没有其他驱逐或异步驻留工作。分页op类型及状态码保留原值，未猜测搬运类型或字节数；ExecutionTime100ns字段均为0，不能解释为GPU执行耗时为0。两种区间可能重叠，不能相加当作关键路径成本。同步MakeResident这一段不足以独自支撑总decode≥20%的收益，因此降低“仅缩短这段调用”的优先级；该结论不是全部runtime的收益上限，异步paging fence、UR和其他运行时工作仍需单独关联。此旧trace也不用于解释后来MMVQ采样显存不足。首次通过相对路径runpy启动的复核仅在最终结果的脚本路径键比对失败，原结果及失败说明保留；改用绝对路径启动后，源码及结果未修改，原始字节和全部结果比对均通过。

```powershell
python -X utf8 build-sycl-a770-graph-etw-model-20261029/residency-review.py --self-check
python -X utf8 build-sycl-a770-graph-etw-model-20261029/residency-review.py --check
```

## 队列合并与批量执行

按用户建议，已将`window-reuse --check`与本组MMVQ冷质量复核合并为一次纯CPU批处理，两项原审计均通过，无GPU请求或锁。先完成批处理，再用CIM核对等待者22788的命令、创建时间、零子进程及未持锁状态，只撤掉这个重复只读等待者；原任务脚本和结果不变。记录见`offline-audit-batch-2026-10-29.json`，可用`python -X utf8 bench/results/a770/check_offline_audit_batch_20261029.py`复核；不创建通用调度器。

后续新建的同基线性能候选可合为`A-B-A-C-A`，每个候选仍用相邻A的几何均值、各自三次正式请求、同场景独立复测及≤3%漂移；两候选比各跑`A-B-A`少一个模型进程。仅在EXE、完整环境、参数、输入、预热/历史和运行时DLL等基线身份一致时共享中间A。运行时环境、精度或诊断开关不同的候选保留新进程；冷评分、热历史、诊断和性能各自验收，依赖不合并跳过。已完成的MMVQ采集沿用原冻结顺序，未中途更换对照。

新建`build-sycl-a770-gu-fixed-three-batch-20261029/`将两个GU候选合为一个持锁任务：B仅启用现有`FIXED=true`的N2560编译特化，C仅将4条目分块后的2+1余项改为3条目权重复用，两者均WG256/SG16，原逐条目点积、FP32累加、Down及精度不变。A为同helper的未改动版本，O为冻结原IQ对象；两轮全新进程各跑`O-A-B-A-C-A-O`，混合驻留every4/every8各一组，计时进程由分别执行的40个减为28个。每个候选分别对相邻A及两端O检查两轮≥1%/漂移≤3%；原对象生成的完整输出、Q8字节、guard、随机/离群及1..7条目校验保留。旧计时输入只有1/2条目，新合成输入的T3..6实际包含3条目组并打印直方图；这不是模型路由重放。构建/native资源与数值/速度阶段依次执行，CPU自检已通过；启动身份、脚本哈希和状态见该目录`launch.json`与`run.stdout`，尚无性能或整模型收益结论。

本次队列合并所属会话的原始`/goal`仍是“优化NEO、IGC，得到不小于20%的decode性能提升，先做windows的，后面再改linux的”。共享报告中的单场景1%条件用于该目标的组件筛选，不能据此宣称这个20%目标完成；仍须同一组合EXE及相同负载的最终独立实测，Windows通过后继续Linux。目前没有这份完成证据。

GU批量后续：四项构建、三份原生模块提取及反汇编全部exit0，独立`build.py --check`通过；同helper基线／固定N／三条目余项均为128GRF、SG16、零spill。EOT前静态指令分别4023／3335／5891，固定N减少17.1017%，其dp4a及send.ugm仍分别为112／107；三条目变体多一个分支，静态总数不能当作动态工作量或速度。首次`check-random-0`已exit0、48项完整数值检查通过，但collector错误地期待`MODEL_GU_BATCH`标签，而冻结探针沿用`MODEL_GU_SCHEDULE`，导致见证检查停止；没有进入性能计时，原脚本、协议、stdout/stderr及失败结果全部保留。`resume.py`仅修正继承的日志标签读取、将新输出放入`screen-v2/`并复用已验证构建；自检实际重放原失败日志并拒绝错误mode，CPU检查通过。唯一续跑PID21464及创建时间记录在`resume-launch.json`，当前重新等待共享锁，不重编译或覆盖旧结果，尚无候选速度结论。

## 已排除或降级的方向

### GU 两候选批量的独立复核与模型队列

`build-sycl-a770-gu-fixed-three-batch-20261029/resume.py --check`已通过：8个随机/离群进程、28个计时进程的完整原对象输出、Q8字节、guard、原始11批样本、实际三条目直方图、两轮新进程及两种混合驻留均重新检查。B固定N没有满足全部独立条件的T；C三条目余项仅T5/T6通过。下面每格为T5／T6的吞吐变化，均是算子完整链，不能当作模型提升。

| 驻留 | 独立轮次 | 对相邻helper A | 对两端冻结原对象O |
| --- | --- | ---: | ---: |
| mixed4 | 1 | +2.085588%／+1.756418% | +2.044329%／+1.802638% |
| mixed4 | 2 | +1.814085%／+1.472733% | +2.138434%／+1.755673% |
| mixed8 | 1 | +1.829400%／+3.278138% | +2.496898%／+3.913992% |
| mixed8 | 2 | +3.691781%／+3.597393% | +4.335545%／+4.207398% |

T5/T6的全部配对漂移均≤3%；首轮mixed4原对象T4有严重漂移，其约41%的表观变化不被采用。T5/T6中三条目组分别占5/30和10/30，这只是合成输入，真实已采集窗口比例更小，不外推为真实decode收益。B虽减少17.10%静态指令，仍未得到独立稳定收益。

特定外部编译竞争已核查：最后计时进程stdout写入于UTC05:30:28.834，最终screen结果05:30:29.348；此前CIM观察到的icx PID21696（parent6188）创建于05:30:37.142，晚于最后stdout约8.308秒。这排除了该编译与本批计时重叠，但不是全过程竞争事件监控。`review-screen.py`从原始结果、日志哈希和文件时间生成`screen-review.json`；首次独立`--check`仅因内存tuple被JSON写成list而比较失败，原脚本和结果保留。独立`check-screen-review.py`仅规范化该JSON表示，重新运行全部原始检查及哈希并通过，不会重跑GPU。

下一条队列将隔离模型链接、原9场景/758固定目标四进程精确质量、完整模型相邻对照及独立确认串为有限任务，前一阶段通过才启动下一阶段。仅复用已经检查的`model-hookcheck/model-batch.obj`，将合法device-counter链接中的`fusion.obj`替换为该副本；原VERIFY、IQ和其他链接输入保持，候选mode3，基线mode0。新的独立map必须唯一归属`model-batch-HEX.obj`；不重新编译内核，不叠加append/MMVQ组件百分比。

第一次模型链PID21904在任何编译/GPU启动前因共享基线`baseline/model.map`哈希变化而停止，失败原样保留。逐项检查原452文件记录，只有这个诊断输出变化，原EXE及工作区链接输入仍匹配；该map不在解析的链接输入集合内，也不作为原EXE的归属证据。恢复入口`model/resume.py`明确记录旧/新map哈希及原失败，只排除这个已变动的非输入输出，并将本候选map重定向到私有目录。旧记录未包含外部LevelZeroSDK库的哈希，本次候选链接另行冻结该库；不声称已验证其历史字节。原基线及旧记录不改写。恢复PID3804及创建时间、命令和脚本哈希见`model/resume-launch.json`；私有链接已exit0，独立来源检查通过，入口唯一归属`model-batch-1d08b2.obj`。其后先等待另一个编译结束才启动质量A1，实际6297槽、READY空闲1482MiB，冷评分仍在进行，尚无质量通过、速度或采用结论。

另复用已有`Win32_ProcessStartTrace`观察器，独立PID21800在不取得GPU锁、不增加模型请求的情况下记录后续编译启动，READY已出现。它必须实际覆盖后续完整测速区间且无编译事件才可补充竞争证据；当前不提前称覆盖通过。首次观察器因读取PowerShell UTF-8 BOM记录而在创建监听前退出，原stderr保留，V2仅修正BOM读取并要求READY时间早于测速监控起点。它没有改变运行中的模型脚本、评分或计时。

13:49续记：原模型链3804与观察器21800均已结束。质量A1完成followup/long/code共3/9场景后，下一请求前的原`busy`检查检测到外部icx/clang，停止采集并正常退出模型；原生观察器另外记录15个编译启动事件，归于外部`ninja strata`构建链。候选B尚未启动，未产生任何模型速度；不能将其称为候选质量失败或退化。`interruption-review.json`固定原partial、协议、评分、stderr和事件证据，原私有链接仍通过来源检查。

唯一新测量入口`model/measure-v2.py`复用原成功链接，不再编译或链接；质量和性能分别另存`quality-v2/`、`performance-v2/`，原失败不覆盖。CPU检查通过后以PID11084（UTC05:49:22）排在已有INT8-v4进程21512（创建1793252724104ms）之后，等待时绑定真实进程句柄并核对PID/创建时间/命令。新链用一个外层共享lease包住全部质量与独立测速，内部复用原collector并保留请求前busy检查；原生编译启动监听贯穿整链，任何事件都拒绝本轮。前序任务不保证成功也不作为本候选前置通过证据，只用于避免与现有构建链竞争；取得lease后还会等待实际编译结束。新来源哈希、启动身份和状态分别保存在`measure-v2-launch.json`与`measure-v2-state.json`，当前排队，尚未通过质量、速度、热历史或实际配置验收。

```powershell
python -X utf8 build-sycl-a770-gu-fixed-three-batch-20261029/resume.py --check
python -X utf8 build-sycl-a770-gu-fixed-three-batch-20261029/check-screen-review.py
python -X utf8 build-sycl-a770-gu-fixed-three-batch-20261029/model/resume.py --self-check
python -X utf8 build-sycl-a770-gu-fixed-three-batch-20261029/model/measure-v2.py self-check
```

11:55后续：原ETW11612、export15288、等待中的MMVQ12220及工具句柄均已不存在；系统启动时间仍为11:01，不能将本次终止归因为重启。原始partial记录build/start/worker/stop全部exit0，五条完整输出及原生decode边界已实际通过；export无退出记录、原CSV停在3966985514字节，stats尚未生成，不能判定完整事件覆盖。保留全部原始材料及`export-interruption.json`后，只清理本任务11612的遗留锁。新增`recover-export.py`及`audit-recovered.py`的CPU检查通过：先复核原协议、模型退出、全部来源哈希，再从同一已停止ETL导出新的CSV/stats；新审计复用原PID/QPC/schema/关联逻辑，不重跑GPU或放宽包条件。原partial/CSV不改写。离线恢复2932及其xperf3028已通过CIM身份确认存活；MMVQ仅恢复唯一质量进程23192，已经取得共享锁并进入A1准备，原四进程/九场景/758目标门槛不变。恢复结果、事件审计和质量尚未完成，不计速度或采用。

- 标准FP16 KV下的MTP Q4已完成五引擎贪心整模型对照。`build-sycl-a770-xpu-mtp-q4-fp16-20261029/measure-greedy/results.json`为`complete=true`，本轮复用原`audit.py`只读复核完整协议、退出码、来源/日志哈希、相邻配对与请求区间显存，检查通过且未改写原审计文件。50条完整请求共4970输出token，45条预热/测量请求均匹配同格式完整参考；Code两轮吞吐为−2.612961%/−2.447057%，Prose为−1.450750%/−1.250793%，Short为−1.133963%/−0.813444%，六项基线漂移绝对值均不超过0.281533%，最低采样空闲1413.878906MiB。保留请求的MTP接受/提议数也发生变化：Code为73/93→71/92，Prose为68/91→71/98，Short为42/58→42/59；各臂在对应历史位置重复。这证明两臂推测解码工作量不同，不能将整模型变化仅归因于Q4 GEMV本体，也尚未定位回退主因。`confirmed_cases=[]`、`sampling_validated=false`、`adopted=false`保留；本组不进入采用或新的质量队列，继续优先现有append和局部MMVQ验证。
- 标准FP16 KV下的共享gate/up成对提交已完成五引擎贪心整模型对照。`build-sycl-a770-xpu-shared-pair-fp16-20261029/measure-greedy/results.json`为`complete=true`，本轮复用原`audit.py`只读重算全部协议、退出码、来源/日志哈希、相邻配对与请求区间显存，检查通过且未改写原审计文件。50条完整请求共4970输出token，45条预热/测量请求均匹配同格式完整参考；Code两轮吞吐为+0.337143%/+0.483354%，Prose为+0.155128%/+0.497595%，均未达到1%。Short初筛+1.755553%，独立复测+0.080993%，也未确认；六项基线漂移均合格，最低采样空闲1417.605469MiB。`confirmed_cases=[]`，`sampling_validated=false`、`adopted=false`保留；本结果不替代固定目标或热历史质量。该组未通过当前单场景独立速度门槛，继续优先已有append和局部MMVQ验证，不把算子成对提交的收益外推为模型收益。
- 现有 VTune graph 采集未覆盖 kernel 重放：微型探针应记录 20/21 次，只记录到 1 次；原模型 graph 报告没有计算 kernel。因此 NEO 时间占比目前未知。
- 主要 IQ2_S GU/Down 没有 spill；六个有spill热点虽已证明256 GRF改变资源，全局GRF256筛选的Code/Prose仅+0.64%/−0.15%，原样独立确认降级，局部收益与退化仍需归因。自动大GRF对已核验的六项和主要GU均生成相同代码，不继续把这些实例的自动模式当作独立性能候选。
- 仅优化 SwiGLU/Q8 的 GPU 工作量，诊断空间只有 0.43%；融合的调度收益仍需另测。
- 已完成的同代码 WG128/256 对照，没有在混合驻留负载上得到稳定收益；已有四种双缓冲搬运方案也没有通过重复门槛，不原样重跑。
- GR的SiLU提前计算已有两种未通过的历史实现；其源码不同于当前对象，较晚一轮还记录外部编译活动。当前只降低原样重测优先级，不把历史算子数字写成当前整模型收益或退化。
- host USM 的 `device_read_only` 在已审计的 UR 路径不传递；上游 NEO 普通 host 分配也未继续传递该标志。不能因 API 接受参数就声称缓存策略生效；这项源码证据尚非本机驱动测量。
- Windows 的公开 NEO 源码不支持照搬 Linux 环境 debug key；用本机受支持 API 和原生指令验证选项。

本次目标吞吐+20%意味着总decode耗时至少减少16.67%。以上比例不足以预测最终增益；最终须在同一组合EXE上关闭profiler，在正常graph中完成固定输入、完整输出、相邻基线漂移≤3%、独立重复、质量和显存余量验证，并单独验证实际生产配置；Windows通过后继续Linux。

## 证据与复核

- [原始采集的归因、覆盖与对象哈希](decode-gpu-profile-2026-10-28.json)
- [历史 host/device 计数及对象范围](host-device-metrics-2026-10-28.md)
- [正常graph CPU、实际提交路径及独立完整模型对照状态](runtime-paths-2026-10-29.md)
- [已测内存搬运路径](memory-path-2026-10-27.md)、[同代码 WG 大小对照](gu-schedule-2026-10-28.md)
- [本次重新分组与 spill 排序脚本](check_neo_igc_hotspots_20261029.py)，只读原始 CSV，不启动 GPU：

```powershell
python bench/results/a770/check_decode_gpu_profile_20261028.py --local
python bench/results/a770/check_neo_igc_hotspots_20261029.py
python -X utf8 build-sycl-a770-runtime-graph-cpu-sw-20261029/audit.py
python -X utf8 build-sycl-a770-runtime-append-trace-20261029/audit.py
python -X utf8 build-sycl-a770-runtime-append-review-20261029/recover.py --check
python -X utf8 build-sycl-a770-igc-spill-native-20261029/run.py --self-check
python -X utf8 build-sycl-a770-igc-spill-native-20261029/recover.py --check
python -X utf8 build-sycl-a770-igc256-model-20261029/inspect_isa.py
python -X utf8 build-sycl-a770-gu-index32-20261029/prepare.py --self-check
python -X utf8 build-sycl-a770-gu-index32-20261029/run.py --self-check
python -X utf8 build-sycl-a770-gu-index32-20261029/run.py --check
python -X utf8 build-sycl-a770-gu-index32-20261029/recover-screen.py --check
python -X utf8 build-sycl-a770-matmul-order-review-20261029/review.py --check
python -X utf8 build-sycl-a770-igc256-model-20261029/run.py --self-check
python -X utf8 build-sycl-a770-igc256-model-20261029/audit.py --self-check
python -X utf8 build-sycl-a770-igc256-confirm-20261029/run.py --self-check
python -X utf8 build-sycl-a770-graph-wait-callers-20261029/check.py
python -X utf8 build-sycl-a770-gr-source-review-20261029/check.py --check
python -X utf8 build-sycl-a770-igc256-cohort-review-20261029/review.py --check
python -X utf8 build-sycl-a770-igc-window-review-20261029/review.py --check
python -X utf8 build-sycl-a770-gdn-step-scoped-20261029/run.py --self-check
python -X utf8 build-sycl-a770-gdn-step-model-20261029/run.py --check
python -X utf8 build-sycl-a770-gdn-step-model-20261029/confirm.py --self-check
python -X utf8 build-sycl-a770-graph-etw-probe-20261029/coverage.py --check
python -X utf8 build-sycl-a770-graph-etw-qpc-20261029/run.py --self-check
python -X utf8 build-sycl-a770-graph-etw-qpc-20261029/audit.py --self-check
python -X utf8 build-sycl-a770-graph-etw-qpc-guid-20261029/run.py --self-check
python -X utf8 build-sycl-a770-graph-etw-qpc-guid-20261029/recover.py --check
python -X utf8 build-sycl-a770-graph-etw-model-20261029/run.py --self-check
python -X utf8 build-sycl-a770-graph-etw-model-20261029/audit.py --self-check
python -X utf8 build-sycl-a770-root-sync-path-20261029/run.py --self-check
python -X utf8 build-sycl-a770-root-sync-path-20261029/run.py --check
python -X utf8 build-sycl-a770-igc-scoped-mmvq-20261029/run.py --self-check
python -X utf8 build-sycl-a770-igc-scoped-mmvq-device-counter-20261029/build.py --self-check
python -X utf8 build-sycl-a770-igc-scoped-mmvq-device-counter-20261029/recover-build.py --check
python -X utf8 build-sycl-a770-igc-scoped-mmvq-device-counter-20261029/quality.py --self-check
python -X utf8 build-sycl-a770-igc-scoped-mmvq-device-counter-20261029/performance.py --self-check
python -X utf8 build-sycl-a770-igc-scoped-mmvq-device-counter-20261029/hot-history.py --self-check
```

按用户“先剖析热点”的要求，尚未取得GPU锁的整模型选项扫测已延后；原脚本和失败记录保留。未修改生产配置；本次沿用同场景独立1%及质量、热历史、显存、实际配置验收。历史≥20%及后续Linux方向不增加本次完成门槛；当前仍缺质量、热历史和实际配置的完整采用证据。

GRF256筛选的无GPU复核 `python -X utf8 build-sycl-a770-igc256-model-20261029/audit.py` 已通过：从原始协议重新构造每条完整输出与DONE计时字段，检查请求顺序、实际6297槽、READY余量、每进程四组48层预填扫描、来源哈希及加速比。`--confirm` 分支为未启动的独立确认保留，准备工作不计性能验收。

## 合法device-counter MMVQ的新基线初筛：不采用

`build-sycl-a770-igc-scoped-mmvq-device-counter-20261029`的四进程冷评分通过原始9场景/758目标精确相等要求。随后新A1/B1/A2整模型测速已完成，三个进程均exit0，全部输出、每进程八组48层扫描、合法EXE身份及本组显存/频率覆盖检查通过；独立`performance.py --check`也exit0。`performance/results.json complete=true`表示采集及复核完整，`passes_screen=false`、`passes_confirmation=false`，没有合格场景。

| 场景 | 相邻基线耗时几何均值 | 候选耗时中位数 | 吞吐变化 | 基线耗时漂移 | 判定 |
| --- | ---: | ---: | ---: | ---: | --- |
| Code2K | 4455.362 ms | 4490.500 ms | −0.782504% | −4.125093% | 无正收益，且漂移超3%，不能据此确认退化幅度 |
| Prose2K | 4745.826 ms | 4832.200 ms | −1.787474% | −1.437148% | 稳定基线下未过正收益门槛 |

显存也不满足采用门槛：A1/B1/A2完整请求内的最低采样空闲显存分别为1004.680、1002.078、975.578MiB，均低于1024MiB，`sampled_memory_passed=false`。本组环境额外固定`SYCL_CACHE_PERSISTENT=0`，与runtime append已确认测速的共同环境存在这一项差异；本结果不能用于推翻或替代另一个cohort的测量。旧prefill基线上的约1.22%收益没有迁移到本次合法组合，不叠加计算、不进入独立确认或生产采用。后续优先补齐已有runtime正收益的质量、热历史及实际配置验证。

```powershell
python -X utf8 build-sycl-a770-igc-scoped-mmvq-device-counter-20261029/performance.py --check
```

## 新结构实验：有符号码本分解

[IQ2_S signed-half 实验](signed-grid-2026-10-29.md)利用 1024 个八码字仅含 81 种四字节半码字的结构，预先合入符号后，以 5184 字节有符号表加 2048 字节映射替换热循环中的符号展开。全部 262144 种组合的 CPU 位级等价检查、编译、6 个 GPU 边界检查和 13 个计时进程已完成。部分审计通过，但首轮同辅助源码对照没有合格 T：mixed4 为 −0.950% 至 +0.170%，mixed8 为 −1.762% 至 −0.815%；全显存诊断为 −24.057% 至 −19.936%。外部编译在 A2-mixed8 前导致执行器停止，完整筛选保持未完成，更早编译重叠未被独立排除，不宣称已确认退化幅度。停止该候选的后续确认和接入；较小码本并不自动更快，新增依赖查表成本尚未完成原生归因。

真实路由的另一项[三条尾项复用分析](gu-remainder3-real-routes-2026-10-29.md)已通过只读复核：在候选覆盖的 GU22 层，4/3/2/1 比现有 4/2/1 少约5%的逻辑权重准备遍数；这不是硬件字节或整模型加速。实际主要是 T3/T4 窗口，合成筛选通过的 T5/T6 几乎没有出现，须继续看现有完整模型验证，不能直接移植合成吞吐数字。

新发现的格式覆盖缺口是[IQ2_XXS两条复用](gu-iq2xxs-pair-2026-10-29.md)：当前11个GU16层仍走逐条dot，而已有`Split<16>`可直接复用。保留真实路由的逻辑准备计数减少Code19.858%/Prose19.067%，仅为这一格式的机会量；不是局部或整模型提速。隔离候选保持SG32/WG256及原解码/缩放，源码自检和只读计数独立复核通过，GPU编译及双基线完整链已排队；尚无正确性、寄存器或性能结论。

队列状态续查：上述 GU 批量的完整算子复核已通过，计时进程 40→28；新增四项纯 CPU 审计的固定批量入口全部通过。本地构建入口已接入已有共享锁。INT8 V4 和 GU 模型 measure-v2 随后均因外部编译停止，不能继续称为等待；失败完整保留。GU 已用现有 EXE 排入私有 v3 质量/独立测速链，尚无新整模型结论。见[合并内容、检查命令与实际启动记录](queue-batch-followup-2026-10-29.md)。

### GR 四内核批量资源提取的新结果

同一冻结 HC 对象的四份 native 模块已提取并独立复核：up/reduce/norm 都有每工作项1256B的global `private_space`，metadata `private_size`分别20096/20096/40192B；四者仍没有报告寄存器spill。因而“无spill”不能解释为“无private访存”。up静态SLM12288B，norm64B；down的4096B动态local由调用者提供。详见[资源表与候选边界](gr-native-resources-2026-10-29.md)。新排队批次只测试三内核的固定索引参数选择，先要求同源控制机器码复现，再检查消除private、完整数值和后续速度，不重复旧SLM/SiLU扫测。另核实SYCL `gpu_stamp_kernel`当前写入常数0，不能仅开启graph阶段开关就宣称获得了有效GPU时间戳；相关正常graph阶段成本仍待有效计时机制确认。本会话最终20%目标保持active，资源改进不等于吞吐提升。

后续四内核离线 IGA 批量检查已确认实际 private 写入：up/reduce/norm 入口的 private base 参数经线程步长寻址后，分别到达 PC `0x1128` / `0x11a0` / `0x1138` 的 scalar/transpose UGM store。逐条 ABI、PC、寄存器和哈希复核通过，没有编译或 GPU 执行。该证据说明 private 并非只有 metadata 预留，但仍不能把1256B分配量当作每工作项实际流量或预测收益；静态指令/send计数及复核命令已补入[GR资源报告](gr-native-resources-2026-10-29.md)。固定索引候选的资源、质量和条件图测速仍按原排队协议执行。

GR固定索引批次现已通过：同源重建的四份机器码精确复现原对象，候选up/reduce/norm的private分配消失、无spill，GRF128/SIMD/SLM保持，down原码不变。三路径六项parity/null全部exit0，null的32个T/sink/changed见证逐项匹配原对象；独立资源检查和新CPU IGA对照均通过。候选up静态指令3265→1213、UGM消息83→17，reduce为2437→1860/102→82，norm为1565→1090/51→26；这些不是动态流量或速度。完整HC图筛选19124已持锁执行两轮双基线协议，最终模型decode≥20%仍无完成证据。同期GU v3在B2第二个长输入的prefill因非有限值被原检查拒绝，未进入测速；原失败及无编译事件的guard保留，不将A1/B1的精确评分局部通过拼成完整质量验收。详见[批次终态与GR资源/ISA表](queue-batch-followup-2026-10-29.md)。

后续完整HC小图两轮已结束，独立检查通过，但`passed_tokens=[]`；条件模型21856按原协议跳过链接与模型验证，未将资源改善算作速度通过。合批图诊断17480把chain1/96和T1/T6共用每臂一个进程，4→2进程，已冻结并等待共享lease，检查节点、输出poison/原checksum及提交/尾wait。新增公开UR源码与原生小probe日志复核发现，四种配置的连续回放循环各19个间隔均恰好一次`zeEventHostSynchronize`，append也保留；因此提交循环不可等同纯CPU成本，也不能仅凭最后才queue wait就假定5000张图同时在途。七项固定CPU批量复核全部exit0，详见[原始证据与边界](gr-native-resources-2026-10-29.md)。

16:47对其他现有FP16/V4首轮的只读核对补充一条范围限制：Prose首轮约+11.466%、Code−1.068%尚在独立确认，且FP16改变value-cache数值。已关闭A1/B1/A2的记录显示，候选Code/Prose128输出相对原参考分别有86/68个位置不同；B1的接受/提议为72/91、68/89，原版Code为73/92、Prose随保留历史为72/95、71/96、71/98。因此这不是保持原输出和草稿工作量相同的NEO/IGC优化证据，不能直接与runtime/GR收益乘算或宣称完成20%。该策略本身的完整758目标及匹配热历史质量仍须另验收；未改其在测脚本或结果。当前目标仍为Windows同一冻结负载的整模型decode≥20%，之后才处理Linux。
