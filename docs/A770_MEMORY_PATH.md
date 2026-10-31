# A770 GU/Down 内存路径执行清单

本组先执行 A/B/C，再决定是否调整缓存。测量机器为 A770 16GB、PCIe 3.0、Windows，使用现有 DPC++ 2025.3 工具链。下文保留原执行协议；[已完成的 eager/graph 对照及终审](../bench/results/a770/memory-path-2026-10-27.md)记录 5808 批计时。T1 小工作集显存驻留快 12.07–14.55 倍，但 C1/C5/C10/C30 均未通过独立重复收益门槛；没有整模型接入。当前验收目标为 Code/Prose 2K、完整 128 输出稳定达到 35 tok/s，40 tok/s 为进一步目标，均尚未达成。

| 方案 | 执行状态与后续选择 |
| --- | --- |
| A：原主存路径 | 已测；实际 `malloc_host` 远程读取，另有运行时指针类型和系统读计数验证 |
| B：device USM 常驻 | 已测；给出小工作集的本地计算对照，不外推为整模型加速倍数 |
| C：分块双缓冲 | 已测1/5/10/30专家四种块大小、三种GU格式、正反序及graph；均未复现1%收益，保留原路径 |
| D：shared USM 预取 | 当前权重不是shared USM，不直接套用页面迁移预取；C已覆盖显式搬运对照 |
| E：软件专家缓存 | [当前6297→6561槽对照](../bench/results/a770/cache-current-2026-10-28.md)：Prose +1.460%达到初筛，但运行时采样空闲最低922MiB，未过1024MiB、不采用；先检查实际窗口复用 |
| GEMV读取、解包与批量复用 | 下文记录已完成的工作组、协作读取、字段重排和独立复测；继续按实际解码剖析选择候选 |

当前10%上限裁剪的[单场景速度复测](../bench/results/a770/down-retry-fp32-2026-10-28.md)已完成：Code两轮+3.996%/+3.746%，独立候选中位数30.42187 tok/s；Prose两轮−6.229%/−6.332%，所有对应基线漂移合格。运行时采样最低空闲1442.117MiB，首次热历史质量因漏采646个长请求目标评分而停止，不能判断模型质量；[私有main采集修正](../bench/results/a770/warm-logpos-2026-10-29.md)的真实日志自检通过，已排队编译及重验。[不设上限组](../bench/results/a770/down-retry-uncapped-2026-10-29.md)也已完成冷质量和独立性能，Code中位数31.243135 tok/s，热质量同样因评分漏采待补；两组均未采用、未达稳定35。后续[实际窗口专家重复次数诊断](../bench/results/a770/window-reuse-2026-10-29.md)和[完整INT8融合对原生grouped GEMV](../bench/results/a770/int8-native-grouped-2026-10-29.md)仍在等待共享锁。先区分逐路由热度、唯一专家数与分块读取模型，再决定缓存策略候选；逻辑计数不作为真实PCIe流量。

INT8对照后另排[当前6297槽换入上限32→16](../bench/results/a770/adapt16-current-2026-10-29.md)，只使用现有命令行参数，检查减少额外搬运与缓存命中之间的权衡。旧4191槽单场景信号仅作动机，新候选仍需当前完整请求、同场景独立性能、物理显存和热历史质量验收。

## P0：已确认的路径

- [x] 后续[主存/显存硬件计数对照](../bench/results/a770/host-device-metrics-2026-10-28.md)在运行时用 `get_pointer_type` 确认 host/device USM 类型；不把 host USM 解释为 shared USM 页面迁移。
- [x] 源码 `sycl/src/core/gguf_expert_source.cpp` 的镜像分配使用 `sycl::malloc_host`，`device_alias()` 返回同一镜像地址。
- [x] 冻结原版 `quality-cache-combo/A1.stderr` 记录 24576/24576 个缺失专家、33.02 GiB 固定主存镜像，GPU 通过 PCIe 读取。此证据对应主存远程读取；不能按 shared USM 页面迁移解释。
- [x] 保留历史串行搬运失败结果：[完整链慢约 3.7%–4.5%](../bench/results/a770/expert-stage-2026-10-25.md)。历史实验同队列执行 GPU copy kernel，没有独立复制队列或双缓冲重叠，不能替代本组 C。

## 固定条件

2026-10-29补充当前decode分派核对：冻结的`build-sycl-a770-gu-q8-export-20261027/verify.cpp`来源哈希与device-counter构建清单一致；已完成append性能组的共同环境中`STRATA_VERIFY_DEVICE_PLAN`、`STRATA_VERIFY_NO_HOST`、`STRATA_VERIFY_ADAPT`均为1。该源码据后两个开关设置`collect_routing_`，主grouped调用使用显存或pinned mirror专家指针，而第二次PCIe grouped调用及CPU结果合并在`!collect_routing_`分支内。实际启动日志也记录24576个专家的完整主存镜像。

因此，虽然两臂CLI仍保留`--pcie-frac 0.16`，不能将其解释为这条device-plan/no-host decode路径仅让GPU读取16%的缺失专家，也不据此新增分流比例扫参。这里核对的是冻结源码、实际环境和启动见证，没有运行新的分流比例性能对照；不外推prefill、host-served回退路径或其他EXE。后续PCIe4.0对照应核对实际路由与主存读取，而非仅比较这个CLI数值。

复用冻结 `build-sycl-a770-vram-down-sg16/iq.obj` 的 `native_expert_grouped`，不修改 kernel、解包、累加精度或量化格式。真实权重来自 `experts-{22,16,42}.bin`：分别为 IQ2_S、IQ2_XXS、Q2_0 GU；Down 固定 Q2_0，N=2560、F=640。每种格式八组、每组 30 个真实专家，种子 7702102；输入先用原有 Q8_1 量化。T=1 是单 token GEMV，T=2/4/6 检查实际小窗口复用。GU16/Down4，各路径相同。

三个路径都分配相同的主存、完整 device USM 副本、两个最大 30 专家的 staging buffer，以及输出和 scratch。初始化、一次性驻留复制、检查读回不进入计时。B 只给出相同小工作集常驻显存的上限，不代表整个模型能常驻。

| 组 | 读取路径 | 唯一变化 |
| --- | --- | --- |
| A | 固定主存镜像 → 原始完整链 | 当前远程读取 |
| B | 初始化时显式复制 → device USM → 同一完整链 | 权重驻留位置 |
| C1/C5/C10/C30 | 每次从不可变主存复制压缩权重 → 两个 device buffer → 同一完整链 | 每块专家数与复制调度 |

本组使用 eager 提交对照内存路径，A/B 都有与 C 对应的队列计时标记。C 分块会增加完整链调用次数，代价计入总耗时；没有同时改为融合 kernel。跨队列实际能否重叠由事件时间记录检验，不能仅凭有两个队列宣称已重叠。只有获得可重复收益后才做命令图与整模型接入。

## 执行与检查

- [x] 运行离线检查：`python build-sycl-a770-memory-path-20261027/run.py --self-check`。
- [x] 等待已有实验；编译、链接、哈希和 GPU 测量均通过共享 `gpu_lock()` 与 `busy()`，不删除他人的锁。
- [x] 冻结脚本、worker、原始 IQ 对象、ABI 头、编译命令、环境来源及真实权重 SHA256。协议写入后不原地修改。
- [x] C 的每个 slot 在复制前依赖该 slot 上次 Down 完成；计算依赖本次 memcpy 完成。两个 buffer 在两条队列全部完成前保持有效。
- [x] 检查非零 entry 起点、分散输出、完整输出和 Q8 中间字节逐位一致、未使用行不变、scratch/staging 外侧守卫和不可变指针表。检查连续八组提交后的全部八组输出，避免只检查最后一次覆盖的输出。
- [x] 每组先预热 16 条完整链，然后保留 11 批，每批连续轮换八个真实权重组。不删除离群值，记录中位数、MAD、最小和最大值。
- [x] 每种格式两个新进程：正序 A/B/A/C1/A/C5/A/C10/A/C30/A，反序 A/C30/A/C10/A/C5/A/C1/A/B/A。每个候选都有相邻 A 对照；反序进程的完整基线输出和 Q8 字节还须与正序保存的参考逐位相同，两个进程均通过才算算子确认。
- [x] 使用 `python build-sycl-a770-memory-path-20261027/run.py` 运行三种 GU 格式；完成后 `--check` 重建数据并核对来源。所有失败和部分结果保留。

## 字段定义和验收

| 字段 | 定义 |
| --- | --- |
| `total_us` | 从提交到两条队列完成的主机时间，每条专家完整链的均值；包含提交、搬运和等待 |
| `transfer_us` | 本批 H2D memcpy 事件执行时间之和除以八；A/B 稳态为零，一次性初始化不在其中 |
| `compute_queue_span_us` | 每次复制依赖满足后的计时标记结束，到该块最后 Down 事件结束的跨度之和除以八；包含队列/主机提交间隙，不是纯 kernel 时间 |
| `down_kernel_us` | 原有 API 最后一个 Down 命令的事件执行时间之和除以八；不等同 GU+Down 总时间 |
| `copy_down_overlap_us` | H2D memcpy 与 Down 命令事件区间交集之和除以八，只反映已观测命令区间 |
| `weight_bytes` | 每条完整链涉及的压缩专家逻辑字节数，不能解释为真实 PCIe 或硬件缓存行流量 |
| `kernel_us` | 留空：冻结 API 不返回 GU/SwiGLU 的全部独立事件，不能用队列跨度冒充 |
| `host_read_gbps` / stalled / occupancy | 留空：本组没有硬件计数器采样；需要另用支持当前 A770 的 profiler 对相同工作负载测量 |
| `tokens_per_sec` | 留空：本组是专家算子，不是完整模型生成 |

算子筛选按相邻 A 耗时几何均值计算吞吐变化，收益至少 1%，基线耗时漂移不超过 3%。吞吐相对变化绝对值小于 0.3% 正负均视为噪声，保留原数值。两个独立进程分别判定 T1/2/4/6，不用平均值掩盖失败或跨格式推断。以下原执行协议与历史判定保持；后续整模型按用户最新要求允许单场景达到1%后继续独立确认，无需Code/Prose联合通过，质量与显存检查仍需通过。

- [ ] 若 C 有重复收益，先确认复制是否实际重叠、最佳块大小及 staging 显存成本，再接入原始 Code/Prose 2K 完整链。
- [ ] 后续整模型仍须正确性、质量、显存余量和独立速度确认，同一场景在初筛和独立确认中均至少提升1%即可继续验收；Code/Prose分别公开，35 tok/s单独检查。原算子实验协议和历史判定保持。
- [ ] 若 B 快而 C 慢，保留证据，优先研究热专家驻留及减少搬运字节；不能据此宣布分块搬运成功。
- [ ] D 不套用 shared USM 预取到当前 host USM。E 缓存容量扫描等待 A/B/C 结果和缓存对象确认后另建单变量协议。

## 2026-10-28：按实际设备补充硬件指标

在共享独占锁下，通过已安装的 Level Zero loader 和 Python 标准库查询本机 A770。目录返回 2128 个指标组、46580 个指标条目，去重后为 5082 个指标名；同名组可能分别支持事件和时间采样。这些数字是目录查询结果，尚不能证明每个指标都能成功采集。

| 需要确认的行为 | 本机实际目录 | 解释边界 |
| --- | --- | --- |
| GPU 读取系统主存 | `SYSMEM_BYTE_READ`，`DataportWrites` 组 | 单位为 bytes，须结合采集区间换算带宽 |
| 主机读取显存 | `HOST_TO_GPUMEM_BYTE_READ`，`DataportReads` 组 | 目录描述为 host 从 GPU local memory 读取；方向与上一项不同 |
| 显存读流量 | `GPU_MEMORY_BYTE_READ` | 与主存读取分别记录 |
| L3 读流量 | `L3_BYTE_READ`，`ComputeOverview` 组 | 不是直接 PCIe 读带宽 |
| 停顿与占用率 | `XVE_STALL`、`XVE_THREADS_OCCUPANCY_ALL` | 百分比本身不能唯一归因于主存延迟 |
| 核心频率 | `AvgGpuCoreFrequencyMHz` | 与相同负载一并采集 |
| TLB misses | 此目录未找到 TLB 名称或描述 | 保留缺项，不能据此断言硬件没有 TLB 或相关瓶颈 |

目录工具位于本机 `build-sycl-a770-metric-catalog-20261028/catalog.py`，其 `--self-check` 检查 x64 ABI、空/非空枚举和驱动错误处理。真实查询已完成，原结果保留，不覆盖重跑。

两组[主存/显存计数对照](../bench/results/a770/host-device-metrics-2026-10-28.md)已完成：复用原始 IQ2_S GU/Q2_0 Down 对象、240 个真实专家和原完整输出参考，仅将权重分配切换为 host/device USM，并用 `get_pointer_type` 验证。四进程共 288 项完整输出/守卫检查通过，每进程 GU、Down、SwiGLU 各 2088 次调用。默认 Global Memory Accesses 预设实际选中反方向的 `HOST_TO_GPUMEM`；独立 V2 明确选择 `SYSMEM_BYTE_READ`，采集和额外终审通过。主存路径实际系统读取为 8.69–9.38 GB/s，驻留显存后低于 0.01 GB/s，停顿明显下降。ezpp 继续用于主机阶段计时，不能提供这些硬件计数。这些是瓶颈诊断，分析开启时的耗时不作性能收益验收。

2026-10-29补充页粒度核查：公开UR固定提交`5e422374f5f6710714d970f66c71658182d448d7`的[host分配实现](https://github.com/oneapi-src/unified-runtime/blob/5e422374f5f6710714d970f66c71658182d448d7/source/adapters/level_zero/usm.cpp#L256)以flags=0调用`zeMemAllocHost`；[对齐参数检查](https://github.com/oneapi-src/unified-runtime/blob/5e422374f5f6710714d970f66c71658182d448d7/source/adapters/level_zero/usm.cpp#L1235)拒绝超过64KiB的值。因此提高SYCL地址对齐不能作为使用更大后备页或降低TLB miss的证据。[原生页大小查询](https://github.com/oneapi-src/unified-runtime/blob/5e422374f5f6710714d970f66c71658182d448d7/source/adapters/level_zero/usm.cpp#L741)读取`zeMemGetAllocProperties.pageSize`，本机实际分配尚未查询。

同一源码中的`UR_L0_USM_RESIDENT`默认值0x2只选择device分配，host控制位在高半字节；启用后调用`zeContextMakeMemoryResident`。这不能证明host权重迁入显存、使用大页或稳态GEMV会更快。本次仅归档公开源码SHA256及七段准确摘录，保存在本机`build-sycl-a770-host-usm-pages-audit-20261029/source-audit.json`；没有证明公开源码与已安装Windows DLL完全一致，没有新增GPU测量或修改运行时。后续只有取得实际页属性及相关瓶颈证据，才考虑独立分配策略实验。

本机只读复核：`python -X utf8 build-sycl-a770-host-usm-pages-audit-20261029/check.py`。该检查仅核对缓存公开源码、摘录及host分配入口到pool的调用路径，不调用驱动。

随后完成[已提交前缀热度试验](../bench/results/a770/cache-accepted-prefix-2026-10-28.md)：四进程/40 条质量请求的固定目标评分均精确匹配参考，但完整 Code/Prose 2K/128 筛选分别为 −1.119%/−0.522%，逻辑未命中反而增加 2.746%。候选没有采用。验收计数已改为完整窗口后立即输出，修正 `_Exit(0)` 跳过析构汇总的问题；生产保持原策略。

新的[整模型 decode 采集覆盖检查](../bench/results/a770/decode-gpu-profile-2026-10-28.md)发现：本机 VTune 路径对 graph 的21次执行只报告1次 kernel，不能用它直接计算逐 kernel 占比。eager 正对照准确记录暂停后的20次，随后完整2K eager解码取得112窗口、GU/SwiGLU/Down各5376次的交叉核对；专家链占累计kernel时间35.94%，不代表正常graph的时间比例或性能收益。冻结2K基线实际IQ2_S GU为WG256，和上述硬件计数组的WG128算子对象不同；A/B/C搬运对照则使用相同冻结`iq.obj`。后续线程/缓存优化必须先确认实际链接对象，不能混用两套内核的速度或停顿数据。

随后完成[GU 工作组大小的双基线对照](../bench/results/a770/gu-schedule-2026-10-28.md)：保留冻结原对象，另用同一 helper 比较 WG256/WG128。六个边界进程、27个计时进程及1782个批次检查通过；两种主存/显存混合比例中，WG128 相对同代码 WG256 没有任何形状达到重复1%收益。纯显存对原对象的正值主要来自 helper 差异，不能外推为混合模型收益。候选未接入模型，生产配置保持。

[SG16 协作读取](../bench/results/a770/gu-coop-load-2026-10-28.md)继续保持原始布局：每对82字节块用41个对齐DWORD读取，再用 subgroup shuffle 分发字段。六个边界进程和27个计时进程的完整输出/Q8/守卫检查通过，1782批全部保留；两种混合比例均未取得收益，纯显存也明显回退。减少加载语句不能证明总时延下降；该结果支持下一步独立测试同字节数的字段重排，尚无整模型接入。

随后完成[同字节数字段重排](../bench/results/a770/gu-fieldpack-2026-10-28.md)：每对仍为164字节，GPU直接读取对齐字段，不用 subgroup 分发；每个候选进程先反向核对1536000个真实块对。完整输出及1782批审计通过，但两种混合比例相对同 helper 标量路径均未取得重复1%收益，不接入模型。原布局重建 helper 的 T5 探索性信号也已独立复测：660批完整检查通过，mixed4 为−0.966%/+0.950%，mixed8 两轮漂移超限；未复现1%收益，不计作 repack 或模型收益。

接着按原生 MMVQ 热点完成[IQ4_XS 寄存器查表](../bench/results/a770/iq4xs-register-lut-2026-10-28.md)：真实输出头、QKV、共享专家上投影、SSM输出共45进程通过完整检查，2376批全部保留，查表没有通过任何形状/列数的双基线门槛。标量 SG16 又独立完成10进程/660批，2560→640三/四/六列通过，其他预选组合因漂移或未复现排除。随后原始Code/Prose 2K/full128整模型筛选为+0.559%/+0.818%，未过联合1%门槛、不采用；24次完整2K输出及3次Short匹配原参考，末尾扫描计数误判经单独审计恢复，原失败保留。

随后测试[IQ4_XS 逐字解包与点积融合](../bench/results/a770/iq4xs-stream-dot-2026-10-28.md)：修正每块FP32表达式边界后，九个边界及36个计时进程完整输出通过，2376批全部保留。活跃三至六列相对原版两轮回退35.077%–43.138%，相应基线漂移均不超过1.372%；相对同helper控制也没有合格形状。不接入模型，不以减少中间值或融合语句数作为提速证据。

再按已观察到的spill测试[IQ4_XS GRF256](../bench/results/a770/iq4xs-grf-2026-10-28.md)：十二个实际原生模块验证GRF128/256属性生效，GRF256不再列出spill。2376批初筛后，新的18进程/1188批双基线确认只保留共享投影2560→640的五/六列，约15%–22%算子收益；QKV六列不足门槛、共享三列漂移超限。已有eager采集未出现640行五列，六列仅约0.050%的累计计算kernel时间，不能外推正常graph占比；本轮保留算子结果、不接入模型，继续优先主存缺失和专家链。

随后对[Q2 Down 固定五步展开](../bench/results/a770/down-unroll-2026-10-28.md)完成六个边界进程、27个计时进程及1782批检查。两种混合驻留相对同helper均无重复1%合格形状，未接入模型；两份原生核均为128 GRF/SG16，展开减少的静态分支没有转化为稳定收益。控制核的超长节名反汇编失败经独立容器改名审计恢复，原指令不变、原失败保留。

## 当前执行清单：缓存容量补验

已完成的 A/B/C 不再重复。新的 `build-sycl-a770-cache6824-current-20261028/run.py` 只用当前原始 FP32 Down-retry EXE 比较实际6297/6824槽，不叠加旧组增幅，不修改数学、权重或生产配置。实际A1完成八条2K/128、输出均匹配；B1实际6824槽但READY仅907MiB，未达到固定1024MiB，在Short或2K请求前停止，两引擎均退出0。独立`audit-failure.py`已核对原始协议、输出和来源，`failure-audit.json`完成；没有候选速度或完整A/B/A结果，不作收益/回退结论。后继`build-sycl-a770-cache6250-current-20261028/run.py`保留同一门槛：请求6250已校准为实际6561槽、READY空闲1474MiB，Short16精确匹配、引擎退出0。首个仅导入阶段的命令行串入失败已单独保存并修正，未发出模型请求；上述是随后实际校准结果，不是2K性能或峰值余量验收。

- [x] 固定原始Code/Prose2048输入、完整128输出、graph/FP16KV、chainON/batchOFF及实验min-p0.5；生产仍为0.8。
- [x] 完成容量单变量、配对门槛、显存样本及逐请求48层扫描的离线自检；Short初始化不要求bulk-prefill扫描。
- [x] 编译只读Sysman监测并取得两项有效显存状态：A770唯一device-memory模块、16GiB物理容量，保留状态码及free/size原数值。
- [x] 中间容量实际6561槽、Short16及1474MiB启动余量通过。两个零请求脚本失败均保存；`build-sycl-a770-cache6250-current-20261028/finish.py`补齐引用并以标准库符号表检查复用函数的全局名称，新的完整测速写入`performance-v2/`，不覆盖旧记录。
- [x] 完成独占GPU的A1/B1/A2：24条完整2K/128输出、3条Short全部匹配参考，三引擎退出0；逐请求48层Down扫描共1152次通过。
- [x] [当前容量筛选](../bench/results/a770/cache-current-2026-10-28.md)：Code +0.034%属于0.3%噪音区间，Prose +1.460%；两端基线耗时漂移+0.856%/+2.020%。原联合协议未自动运行独立确认；按用户随后更新的单场景规则，Prose达到1%初筛，仍待独立复测及显存问题处理。
- [x] 24条完整请求共630项显存样本、decode共112项频率样本，覆盖通过；6561槽运行时最低空闲922.074MiB，低于固定1024MiB，不采用。READY为1461MiB，不能替代运行时余量，也不能据此断言发生WDDM换页。
- [ ] 当前容量的固定目标质量、完整历史及长上下文检查未执行；先查明运行时显存占用，再按单场景规则决定Prose后续确认。生产未改，稳定35/40未达到。
- [x] 另建`build-sycl-a770-graph-memory-20261028/`诊断固定6297槽的graph/eager/graph。6条完整2K及3条Short输出匹配、三引擎退出0；12个相位共175项物理显存样本通过。eager完整请求内空闲中位数约1166MiB，和graph约1160–1171MiB接近；关闭verifier命令图未明显回收首次请求后的数百MiB占用，尚未定位具体来源。该组不作吞吐验收，不据此扩容；[完整数值与边界](../bench/results/a770/cache-current-2026-10-28.md#后续首次请求的显存占用)。
- [x] [首次请求的UR分配追踪](../bench/results/a770/usm-allocation-2026-10-29.md)：Short16仅新增1.661MiB device USM、无释放，Code/Prose结束时逻辑存活量相同；360项物理显存样本及全部原始输出检查通过。不能据此归因于USM池，追踪耗时不作速度证据。
- [x] 按首次Short加载98个程序/kernel的线索，隔离重链接`per_source`。默认60秒和共同300秒保护下，候选均未完成首个Short，前置原版均匹配；没有候选吞吐或显存回收结论，不采用。[两组完整失败审计](../bench/results/a770/code-split-2026-10-29.json)已持锁完成、退出0，不继续延长时限或据此扩容。
- [ ] [不设上限的末两位缺失专家裁剪](../bench/results/a770/down-retry-uncapped-2026-10-29.md)：四进程冷质量通过，各982行/758目标完整核对，汇总PPL−0.161991%。新六进程完整2K/128独立性能确认Code +6.104945%/+6.305987%，独立中位数31.243135、最低30.608097 tok/s；Prose +0.312311%/+0.627831%未过1%。48个完整长请求、1211项显存样本最低1596.636719MiB，通过1024MiB；215项解码频率样本均2400MHz且节流标记0。候选八条请求的输出及MTP工作量按历史位置重复，并非相同输出的纯算子提速。原热A1的18条请求完成、引擎退出0，但只采到271行，112个短场景目标精确匹配、646个长场景目标缺失；控制器在解析时停止，没有候选热进程，不能判断热质量。独立修正入口已核对真实失败并等待既有私有main构建。没有生产采用，未达稳定35。
