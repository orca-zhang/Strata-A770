# DeepInfer 优化 checklist

对照 DeepInfer `9bd9cf243fc53ef9afbf8f04d391d989117d6ab9` 的异步调度、按成本分配验证预算和 KV 前处理融合。
目标为 Strata 的 A770/SYCL 路径；不把 DeepInfer 的集群吞吐外推到单卡。

## 基线与采用标准

- [x] 冻结实施前工作区：988 个文件，34,619,146 字节；原始修改、补丁和逐文件 SHA256 留在独立构建目录。
- [x] 不增加依赖、框架或模型权重；保留生产 EXE。2026-11-02 在当前 Orca 配置启用累计阈值0.5，原配置备份见末节。
- [x] 从冻结源码和当前源码分别完整构建，记录源文件与产物哈希；96 个未改依赖对象/库逐字节相同。
- [ ] 每项分别检查、计时，再检查组合。
- [x] 稳定算子收益可保留；算子、阶段和整模型结果分别报告，不连乘。
- [x] 按用户最新更新，性能提升门槛为1%，算子正确性通过且观测收益为正即可保留源码；历史筛选和漂移记录保留，覆盖不足单独说明，整模型提速仍需单独验证。

## 实现

- [x] QSA forward 使用已有批量 indexer append，保持尾部快照顺序。
- [x] 批量写入 FP16、Q8、Q4 和 K8V4，位置取自设备数组，保留镜像与缺页处理。
- [x] `STRATA_NO_BATCH_KV_STEP=1` 恢复逐 token 对照路径。
- [x] 本轮批量路径默认关闭；显式 `STRATA_NO_BATCH_KV_STEP=0` 启用，`=1` 为逐 token 对照。
- [x] `DraftPolicy` 学习纯 MTP 已观察前缀的提交收益，按收益/轮次耗时缩短阈值窗口。
- [x] 窗口策略：八轮冷启动、3% 切换余量、每十六轮原策略探测；忽略非法时间和未观察后缀。
- [x] `STRATA_MTP_COST_WINDOW=1` 启用成本窗口；默认关闭，suffix 接受结果不进入 MTP 前缀统计。
- [x] `STRATA_MTP_CHAIN_SUBMIT=1` 连续提交固定长度草稿链，链尾等待；默认关闭。
- [x] 草稿链优化仅适用于概率阈值为 0 且 top2 诊断关闭；0.50 阈值仍走原早停路径。
- [x] `STRATA_QSA_FUSED_KV_STORE=1` 融合 K Norm/RoPE 和 K/V Q8 写入；默认关闭。
- [x] 融合仅适用于 256 维头、64 维 RoPE、无 Hadamard 的 Q8 KV；其余路径回退。

## 验证

- [x] CPU 窗口策略：冷启动、部分/全部拒绝、全部接受、实测成本覆盖先验、探测和 suffix 隔离。
- [x] KV 编码、scale、未用存储和哨兵逐字节一致；覆盖 1/4/6 token、非零位置和步长、跨页、缺页、镜像和环形映射。
- [x] 图重放更换设备位置；融合覆盖 None/Linear/YaRN RoPE。
- [x] 原 indexer、RoPE、KV 与会话状态回归。
- [x] Short/Q8：基线、批量、成本窗口、融合和组合的 greedy 输出一致；取消与恢复通过，基线/组合的 EOS、parked/live 会话恢复通过。
- [x] FP32 修复后 Short/Q8 五模式完整回归：greedy、固定种子采样、惩罚、coupled draft、取消与恢复；基线/组合另覆盖 EOS 和 parked/live 会话恢复。
- [x] `spec_min_p=0` 的基线、连续提交、组合回归通过；与 `0.50` 下五模式交叉比较，8 种配置的48-token greedy/固定种子采样输出均一致。
- [x] Q8 四场景 greedy：基线、批量、成本窗口、融合和组合共20组请求，均生成48 token；四种候选在各场景与基线逐 token 相同。
- [x] 基线/组合四场景固定种子采样与惩罚、前缀快照重放：每场景两次48 token，跨模式与重复输出均相同。
- [x] 整模型基本功能：以上 greedy/采样覆盖，加上 Short 的取消、EOS、parked/live 会话恢复；长输入单项采样和阈值0扩展尚未覆盖。
- [x] Q8/Short 单项及组合计时；0.50下成本窗口/组合、0下组合完成独立复测，阶段计时另存。
- [ ] Short、Code 2K、Prose 2K、Long 8K：decode、prefill、TTFT、提议与接受数。

早期算子筛选用独立进程 A/B/A/B/A；预热后分批计时，提升至少 3%、相邻基线漂移不超过 3% 才重复确认。后续源码采用用户确认的正收益标准，仍排除基线漂移超过3%的配对。
GPU 测试串行，计时期间不编译，不运行其他 GPU 工作。比较固定实际缓存容量，分析器关闭。
整体稳定下降或单场景回退超过 2% 的路径保持关闭；整模型稳定提升至少 1% 才声明整模型提速。

## 复测与结果

构建与原始记录目录：`build-sycl-deepinfer-20261023/`。结果核实后在此补充命令和链接；未完成检查不计为通过。

已完成的[独立回归原始记录](../bench/results/a770/deepinfer-2026-10-23/checks-final/results.json)：
窗口策略 58 项；批量 KV/融合在解析角度和查表两个进程各 171 项；indexer 每种模式 942 项和动态图重放；会话快照 1959 项。
Short 的[五模式贪心回归](../bench/results/a770/deepinfer-2026-10-23/functional-q8-short-greedy/results.json)固定实际缓存 5771 slots / 7922 MiB，输出上限48；明确跳过采样。
该回归期间允许 CPU 编译并行，耗时不用于性能结论。

### 已确认的算子结果

设备为 Arc A770 16GB，CPU 为 Xeon E5-2696 v4，工具链为 oneAPI DPC++ 2025.3.2；完整驱动和哈希见 [build.json](../bench/results/a770/deepinfer-2026-10-23/build.json)。
512 次预热、9 批 × 2048 次提交，批尾等待，取每批耗时中位数；每个 A/B 分支独立进程。
下表只列在[延长计时筛选](../bench/results/a770/deepinfer-2026-10-23/operators-long-screen/results.json)和[独立确认](../bench/results/a770/deepinfer-2026-10-23/operators-confirm/results.json)中均找到满足漂移门槛的测点。

| 算子 | token 数 | 筛选/确认吞吐提升 | 筛选/确认相邻基线漂移 |
|---|---:|---:|---:|
| FP16 KV 批量写入 | 4 | +313.01% / +332.20% | -0.36% / -1.39% |
| Q8 KV 批量写入 | 4 | +318.17% / +264.64% | +0.15% / -2.21% |
| Q8 KV 批量写入 | 6 | +558.17% / +514.45% | -2.94% / -2.73% |
| Q4 KV 批量写入 | 6 | +535.26% / +541.78% | -0.21% / +2.13% |
| Q8 Norm/RoPE + 缓存写入融合 | 4 | +41.46% / +40.95% | -1.22% / -2.21% |
| indexer 批量 append | 4 | +119.56% / +116.81% | -2.55% / +0.19% |

Q8 融合的基线已使用批量 KV 写入，因此该行是融合的额外收益。不是全模型收益，不能和批量写入的收益相乘。
1 token 没有稳定的批量收益，forward 仍只在 `n > 1` 时采用批量路径。
K8V4 的字节回归通过，但本轮两组数据没有同一测点重复满足基线漂移门槛，不声明其稳定加速。
最早的[短计时筛选](../bench/results/a770/deepinfer-2026-10-23/operators-screen/results.json)保留作原始记录，不用于最终采用结论。

### 整模型状态

原配置为 FP16 KV，Q8 融合在此配置不生效；Q8 需要独立 `--kv int8` 测试，不能把格式变化计作融合收益。
早期批量 prefill 诊断请求专家缓存4000，实际 **4191 slots / 5760 MiB**。
后续窗口读取功能回归和 Short 计时均固定请求5500，实际 **5771 slots / 7922 MiB**；各进程 INFO 核对一致。
输入为 Short 23、Code 2048、Prose 2048、Long 8000 token；功能输出上限48，Short 性能输出上限128。
性能脚本预热2次、保留3次原始请求，以 decode 每 token 耗时的中位请求作摘要；窗口读取模式每个进程先执行 Short/16 token 初始化预热。

冻结基线在首个 Code2K 批量 prefill 期间无进一步协议输出，已终止本实验子进程并保留[失败日志](../bench/results/a770/deepinfer-2026-10-23/functional-q8/failure.json)。
这次还未进入候选执行，不能作为优化的回退或正确性结果。目前没有满足四场景采用门槛的性能结论。

较小的256-token分块也没有完成原批量 prefill。`STRATA_PREFILL_SYNC=1` 的诊断能推进多个阶段，但该运行没有完成，且改变了同步行为，不计入性能结果。
使用现有 `--short-read 8192` 的验证窗口 prompt 路径，五模式四场景 greedy 已全部完成；原批量 prefill 的四场景性能验收仍未完成。

2026-10-24 继续排查的[限时诊断记录](../bench/results/a770/deepinfer-2026-10-23/prefill-investigation-20261024/summary.json)：固定 Q8、5771 slots / 7922 MiB、`--prefill 256 --short-read 0`，先强制 Short 的22个 prompt 位置进入批量路径。
原设置、`SYCL_UR_USE_LEVEL_ZERO_V2=0` 和 `UR_L0_V2_FORCE_DISABLE_COPY_OFFLOAD=1` 三组均未在100秒进程限时内完成第一条请求；这不是性能样本，也不能证明发生死锁。
新增四行 `STRATA_TRACE` 日志，记录层号、累计毫秒和阶段提交；不开启额外 GPU 等待，缺省关闭。
该[独立诊断构建](../bench/results/a770/deepinfer-2026-10-23/prefill-trace/build.json)在180秒进程限时前推进到第27层（从0计数），相邻层启动间隔中位数5028毫秒；此前超时至少包含执行缓慢的因素。
该运行仍未完成，不计正确性通过或整模型提速。

[独立 GEMM 探针](../bench/results/a770/deepinfer-2026-10-23/mkl-scalar-probe-final/results.json)比较了0和24,576个1 MiB pinned 分配：栈标量指针查询约0.064/0.095微秒，而 FP16 输入、FP32 输出的 GEMM 调用约25–30毫秒。
直接使用类型明确的 oneMKL 接口没有稳定优势，因此未修改 DPCT 标量调用。
去掉 `SYCL_PROGRAM_COMPILE_OPTIONS` 的[对照](../bench/results/a770/deepinfer-2026-10-23/mkl-runtime-no-program-options/results.json)仍有相同量级开销。
oneMKL verbose 与 UR 调用日志显示：该设备上的 `gemm[half,half,float]` 内部调用 `gemm[float]`，且反复申请临时内存、将设备输入复制到主机。
这些调用日志用于诊断，不作为性能验收。

首版默认关闭的 `STRATA_PREFILL_GPU_WIDEN=1` 原型复用原有32 MiB workspace，在 GPU 将 FP16/BF16 转换为 FP32，再使用相同 `compute_mode::unset` 的 FP32 GEMM。
首版没有增加设备分配；容量不足及非 in-order 队列沿用原路径。此开关仅改变批量 prefill GEMM，和前述四项实验开关独立。下表记录失败原型，不作为当前实现的采用依据。
[独立构建与源码哈希](../bench/results/a770/deepinfer-2026-10-23/prefill-widen/build.json)使用冻结的其余库与修复后的 sampler 对象；新实验两侧都链接本机 oneMKL 2025.3 BLAS DLL import，不与旧静态库引擎混做 A/B。
新增 `prefill_gemm_parity`，五个进程各96组检查通过：覆盖 FP16/BF16、1/4/6/22 token、非默认步长、beta=0/1/-0.25、NaN旧输出、输出尾部保护和超过 workspace 容量的回退。
有效输出逐字节一致。原 oneMKL mixed-input 的 beta=0 会改写未使用的行 padding；新路径保留 padding，此差异单独记录，不混入有效矩阵比较。

下面是 A770、oneAPI 2025.3.2、T=4/N=1280/K=2560 的[五进程筛选](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-operators/results.json)与[独立复测](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-operators-confirm/results.json)。
每进程每类型预热4次，再计5批×16次，批尾等待；没有额外专家 pinned 分配，分析器关闭。

| 输入类型 | 筛选 / 复测吞吐提升 | 对应基线漂移 |
|---|---:|---:|
| FP16 | +285.14% / +283.74% | -0.34% / +1.12% |
| BF16 | +244.33% / +236.43% | -2.99% / -2.41% |

BF16筛选中另一个对照的基线漂移为+6.51%，该对照不计入稳定收益；全部样本和逐批耗时保留在结果文件中。
以上仅是首版原型所测 GEMM 的算子收益，不能外推到完整 prefill 或模型。强制批量 prefill 的 Short 对照已完成：关闭开关生成正常 token，首版原型两次均生成48个 token 0；这是明确的模型正确性回归，不能作为采用或模型加速依据。
`STRATA_DBG_NAN=1` 进一步确认首层 MoE 输入及最终 logits 非有限。补充 HC 形状、连续调用后的219组算子检查仍通过；[增加等待](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-sync-model/results.json)和[仅保留 FP16](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-f16-model/results.json)的版本仍输出全0，均未通过模型正确性。

继续用[模型实际输入逐次对照](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-actual-compare-model/results.json)，首次优化的共享专家投影 `T=22 N=640 K=2560` 的14,080个有效输出全部不同。原 FP16 输入、权重和旧 GEMM 输出均有限，新输出含非有限值。
紧密输出、160 MiB共享分配的219组检查仍通过；因此小型独立分配未覆盖模型的实际缓存布局。
[大偏移最小复现](../bench/results/a770/deepinfer-2026-10-23/mkl-large-offset-probe/results.json)包含16组：0/4 GiB偏移、640/1280输出维度、4/22 token、独立/同分配块输出。
在这台 A770、oneMKL 2025.3 上，只有4 GiB偏移且输入/输出位于同一分配块的4组失败，全部有效输出不同；GPU 转换后的输入逐元素一致，独立输出的12组通过。此结果定位到所测组合的大偏移 GEMM 行为，未推断其他设备或版本。

上一轮修复版仅优化 FP16；BF16 完整恢复旧实现。每次符合容量条件的 GEMM 使用独立设备输出暂存，保留原输出内容（含 padding），GEMM 完成后复制回缓存切片。
权重/输入转换仍复用原有32 MiB workspace；新增一次 `T × ldy × sizeof(float)` 的临时设备分配和往返输出复制，计时包含这些成本。分配失败沿用原路径，开关仍默认关闭。
[修复版独立构建](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-output-stage/build.json)分别在关闭/开启开关时通过255组检查；加入了实际出错的640维投影。
[超过4 GiB的共享分配回归](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-output-stage-arena/results.json)同样在关闭/开启时各255组通过，覆盖1/4/6/22 token、beta=0/1/-0.25、NaN旧输出、步长、连续输入变化和 workspace 容量回退。
[修复版 Short/Q8 模型检查](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-output-stage-model/results.json)通过：强制22个 prompt 位置走批量路径，48-token greedy 与关闭开关的正常参考逐 token 相同，接受/提议数均为31/39，实际缓存保持5771 slots / 7922 MiB。
这是冷请求诊断，未作整模型性能采用依据。两次单独记录的 prompt 耗时为原路径254588.6 ms、修复版74681.2 ms；没有预热、交错基线与四场景复测，不据此声明整模型提速。

修复版 FP16 GEMM 完成[筛选](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-output-stage-operators/results.json)和[独立确认](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-output-stage-operators-confirm/results.json)，两轮各5个独立进程 A/B/A/B/A。
T=4/N=1280/K=2560，每进程预热4次、计5批×16次；同一 EXE、oneMKL DLL、其余依赖和环境，分析器关闭，GPU 串行，计时期间不编译。

| 修复版 FP16 GEMM | 筛选 | 独立确认 |
|---|---:|---:|
| 相邻基线几何均值 | 26.224 ms | 25.604 ms |
| 候选耗时 | 7.187 ms | 7.160 ms |
| 吞吐提升 | +264.88%（3.65倍） | +257.62%（3.58倍） |
| 相邻基线漂移 | +0.970% | -0.011% |

筛选中第二组对照漂移为-3.266%，超过3%门槛，已排除；确认中另一组对照也通过。原始记录保留全部比较，表中使用各轮第一组有效对照。
上述测量中 BF16 没有修改，计时仅作环境对照，不声明其收益。上一轮保留正确性通过且算子稳定加速的 FP16 路径；`STRATA_PREFILL_GPU_WIDEN` 继续默认关闭，四场景整模型性能验收仍未完成。
[修复版固定种子采样及缓存重放](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-output-stage-sampling/results.json)也通过：Short/Q8、同一 FP32 采样器、`temperature=0.7 top_p=0.9 top_k=20 seed=1234 penalty_repeat=1.1 penalty_freq=0.1 penalty_present=0.2 penalty_last_n=128`，仓库可复现配置 `STRATA_IQ_MT_MIN=1`。
关闭/开启开关各生成两次48 token，四份输出逐 token 相同，接受/提议数均为28/42；第二次均复用16个 prompt token。启用 `--prompt-cache 128`，强制批量读取其余位置；缓存仍为5771 slots / 7922 MiB。该组仅作功能检查，未比较性能。
最终源码、构建、96个未改依赖对象/库和生产文件的[哈希核对记录](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-final-check/results.json)与原始失败样本分别保留。

```powershell
python build-sycl-deepinfer-20261023/prefill_probe.py --variant default --short-primer --seconds 100 --out build-sycl-deepinfer-20261023/prefill-health-repeat
python build-sycl-deepinfer-20261023/build_prefill_trace.py --seconds 180 --out build-sycl-deepinfer-20261023/prefill-trace-repeat
python build-sycl-deepinfer-20261023/mkl_scalar_probe.py --out build-sycl-deepinfer-20261023/mkl-scalar-probe-repeat
python build-sycl-deepinfer-20261023/mkl_runtime_probe.py --variant no-program-options --out build-sycl-deepinfer-20261023/mkl-runtime-repeat
python build-sycl-deepinfer-20261023/mkl_scalar_probe.py --source build-sycl-deepinfer-20261023/mkl_widen_probe.cpp --out build-sycl-deepinfer-20261023/mkl-widen-repeat
python build-sycl-deepinfer-20261023/build_prefill_widen.py --source-build build-sycl-deepinfer-20261023/prefill-widen-output-stage --checks 255 --out build-sycl-deepinfer-20261023/prefill-widen-repeat
python build-sycl-deepinfer-20261023/test_gemm_arena.py --build-root build-sycl-deepinfer-20261023/prefill-widen-output-stage --checks 255 --offset-gib 4 --out build-sycl-deepinfer-20261023/prefill-widen-arena-repeat
python build-sycl-deepinfer-20261023/test_prefill_widen.py --build-root build-sycl-deepinfer-20261023/prefill-widen-output-stage --phase operators --out build-sycl-deepinfer-20261023/prefill-widen-operators-repeat
python build-sycl-deepinfer-20261023/test_prefill_widen.py --build-root build-sycl-deepinfer-20261023/prefill-widen-output-stage --phase model --switches 1 --reference build-sycl-deepinfer-20261023/prefill-widen-model-short/results.json --out build-sycl-deepinfer-20261023/prefill-widen-model-repeat
python build-sycl-deepinfer-20261023/test_prefill_widen_sampling.py --out build-sycl-deepinfer-20261023/prefill-widen-sampling-repeat
```

以上 `--source-build` 使用冻结的修复版源码；构建脚本同时重编 `gemm.dp.cpp` 和 `prefill.cpp`，保持新增 workspace 字段的类布局一致。常规 SYCL 测试构建可运行 `prefill_gemm_parity`；大偏移回归另显式设置 `STRATA_GEMM_SHARED_ARENA=4`，需要4 GiB以上空闲显存并串行运行。

### BF16 GPU 转换扩展（2026-10-24）

- [x] 复用现有转换函数、32 MiB workspace 和独立输出缓冲，BF16 用位移精确扩展为 FP32，调用相同的 FP32 GEMM。
- [x] `STRATA_PREFILL_BF16_GPU_WIDEN=1` 独立启用，默认关闭；仅处理 `ldy=N` 的连续输出。交错 GDN 输出、容量不足和非 in-order 队列保留原路径。
- [x] 单 token、单输出点积保留原调用，覆盖 FP16 和 BF16；不改变点积的归约顺序。
- [x] [独立构建](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-bf16-final/build.json)关闭/开启各485组检查，全部逐字节相同；覆盖输入/输出步长、GDN padding、beta、连续调用、尾部保护及容量回退。
- [x] [超过4 GiB偏移的共享分配](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-bf16-arena/results.json)，关闭/开启各485组检查，全部逐字节相同。
- [x] [Short/Q8 greedy](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-bf16-model/results.json)关闭/开启各48 token，与旧参考相同；接受/提议数均为31/39。
- [x] [固定种子采样与缓存重放](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-bf16-sampling/results.json)四份48-token输出相同，接受/提议数均为28/42，第二次均复用16个 prompt token；沿用上文采样、惩罚和可复现配置。
- [x] 两轮独立进程 A/B/A/B/A 算子测量通过，保留完整逐批结果和无效对照。
- [x] 测试工具重试 Windows 锁文件读取时的瞬态 `PermissionError`；[最小检查](../bench/results/a770/deepinfer-2026-10-23/check_lock_retry.py)通过，保持独占锁且不删除其他进程的锁。此修复发生在算子计时后，只影响排队。
- [x] [最终源码、构建、96个原依赖及生产文件哈希核对](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-bf16-final-check/results.json)通过，结果及失败原型分别归档。

新增单输出检查时，[首版扩展](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-bf16/results.json)发现3组差异，均为 `T=N=1, beta=0`，最大绝对差为 `5.72e-6`，其中也包括上一轮 FP16 路径。
oneMKL 的标量点积与显式 FP32 GEMM 采用不同的归约顺序；当前共享函数对此形状直接回退。带输出 padding 的 BF16 同样回退，逐字节检查包含原 oneMKL 对行间空位的写入。

本轮两侧共同启用已验证的 `STRATA_PREFILL_GPU_WIDEN=1`，只切换 BF16 开关；其余四项 DeepInfer 实验均关闭。
模型检查固定 Q8、5771 slots / 7922 MiB，强制批量 prefill；greedy 开启 trace，只作功能检查。
设备仍为 Arc A770、Xeon E5-2696 v4、oneAPI 2025.3.2 / oneMKL 2025.3；算子为 `T=4/N=1280/K=2560, ldx=2573, ldy=1280`，每进程预热4次、计5批×16次，包含临时输出分配和复制，分析器关闭。

| BF16 GEMM | [筛选](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-bf16-operators/results.json) | [独立确认](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-bf16-operators-confirm/results.json) |
|---|---:|---:|
| 相邻基线几何均值 | 22.996 ms | 22.838 ms |
| 候选耗时 | 7.294 ms | 7.353 ms |
| 吞吐提升 | +215.28%（3.15倍） | +210.59%（3.11倍） |
| 相邻基线漂移 | -0.772% | -1.089% |

确认第一组对照漂移为-6.519%，已排除；表中确认使用第二组有效对照，筛选两组对照均通过。FP16 在两侧均启用，仅作共同依赖和环境对照。
这些是所测 BF16 GEMM 的收益；四场景整模型性能尚未验收，开关继续默认关闭，生产 EXE 和配置不替换。

```powershell
python build-sycl-deepinfer-20261023/build_prefill_widen.py --source-build build-sycl-deepinfer-20261023/prefill-widen-bf16-final --reuse build-sycl-deepinfer-20261023/prefill-widen-bf16-final --out build-sycl-deepinfer-20261023/prefill-bf16-repeat
python build-sycl-deepinfer-20261023/test_gemm_arena.py --build-root build-sycl-deepinfer-20261023/prefill-widen-bf16-final --offset-gib 4 --out build-sycl-deepinfer-20261023/prefill-bf16-arena-repeat
python build-sycl-deepinfer-20261023/test_prefill_widen.py --build-root build-sycl-deepinfer-20261023/prefill-widen-bf16-final --dtype bf16 --phase operators --out build-sycl-deepinfer-20261023/prefill-bf16-operators-repeat
python build-sycl-deepinfer-20261023/test_prefill_widen.py --build-root build-sycl-deepinfer-20261023/prefill-widen-bf16-final --dtype bf16 --phase model --switches 0,1 --reference build-sycl-deepinfer-20261023/prefill-widen-model-short/results.json --out build-sycl-deepinfer-20261023/prefill-bf16-model-repeat
python build-sycl-deepinfer-20261023/test_prefill_widen_sampling.py --build-root build-sycl-deepinfer-20261023/prefill-widen-bf16-final --bf16 --out build-sycl-deepinfer-20261023/prefill-bf16-sampling-repeat
python build-sycl-deepinfer-20261023/check_lock_retry.py
```

### 批量大小与跨块 prefill 补测（2026-10-24）

- [x] GEMM 检查增加 `T=64/256`，连同原来的 `1/4/6/22`，每侧共725组；覆盖 FP16/BF16、BF16输入/输出步长、beta、容量回退、尾部保护和连续调用。
- [x] [普通分配](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-bf16-batches/results.json)、[共享 arena](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-bf16-batches-arena0/results.json)、[超过4 GiB偏移的共享 arena](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-bf16-batches-arena4/results.json)各运行关闭/开启两侧，共4350组比较，零字节差异。
- [x] [Code 256-token、Prose 512-token 的真实批量 prefill 对比](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-bf16-batch-chat-fixed-cache/results.json)：保留原输入末尾64 token 的完整指令和 assistant 模板，两侧均生成满8 token，逐 token 输出及接受/提议数相同。
- [x] Prose 512-token 的[窗口读取独立参考](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-bf16-batch-chat-fixed-cache/window-reference/results.json)与批量路径的8个输出 token 完全一致。
- [x] [源码、独立构建、96个依赖及生产文件核对](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-bf16-batches-final-check/results.json)；原型、容量检查拒绝记录和有效结果分别保存。
- [ ] 完整 Code2K、Prose2K、Long8K 的批量 prefill 及四场景性能验收；本轮有界输入不替代这些检查。

较大输入需要扩大测试 arena：现在使用256 MiB，workspace 位于64 MiB，X/W/参考输出/候选输出分别位于112/128/176/208 MiB；测试先检查各切片容量，避免扩展批量后输入或输出重叠。
[独立构建](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-bf16-batches/build.json)只重编检查程序并重新链接，GEMM/prefill 对象和模型 EXE 与上一轮相同；没有新增运行时优化或性能测量。

[只截前缀的原型记录](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-bf16-batch-inputs/results.json)保留为输入构造诊断。直接截断会丢掉原 prompt 的尾部指令和 assistant 模板，Code 两侧均生成3 token 后 EOS，Prose 两侧均只输出 EOS，不能据此认定长输入生成检查充分通过。复测保留原输入末尾64 token，并要求两侧均生成满8 token。

保留模板后的首次尝试被[实际容量检查拦截](../bench/results/a770/deepinfer-2026-10-23/bf16-batch-chat-cache-rejection/results.json)：基线为5771 slots / 7922 MiB，候选启动时为5587 slots / 7669 MiB，窗口参考为5591 slots / 7675 MiB，均未形成有效对照。原生专家缓存按 `min(请求预算, 可用显存 - reserve)` 限制容量；复测改用请求4000，强制各进程的 INFO 均为4191 slots / 5760 MiB，保持原1536 MiB reserve。该组单独记录容量，不与此前5771-slot结果合并。

| 有效输入 | 实际批量位置 / rows | 两侧生成 token | 两侧接受 / 提议 |
|---|---|---:|---:|
| Code 256 token | 0 / 255 | 8 | 5 / 5 |
| Prose 512 token | 0 / 256，256 / 255 | 8 | 3 / 3 |

两组均为 greedy、Q8、无 prompt 复用，沿用 FP16 widening 开启、只切换 BF16 的对照；四项 DeepInfer 实验关闭。此处位置/rows来自实际 prefill trace，最后一个输入 token 沿用单步路径。

复测本节冻结的扩展检查时显式传入 `--checks 725`；重放上轮冻结源码仍使用485组。

```powershell
python build-sycl-deepinfer-20261023/build_prefill_widen.py --source-build build-sycl-deepinfer-20261023/prefill-widen-bf16-batches --reuse build-sycl-deepinfer-20261023/prefill-widen-bf16-batches --checks 725 --out build-sycl-deepinfer-20261023/prefill-bf16-batches-repeat
python build-sycl-deepinfer-20261023/test_gemm_arena.py --build-root build-sycl-deepinfer-20261023/prefill-bf16-batches-repeat --offset-gib 0 --checks 725 --out build-sycl-deepinfer-20261023/prefill-bf16-arena0-repeat
python build-sycl-deepinfer-20261023/test_gemm_arena.py --build-root build-sycl-deepinfer-20261023/prefill-bf16-batches-repeat --offset-gib 4 --checks 725 --out build-sycl-deepinfer-20261023/prefill-bf16-arena4-repeat
python build-sycl-deepinfer-20261023/check_bf16_arena.py build-sycl-deepinfer-20261023/check_bf16_batch_inputs.py --build-root build-sycl-deepinfer-20261023/prefill-bf16-batches-repeat --keep-tail-tokens 64 --expert-cache 4000 --expected-cache 4191,5760 --out build-sycl-deepinfer-20261023/prefill-bf16-inputs-repeat
```

### FP16 输出步长与偏移回归（2026-10-24）

- [x] 扩展 `prefill_gemm_parity`：FP16 输出步长、起始偏移0/7、首尾保护区、全部填充字节和连续调用；每侧1,015组，含1,008组矩阵比较与7组连续调用。
- [x] [修复前对照](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-f16-stride-before-fix/results.json)检出82组填充区差异，全部为 FP16、`beta=0`；有效矩阵值均相同，偏移0和7各41组。
- [x] `gpu_widen_gemm` 对 FP16/BF16 统一限定 `ldy=N`；有步长的输出保留原 oneMKL 调用。
- [x] 修复后[普通分配](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-f16-stride/build/results.json)、[共享 arena](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-f16-stride/arena0/results.json)、[超过4 GiB偏移的 arena](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-f16-stride/arena4/results.json)各完成关闭/开启对照，共6,090组，零差异。
- [x] [Short 同一 EXE 的 FP16 开关对照](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-f16-stride/model-checks/Short/results.json)：两侧均生成8 token，逐 token 相同，接受/提议均为5/6。
- [x] [Code 256-token 的 FP16 输出参考对比](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-f16-stride/model-checks/Code2K-256/results.json)：生成8 token，与冻结参考逐 token 相同，接受/提议为5/5。
- [x] [Prose 512-token 的 FP16/BF16 组合参考对比](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-f16-stride/model-checks/Prose2K-512/results.json)：完成位置0/256的两个 prefill 块，生成8 token，与冻结参考相同，接受/提议为3/3。
- [x] [Short 的 FP16/BF16 组合检查](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-f16-stride/model-checks/combined-Short/results.json)：8个输出 token 与本轮开关对照相同，接受/提议为5/6。
- [x] [源码、独立构建、96个依赖及生产文件复核](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-f16-stride-final-check/results.json)；修复前失败、错误断言探针及主动中止的模型基线分别保留。

旧 oneMKL 混合输入调用在 `beta=0` 时会写入行间填充区，而显式 FP32 GEMM 保留填充区；仅比较有效矩阵值会漏掉这一行为差异。此前 BF16 已保留原调用，本次对 FP16 使用相同限制，覆盖交错输出与 `Gemm::native` 的 `Y+r0` 行切片，避免额外输出暂存复制涉及这类切片。

[最早探针](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-f16-stride-probe/results.json)沿用了“填充区必须保持初值”的断言，在容量回退后报错；这一断言不适用于原 oneMKL 行为。最终检查改为与原调用逐字节相同，并单独统计有效值与填充区差异。

本次是正确性修复；连续输出的转换、独立输出暂存和数学顺序沿用既有实现。没有新增性能测量，两个 widening 开关继续默认关闭。

[独立构建](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-f16-stride/build/build.json)仅重新编译 GEMM 并重新链接，复用同一 prefill 对象、检查对象及其余依赖。模型均为 greedy、Q8、输出上限8、无 prompt 复用；实际缓存固定为4191 slots / 5760 MiB。Code/Prose 的输入 token 与冻结参考一致，均保留最后64 token 的指令和 assistant 模板。四项 DeepInfer 实验关闭；完整2K/8K批量输入和四场景性能验收仍待完成。

FP16 关闭的 Code 256-token 批量预填充基线运行数分钟后主动中止，以减少重复验证成本；其[未完成记录](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-f16-stride/Code2K-256/results.json)单独保留，不计作通过或性能测量。算子三种布局已经完成；模型改用 Short 同一 EXE 的开关对照，以及此前同输入、同工具链、同实际缓存容量的 Code/Prose 冻结输出参考。

现有 CMake 目标可直接复测完整算子检查。重放本次冻结构建时使用 `--checks 1015`；模型完整参数保存在各组结果的 `argv` 中。输出目录必须是新目录，GPU 测试串行执行。

```powershell
cmake --build <sycl-build> --target prefill_gemm_parity
ctest --test-dir <sycl-build> -R '^prefill_gemm_parity$' --output-on-failure
python build-sycl-deepinfer-20261023/build_prefill_widen.py --source-build build-sycl-deepinfer-20261023/prefill-widen-f16-stride/build --reuse build-sycl-deepinfer-20261023/prefill-widen-f16-stride/build --checks 1015 --out build-sycl-deepinfer-20261023/prefill-f16-stride-repeat
python build-sycl-deepinfer-20261023/test_gemm_arena.py --build-root build-sycl-deepinfer-20261023/prefill-f16-stride-repeat --offset-gib 0 --checks 1015 --out build-sycl-deepinfer-20261023/prefill-f16-stride-arena0-repeat
python build-sycl-deepinfer-20261023/test_gemm_arena.py --build-root build-sycl-deepinfer-20261023/prefill-f16-stride-repeat --offset-gib 4 --checks 1015 --out build-sycl-deepinfer-20261023/prefill-f16-stride-arena4-repeat
python build-sycl-deepinfer-20261023/test_prefill_widen.py --build-root build-sycl-deepinfer-20261023/prefill-f16-stride-repeat --phase model --dtype f16 --switches 0,1 --cases Short --max-tokens 8 --expert-cache 4000 --expected-cache 4191,5760 --out build-sycl-deepinfer-20261023/prefill-f16-stride-short-repeat
```

### 省略 beta=0 的旧输出复制：未采用（2026-10-24）

- [x] 冻结当前源码与构建，复核96个依赖和生产文件；运行时源码没有改动。
- [x] 当前工作区的 MTP 策略 [56项 CPU 检查](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-beta-zero/cpu/checks.log)全部通过，包含另一项实验新增的累计概率阈值。
- [x] 独立候选仅在 `beta=0` 时省略旧输出到独立暂存区的复制；保留输出步长回退、独立分配和 FP32 数学顺序。
- [x] [关闭/开启 widening 的正确性检查](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-beta-zero/build/results.json)每侧1,015组，共2,030组，零差异；包含 NaN 初值、非零 beta、padding、偏移和连续调用。
- [x] 五个独立进程完成 A/B/A/B/A 算子筛选；四组配对均未达到3%的采用门槛，候选不合入运行时源码。

设备仍为 A770 16GB、Xeon E5-2696 v4，DPC++ 2025.3.2 / oneMKL 2025.3。A 为上一节冻结构建，B 仅重编译 GEMM、重新链接，复用相同的检查对象、prefill 对象及其余依赖。
两侧均开启 FP16/BF16 widening；这里只测省略复制的增量，不与原始混合输入路径比较。

| 输入 | 第一次配对吞吐变化 | 第二次配对吞吐变化 | 相邻基线漂移 |
|---|---:|---:|---:|
| FP16 | +0.83% | +0.08% | -0.54% / +0.61% |
| BF16 | -0.08% | +0.11% | -0.03% / -0.07% |

形状固定为 `T=4, N=1280, K=2560, ldy=1280`；FP16 的 `ldx=2560`，BF16 的 `ldx=2573`。省略的复制为20 KiB。
每进程预热4次，5批 × 16次调用，取批均耗时的中位数；GPU 独占，未开启分析器。
[原始结果](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-beta-zero/results.json)包含每批数据、参数、环境、源码/构建/EXE 哈希和依赖复核；[归档清单](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-beta-zero/archive.json)只含源码与记录。

筛选未通过，因此没有追加共享 arena、整模型或延长计时确认；这项结果不能说明其他形状的收益，也没有新增整模型提速结论。
既有 widening 开关继续默认关闭；生产 EXE 和配置未替换。复测使用新目录，以下脚本仅增加可指定结果目录的入口，实验步骤与已执行版本相同：

```powershell
python build-sycl-deepinfer-20261023/check_bf16_arena.py build-sycl-deepinfer-20261023/check_beta_zero.py build-sycl-deepinfer-20261023/prefill-beta-zero-repeat
```

### 扩大 GPU widening workspace：保留，默认关闭（2026-10-24）

- [x] 冻结当前源码、构建和96个依赖对象；保留其他会话已有的 half-output 实验，两侧均关闭该实验。
- [x] 仅当 `STRATA_PREFILL_GPU_WIDEN=1` 或 `STRATA_PREFILL_BF16_GPU_WIDEN=1` 时，将 workspace 从32 MiB扩至128 MiB；否则保持32 MiB。
- [x] `bytes_needed`、初始化和 `relayout` 使用相同预算，额外96 MiB计入借用内存；保留独立输出分配、旧输出复制、padding 回退和 FP32 数学顺序。
- [x] 扩展既有 `prefill_gemm_parity --large`，覆盖1/4/6/22/64/256 token、非默认输入步长、三种 beta、容量回退，以及共享 arena 和4 GiB以上偏移。
- [x] 两次独立构建的普通检查各2,030组，加三种大形状检查各270组，合计4,870组，零字节差异。
- [x] 两轮独立进程 A/B/A/B/A 筛选与确认，所测四种大 GEMM 均有满足3%收益和3%漂移门槛的重复结果。
- [x] Short 23、Code 256、Prose 512 token 输入各生成8 token，greedy 输出和接受/提议数与冻结参考相同；固定种子采样与惩罚、缓存重放各48 token相同。
- [ ] 完整 Short / Code 2K / Prose 2K / Long 8K 的整模型性能与组合验收。widening 继续默认关闭，生产 EXE 和配置未替换。

设备为 A770 16GB、Xeon E5-2696 v4，DPC++ 2025.3.2 / oneMKL 2025.3。原32 MiB容量不足以容纳这些权重的 FP32 转换，原路径回退到主机转换；128 MiB覆盖更多大投影，超过容量仍沿用原回退。
A/B 两侧均开启 FP16/BF16 widening，仅比较32/128 MiB；同一检查对象、其余依赖、FP32 sampler 和工具链，关闭分析器并持有 GPU 锁。每进程预热4次，5批 × 16次，取批均耗时中位数，与相邻两个基线的几何均值比较。

| 类型 | N | 首轮首个有效组吞吐提升 / 基线漂移 | 独立确认有效组吞吐提升 / 基线漂移 |
|---|---:|---:|---:|
| FP16 | 4096 | +668.58% / −0.79% | +715.12% / −0.67% |
| FP16 | 12288 | +910.34% / −0.15% | +917.09% / −1.77%；+927.78% / +1.44% |
| BF16 | 4096 | +506.53% / −0.02% | +506.67% / −0.61%；+505.44% / −1.06% |
| BF16 | 12288 | +645.11% / −0.13% | +632.66% / +0.53%；+633.20% / −2.21% |

形状为 `T=4, K=2560, ldy=N`；FP16 的 `ldx=2560`，BF16 在 N=4096 时为2560、N=12288时为2573。
首轮 FP16 N=4096 从66.788 ms降至8.690 ms，N=12288从179.798 ms降至17.796 ms。首轮第二组 FP16 N=12288 的基线漂移+3.988%，确认第一组 FP16 N=4096 漂移+8.160%，两组排除。
这里约6–10倍是所测大 GEMM 的算子吞吐，未测出整模型提速结论。

模型检查固定实际专家缓存：greedy 为4191 slots / 5760 MiB（请求4000），采样为5771 slots / 7922 MiB（请求5500）。greedy 接受/提议依次5/6、5/5、3/3，均无前缀复用；Prose 512跨两个 prefill 块。
采样参数为 `temperature=0.7 top_p=0.9 top_k=20 seed=1234 penalty_repeat=1.1 penalty_freq=0.1 penalty_present=0.2 penalty_last_n=128`，使用既有可复现配置；两次均接受/提议28/42，前缀复用0/16。
这些是有界功能检查，未代替完整长输入性能验收。

[汇总与源码/构建哈希](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-workspace/summary.json)、[首轮原始结果](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-workspace/screen/results.json)、[独立确认和模型记录](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-workspace/confirm/results.json)、[最终构建](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-workspace/confirm/build/build.json)、[最终核验](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-workspace/final-check.json)。
[首轮归档](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-workspace/screen/archive.json)和[确认归档](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-workspace/confirm/archive.json)保留执行脚本、冻结源码、补丁与日志。
准备时检测到并行源码修改而停止的记录另见[预检查](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-workspace/preflight-failure.json)；重新冻结后才进行上述测量。

复测使用新输出目录，GPU 串行运行；完整确认脚本从已冻结的输入重建，不修改生产文件。归档中的 `replay_confirm_widen_workspace.py` 与私有目录入口相同，仅将源码保护改为比较复测入口/出口哈希，测量步骤未变；该入口修改只做了语法检查。

```powershell
python build-sycl-deepinfer-20261023/build_prefill_widen.py --source-build build-sycl-deepinfer-20261023/prefill-widen-workspace-confirm/build --reuse build-sycl-deepinfer-20261023/prefill-widen-workspace-confirm/build --checks 1015 --out build-sycl-deepinfer-20261023/workspace-parity-repeat
python build-sycl-deepinfer-20261023/check_bf16_arena.py build-sycl-deepinfer-20261023/confirm_widen_workspace.py build-sycl-deepinfer-20261023/workspace-confirm-repeat
```

### 合并 widening 的主机等待：用户确认后合入，默认关闭（2026-10-24）

- [x] 冻结上一节128 MiB版源码和构建，保护96个依赖对象、生产 EXE 与配置。
- [x] 独立候选删除转换后的队列等待和 GEMM 后的事件等待，保留 in-order 限制及最终拷回等待，数学顺序、缓冲分配和回退不变。
- [x] 普通检查关闭/开启各1,015组，大形状普通分配、共享 arena、超过4 GiB偏移各270组，合计2,840组，零字节差异。
- [x] 小/大形状各运行独立进程 A/B/A/B/A；12组配对的基线漂移均在3%以内，收益均未达到3%，最初决定不合入。
- [x] 用户随后明确同意合入：运行时源码与已验证候选逐字节一致，删除前两处等待，保留链尾等待和原有开关；widening 继续默认关闭。此次仅对源码保留门槛作例外，不声明稳定提速。

A770 16GB、Xeon E5-2696 v4，DPC++ 2025.3.2 / oneMKL 2025.3。两侧均开启 FP16/BF16 widening，使用128 MiB workspace，关闭 half-output 实验和分析器；同一检查对象、prefill 对象、FP32 sampler 和其余依赖，GPU 串行运行。
每进程预热4次，5批 × 16次，取批均耗时中位数；每个候选与相邻基线的几何均值比较。

| 类型 | N | 第一组吞吐变化 | 第二组吞吐变化 | 相邻基线漂移 |
|---|---:|---:|---:|---:|
| FP16 | 1280 | +1.42% | +1.34% | −0.88% / +0.26% |
| BF16 | 1280 | +0.76% | +2.71% | −0.46% / +0.03% |
| FP16 | 4096 | −0.45% | +1.99% | +0.42% / −1.12% |
| BF16 | 4096 | +1.83% | +1.85% | +0.27% / +0.15% |
| FP16 | 12288 | +0.42% | +2.43% | +0.73% / +0.31% |
| BF16 | 12288 | −0.05% | +0.31% | +1.45% / −1.36% |

形状为 `T=4, K=2560, ldy=N`；FP16 的 `ldx=2560`，BF16 在 N=4096时为2560，其余为2573。
筛选未通过，因此当时未追加独立性能确认或模型测试；这些结果不构成整模型提速结论。用户随后明确同意保留源码，生产 EXE 与配置仍未替换，整模型验证仍待完成。
[原始结果](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-queue/results.json)、[候选构建](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-queue/build/build.json)、[筛选阶段未采用结论](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-queue/decision.json)、[归档哈希](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-queue/archive.json)及[筛选阶段核验](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-queue/final-check.json)保留原始记录，另见[用户确认后的合入记录](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-queue/integration.json)。归档不包含二进制产物。
归档核对发现共享 arena 两份正确性日志被后续同名计时日志覆盖；[日志补测记录](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-queue/logging-repair.json)保留原始结构化结果的哈希，用同一候选补跑这540组，补全原始日志，不重复计入上述2,840组。复测入口仅修正日志前缀，执行版和修正版脚本均保留。
复测入口从上一节冻结构建重建基线和候选，并比较复测开始/结束时的工作区哈希，支持合入后的复测；入口保护调整仅做语法检查，测量步骤不变。执行版和调整版脚本分别保留。使用新输出目录：

```powershell
python build-sycl-deepinfer-20261023/check_bf16_arena.py build-sycl-deepinfer-20261023/check_widen_queue.py build-sycl-deepinfer-20261023/prefill-widen-queue-repeat
```

2026-10-24 的[性能数据复核](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-queue/performance-audit.json)重算了等待合并及 workspace 筛选/确认的20个进程、70个中位数、350批计时、28组配对；97份归档哈希一致，未发现计算错误，workspace 的两组超限漂移仍排除。该次复核时的源码与等待合并候选一致。
等待合并各形状两次配对的几何平均吞吐变化为：FP16 N=1280/4096/12288分别+1.38%/+0.76%/+1.42%，BF16分别+1.73%/+1.84%/+0.13%。这仅是描述统计；每个形状只有两个候选进程，两组配对还共享中间基线，不能作为独立确认或置信区间。最高+2.71%只属于单次配对，不代表整体提升。
`total_us` 包括转换、输出分配/复制、内部等待和释放，测的是完整 GEMM 调用；`submit_us` 也包含内部等待，不能解释为纯 CPU 提交开销。workspace 扩容与等待合并使用不同对照，收益不能相加或连乘；当前仍没有等待合并的整模型 prefill、decode、TTFT 对照数据。
以下[复核脚本](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-queue/check_performance_data.py)只读取记录和重算统计，不运行 GPU；私有 EXE 不在公开归档中，缺失时其哈希检查记录为 `null`：

```powershell
python bench/results/a770/deepinfer-2026-10-23/prefill-widen-queue/check_performance_data.py
```

### 权重 widening 向量化：保留，沿用默认关闭的开关（2026-10-24）

- [x] 只将连续的权重转换改为 `sycl::vec` 每次读写4个元素，尾部仍逐元素处理；激活转换、FP32 GEMM、输出暂存和回退不变，没有新增开关或依赖。
- [x] 三轮独立 A/B/A/B/A，共30个进程、450批计时、36组配对；全部原始正值、负值及超限数据保留。
- [x] 独立候选2,840组、合入组合2,300组布局/数值检查全部逐字节一致；另有计时进程的270组比较通过。
- [x] 当前源码包含并行合入的默认关闭 XMX 路径，已独立重编译后检查；本轮将 XMX/half-output 设为0，验证原 FP32 widening 路径。
- [x] Short/Q8：基线与组合候选各执行8-token greedy、两次48-token固定种子惩罚采样；输出及跨版本接受/提议数相同，采样请求均从16-token快照恢复，重放一致。实际缓存均为4191 slots / 5760 MiB。
- [x] 仅将已测试片段写入工作区，最终源码与组合构建逐字节一致；两个 widening 开关继续默认关闭，生产 EXE 和配置未替换。

设备为 A770 16GB、Xeon E5-2696 v4、PCIe 3.0 ×16、Windows，驱动32.0.101.9034；DPC++ 2025.3.2 / oneMKL 2025.3。
两侧均开启已有 FP16/BF16 GPU widening、使用128 MiB workspace及相同冻结依赖，原生内存池为 `1;512M;host:128M,8,64K;device:128M,8,64K;shared:128M,8,64K`，分析器关闭。
形状为 `T=4, K=2560, ldy=N`；FP16的 `ldx=2560`，BF16在N4096时为2560，其余为2573。每进程预热4次，测5批×16次，取批均耗时中位数；计时包括分配、转换、GEMM、复制、等待和释放。

下表为各轮有效配对吞吐变化的几何均值；汇总合并所有有效配对，仅作描述统计，各轮两组配对共享中间基线，不作为置信区间。

| 输入 / N | 首轮 | 独立复测 | 追加一轮 | 有效配对汇总 |
|---|---:|---:|---:|---:|
| FP16 / 1280 | +0.555% | +1.127% | -0.231% | +0.482%（6组） |
| FP16 / 4096 | +0.899% | -0.996% | +1.106% | +0.332%（6组） |
| FP16 / 12288 | +0.939% | +0.251% | +0.995% | +0.824%（5组） |
| BF16 / 1280 | +0.034% | +0.063% | +1.009% | +0.428%（5组） |
| BF16 / 4096 | +0.840% | +0.900% | +1.457% | +1.065%（6组） |
| BF16 / 12288 | +0.786% | +0.321% | +0.969% | +0.692%（6组） |

独立复测的首个BF16/N1280、FP16/N12288配对基线漂移分别为-4.529%、-3.630%，排除后剩34组，其中30组为正。
有效负值包含FP16/N4096的-2.338%，以及追加轮FP16/N1280的-0.198%/-0.264%；追加一轮的决定及全部负值保留在原始记录中。
这是小幅且有噪声的算子收益，按用户确认的标准保留源码。算子计时使用并行合入XMX之前的冻结源码；最终组合仅完成正确性和功能检查，没有追加组合性能计时。
Short功能请求记录的prefill、decode和TTFT不作为性能验收；完整四场景性能矩阵仍待完成。

[结果摘要](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-weights-vector/summary.json)、[首轮](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-weights-vector/screen/results.json)、[独立复测](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-weights-vector/confirm/results.json)、[追加轮](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-weights-vector/repeat/results.json)、[组合构建](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-weights-vector/merged/build/build.json)、[模型功能](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-weights-vector/model/results.json)、[合入哈希](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-weights-vector/integration.json)。归档包含源码、补丁和原始日志，不包含EXE或权重。

复算数据不需要GPU；当前CMake检查已开启两个widening开关并关闭XMX。CMake检查在没有其他GPU任务时运行；模型复测入口自动等待共享锁，结果目录必须是新目录：

```powershell
python bench/results/a770/deepinfer-2026-10-23/prefill-widen-weights-vector/check_results.py
cmake --build <sycl-build> --target prefill_gemm_parity
ctest --test-dir <sycl-build> -R '^prefill_gemm_parity$' --output-on-failure
python build-sycl-deepinfer-20261023/check_bf16_arena.py build-sycl-deepinfer-20261023/check_widen_weights_model.py build-sycl-deepinfer-20261023/prefill-weights-model-replay build-sycl-deepinfer-20261023/prefill-widen-weights-vector-merged/build
```

### 激活 widening 二维索引：未采用（2026-10-24）

- [x] 冻结包含已保留权重向量化的组合源码和依赖；候选仅用二维 SYCL 网格替换激活转换的展平索引，共两行运行时代码，数学运算与步长处理不变。
- [x] 基线/候选普通检查各2,030项，候选大形状、共享 arena 和超过4 GiB偏移各270项，加上计时进程270项，共5,140项逐字节对照，零差异。
- [x] 两组独立 A/B/A/B/A，共10个进程、90个中位数、450批计时、36组配对；8组基线漂移超过3%，按预设规则排除。
- [x] 当前源码重新构建并通过56项 `draft_policy_test` 检查；MTP Q8_0 gate/up/down布局及非法形状拒绝检查通过。
- [x] 初筛未通过，候选不写入运行时源码；未追加独立确认或整模型测试。已有开关默认值、生产EXE和配置未修改，96个冻结依赖哈希相同。
- [x] 原始数据、冻结源码、构建命令/哈希、工具脚本及离线审计入口已归档。

平台为 A770 16GB / Xeon E5-2696 v4 / PCIe 3.0 ×16 / Windows，驱动32.0.101.9034，DPC++2025.3.2 / oneMKL2025.3。
两侧使用相同128 MiB workspace和all128内存池，开启已有FP16/BF16 widening，关闭XMX、half-output及分析日志。
计时形状为 `T=4/40/256, N=1280/4096/12288, K=2560, ldy=N`；FP16的ldx为2560，BF16在N4096时为2560，其余为2573。
每进程预热4次，测5批×16次，取批均耗时中位数；范围为完整同步GEMM API，包含分配、转换、GEMM、拷贝、等待和释放。

预设保留条件要求初筛和独立确认的两次候选轮次各自吞吐中位数为正、至少75%的合格配对为正，且单形状几何均值回退不超过1%；没有要求收益达到3%。初筛结果如下：

| 候选轮次 | 漂移合格配对 | 正收益配对 | 吞吐变化中位数 |
|---|---:|---:|---:|
| 第一轮B | 14 | 5 | -0.447% |
| 第二轮B | 14 | 5 | -0.258% |

合计28组合格配对中10组为正，未达到保留条件。没有对下降原因作内核或调度归因；这不是整模型性能结果。
各轮配对共享中间基线，摘要仅作描述统计。全部负值、超限配对和每个形状的结果均保留在[结果摘要](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-activation-grid/summary.json)和[原始记录](../bench/results/a770/deepinfer-2026-10-23/prefill-widen-activation-grid/results.json)中。

初筛测量及配对保存后，脚本在打印汇总时因字段名误写而中断。已修正字段名，用独立离线审计复算全部原始批次和配对，没有重跑或替换计时数据；原中断状态、实际执行脚本和修正脚本均已归档。
本轮另复核前两轮workspace/等待和权重向量化的结果审计，均通过；历史构建的源码匹配状态只对应各自冻结版本。

离线复核不需要GPU、编译器或私有构建目录；GPU复测使用新的输出目录和共享串行锁：

```powershell
python bench/results/a770/deepinfer-2026-10-23/prefill-widen-activation-grid/check_results.py
python build-sycl-deepinfer-20261023/check_bf16_arena.py build-sycl-deepinfer-20261023/check_widen_activation.py build-sycl-deepinfer-20261023/prefill-widen-activation-repeat
python tools/test_mtp_rt.py
```

### CPU tokenizer 字节映射：未采用（2026-10-24）

- [x] 冻结 tokenizer、服务端准备路径和现有依赖，分别测量分词与 prompt 准备，不增加依赖。
- [x] 独立测试单行候选：把 UTF-8 字节的 Python 生成器映射改为 Latin1 解码加 `str.translate`。
- [x] 两轮各通过10,000组 Unicode/无合并词表检查、1,600组真实词表 token ID 比对及非法代理字符检查；20个计时进程的输出哈希一致。
- [x] 候选通过23项回归，覆盖真实词表、字面 think/control 标记及图片标记；新增 UTF-8 字节映射检查也在现有实现上通过。
- [x] 保留初筛、唯一一次完整复测、全部负值与排除项；按预先记录的门槛判定不采用，没有追加第三轮。
- [x] 该项实验结束时 tokenizer 与该项基线逐字节相同，仅保留最小回归检查；后续缓存初始化改动另列，生产 EXE 和配置哈希未变。

在 Xeon E5-2696 v4、Windows/64位 CPython 3.11 上，子进程固定逻辑CPU 40。
使用248,320词表/247,587 merges，每阶段独立进程 A/B/A/B/A，预热3次、计时7批。
Short、Prose8K、Code8K、Unicode8K、Prose32K 各测 `Tokenizer.encode` 与 `Service.encode_prompt`；8K/32K为字符数。
后者使用真实模板与 MockEngine，测量CPU准备耗时，不含HTTP或模型生成，没有专家缓存容量。

| 测量 | 相邻基线漂移≤3%的配对 | 合格配对中正收益 | 至少两组合格配对的场景 |
|---|---:|---:|---:|
| 初筛与确认 | 15/40 | 15/15 | 5/10 |
| 唯一一次完整复测 | 14/40 | 13/14 | 4/10 |

复测未达到事先约定的30/40配对、全部10场景各至少两组的覆盖要求。
短文本 `Tokenizer.encode` 的一组合格配对为−14.69%，两组合格配对的几何平均收益为−5.58%，超过2%的回退门槛；确认阶段第一轮也只有2/3合格配对为正，低于75%。
因此没有采用该单行改法，也没有CPU准备路径或整模型提速声明。
复测等待共享GPU锁后运行；该锁不约束其他CPU任务。测量漂移的具体原因未被隔离，不能归因于某个内核或驱动。

初轮全部10个子进程完成后，汇总代码因一个候选轮次没有合格配对而报 `StatisticsError: no median for empty data`。
保留原脚本与中断状态，修正汇总逻辑后从原始JSON恢复缺失配对，没有替换计时样本。
[方法与结果报告](../bench/results/a770/tokenizer-translate-2026-10-24/README.md)、[完整结果摘要](../bench/results/a770/tokenizer-translate-2026-10-24/summary.json)和[逐文件哈希](../bench/results/a770/tokenizer-translate-2026-10-24/archive.json)均已落盘。
离线审计复算200个中位数、1,400个批次和80组配对，无需GPU、权重或私有构建目录；原始复测命令与冻结源码见报告。

```powershell
python bench/results/a770/tokenizer-translate-2026-10-24/check_results.py
python -m unittest serve.test_detok.HeapBpe.test_byte_mapping_is_utf8 -v
```

### CPU token_bytes 缓存初始化：按更新标准保留（2026-10-24）

- [x] 单独冻结两行候选：构造函数初始化已有缓存，`token_bytes()` 直接取属性，省去每次调用的空字典分配和 `setdefault`。
- [x] 全部248,320个真实词条冷/热缓存共496,640次字节比对通过；另通过7,576次流式前缀/待解码状态、192次解码错误策略及非法ID检查。
- [x] 冻结候选通过现有9项回归，零跳过；新增可独立运行的空字节、缓存隔离、重复访问和非法ID检查，在现有源码上通过。
- [x] 等待共享GPU锁后完成两阶段独立进程 A/B/A/B/A；保留全部20组配对、350个计时批次和50个中位数。
- [x] 原覆盖门槛未通过；用户本轮明确改为性能门槛1%、算子正收益即可保留后，按新标准合入已测两行改动。原判定与数据均保留，生产 EXE/配置不变。
- [x] 合入后源码与冻结候选逐字节相同，两个最小回归检查通过；不追加采样寻求通过。

硬件为 Xeon E5-2696 v4、Windows 10、64位 CPython 3.11.9，子进程固定逻辑CPU 40。
预热5次、计时7批；输入为真实词表编码的英文、代码、CJK、组合字符、emoji和韩文混合文本，长度按token ID计。
只测CPU字节转换、批量解码及 `Detokenizer.push`，不含HTTP或模型生成，没有专家缓存容量。
筛选阶段4/10组、确认阶段6/10组满足相邻基线漂移≤3%；分别只覆盖3/5、4/5场景，未达到事先约定的每阶段至少8/10组、覆盖全部场景的门槛。

| CPU测点 | 两阶段合格配对 | 合格配对的几何平均收益 |
|---|---:|---:|
| 已缓存字节，16,384 token | 1/4 | +41.28% |
| 清缓存后转换，4,096 token | 3/4 | +10.41% |
| 批量解码，16,384 token | 2/4 | +38.29% |
| 流式解码，4,096 token | 1/4 | +15.31% |
| 批量解码，64 token | 3/4 | +43.21% |

合格配对10/10为正，五个测点的合格配对几何平均收益均为正，因此按用户更新后的算子规则保留源码。
原覆盖门槛仍未通过，不作为稳定加速结论；最大基线漂移13.07%的原因未被隔离。
表中数据不外推到模型生成，不连乘。全部排除项、源文件/依赖/生产文件哈希保留在[原始记录与方法](../bench/results/a770/token-bytes-cache-2026-10-24/README.md)和[完整判定](../bench/results/a770/token-bytes-cache-2026-10-24/summary.json)。
本次[用户规则更新](../bench/results/a770/token-bytes-cache-2026-10-24/adoption-update.json)与[原规则判定](../bench/results/a770/token-bytes-cache-2026-10-24/summary.before-adoption.json)单独保存，没有改写原始计时或事先记录的门槛。
离线审计无需GPU、权重或私有构建目录；复测命令与冻结源码见报告。

```powershell
python bench/results/a770/token-bytes-cache-2026-10-24/check_results.py
python -m unittest serve.test_detok.HeapBpe.test_token_bytes_cold_and_cached -v
```

### BPE 规则表局部引用：未采用（2026-10-24）

- [x] 冻结两行候选，短词每轮合并复用规则表引用；合并顺序不变。
- [x] 通过10,000组 Unicode、1,600组真实词表 token ID 对照和10项回归，零跳过。
- [x] 完成独立进程两阶段 A/B/A/B/A；9个算子测点与10个CPU准备测点，共190个中位数、1,330个批次、76组配对。
- [x] 算子筛选的合格配对几何平均收益−2.09%，确认+2.32%；符号不一致，且有CPU准备回退，因此不合入，没有追加采样寻求通过。

基线已包含上一项缓存初始化改动。Xeon E5-2696 v4、Windows/CPython 3.11.9、逻辑CPU 40，计时期间未启用 profiler。
`Tokenizer.encode` 筛选为−4.73%，确认没有满足3%漂移要求的配对；`Service.encode_prompt` 分别为−2.82%/+1.18%，确认仅一组合格配对。
这些是CPU算子与准备阶段测量，不含模型生成。首次验证因词表路径设置晚于测试导入而跳过5项，已修正并保留原日志；该次失败没有启动性能进程。
[报告与原始数据](../bench/results/a770/bpe-ranks-2026-10-24/README.md)保留负值、排除项及冻结源码。

```powershell
python bench/results/a770/bpe-ranks-2026-10-24/check_results.py
```

### token_bytes 首次转换：保留正收益源码（2026-10-24）

- [x] 用内置 `bytes(map(...))` 替代缓存未命中时的 Python 字符/bytearray 循环，保留非法ID验证及非法字符 `KeyError` 类型和消息。
- [x] 不增加依赖、缓存或开关；上一项缓存初始化保留，输出字节和精度不变。
- [x] 全词表冷/热共496,640次字节对照、7,576次流式前缀/待解码状态、192次解码策略及10项回归通过，零跳过。
- [x] 增补 `a中b` 的重复非法字符错误检查；合入后两个最小回归通过，源码与已测候选逐字节相同。
- [x] 两阶段独立进程 A/B/A/B/A，共50个中位数、350个批次、20组配对；按用户“算子正收益即可保留”标准合入源码，生产 EXE/配置不变。

硬件、Python及CPU固定方式同上；计时等待共享GPU/编译锁，预热5次、计时7批。
改变的测点每次清缓存并转换真实词表的4,096个 token，32次/批，构造 tokenizer 不计时。

| 首次转换算子 | 合格配对 | 吞吐收益 | 相邻基线漂移 |
|---|---:|---:|---:|
| 筛选 | 1/2 | +44.02% | +0.30% |
| 确认 | 1/2 | +36.57% | +2.88% |

每阶段仅一组合格对照，覆盖有限；另外两组因漂移超3%排除，原始数据均保留。全部测点的最大基线漂移8.06%，原因未被隔离。
暖缓存字节、批量解码和流式解码作为对照；流式确认的两组合格配对几何平均为−1.03%，筛选无合格配对，不声明该路径提速。
本轮只保留首次转换的正收益源码，不外推到HTTP、prefill、decode或TTFT，不与前项收益连乘，没有整模型加速结论。
[方法与完整结果](../bench/results/a770/token-bytes-map-2026-10-24/README.md)包含样本覆盖、负值及源码/依赖/生产文件哈希；[采用判定](../bench/results/a770/token-bytes-map-2026-10-24/summary.json)可离线复算。
归档后附加汇总输出因字段名错误中断，已保留原脚本并修正打印；源码检查与离线审计在此前已通过，没有重跑或替换样本。

```powershell
python bench/results/a770/token-bytes-map-2026-10-24/check_results.py
python -m unittest serve.test_detok.HeapBpe.test_token_bytes_cold_and_cached -v
```

### CPU 批量 token 解码：保留正收益源码（2026-10-24）

- [x] 仅将 `decode()` 的生成器替换为 `map(self.token_bytes, ids)`，复用已有缓存与字节转换；不新增依赖或开关。
- [x] 冻结包含前两轮优化的源码与依赖；两阶段独立进程 A/B/A/B/A，共60个中位数、420个计时批次、24组配对。
- [x] 全词表496,640次字节对照、7,576次流式状态及192次解码策略对照通过；新增空输入、多字节/非法UTF-8、六种错误策略、迭代器及非法ID检查。
- [x] 候选和合入后各跑11项回归，零跳过；运行源码与已测候选逐字节相同，生产EXE与配置哈希不变。
- [x] 按用户已指定的算子正收益规则保留；保留脚本额外主测点门槛失败的判定，不补采样、不更改漂移排除规则。

Xeon E5-2696 v4、Windows/CPython 3.11.9、逻辑CPU 40；等待共享GPU/编译锁，预热5次、计时7批，关闭profiler。
64-token CPU解码的合格对照在筛选/确认分别为 **+27.08% / +23.58%**，各仅1/2组，覆盖有限。
确认阶段Code 2K为+24.20%（2/2），Prose 2K为+25.74%（1/2），16K为+13.81%（1/2）；这些测点筛选阶段均无合格对照。
总计18/24组因相邻基线绝对漂移超过3%排除，最大漂移17.70%，原因未单独隔离；冷缓存与流式对照均无合格配对。
脚本额外要求16K两阶段均有合格收益，该覆盖门槛未通过；完整失败判定和用户规则采用判定分别保留。
本轮仅报告CPU解码算子收益，不声明稳定加速，不外推到模型decode、prefill、TTFT或HTTP，也不与前两轮收益连乘。
[方法、全部原始结果与复测命令](../bench/results/a770/decode-map-2026-10-24/README.md)及[完整判定](../bench/results/a770/decode-map-2026-10-24/summary.json)已归档。

```powershell
python bench/results/a770/decode-map-2026-10-24/check_results.py
$env:STRATA_TOKENIZER = 'G:\GitHub\Strata-data\packs\iq2_xs\tokenizer'
python -m unittest serve.test_detok -v
```

### 无 FP64 设备的采样修复

固定种子、惩罚和 coupled draft 采样在冻结基线中报错：`'double' is not supported in 'Intel(R) Arc(TM) A770 Graphics' device`。
临时加入 `OverrideDefaultFP64Settings=1 IGC_EnableDPEmulation=1 NEOReadDebugKeys=1` 后，后端仍报 `Double type is not supported on this platform`。
以上是修复前的失败记录。后续增加了下面的设备精度回退，单独记录验证结果。

- [x] SYCL 主机端查询 `sycl::aspect::fp64`，设备不支持时自动使用 FP32 概率计算；支持的设备保留原 FP64 运算顺序。
- [x] 默认 split、one-block、旧版参考和 coupled draft 使用各自独立的精度模板内核，避免 FP32 内核仍携带 FP64 能力要求。
- [x] 保留 Philox、惩罚、top_k → top_p → min_p → temperature 顺序，无新增开关或依赖。
- [x] 扩展现有 `sampler_parity`：coupled 概率对照 CPU FP64，重复 seed、子词表、连续六个草稿、历史状态与位置/参数变化后的图重放。
- [x] A770 上 split、one-block、旧版三条路径全套回归均零失败，日志确认使用 FP32。
- [x] 每条路径的 coupled 图重放/计数器/概率检查均为 216 次、零差异；所选 token 概率相对 CPU FP64 参考的最大绝对误差 `8.75e-8`。
- [x] Short/Q8 五模式整模型固定种子采样回归；每种模式两次48-token输出一致，且与基线一致，贪心输出与修复前一致。

FP32 的累计概率在边界附近可能与 FP64 选择不同 token，不能承诺跨精度逐 token 相同。
原 A770 采样会拒绝运行，因此这项回退用于恢复该设备的采样能力；不把它记作整模型提速。
独立构建保存在 `build-sycl-deepinfer-20261023/sampler-fp32/`；两份整模型引擎复用同一个修复后的采样器对象。
采样器的[构建命令与哈希](../bench/results/a770/deepinfer-2026-10-23/sampler-fp32/build.json)、[三路径原始结果](../bench/results/a770/deepinfer-2026-10-23/sampler-fp32/checks/results.json)均已保留。
FP64 模板已编译；额外 CPU SYCL 运行探针报无可用 CPU 设备，因此没有将其计作 FP64 硬件回归通过。
Short 的[完整整模型回归](../bench/results/a770/deepinfer-2026-10-23/functional-sampler-q8-short/results.json)使用同一修复采样器、5771 slots / 7922 MiB、现有验证窗口 prompt 路径。
采样参数为 `temperature=0.7 top_p=0.9 top_k=20 seed=1234 penalty_repeat=1.1 penalty_freq=0.1 penalty_present=0.2 penalty_last_n=128`，coupled draft 开启。
该组用于正确性验证，不计作性能比较；Code2K/Prose2K/Long8K 的完整矩阵仍未完成。
阈值0的[连续提交回归](../bench/results/a770/deepinfer-2026-10-23/functional-sampler-chain-short/results.json)同样通过；[8种配置的输出交叉检查](../bench/results/a770/deepinfer-2026-10-23/sampler-fp32/output-equivalence.json)核对修复前贪心输出及两个阈值下的采样输出。
基线和组合模式均另跑 EOS、取消恢复、parked/live 会话恢复。生产 EXE 和配置未写入，96 个原依赖对象/库哈希复核仍相同。
2026-10-24 的[四场景回归](../bench/results/a770/deepinfer-2026-10-23/functional-sampler-q8-allcases/results.json)补齐五模式的 Short、Code2K、Prose2K、Long8K greedy 对比；各模式也重复 Short 固定种子采样、惩罚、取消和恢复检查。
仍使用窗口读取、5771 slots / 7922 MiB、输出上限48；功能运行耗时不作性能结论。
[模型头部记录](../bench/results/a770/deepinfer-2026-10-23/model-shape.json)确认 K/V 头维度256、RoPE 维度64；未开启 Hadamard，符合 Q8 融合条件。
[四场景采样与重放](../bench/results/a770/deepinfer-2026-10-23/functional-sampler-q8-long-sampling/results.json)另用基线和组合验证相同 seed/惩罚参数，每种输入两次48-token输出逐 token 相同，且跨模式一致。
该组开启现有 `--prompt-cache 128 --prompt-cache-every 512`；Code/Prose 重放复用2041 token，Long 复用7993 token，实际缓存仍为5771 slots。
这些是功能检查，不作性能比较；[输出与构建哈希复核](../bench/results/a770/deepinfer-2026-10-23/continued-output-equivalence.json)核对跨组 Short 参考、四场景输出、96个共用对象以及生产文件。
在正常配置的独立 SYCL 构建中，复测现有目标即可：

```powershell
cmake --build <sycl-build> --target sampler_parity
ctest --test-dir <sycl-build> -R '^sampler_parity' --output-on-failure
```

### Short 计时筛选与阶段观测

[首轮筛选](../bench/results/a770/deepinfer-2026-10-23/model-sampler-q8-short/results.json)使用 Q8、5771 slots / 7922 MiB、输出上限128；每个进程预热2次、保留3次，另先做一次16-token初始化。
基线与四个候选交错运行，四组比较的全部保留请求输出相同。只覆盖 Short，不作为四场景整模型提速结论。

| 候选 | Short decode 首轮 | 独立复测 | 最大相邻基线耗时漂移 |
|---|---:|---:|---:|
| 批量 KV/indexer | +0.29% | 未复测 | 0.047% |
| 成本窗口 | +1.17% | +1.05% | 0.213% |
| Q8 Norm/RoPE/store 融合 | +0.24% | 未复测 | 0.217% |
| 组合 | +1.49% | +1.32% | 0.307% |

批量和融合未达到1%的 Short 筛选门槛；算子的已确认收益仍单独保留。
成本窗口和组合的[独立复测](../bench/results/a770/deepinfer-2026-10-23/model-sampler-q8-short-confirm/results.json)同样通过 Short 的1%筛选门槛，全部保留请求输出一致；该表使用常用阈值0.50，连续提交在此阈值不生效。
这些结果只确认 Short decode；四场景性能矩阵未完成，四条路径继续默认关闭。
[阶段统计](../bench/results/a770/deepinfer-2026-10-23/model-sampler-q8-short/stage-summary.json)对齐同一个中位请求：成本窗口的 verify 毫秒/token 下降约2.82%，draft 反而增加约6.59%；组合分别为下降约3.27%、增加约6.82%。
这是原有主机阶段计时的观测，不是设备内核计时；每窗口阶段均值只打印到0.01毫秒，归一化数值保留该舍入误差。
[复测阶段统计](../bench/results/a770/deepinfer-2026-10-23/model-sampler-q8-short-confirm/stage-summary.json)保留同样的请求对齐和舍入说明。
两组测试的引擎、依赖、缓存、预热和输出上限相同；测试脚本只将共享锁轮询从250毫秒缩短为50毫秒，发生在模型进程启动之前，各组均保存脚本快照。

阈值0的[连续提交/组合筛选](../bench/results/a770/deepinfer-2026-10-23/model-sampler-chain-short/results.json)和[追加对照及组合复测](../bench/results/a770/deepinfer-2026-10-23/model-sampler-zero-short-confirm/results.json)使用相同缓存、预热和输出上限：

| 候选 | Short decode 首轮 | 追加对照/复测 | 最大相邻基线耗时漂移 |
|---|---:|---:|---:|
| 连续提交单项 | +0.20% | 未复测 | 0.036% |
| 成本窗口单项 | 未测 | +5.10%（一次对照） | 0.048% |
| 组合 | +6.10% | +5.92% | 0.105% |

连续提交单项未达1%的 Short 门槛；其 draft 毫秒/token 仅下降约2.57%，未声明稳定算子收益。
组合的 verify 毫秒/token 在两组分别下降约7.45%和7.27%；[首轮阶段统计](../bench/results/a770/deepinfer-2026-10-23/model-sampler-chain-short/stage-summary.json)与[复测阶段统计](../bench/results/a770/deepinfer-2026-10-23/model-sampler-zero-short-confirm/stage-summary.json)保留原始主机计时。
以上百分比均对比同一阈值的相邻基线；连续提交仅对阈值0生效，常用0.50仍走原早停路径。
四个计时组共24个独立进程、72个保留请求，全部在第78个token正常EOS，输出逐 token 相同，也与功能回归前48个token一致。
原始JSON包含每次请求的 decode、prompt、TTFT、接受/提议数；Long/Code/Prose 的完整性能矩阵仍未完成。

### 四场景前缀复用组合初筛（2026-10-24）

- [x] 冻结同一个候选EXE（SHA256前缀 `41824ddcfa4f`）、依赖与输入，三次独立进程 A/B/A，仅切换 DeepInfer 组合开关。
- [x] 常用阈值0.50、Q8 KV、实际5771 slots / 7922 MiB；输入23/2048/2048/8000 tokens，输出上限128，分析器与阶段打印关闭，GPU串行运行。
- [x] 复用已有 `--prompt-cache 128 --prompt-cache-every 512`；每场景一次初始请求、两次预热、三次保留，共72次请求、36次保留样本。
- [x] 所有初始、预热和保留输出逐 token 相同；各臂前缀复用量一致，全部统计与原始协议独立核对，离线审计通过。
- [x] 原始计时、接受/提议数、源码/构建/生产文件哈希、独立复算及复测入口落盘；运行时默认值、生产EXE与配置未改。

同页A770 / Xeon / Windows平台；以下仅为**重复提示、前缀恢复后的模型生成**，每场景仅一组合格或排除的 A/B/A 对照，尚无独立确认。

| 场景 | 组合decode吞吐变化 | 两侧基线耗时漂移 | 本轮处理 |
|---|---:|---:|---|
| Short | +2.29% | −2.59% | 合格，小幅正收益 |
| Code 2K | +1.32% | −3.19% | 超过3%漂移上限，排除 |
| Prose 2K | +1.18% | +2.80% | 合格，小幅正收益 |
| Long 8K | +0.14% | +2.55% | 合格，未达1%模型门槛 |

四场景描述性几何平均+1.23%包含被排除的Code结果，不能作为完整验收值；因四场景未全部合格，本轮筛选未通过，没有追加采样寻求通过。
Short 的 TTFT 相对两侧基线几何平均从246.23增加到257.84毫秒，增加约11.61毫秒（4.71%）；生成吞吐收益不代表首字延迟改善。
其他场景的TTFT和全部负值保留在报告中。组合继续默认关闭，已确认正收益的算子源码保留。

本轮使用冻结的 DeepInfer 构建，不包含后续CPU tokenizer或其他会话的XMX实验，收益不相加、不连乘。
初始长输入仍走窗口读取路径，未做该场景预热；8K单次约396–404秒，这些读数不作为原生批量prefill验收。
后续仍需原生批量prefill的四场景性能、单项对照和独立确认；本轮没有最新全部优化的稳定整模型加速声明。
[全部结果、TTFT、原始数据与复测命令](../bench/results/a770/deepinfer-cached-matrix-2026-10-24/README.md)及[完整判定](../bench/results/a770/deepinfer-cached-matrix-2026-10-24/results.json)已归档。

```powershell
python bench/results/a770/deepinfer-cached-matrix-2026-10-24/check_results.py
```

### 补充：Q8 单 subgroup 写入（2026-10-24）

- [x] 保留一个默认关闭的 SYCL 编译选项 `STRATA_SYCL_Q8_SINGLE_SUBGROUP`；32 个线程各处理两个值，省掉 Q8 组内两个 subgroup 之间的共享内存归约和屏障。
- [x] OFF/ON 使用同一冻结工具链、头文件和依赖库对象独立构建；完整 KV 指纹与冻结基线相同，编码/scale 与 CPU 参考一致。
- [x] 补上批量 Q8 本身在位置变化后的图重放；`kv_steps_parity` 从171项增至198项，两个编译分支均通过。
- [x] 初版直接提交、初版图重放和保留源码直接提交各做两轮 A/B/A/B/A；共60个计时进程、72组对照，原始耗时、负值和漂移排除全部归档。
- [x] 按“算子正收益可保留”留下可选源码；保留源码的24组对照仅8组漂移合格，其中4组正值、4组负值。批量6-token有重复正值，但数值跨度大，不能声明稳定算子收益。
- [x] 图计时24组中23组漂移超限，唯一合格组为−10.59%；不将图的正值算作提速。初次大批图提交的中止记录也已保留。
- [x] 编译选项默认OFF，四条既有DeepInfer开关默认值、生产EXE与配置均未改；本次未重新验收该新选项的整模型功能或性能。

平台、适用范围、逐项正负值与复测入口见[完整报告](../bench/results/a770/q8-subgroup-2026-10-24/README.md)。
这是算子候选，收益不与前文模型或 tokenizer 百分比叠加。修改后的OFF分支也已通过冻结参考检查。

```powershell
python bench/results/a770/q8-subgroup-2026-10-24/check_results.py
```

### 补充：MTP 结束路径与成本统计（2026-10-24）

- [x] SYCL 服务端在输出后统一判断 EOS、输出上限和已经收到的取消；末轮跳过下一条草稿链及历史准备，仍等待已有提交完成。
- [x] 没有执行草稿的末轮不再更新“验证＋提交＋草稿”的轮次成本；CLI 的输出上限观测条件也同步过滤。
- [x] 四个独立引擎使用同一冻结工具链和依赖，同类 A/B 共用 MTP 对象；诊断日志仅加入私有构建，生产源码不含这些日志。
- [x] 最终服务端矩阵 46 次真实模型请求、317 个诊断轮次通过：阈值 0/0.50、1/4/6 输出上限、固定种子采样及惩罚、EOS、取消、greedy/采样恢复。对应输出逐 token 一致，真实轮次包含全拒绝、部分接受和全部接受。
- [x] 独立原始日志审计复现基线的 16 次末轮成本观测；候选不再观测这些末轮。两次验证阶段取消各省去一条草稿链，后续请求输出一致。
- [x] 原有 DraftPolicy 二进制的 53 项检查通过；新增 CLI 条件直接从源码抽取并调用真实策略，54 对、108 项 CPU 检查通过。CPU 检查不代替 CLI 整模型验证。
- [x] 实际专家缓存固定为 5771 slots / 7922 MiB，Q8 KV；原始参数、源码/构建哈希、日志与脚本失败/中止记录均归档。独立审计通过，生产 EXE 和配置哈希未变。
- [ ] CLI 整模型/prefill：既有基线的多 token 提示未进入 decode；单 token 提示返回 0 token（已有 `spec_pos=0` 被 `spec_pos>0` 排除）。本轮仅验收新增 CLI 调用条件，不将这些尝试记作整模型通过。

本轮验证结束路径的正确性及实际调用次数，没有测量整模型提速；实验开关默认值不变。
取消在末轮判断之后才到达时，仍需等待已经提交的草稿工作。服务端使用窗口读取提示路径，原生批量 prefill 仍待独立验收。
[完整报告与复测命令](../bench/results/a770/mtp-terminal-round-2026-10-24/README.md)、[结果](../bench/results/a770/mtp-terminal-round-2026-10-24/results.json)及[源码差异](../bench/results/a770/mtp-terminal-round-2026-10-24/terminal-round.patch)保留了本轮范围和限制。

```powershell
python -X utf8 bench/results/a770/mtp-terminal-round-2026-10-24/check_results.py
```

### 补充：CLI 单 token 解码入口（2026-10-24）

- [x] 修复原生 IQ pack 单 token 提示正常退出却输出 0 token：SYCL 与 CUDA/HIP 共用入口各改两行，以 -1 表示未启用，接受合法的位置 0。
- [x] 直接抽取两种入口的初始化、交接和真实进入条件，修改前/后共 432 项 CPU 检查通过，覆盖关闭推测解码、位置 0/7、1/4/6 输出上限及 EOS。
- [x] 独立 A770/SYCL 候选仅重编入口，复用上一轮干净构建作为基线；头文件、MTP、采样器、验证器及其余库相同。
- [x] 11 次真实 CLI 调用通过：基线复现 0 输出；候选在阈值 0/0.50 下覆盖 1/4/6 输出上限、固定种子采样和首 token EOS，输出与同输入服务端逐 token 相同。
- [x] 16 次服务端请求通过：单 token/Short 提示、greedy/采样及惩罚，对应基线与候选、不同阈值输出一致。实际缓存为 5771 slots / 7922 MiB，Q8 KV。
- [x] 原始输出、参数、源码/构建哈希和独立审计归档；生产 EXE、配置及实验开关默认值未改。本轮没有测量或声明整模型提速。
- [ ] CUDA/HIP 完整构建与实机验证、CLI 多 token 原生批量 prefill 仍待完成；本轮 CPU 控制条件检查不替代这些验收。
- [x] 首轮即结束时的 MTP 零草稿轮次摘要 NaN 已由下节修复；原始复现日志保留，生成输出不变。

[完整报告与复测命令](../bench/results/a770/cli-decode-entry-2026-10-24/README.md)、[原始结果](../bench/results/a770/cli-decode-entry-2026-10-24/results.json)及[本轮源码差异](../bench/results/a770/cli-decode-entry-2026-10-24/cli-entry.patch)。

```powershell
python -X utf8 bench/results/a770/cli-decode-entry-2026-10-24/check_results.py
```

### 补充：MTP 零草稿轮次摘要（2026-10-24）

- [x] SYCL 与 CUDA/HIP 共用入口各改一行：草稿轮数为 0 时摘要显示 `0.000 ms/round`，有草稿时保留原计算，继续显示轮数、prompt 耗时与 VRAM。
- [x] 冻结当前源码与依赖，复用上轮干净 CLI 入口候选作为基线；只重编一个独立 SYCL 入口，其他对象与库相同。
- [x] 复用已有入口检查，两种后端共 216 项 CPU 检查通过；9 次 A770/SYCL CLI 回归包含基线 NaN 复现、正常草稿轮次、阈值 0/0.50、1/6 输出上限、首 token EOS 和固定种子采样。
- [x] 候选四个零草稿场景均显示 `0.000`，三个有草稿场景的均值有限且非负，完整 stdout 不含 NaN/Inf；全部输出逐 token 等于上一轮同参数参考。
- [x] 实际 CLI 专家缓存为 5771 slots / 7.74 GiB，Q8 KV；原始输出、参数、源码/构建哈希及独立审计归档。生产 EXE、配置与实验开关默认值未改。
- [ ] CUDA/HIP 完整构建与实机运行、CLI 多 token 原生批量 prefill 仍待独立验收。

本轮修复解码后的统计输出，没有新增性能测量。零草稿轮次的 `0.000` 表示未执行草稿，不作为耗时测量。
[完整报告与复测命令](../bench/results/a770/mtp-summary-2026-10-24/README.md)、[结果](../bench/results/a770/mtp-summary-2026-10-24/results.json)及[两行源码差异](../bench/results/a770/mtp-summary-2026-10-24/mtp-summary.patch)。

```powershell
python -X utf8 bench/results/a770/mtp-summary-2026-10-24/check_results.py
```

### 补充：CLI 短提示验证窗口读取（2026-10-24）

- [x] 原生 SYCL CLI 复用服务端的短提示窗口策略，使用已有 `--short-read`；设为 0、超过阈值或部分 `--prefill-until` 保留批量路径。无新依赖/内核/API，生产 EXE 和配置未替换。
- [x] 提示阶段关闭 head sampling，提交所有提示 token，依次填入 MTP KV，等待提交后进入生成；提示耗时与 decode 阶段统计分开。
- [x] 456 项 CPU 检查、11 次 CLI、11 次服务端请求通过，覆盖窗口边界、固定种子采样、阈值 0/0.50、自然/指定 EOS、无 MTP 及批量回退；对应输出逐 token 一致。
- [x] 同时修复未加载 MTP 时释放默认队列导致的退出崩溃：旧基线单 token 复现异常退出；最终无 MTP 候选正常退出。只有该队列初始化头文件发生一行变化，MTP/采样器/验证器对象和其余库不变。
- [x] 两 token 提示独立进程 A/B/A/B/A，实际缓存 5771 slots / 7922 MiB，Q8 KV，诊断/性能分析关闭。两组相邻基线对照的 TTFT 均通过 1% 收益及 3% 漂移门槛；延迟下降 99.52%, 99.52%，相邻基线漂移 +0.91%, -0.81%。
- [x] 6-token decode 耗时变化为 +8.01%, +9.32%（正数表示变慢）；保留全部数据，未声明 decode 加速。这里比较的是提示读取路径造成的后续短输出阶段变化，并非新 decode 内核的算子测量。
- [x] 冻结源码/依赖、原始计时与失败记录、独立测试构建及离线审计归档；不将本轮短提示 prefill/TTFT 结果泛化为服务端 decode 或四场景整模型提速。
- [ ] 长提示原生批量 prefill、完整四场景整模型矩阵，以及 CUDA/HIP 的对应路径仍待验收。

[完整报告](../bench/results/a770/cli-prompt-windows-2026-10-24/README.md)、[功能结果](../bench/results/a770/cli-prompt-windows-2026-10-24/results.json)、[计时](../bench/results/a770/cli-prompt-windows-2026-10-24/measurements.json)和[源码差异](../bench/results/a770/cli-prompt-windows-2026-10-24/cli-prefill.patch)。

```powershell
python -X utf8 bench/results/a770/cli-prompt-windows-2026-10-24/check_results.py
```

### 补充：按剩余输出数限制验证窗口，源码默认采用（2026-10-25）

- [x] SYCL 服务端与 CLI 在选择 MTP / suffix 窗口后各加一处长度限制，省去无法返回的额外验证行；无新依赖、开关或算子实现。
- [x] 冻结既有修改及210项文件/依赖，只重编入口；1,302项CPU接受边界检查通过，97个超长窗口被截短。
- [x] 74次服务端请求与8次CLI调用通过，覆盖阈值0/0.50、1/2/3/4/6输出上限、固定种子采样与惩罚、取消、EOS、parked/live恢复及CLI上下文恰好装满；对应输出逐token一致。
- [x] Short/Q8、4191 slots / 5760 MiB，两轮独立进程A/B/A/B/A，十个进程、20次预热、90次保留请求；诊断和分析器关闭。
- [x] 6-token预定主测点decode吞吐筛选+11.34%～+11.67%、独立确认+11.77%～+12.01%；4-token确认+7.24%～+7.81%，16-token确认+7.16%～+8.03%。十二组decode配对均为正，最大相邻基线漂移0.8973%。
- [x] 源码与已验证候选逐字节相同；保留原始日志、首轮CRLF解析失败、构建哈希和离线审计。本轮未写生产EXE或配置；另一会话将生产short-read从256改为1024，已记录并保留，对照仍使用冻结的256。
- [ ] 长输出、完整四场景性能矩阵、CUDA/HIP与非eager图模式仍待验收。FP16 KV已在下方单独补测；本段仅声明Short/Q8短输出decode收益，不声明prefill、TTFT或四场景整体提速；四条既有DeepInfer实验开关不变。

[完整报告与复测命令](../bench/results/a770/spec-output-limit-2026-10-25/README.md)、[功能回归](../bench/results/a770/spec-output-limit-2026-10-25/checks/results.json)、[计时](../bench/results/a770/spec-output-limit-2026-10-25/measure/results.json)及[源码差异](../bench/results/a770/spec-output-limit-2026-10-25/window-limit.patch)。

```powershell
python -X utf8 bench/results/a770/spec-output-limit-2026-10-25/check_results.py
```

### 补充：输出尾部验证窗口的FP16 KV验收（2026-10-25）

- [x] 复用上述精确基线/候选EXE，冻结当前源码、生产文件和脚本；只将测试KV格式改为FP16，不重编或替换生产EXE。
- [x] 82次调用通过：72次服务请求、两次EOS、六次CLI、两次上下文边界；覆盖阈值0/0.50、固定种子采样与惩罚、取消恢复和parked/live续写，输出逐token一致。使用FP16自身参考，不要求与Q8相同。
- [x] Short/FP16初筛五进程A/B/A/B/A，6-token主测点decode吞吐+9.45%～+10.24%；4-token +8.07%～+8.37%，16-token +7.49%～+8.25%，六组基线漂移均≤3%。实际专家缓存4191 slots / 5760 MiB。
- [x] 首次独立确认的四组4/6-token配对因基线漂移>3%被排除；6-token漂移+3.2454%和−3.0124%，没有有效主测配对，不计为确认通过。16-token两组有效配对为+6.59%～+8.60%。原始样本完整保留。
- [x] 按补测前固定规则补跑一次完整五进程确认，参数、预热、输出参考和1%/3%门槛不变。补充确认6-token decode吞吐+11.22%～+11.65%，4-token +7.71%～+7.93%，16-token +6.87%～+7.65%；六组配对均有效，最大基线漂移0.7073%。
- [x] 共15个计时进程、30次预热、135次保留请求；18组decode配对中14组有效，首次确认的四组无效配对完整保留。补测保存完整数据后发生摘要打印错误，另行核对日志、计算结果及生产/源码保护哈希通过，没有重新采样。
- [x] 达到已测Short/FP16范围的1%门槛，保留两处源码默认长度限制；当前源码与已测候选逐字节一致，生产EXE与配置保持不变。
- [ ] 长输出、四场景完整性能矩阵、非eager图模式与CUDA/HIP另测；本段仅涉及Short/FP16短输出decode。

[完整报告与复测命令](../bench/results/a770/spec-output-limit-fp16-2026-10-25/README.md)、[功能回归](../bench/results/a770/spec-output-limit-fp16-2026-10-25/checks/results.json)、[首次计时及无效配对](../bench/results/a770/spec-output-limit-fp16-2026-10-25/measure/results.json)和[补充确认](../bench/results/a770/spec-output-limit-fp16-2026-10-25/confirm-retry/results.json)。

```powershell
python -X utf8 bench/results/a770/spec-output-limit-fp16-2026-10-25/check_results.py
```

### 默认与交付

- [x] 四条新路径本轮全部默认关闭，保留可复测源码和独立开关。
- [x] 生产配置和 EXE 本轮未写入；配置与冻结副本相同。
- [x] Unsloth 原文、源码版本、算子原始数据与失败记录落盘。
- [ ] 完成所有场景的单项/组合整模型计时；已补齐四场景前缀复用组合初筛，原生批量prefill与独立确认仍待完成，当前没有稳定整模型加速声明。
- [ ] 在原批量 prefill 验收通过后，补齐四场景性能矩阵；长输入单项采样及阈值0的扩展也保留为待测。

原生产 EXE SHA256：`8f0d866ded297cf5a409e84cf5dd61f437c56c8a89aaeb2a85569265b6a3e122`。
共享工作区还在进行独立的累计阈值/提前截断实验。本轮构建使用冻结副本，累计阈值保持0；其他实验后续修改不自动计为本轮验证通过。

### 复测命令

以下命令使用已保留的独立构建目录，`--out` 每次指定新目录。
整模型命令使用 `sampler-fp32` 下的修复引擎；该目录的 `config-original.json` 是窗口读取测试配置副本。
不要用未修复的旧整模型 EXE 复测 A770 采样。`--greedy-only` 会跳过采样，不能作为完整采样验收。

```powershell
python tools/bench_deepinfer.py --build-root build-sycl-deepinfer-20261023 --phase checks --out build-sycl-deepinfer-20261023/checks-repeat
python tools/bench_deepinfer.py --build-root build-sycl-deepinfer-20261023 --phase operators --out build-sycl-deepinfer-20261023/operators-repeat
python tools/bench_deepinfer.py --build-root build-sycl-deepinfer-20261023/sampler-fp32 --phase functional --kv int8 --cases Short,Code2K,Prose2K,Long8K --max-tokens 48 --expert-cache 5500 --modes baseline,batch,cost,fused,combined --out build-sycl-deepinfer-20261023/functional-sampler-repeat
python tools/bench_deepinfer.py --build-root build-sycl-deepinfer-20261023/sampler-fp32 --phase functional --kv int8 --threshold 0 --cases Short --max-tokens 48 --expert-cache 5500 --modes baseline,chain,combined --out build-sycl-deepinfer-20261023/functional-chain-repeat
python build-sycl-deepinfer-20261023/sample_long.py --out build-sycl-deepinfer-20261023/functional-long-sampling-repeat
python tools/bench_deepinfer.py --build-root build-sycl-deepinfer-20261023/sampler-fp32 --phase model --kv int8 --cases Short --max-tokens 128 --warmup 2 --retained 3 --expert-cache 5500 --modes baseline,batch,baseline,cost,baseline,fused,baseline,combined,baseline --out build-sycl-deepinfer-20261023/model-sampler-repeat
python tools/bench_deepinfer.py --build-root build-sycl-deepinfer-20261023/sampler-fp32 --phase model --kv int8 --cases Short --max-tokens 128 --warmup 2 --retained 3 --expert-cache 5500 --modes baseline,cost,baseline,combined,baseline --out build-sycl-deepinfer-20261023/model-cost-repeat
python tools/bench_deepinfer.py --build-root build-sycl-deepinfer-20261023/sampler-fp32 --phase model --kv int8 --threshold 0 --cases Short --max-tokens 128 --warmup 2 --retained 3 --expert-cache 5500 --modes baseline,chain,baseline,combined,baseline --out build-sycl-deepinfer-20261023/model-chain-repeat
python tools/bench_deepinfer.py --build-root build-sycl-deepinfer-20261023/sampler-fp32 --phase model --kv int8 --threshold 0 --cases Short --max-tokens 128 --warmup 2 --retained 3 --expert-cache 5500 --modes baseline,cost,baseline,combined,baseline --out build-sycl-deepinfer-20261023/model-zero-repeat
python build-sycl-deepinfer-20261023/continued_output_check.py functional-sampler-repeat functional-chain-repeat
python build-sycl-deepinfer-20261023/analyze_short.py model-sampler-repeat model-cost-repeat model-chain-repeat model-zero-repeat
```

原批量 prefill 未完成时，功能回归可指定独立的 `--config build-sycl-deepinfer-20261023/config-window-prompt.json`。
此时 TTFT/prompt 耗时属于验证窗口读取路径，不能代表原批量 prefill。

## Unsloth RoPE 评估

- [x] 核对用户指定的[《使用 Unsloth Kernels + Packing 让 LLM 训练速度提升 3 倍》](https://unsloth.ai/docs/zh/bo-ke/3x-faster-training-packing)。
- [x] 核对官方源码，固定提交与内容 SHA256。
- [x] 对比入口、布局、数值顺序和现有融合；没有新增 Torch/Triton 依赖。
- [x] 结论：可以参考调度和访存思路，当前版本不能直接替换 SYCL 内核。本轮不额外复制一套 RoPE。

核对版本：[Unsloth `c5482a5b5714f70fd8e376d9be1f38d012af4b13`](https://github.com/unslothai/unsloth/blob/c5482a5b5714f70fd8e376d9be1f38d012af4b13/unsloth/kernels/rope_embedding.py)。
`rope_embedding.py` SHA256：`aecdee1f749feba5714e020bb1b44848dccf8a19d4b253208db26dfd17338d4a`。
其 [device_type.py](https://github.com/unslothai/unsloth/blob/c5482a5b5714f70fd8e376d9be1f38d012af4b13/unsloth/device_type.py) 已包含 Intel XPU 分支；这不等于该版本的每个 RoPE 入口均可在 A770 上直接运行。

| 项目 | Unsloth 当前实现 | Strata 的适用性 |
|---|---|---|
| 调用环境 | Python、Torch tensor 和 Triton JIT | Strata 使用 C++/SYCL、USM 和已有图执行，直接调用会新增依赖和桥接 |
| decode 专用入口 | `decode_rope_inplace` 明确检查 `Q.device.type == "cuda"`，且 seq_len=1 | 不能直接用于当前 A770/SYCL 或多 token 验证窗口 |
| Q/K 合并提交 | 一个 grid 的不同 group 处理 Q 和 K | 思路可移植；Strata 的 K 后面紧接缓存写入，Q 来自另一次投影，合并需要重排执行顺序 |
| 多头分组 | 多个 head 共用 sin/cos tile，`ROPE_TILE_ELEMENTS=1024` | 可能降低纯 RoPE 的调度开销；其 CUDA warp 参数不能当作 A770 最优参数 |
| 布局 | NeoX 半区配对、任意 tensor stride、原位或直接写目标布局 | Strata 同样半区配对，但 HD256 只旋转前64维，其余192维仍须正确保留/归一化 |
| 角度 | 从调用者提供的 sin/cos 表加载 | Strata 已有 `STRATA_ROPE_TABLE=1`；原解析角度和 FP64 建表的低位不同，不能默认切换来声称精度不变 |
| 运算顺序 | 明确使用 `tl.fma` 并启用 contraction | Strata 服从 pinned ggml 的 FP32 数学顺序和 `/fp:precise`；不能假定直接换成其 FMA 顺序后 KV 字节仍相同 |
| 融合范围 | Q/K 旋转 | Strata 已有 Norm+RoPE，本轮 Q8 路径再融合量化与缓存写入，保留原 scale 编码和主机镜像 |

因此本轮优先验证已有的融合入口。没有执行原版 Unsloth 内核，不能给出它在 A770 上的速度数字。
若以后纯 RoPE 成为实测瓶颈，可单独移植多头 tile，并以现有 `rope_parity`、KV 字节检查和整模型输出作为采用门槛。

文章给出的 RoPE 微基准是短序列约 1.9 倍、长序列约 2.3 倍；标题的通常 3 倍/最高 5 倍还包含训练去 padding、样本 packing 和 MLP 内核。
这些训练收益不能作为 Strata 单会话推理收益。当前验证窗口按实际 token 数执行，没有文章中多样本训练的 padding 浪费，也不使用反向传播。
文中的显式 position ids 支持用于打包后的序列位置重置；Strata 已从设备位置数组取位置，并在图重放检查中改变位置。

仍有可移植的增量：在 SYCL 中联合提交 Q/K Norm+RoPE，让多头共用角度加载。
当前 QSA 的 K 投影、Norm/RoPE 和缓存写入先执行，随后才执行 Q 投影；联合提交需要提前 Q 投影并保持 indexer 快照/提交顺序。
应将它作为独立候选和现有 Q8 写入融合比较，保持 HD256/rotation64、FP32 舍入、YaRN/IMRoPE 和 KV 字节一致。
本轮没有增加这第五条实现，也没有把现有融合的实测收益归因于 Unsloth。

### GEMM 独立输出区复用：未采用（2026-10-25）

本轮隔离候选为每个 `Gemm` 保留一个独立 FP32 输出区，只在容量不足时扩容，替代每次调用的申请和释放。
旧输出复制、FP32 GEMM、最终拷回和等待、输出步长回退均保留；独立分配继续规避 oneMKL 的共享分配大偏移错误。
候选没有合入运行时源码，生产 EXE 和配置没有因本轮实验改变。

设备为 A770 16GB、Xeon E5-2696 v4、Windows，DPC++ 2025.3.2 / oneMKL 2025.3；两臂使用相同冻结依赖和现有原生 USM 内存池。
两臂均开启 FP16/BF16 GPU widening，关闭原生 XMX 和 half-output。每组按 A/B/A/B/A 启动五个独立进程，分析器关闭，显卡独占。
常用批量形状为每种精度六组，覆盖 T=22/40/256；每个形状预热后取七批、每批三次完整同步 GEMM 调用的中位数。
基线的输出分配和释放计入耗时；候选的首次扩容发生在预热前。另保留 T=4 的诊断对照。

| 输入 | 第一组几何平均吞吐变化 | 第二组几何平均吞吐变化 |
|---|---:|---:|
| FP16 | −0.240% | +0.324% |
| BF16 | +0.095% | +0.195% |

以上均为完整六形状的描述统计。FP16 的 N=2560/K=640/T=40 第二次配对基线漂移为 +3.070%，超过 3% 漂移门槛。
四个分组都没有达到测量前固定的 3% 吞吐门槛，因此未追加独立确认或整模型计时，也没有整模型提速结论。

5,345 项专门正确性检查通过，包含两臂 widening 关闭/开启、超过 4 GiB 的共享分配和大矩阵；计时程序另有 280 项检查通过。
16 种批量形状在五个进程中的完整输出哈希均一致。已核对原始摘要、44 组配对和四个几何均值，并从批次记录重算 T=4 对照的 30 个中位数。
既有批量检查程序只打印中位数，没有输出七个批次的独立耗时，因此这部分不能从日志重新计算中位数。
在本轮已有内存池的配置下，没有确认省掉这两次分配/释放调用带来的收益；不推断其他平台或分配器配置。

[数值、环境与证据哈希](../bench/results/a770/widen-output-reuse-2026-10-25.json)及[未采用的候选补丁](../bench/results/a770/widen-output-reuse-2026-10-25.patch)已保留，补丁通过 `git apply --check`。

### DeepInfer-Next：带阈值的 MTP 连续提交实验（2026-11-02）

作者的[基准说明](https://github.com/Zhidao-Lab-OSS/DeepInfer-Next/blob/2a85f8f0091e54dab5c38758e29ebdef2594e7f9/deepinfer/model_executor/models/qwen4_exp/README.md)
使用 **2 × RTX 4090 48GB**、NVFP4 专家和 MXFP8 稠密投影。
300 tok/s 对应不思考的日志转 JSON；代码／数学约249 tok/s，短提示约163～188 tok/s。
这些是作者报告的双卡结果，本机没有复测；不能解读为普通单张24GB 4090的通用速度。
其 MTP 使用48K草稿词表、按累计置信度截短，以及带滞后的2／5草稿图切换。
Strata 已有词表子集、累计置信度和成本窗口入口，本轮优先复用这些能力，
不将双卡常驻权重的吞吐直接外推到 A770＋RAM 专家路径。

对照社区分支 DeepInfer-Next `2a85f8f0091e54dab5c38758e29ebdef2594e7f9` 的设备端草稿链，
尝试先连续提交完整 MTP 草稿链，再按原来的逐 token／累计概率阈值返回串行早停前缀。
这是减少主机等待的实验，没有设备端跳过 MoE，低置信度后缀仍会被计算。

独立构建和原始记录位于 `build-deepinfer-next-20261102/`；只替换主机端 MTP 对象，
复用生产 GPU images 和其余对象，不写生产 EXE、权重或配置。
Arc A770 16GB、Xeon E5-2696 v4、Windows；固定 5771 专家槽、FP16 KV、MTP4／阈值0.5，
保留生产精度及专家策略。五场景分别连续重复相同提示，首轮完整读取，后续复用前缀；
每臂每场景两次预热、三次保留，三臂独立进程 A/B/A，共75次请求、7950个输出 token。
完整输出、结束原因、草稿计数及缓存命中量一致；原始协议和中位数独立复算通过。
短代码、短问答、Code2K、Prose2K、Long8K 的 decode 吞吐变化依次为
**+0.63%、+0.22%、−1.13%、−2.45%、−2.49%**，最大基线耗时漂移2.06%。
没有场景达到1%采用门槛，候选源码已撤回，原有默认值不变；未对这个失败候选追加采样测试。

[结果及原始记录哈希](../bench/results/a770/deepinfer-next-2026-11-02.json)和
[未采用的补丁](../bench/results/a770/deepinfer-next-chain-rejected-2026-11-02.patch)已保留。
后续三个 Orca L0 候选也未达到整模型1%门槛，均未采用：

| 候选 | 有效计时结果 | 结论 |
|---|---|---|
| 只补算接受前缀 | 五场景 −0.86%～+0.32% | 原有开关保持关闭 |
| 接受前缀，并省去当前行重复前向准备 | Prose2K 初筛+1.42%，独立复测+0.38% | 撤回候选 |
| 接受前缀，并复用当前行前向中间结果 | Prose2K +0.87%，短代码+0.12% | 撤回候选 |

后两项各通过72请求、5972输出 token的逐 token 回归，覆盖阈值0／0.5／0.95、
累计阈值0／0.5、固定种子 coupled 采样和惩罚；吞吐未过门槛，因此没有保留默认启用代码。
第二项另一次确认的基线漂移5.69%～6.35%，已排除；表中+0.38%来自漂移0.70%的独立复测。
第三项的 L0／DG2 开关检查通过，DG2 的 MTP 预处理计算源码在归一化异常日志行号后相同。
实验入口为 `tools/bench_mtp_prefix.py`，全部原始结果路径及 SHA256 见上方结果文件。

### Orca L0：累计置信度默认值（2026-11-02）

已在 Orca L0 适配器中启用现有的累计置信度0.5早停，同时裁剪草稿生成和验证窗口。
本机 `strata-iq2_xs.json` 也设为 `STRATA_MTP_CUM_MIN_P=0.5`，下次启动生效；当前生产 EXE 已支持此开关，因此无需替换 EXE。
设为0可恢复原窗口；普通 SYCL、DG2 的源码缺省值仍为0。保留 spec4、完整草稿词表与原权重精度。
这是原始草稿概率乘积的现有启发式，没有移植 DeepInfer-Next 的接受率校准或2／5草稿图。

设备与固定5771专家槽同上。初筛每臂预热10次、保留5次，后两组每臂预热5次、保留3次；均为独立进程 A/B/A。
初筛和缺省复测关闭 suffix lookup；最后一组保留生产 lookup=3、PCIe 分流和自适应换入。
lookup=3按原程序将验证器容量扩为6，MTP上限仍为4；三臂一致。
lookup列的Long8K来自独立单场景、每臂10次预热／5次保留；原混合场景该项基线漂移3.20%，其+6.46%计时已排除，原始输出检查仍有效。
重复固定提示、prompt cache=128，报告 decode 吞吐，不能外推为任意新提示的总延迟收益。

| 输入 | 原 EXE 开关初筛 | 重建缺省独立复测 | lookup 开启复测 |
|---|---:|---:|---:|
| 短问答 | +3.23% | +4.38% | +3.46% |
| Code2K | -1.76% | +0.86% | -1.45% |
| Prose2K | +1.48% | +3.91% | +1.18% |
| Long8K | +3.97% | +3.18% | +4.04% |

lookup开启的四输入有效对照，吞吐比几何均值为 **+1.79%**。
采用条件为该固定四输入均值至少1%、每场景基线耗时漂移不超过3%、单场景回退不超过2%；逐场景是否达到1%另记在JSON，不能将均值称为每种输入的收益。
72次功能请求、5922个输出 token 全部逐 token 一致，覆盖短代码、短问答、中文、2K代码／文章、8K上下文；
阈值0／0.5／0.95、coupled固定种子采样、top-k／top-p、重复／频率／存在惩罚和重新切回贪心。
另有33个真实编译的解析检查；DG2整个入口的预处理结果在归一化异常日志行号后相同。
只隔离重编生产父构建的主机入口，复用原数值库和GPU images；未完整重建包含其他工作区修改的整个引擎。

复测入口为 `tools/bench_mtp_prefix.py --candidate-cumulative-default --keep-suffix`，配合基线 `--exe`、重建候选 `--candidate-exe`、新的 `--out`；
`--functional --cases Anchor,Short,Chinese,Code2K,Prose2K,Long8K --tokens 96 --warmup 0 --retained 1 --reuse-prompt` 复测功能。
构建、原始协议、逐次计时、配置备份路径与SHA256均见[结果记录](../bench/results/a770/deepinfer-next-2026-11-02.json)。


### Orca L0：累计置信度0.5附近的复测（2026-11-02）

以已启用的0.5为新基线，初筛0.35和0.65；保留生产lookup、PCIe分流、自适应换入、FP16 KV和完整草稿词表，固定5771专家槽。
每臂每输入3次预热、3次保留，使用独立进程A/B/A。以下均为重复提示、prompt cache=128的decode吞吐变化。

| 候选阈值 | 短问答 | Code2K | Prose2K | Long8K | 四输入几何均值 |
|---|---:|---:|---:|---:|---:|
| 0.35 | -1.30% | +1.51% | -1.25% | -3.23% | -1.08% |
| 0.65 | -2.84% | -1.44% | -2.90% | +1.15% | -1.52% |

两项均未过原有采用门槛，保留0.5；没有把上一轮0→0.5的收益重复计算。
144请求、16344输出token的完整输出均一致；所有场景基线耗时漂移在3%内。
逐场景结果、原始协议和SHA256见[结果记录](../bench/results/a770/deepinfer-next-2026-11-02.json)的`cumulative_confidence_refinement`。


### Orca L0：按剩余输出预算裁剪MTP尾部（2026-11-02）

服务端和CLI的每轮草稿生成复用`set_max_drafts`，上限为`min(S_mtp-1, 剩余输出数-1)`。
第一草稿仍维护MTP KV；终止轮继续跳过草稿。无需新参数、依赖、词表或GPU内核。
普通SYCL源码不变，DG2入口预处理结果在归一化异常日志行号后完全一致。

以当前累计阈值0.5为基线，A770 16GB、Xeon E5-2696 v4、Windows；固定5771专家槽、FP16 KV、完整248320草稿词表、生产PCIe分流和lookup。
独立进程A/B/A，每臂每输入30次预热、15次保留，重复提示、prompt cache=128。**这里请求最多只输出3个token**，测量decode耗时。

| 输入 | decode吞吐变化 | 基线耗时漂移 |
|---|---:|---:|
| Short | +7.86% | -1.84% |
| Code2K | +2.42% | -0.14% |
| Prose2K | +2.92% | +0.27% |
| Long8K | +4.97% | -0.09% |

四输入吞吐比几何均值 **+4.52%**；独立复测：Short +5.14%（漂移+0.40%）、Chinese +4.33%（漂移-0.10%）。

这些是短输出的尾部收益，不能外推为长回答持续TPS或新提示总延迟的同等提升。
432请求、2448输出token的边界回归覆盖1/2/3/4/8/16输出预算、累计阈值0/0.5、逐token阈值0/0.5/0.95、实际启用的coupled采样及惩罚；输出、结束原因、缓存状态和验证计数全部一致。
另有24请求、2724输出token重放当前基线的128-token请求，完整输出一致；该重放只检查输出，不拼接成性能对照。
连同计时共1266请求、7602输出token逐token一致；528项编译边界检查通过，覆盖极大预算及短请求后深度恢复。

启动配置已改为`build-orca-mtp-budget-20261102/strata.exe`，下次启动生效；原EXE保留，配置备份在`build-deepinfer-next-20261102/strata-iq2_xs-before-budget.json`。
只重编冻结生产入口，复用原数值库和GPU images，未完整重建包含其他用户修改的工作树。
复测可用`tools/bench_mtp_prefix.py --cumulative 0.5 --keep-suffix --tokens 3`配合两份EXE和新的输出目录；CPU检查用`tools/test_mtp_budget.py --cxx <编译器> --out <新目录>`。
源码[补丁](../bench/results/a770/deepinfer-next-budget-2026-11-02.patch)、逐次计时、协议及SHA256见[结果记录](../bench/results/a770/deepinfer-next-2026-11-02.json)的`remaining_output_budget`。

同步现有成本窗口与草稿深度的独立候选初筛也已结束，四输入decode吞吐几何均值-0.67%；未满足采用门槛，未加入适配器或默认启用成本策略。证据保留在`cost_window_draft_cap_screen`。

### Orca L0：预算裁剪基线下的长解码复测（2026-11-02）

以已采用的预算裁剪EXE、累计阈值0.5为基线，只测试已有参数的MTP上限4→3；验证器仍为6，lookup仍为3。
候选显式设置`--spec 6 --mtp-max-t 3`，避免单独添加cap时抑制CLI自动扩容、意外把验证器缩到4。
三臂INFO实际检查verify6、MTP4/3、5771专家槽和lookup3；FP16 KV、完整248320词表、生产PCIe分流、自适应换入及GPU images保持。
设备为A770 16GB、Xeon E5-2696 v4、Windows。独立进程A/B/A，每臂每输入3次预热、3次保留，重复提示、prompt cache=128，最多输出128个token。

| 输入 | decode吞吐变化 | 基线耗时漂移 |
|---|---:|---:|
| Short | -2.82% | +0.12% |
| Code2K | +0.92% | -1.72% |
| Prose2K | -3.74% | +0.31% |
| Long8K | -1.80% | -0.40% |

四输入吞吐比几何均值为 **-1.88%**，未过预先记录的均值≥1%、逐场景回退≤2%、基线耗时漂移≤3%门槛，未进入独立确认、未采用。
72次请求、8172个输出token逐token一致，原始T/TB/DONE/RESUME、结束原因、缓存、进程退出及中位数均独立核对通过；不同MTP上限允许验证计数不同。

另用当前基线开启`STRATA_DECODE_TIMING=1`作9次128-token请求诊断，每场景1次预热、2次保留。
Code2K、Prose2K、Long8K中，验证调用及等待分别约占解码时间83.32%／83.23%／84.52%，MTP草稿约占13.81%／13.82%／12.74%。
这是主机阶段计时，不能解释为GPU计算占比；当前device-plan/no-host路径的旧CPU／cache计数为0，也不能据此认定CPU没有工作。
嵌套等待不相加，分析开启的数据不作为生产提速结论。9次请求、1152个token与未开启分析的基线输出及原始协议一致。

本轮未改引擎或启动配置，继续保留累计0.5和预算裁剪；上一轮3-token请求的+4.52%不计为长回答收益。
本机复测命令为`python -X utf8 build-deepinfer-next-20261102/bench_cap.py --exe build-orca-mtp-budget-20261102/strata.exe --out <新目录> --mtp-cap 3 --keep-suffix --reuse-prompt --warmup 3 --retained 3`。
诊断命令为`python -X utf8 build-deepinfer-next-20261102/diagnose_budget.py --out <新目录>`。
采集脚本、原始记录路径、SHA256及诊断限制见[结果记录](../bench/results/a770/deepinfer-next-2026-11-02.json)的`mtp_window_cap_current_budget_baseline`。

### Orca L0：专家缓存热度衰减复测（2026-11-02）

以预算裁剪EXE、累计阈值0.5为基线，只将已有的`--adapt-decay`从0.5改为0.9。
设备为A770 16GB、Xeon E5-2696 v4、Windows；固定5771专家槽、FP16 KV、完整248320草稿词表，保留生产自适应换入、PCIe分流、verify6／MTP4／lookup3。
独立进程A/B/A，重复提示、prompt cache=128，最多输出128个token；初筛每臂每输入3次预热、3次保留，独立确认增加到5次预热、5次保留。

| 输入 | 初筛decode吞吐变化 | 独立确认decode吞吐变化 | 确认基线耗时漂移 |
|---|---:|---:|---:|
| Short | +5.19% | +4.80% | +0.44% |
| Code2K | +0.15% | -0.66% | +0.02% |
| Prose2K | -1.44% | -1.83% | +0.02% |
| Long8K | +0.30% | -1.05% | -0.10% |

四输入吞吐比几何均值初筛为+1.02%，独立确认只有**+0.28%**，未达到预先设定的至少1%采用门槛；保留生产衰减0.5。
192次请求、21792个输出token全部一致，原始协议、退出状态及中位数独立核对通过。没有为未通过性能门槛的衰减候选追加采样测试。
该结果只描述这四组重复提示的缓存策略，不外推为新提示的端到端延迟收益。
采集入口为`build-deepinfer-next-20261102/bench_decay.py`；初筛与确认原始记录及SHA256见[结果记录](../bench/results/a770/deepinfer-next-2026-11-02.json)的`expert_cache_decay_current_budget_baseline`。

### Orca L0：只按提交前缀统计专家热度（2026-11-02）

隔离候选尝试让`Verifier::count_routing`只统计已提交的输入行，并将服务循环的热度统计及自适应线程启动移至成功提交之后。
它复用已有的`h_commit_[0]`，没有增加类字段、GPU内核或回读；只重编冻结生产构建的入口和验证器主机对象。
这是一项缓存策略实验：回滚草稿也已实际执行专家计算，其路由仍可能预测后续需求。

候选提取实际转换后的统计方法，通过L0／DG2共46项前缀及边界检查，另检查禁用统计和空usage入口。
在生产自适应换入及PCIe分流启用时，Code2K／Prose2K的24次32-token功能请求、768个输出token全部一致，覆盖阈值0／0.5／0.95、coupled固定种子采样、top-k／top-p、惩罚及切回贪心。
原始路由记录逐层核对通过：同样210个验证窗口，基线每层428行，候选384行，分别与窗口数加offered／accepted相符。
采样输出实际改变，重新切回贪心的输出恢复一致。

性能初筛沿用上述A770、5771专家槽、FP16 KV、完整词表、累计0.5、verify6／MTP4／lookup3及生产缓存策略。
独立进程A/B/A，每臂每输入3次预热、3次保留，重复提示、prompt cache=128，最多输出128个token，关闭路由追踪。

| 输入 | decode吞吐变化 | 基线耗时漂移 |
|---|---:|---:|
| Code2K | -1.29% | -0.82% |
| Prose2K | -1.96% | +0.60% |

两输入吞吐比几何均值为**-1.62%**，未达到预先记录的至少1%初筛门槛；未扩展到四场景及独立确认，未采用候选。
36次计时请求、4608个输出token全部一致，原始协议、退出状态和中位数独立复算通过；功能与计时合计60请求、5376个输出token。
本轮两个缓存候选均未改生产源码、EXE或启动配置，继续保留衰减0.5、累计置信度0.5和已有的输出预算裁剪。
复测入口为`build-deepinfer-next-20261102/bench_routing_prefix.py`，配合预算裁剪基线EXE及`orca-routing-prefix-build/strata.exe`，使用新的输出目录。
构建、预先设定的门槛、原始记录及SHA256见[结果记录](../bench/results/a770/deepinfer-next-2026-11-02.json)的`committed_prefix_cache_heat_current_budget_baseline`。

### Orca L0：降低专家缓存更新频率（2026-11-02）

复用预算裁剪生产EXE，只将已有的`--adapt-every`从1改为2；每轮最多换入32个专家、衰减0.5及累计置信度0.5保持。
衰减发生在更新时，因此候选同时改变换入节奏和热度保留时间；不能将测得差异单独归因为主机开销或PCIe流量。
A770 16GB、Xeon E5-2696 v4、Windows，固定5771专家槽、FP16 KV、完整248320草稿词表、verify6／MTP4／lookup3及生产PCIe分流。
独立进程A/B/A，每臂每输入3次预热、3次保留，重复提示、prompt cache=128，最多输出128个token。

两输入初筛Code2K为+7.17%、Prose2K为+6.45%，吞吐比几何均值+6.81%，基线耗时漂移分别+0.34%／+0.90%。
随后独立扩展到四输入，结果如下；所有预热和保留样本均保存在原始记录中。

| 输入 | 四输入组decode吞吐变化 | 基线耗时漂移 | 计时是否满足漂移门槛 |
|---|---:|---:|---|
| Short | -7.86% | -0.51% | 是 |
| Code2K | +0.47% | +6.76% | 否 |
| Prose2K | -1.88% | +0.10% | 是 |
| Long8K | +1.78% | -6.01% | 否 |

Code2K和Long8K的四输入组计时超出3%漂移限制，不能用于稳定提速结论或有效四输入均值。
有效Short对照回退超过预先设定的2%上限，因此候选未采用，未进入独立确认或追加采样测试。
初筛两输入的正数没有稳定扩展到完整场景；候选Short的保留样本仍逐次提速，但没有据此删样本或改门槛。
两组共108次请求、12780个输出token全部一致，原始协议、中位数及进程退出独立核对通过。
生产继续保留更新间隔1；计划、采集脚本与原始记录SHA256见[结果记录](../bench/results/a770/deepinfer-next-2026-11-02.json)的`expert_cache_update_cadence_current_budget_baseline`。

### Orca L0：减少每轮专家换入上限（2026-11-02）

另一个独立候选只将已有`--adapt-swaps`从32改为16，保留每窗口更新和每次衰减0.5。
这是配置的最大换入数，并非实测搬运条数或PCIe字节数；不与间隔2候选合并。
设备、预算裁剪EXE、累计0.5、固定5771专家槽、完整词表、FP16 KV及其余生产策略同上。
独立进程A/B/A，每臂每输入3次预热、3次保留，重复提示、prompt cache=128，最多输出128个token。

| 输入 | 原始配对吞吐观察值 | 基线耗时漂移 | 计时是否有效 |
|---|---:|---:|---|
| Code2K | +10.97% | +6.26% | 否 |
| Prose2K | +8.94% | -4.62% | 否 |

两项均超出预先设定的3%漂移门槛，表中的正数不能视为已确认提速，不计算有效两输入均值。
候选未采用，未进入四输入、独立确认或采样测试。四输入测试的六次预热方案已提前登记，但因初筛无效而未执行；没有在原数据上改门槛或删样本。
36次请求、4608个输出token全部一致，原始协议、中位数及退出状态独立核对通过，参数审计确认仅换入上限不同。
本轮间隔及换入量两个候选合计144请求、17388个输出token，生产继续保留间隔1、上限32、衰减0.5和已有的累计0.5／预算裁剪。
原始数据与SHA256见[结果记录](../bench/results/a770/deepinfer-next-2026-11-02.json)的`expert_cache_swap_limit_current_budget_baseline`；未测频率、功耗或PCIe流量，不能由本轮计时确定漂移原因。

### Orca L0：相同配置的计时校准（2026-11-03）

先运行三臂完全相同的生产预算裁剪EXE，均为间隔1／换入上限32／衰减0.5／累计0.5。
A770 16GB、Xeon E5-2696 v4、Windows；固定5771专家槽、FP16 KV、完整248320草稿词表、verify6／MTP4／lookup3及生产PCIe分流。
每臂每输入6次预热、3次保留，重复Code2K／Prose2K提示、prompt cache=128，最多输出128个token。
全部请求在共享GPU锁内伴随已有的只读Sysman监测，约1Hz；没有修改电源方案、频率或驱动设置，也未启用内核剖析。

| 输入 | 三个相同配置进程的decode中位tok/s | 前后基线耗时漂移 | 中间臂相对前后基线的吞吐差异 |
|---|---|---:|---:|
| Code2K | 32.21／32.64／32.03 | +0.57% | +1.63% |
| Prose2K | 30.00／29.92／29.77 | +0.77% | +0.13% |

两项通过预先记录的漂移≤3%、相同配置中间臂差异≤3%校准门槛；表中的正数是相同配置的变化，不能称为优化收益。
54次请求、6912个输出token全部一致，原始协议、中位数和退出状态独立复算通过。
504个完整监测采样均通过时钟域与请求覆盖核对，其中77个位于18次保留请求内；这些采样的GPU频率范围为1350～2400MHz，限频标记均为0，最高传感器温度56°C，最低空闲显存1235.9MiB。
采样范围包含请求中的预填充及缓存恢复，不能解释为固定GPU时钟或瞬时显存峰值保证。PCIe计数返回不支持，未把零值当成零流量；没有CPU频率或功耗数据。

这组同时增加预热并启用监测，没有单独验证二者的影响，也不能据此解释上一轮漂移。
考虑到相同配置的中间臂已有1.63%差异，本轮在候选计时前将采用门槛从1%提高到3.5%，同时保留每场景漂移≤3%、回退≤2%及关闭监测器后的独立确认要求。
监测解析检查覆盖已校验的真实探针、不支持的PCI状态、截断尾记录及非法频率。
复测入口为`build-deepinfer-next-20261102/bench_monitored_swaps.py --swaps 32 --cases Code2K,Prose2K --warmup 6 --retained 3 --tokens 128 --reuse-prompt --keep-suffix`，配合生产预算裁剪EXE及新的输出目录。
原始记录、校准及候选计划、全部SHA256见[校准记录](../bench/results/a770/orca-cache-calibration-2026-11-03.json)；此校准没有改生产配置或确认提速。

### Orca L0：带监测的换入上限16复测（2026-11-03）

通过上述相同配置校准后，重新独立对照已有的`--adapt-swaps 32→16`，其余参数、EXE和GPU images保持一致。
四输入每臂6次预热、3次保留，独立进程A/B/A；完整输出、原始协议及监测覆盖均核对通过。
采用门槛已在候选计时前提高到四输入几何均值≥3.5%，同时要求逐输入漂移≤3%、回退≤2%，并须通过关闭监测器后的独立确认。

| 输入 | 预热后decode吞吐变化 | 基线耗时漂移 | 计时是否有效 |
|---|---:|---:|---|
| Short | +2.81% | -2.97% | 是 |
| Code2K | +3.26% | -2.14% | 是 |
| Prose2K | +3.41% | -2.61% | 是 |
| Long8K | +8.93% | -4.57% | 否 |

Long8K仍超出漂移限制，不能计算有效四输入均值或据此确认综合提速。
这组36次保留请求内的135个监测采样均为2400MHz、限频标记0，最高传感器温度56°C、最低空闲显存1588.4MiB；这些采样没有解释8K漂移，仍不足以排除未采到的频率变化或其他开销。

候选Short的早期请求爬升较慢，因此在最后一臂基线开始前，额外登记每场景最初3次请求的中位decode耗时检查；该检查是在观察到候选热身差异后增加的限制，不是本轮初始预登记项。
Code2K最初3次请求的吞吐变化为**-9.85%**，对应基线耗时漂移-2.13%，对照有效；Prose2K为+0.68%、Long8K为+9.65%，对应漂移也在3%内。
Short最初3次的原始观察为-13.73%，但基线漂移-3.44%超限，不用于稳定回退结论。
这些是固定提示重复请求的早期缓存学习过程，不能外推为任意新提示、冷启动或总延迟的同等变化。

全局上限16既未过有效四输入计时门槛，又在有效Code2K早期请求中回退，未采用；没有为这个候选追加独立性能确认或采样测试。
108次请求、12258个输出token全部一致，914个完整监测采样通过时间域及全部请求覆盖检查。
连同相同配置校准，本轮共162请求、19170个输出token，生产EXE、启动配置、间隔1／换入上限32／衰减0.5／累计0.5及已有预算裁剪均保持。
复测入口为`build-deepinfer-next-20261102/bench_monitored_swaps.py --swaps 16 --warmup 6 --retained 3 --tokens 128 --reuse-prompt --keep-suffix`，配合预算裁剪生产EXE和新的输出目录。
逐请求记录、早期检查、监测及所有SHA256见[校准记录](../bench/results/a770/orca-cache-calibration-2026-11-03.json)的`swaps16_monitored_screen`。

### Orca L0：提高专家换入的热度差门槛（2026-11-03）

本轮保留每轮最多换入32个专家，只在隔离的Orca L0入口中将全局、按成本加权的换入热度差门槛从0.5提高到1.5。
1.5沿用已有传统路径的数值，是待验证的缓存策略参数；没有新增自适应策略或配置项，也没有改动数值库和GPU images。
L0／DG2共18项检查直接运行转换后的实际选择代码，覆盖热度差边界、槽位容量、换入上限及重复淘汰同一槽位等情况。

先用生产EXE、增加诊断记录的原始路径和候选分别重放Code2K／Prose2K，每场景各3次128-token请求；18请求、2304个输出token全部一致。
两条带记录路径各有371个换入批次；原始路径换入10468个专家，候选5107个，请求复制的自适应换入载荷从15394662400降至7454566400字节，减少**51.58%**。
这不是硬件PCIe总流量；总体路由命中率下降约0.12个百分点。两条路径都未观察到专家换入后四个批次内又被淘汰，未据此宣称发现短周期反复换入。
所有诊断计时均排除在速度结论之外。

随后关闭诊断，使用只读Sysman监测运行独立进程A/B/A。设备仍为A770 16GB、Xeon E5-2696 v4、Windows，固定5771专家槽、FP16 KV、完整248320草稿词表、verify6／MTP4／lookup3及累计0.5。
每臂每场景6次预热、3次保留，重复提示、prompt cache=128 MiB，最多输出128个token；间隔1、衰减0.5及生产PCIe分流保持。
计时前登记四场景几何均值至少提升3.5%、逐场景回退不超过2%、前后基线耗时漂移不超过3%；各场景最初3次请求的中位decode耗时也须通过同样的漂移和回退检查。

| 输入 | 预热后decode吞吐变化 | 基线耗时漂移 | 计时是否有效 |
|---|---:|---:|---|
| Short | -0.65% | +0.37% | 是 |
| Code2K | +8.41% | +1.81% | 是 |
| Prose2K | +9.80% | **+3.19%** | **否** |
| Long8K | +5.87% | -0.09% | 是 |

原始四场景几何均值为+5.78%，因Prose2K漂移超标，不能作为有效四场景收益。
最初3次请求的Short／Code2K／Prose2K／Long8K原始吞吐变化分别为-0.51%／+5.27%／+6.84%／+9.52%，相应基线漂移+0.56%／**+3.99%**／**+3.32%**／+0.13%；Code2K和Prose2K早期计时也无效。
这些是本轮初筛观察值，没有独立确认；未执行已准备的去诊断正式构建、关闭监测的独立确认或coupled采样套件，未采用候选。

108次计时请求、12258个输出token全部一致；899个完整监测采样通过时钟域及全部请求覆盖检查，129个位于保留请求内。
这些保留请求采样的GPU频率为1350～2400MHz，未记录到限频标记，最高传感器温度56°C、最低空闲显存1870.5MiB。
1350MHz样本出现在A1 Code2K，整请求采样也包含缓存恢复／预填充，不能直接归因于decode；Prose2K保留请求采样均为2400MHz，现有监测未确定其漂移原因。
PCIe硬件计数不受支持，没有CPU频率或功耗测量；不能将这些数据解释为瞬时峰值保证或通用新提示端到端收益。

合计126请求、14562个输出token通过原始协议、输出、中位数及退出状态核对。生产EXE、配置和所有已采用设置保持。
复现诊断可运行`python -X utf8 build-deepinfer-next-20261102/diagnose_cache_margin.py`，需要先完成`build_cache_margin.py`的隔离构建，并为脚本使用新的输出目录。
计时入口为`bench_cache_margin.py --exe build-orca-mtp-budget-20261102/strata.exe --candidate-exe build-deepinfer-next-20261102/orca-cache-margin-build/candidate.exe --out <新目录> --swaps 32 --tokens 128 --warmup 6 --retained 3 --reuse-prompt --keep-suffix`，脚本位于`build-deepinfer-next-20261102/`。
预先登记的门槛、实际源码、逐请求记录、未执行阶段和SHA256见[结果记录](../bench/results/a770/orca-cache-margin-2026-11-03.json)。

### Orca L0：按用户决定采用热度差1.5（2026-11-03）

用户在得知上述计时漂移超标后决定采用本候选。原初筛记录和失效门槛保留；采用决定不改变测量结论，稳定提速幅度仍待确认。
此前51.58%的自适应换入复制量下降，是固定5771槽、重复提示诊断中的观察值。

正式EXE位于`build-orca-cache-margin-20261103/strata.exe`，去除了实验新增的诊断记录，只重编冻结预算裁剪基线的入口，复用原数值库和GPU images。
Orca L0的全局加权换入热度差设为1.5，DG2及SYCL路径保留0.5；当前工作树仅补入对应门槛分支。
每轮换入上限32、间隔1、衰减0.5、累计置信度0.5及输出预算裁剪均保留。

正式构建的L0／DG2／SYCL共27项实际选择代码边界检查通过；当前工作树另通过已有的4006项选择回归检查。
上线前使用完整生产参数，自动专家缓存、prompt cache=0，在Code2K／Prose2K中完成24次32-token请求、768个输出token的新旧EXE逐token核对。
覆盖置信度门槛0／0.5／0.95、coupled固定种子采样、top-k／top-p、惩罚及切回贪心；采样确实改变输出，恢复贪心后结果恢复一致，两进程正常退出。
自动缓存分别分配6466和6588个槽位，这组检查仅验证功能，不作为性能配对。

`strata-iq2_xs.json`已切换到正式EXE，下次启动生效；其余配置字段保持。
原EXE保留，原配置逐字节备份在`build-deepinfer-next-20261102/strata-iq2_xs-before-cache-margin.json`；回滚时用该备份恢复启动配置。
精确源码[补丁](../bench/results/a770/orca-cache-margin-adoption-2026-11-03.patch)、配置与EXE的SHA256及补充检查见[采用记录](../bench/results/a770/orca-cache-margin-adoption-2026-11-03.json)。
