# A770：QSA norm / RoPE 融合的实际构建对照

当前没有通过验收的整模型收益，生产构建未改。A770 16 GB / PCIe 3.0 / Windows / DPC++ 2025.3.2。最新恢复轮也被短时编译事件拒绝，见文末；不从残缺计时认定收益或回退。

用户提出的 Router TopK + softmax、路由专家 SwiGLU + Down 输入 Q8_1 量化，当前 SYCL 实现都已融合。`native_router.dp.cpp` 的 `route<NE>` 在一次派发内完成 softmax、Top10 和选中权重归一化；`iq_kernels.dp.cpp` 的 `swiglu_q8_1_entries_kernel` 同时执行激活与量化。冻结 EXE 的实际 eager 采集也有对应调用：route 家族 5682 次，路由专家 SwiGLU/Q8 5376 次、累计 0.040734 秒。不能重复计算这两项收益，也不能把历史算子百分比视为当前模型增益。共享专家属于另一条路径；这不是所有共享专家都已融合的结论。

工作区 `verify.cpp` 已包含 norm/RoPE 和 Q 拆分融合，但当前 2K 测速链接的是 `build-sycl-a770-gu-q8-export-20261027/verify.obj`，其冻结源码仍分开执行。这一区别得到已有 [eager 采集](decode-gpu-profile-2026-10-28.md) 的支持：norm/RoPE 融合调用为零；独立 256 线程 norm 家族 4911 次、0.027557 秒，RoPE 4923 次、0.037480 秒。这些是采集模式下的累计 GPU 时间，含主模型与 MTP 等调用，不能直接换算 graph 关键路径或 tokens/s。GU 仍是更大的已知热点。

新增隔离候选只移植现有 FP32 norm/RoPE 算法到冻结 verifier：

- K 和 indexer 的 norm/RoPE 两次派发合为一次。
- Q 从 Q|gate 的跨距布局直接读取，省去独立 Q 拆分，再融合 norm/RoPE。
- 其他维度、关闭 native、显式关闭 norm/RoPE 时保留原路径；旋转 KV 和原来的位置索引保持。
- 新开关 `STRATA_QSA_NORM_ROPE=1` 仅用于隔离试验，候选默认关闭。实际采用还须独立验收。

基线使用合法 device-counter 版本；保持原 verifier ABI、MTP、IQ、GEMM、预填、缓存和其他链接对象。只有 verifier 对象替换，另增加一个具有独立导出名的融合对象。CPU 检查已通过：三处移植可逆、原 Q 拆分回退完整、链接输入差异精确、未改变权重/量化算法。

GPU 检查安排 72 个组合：HD128/256、1/2/12/24/72/144 行、连续原位/跨距 Q 输入、正负零/随机/大有限值；每组检查 eager 和三次更新位置的 graph 重放。要求全部输出逐位一致、输入与 gate 不变、输出守卫完整；随后才进入原始 Code/Prose 2048 输入、完整 128 输出的 A/B/A 整模型筛选。候选必须留下实际融合分派见证。首轮不是最终性能验收：同一场景仍需两批独立 ≥1%、相邻基线漂移 ≤3%，以及完整历史、质量和采样显存检查；变化 <0.3% 视为噪音。

2026-10-29 04:46 UTC：候选 PID23520（创建时间1793249019591）仍等待独占锁。尚未编译、执行 GPU 数值检查或测速。运行文件：`build-sycl-a770-qsa-norm-rope-20261029/run.py`、`probe.cpp`、冻结 `native-rope-input.cpp`。不组合其他性能候选。

```powershell
python -X utf8 build-sycl-a770-qsa-norm-rope-20261029/run.py --self-check
```

后续运行：候选已取得独占锁，编译和链接完成；`probe.stdout` 实际输出 `PASS 72 cases, eager and 3 mutable-position graph replays, bitwise outputs, read-only input and guards`。这验证了全部288次执行的输出逐位一致、输入不变及守卫检查。已进入原始整模型 A0/候选/A1 筛选，当前 A0 正在运行；尚无配对吞吐结论，也未完成整模型候选输出或独立性能验收。

2026-10-29 后续状态：上述首轮已中止，`results.json complete=false`。A0 完成 Short 与 Code/Prose 预热后，在请求边界检测到并行 icx/clang 编译；候选尚未启动，没有保留请求的配对测速结果。原始输出、错误、退出记录及数值探针结果全部保留。不能把这轮中止解读为融合变快或变慢。

新增 `build-sycl-a770-qsa-norm-rope-v2-20261029/run.py`：复用上述已检查的同一二进制，不重新编译，不拼接旧请求。先做全新 A0/候选/A1；任一场景满足 ≥1% 和漂移 ≤3% 后，再跑独立一组，要求同一场景再次过门槛。原始完整输出、派发见证和全部样本仍须核对。复用原构建清单时，仅对构建结束后继续追加的外层 `queue.stdout` 重新冻结最终摘要；保留原清单，所有源码、对象、二进制和依赖摘要仍逐项核对。

两组筛选共用 Windows `Win32_ProcessStartTrace` 编译进程启动监视器，捕捉请求边界之间的短编译；监视器失败或捕捉到编译时拒绝整组。实际原生订阅的启动、READY 和干净退出检查已通过（`guard-smoke/compiler-guard-results.json passed=true`）；这项 CPU 检查未启动 GPU 或编译器，未改变性能样本。新任务 PID16736、创建时间1793251252612，等待精确的前序 FP16 任务 PID19068/1793250756112，然后仍须取得共享实验锁。启动不等于完成；当前仍无新增性能结论。

```powershell
python -X utf8 build-sycl-a770-qsa-norm-rope-v2-20261029/run.py --self-check
```

即使这两组初筛通过，也不等于完成完整历史、质量、采样空闲显存 ≥1024 MiB 或稳定35验收；生产配置未改。

2026-10-29 06:00 UTC 续查：V3 复用同一已通过探针的二进制，逐项核对计算输入；旧诊断 map 的变化采用独立、明确的元数据记录，没有覆盖原清单或豁免计算文件摘要。V3 的 A0 已完成 Short 和 Code2K 预热，但另一入口再次直接启动 icx/clang；请求边界与原生进程启动监视器均捕捉到竞争。V3 `complete=false`，候选未计时，原始失败、输出与监视器记录全部保留，不能据此判断融合收益或退化。

常用本地构建入口 `tools/build_ninja.bat` 现通过 `tools/build_ninja.py` 复用 `bench_deepinfer.gpu_lock`，避免后续通过该入口启动的构建与测速重叠。参数转发、非零退出码和异常释放检查通过，批处理入口实际执行 `--self-check` 也 exit0；没有重新编译或启动 GPU。此修改只能协调遵守同一锁的入口，直接启动的其他编译仍由测速监视器拒绝。

V4 只复用原有两组筛选、同场景独立 ≥1%、漂移 ≤3%、<0.3% 噪声和编译监视规则，保留 V3 失败来源，不重新编译。PID15920，创建于 2026-10-29 14:00:44 +08:00，等待精确的 INT8 前序进程 PID21512/1793252724104，然后取得共享锁。排队与 CPU 自检不计作性能通过。

```powershell
python -X utf8 tools/build_ninja.py --self-check
python -X utf8 build-sycl-a770-qsa-norm-rope-v4-20261029/run.py --self-check
```

V4 后续：计算文件核对通过，编译监视器正常结束且没有事件，但 A0 的 READY 空闲显存只有 297 MiB，低于既有 1024 MiB 门槛；引擎正常退出，Short/预热/保留请求均为零。原始 `initial/results.json`、协议与失败不改写，不把显存准入失败解释为融合退化。随后另一项相同 6297 槽模型记录 1710 MiB；这说明可用容量发生变化，尚不能唯一归因于进程退出回收、驱动缓存或其他占用。

仅新增 `build-sycl-a770-qsa-norm-rope-v4-20261029/resume.py`，将下一次完整筛选写入独立 `after-memory/`，保留所有既有门槛和失败来源，不降低缓存槽数、不改变精度、不拼接旧计时。`--self-check` 确认上一轮在生成前被拒绝并通过原自检；实际恢复进程 PID3216 于 14:07:52 +08:00 启动，等待已有实验与构建释放共享锁。目前仍无有效配对性能结果。

2026-10-29 14:28 +08:00 复核：恢复轮已退出，`after-memory/results.json complete=false`。A0、候选均完成全部输入和输出检查，A1完成到Code2K保留请求；三者均为6297槽，READY空闲显存分别1318、1318、1326 MiB，均过1024 MiB准入。候选有唯一的实际融合分派见证。编译启动监视器捕获14:27:20.5548913的`clang-offload-bundler.exe`（PID22820，parent21288），`compiler-guard-results.json passed=false`；A1的Prose2K保留请求未执行，不进入独立轮。原失败、协议和事件均保留。

诊断计时：Code2K的A0/候选/A1为4470.0/4463.1/4473.9 ms，表观吞吐变化约+0.20%，本就低于0.3%噪声线；Prose2K只有A0/候选的4813.3/4924.0 ms。整轮有编译事件，以上只用于解释下一步取舍，不能作为干净配对性能结论，也没有证明稳定35 tok/s。此轮未再次创建QSA重跑队列。
