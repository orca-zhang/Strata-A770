# Orca 长输入 TTFT 排查：Windows A770，2026-10-31

## 尾部没有等凑满

原整版本对照中，Code2K／Prose2K／Long8K 的 TTFT 增长 45.96%～51.44%。本轮先排查批量提交：部署版 `ProductBatch` 没有定时器、后台等批或凑满门槛，普通队列操作和等待都会刷新待提交项；单项直接计算。FP16 专家链绕过此打包器，依次提交 gate/up、SwiGLU 和 down。

`STRATA_PREFILL_XMX_EXPERT_BATCH=1` 控制每八个专家等待一次 GPU，最后不足八个也立即等待完成。关闭它会变为每个专家都做 Host 等待。`STRATA_PREFILL_CACHE_TAIL64` 选择专家缓存热度统计的尾部窗口，与任务等批无关。

## 同一 EXE 的提交策略筛查

A770 16 GB、Xeon E5-2696 v4、256 GiB DDR3、Windows；同一 Orca EXE 和内核、8192 context／prefill、MTP4、`spec-min-p=0.5`。固定实际 5771 专家槽，关闭缓存适应和 PCIe 分担。四个全新进程依次运行默认、每专家等待、通用 GEMM、默认。各进程先做 Short16，再做 Code2K16、Long8K16；每个场景每臂一个样本，没有丢弃的批量预热。

| 策略 | Code2K TTFT | 相对默认耗时 | Long8K TTFT | 相对默认耗时 |
| --- | ---: | ---: | ---: | ---: |
| 默认，两侧耗时几何平均 | 32.678 秒 | 基线 | 83.938 秒 | 基线 |
| 关闭专家批量等待，每专家 Host wait | 34.282 秒 | +4.91% | 85.793 秒 | +2.21% |
| 绕过专家链，通用异步 GEMM | 33.183 秒 | +1.54% | 84.115 秒 | +0.21% |

前后默认基线漂移为 +0.377%／+0.225%。八个批量请求的完整 16-token 输出及草稿接受／提议数一致，所有进程正常退出，冻结文件和内核哈希未变。样本较少，以上用于筛查；没有确认这两个候选提速，不据此改默认。

这些条件与原整版本表的自动缓存、0.8 门槛不同，表中百分比只表示本轮提交策略差异。不能用它覆盖原 SYCL → Orca 的 45.96%～51.44% 回退。

[逐请求结果、输出、环境、CLI 与哈希](../../../sycl/orca/prefill-policy-a770-windows-2026-10-31.json)。复现：

```powershell
python -X utf8 tools/bench_orca_prefill.py --self-check
python -X utf8 tools/bench_orca_prefill.py --config <冻结的0.5配置.json> --out <新目录>
```

## 阶段诊断的结论和边界

单独打开 `STRATA_PREFILL_SYNC=1`，每个阶段边界排空 GPU。Native Code2K 外层等待累计 28.179 秒，其中专家链标签 12.125 秒、GDN 初始投影 5.911 秒、GDN out 1.739 秒、QSA 投影 2.242 秒、反量化 2.599 秒、等待复制 1.524 秒。专家链标签包含整条 gate/up、SwiGLU、down，不能把它当成 gate/up 单独耗时；排空还破坏了原流水线，不能拿诊断 wall time 当正式性能。

旧 SYCL 部分 GEMM 在函数内部等待，Native 将异步工作留到后续外层排空。外层计时漏掉前者的内部等待，因此不能直接相减两后端各阶段的等待值，也不能把差额称为纯 CPU 时间。两组诊断都保留：[Native](../../../sycl/orca/prefill-phase-native-a770-windows-2026-10-31.json)、[SYCL](../../../sycl/orca/prefill-phase-sycl-a770-windows-2026-10-31.json)。

诊断中 Native 2K 跨 48 层执行了 18340 次非空专家链，旧 SYCL 为 17801 次；输出路径不同，不能视为完全相同的专家工作量。这个规模支持优先检查小矩阵提交、FP16 专家链与投影效率，尚未证明某一项造成了全部回退。实际生成后的部署源码已经使用专用复制队列；不能只看转换前源码就断言复制挤占计算队列。

整版本切换还包含 FP16 矩阵分块、资源请求、环境和输出轨迹变化。当前 Native 使用已检查的 M4／NSG8、256 GRF 路径；[同源码、两边都采用 M4 的另一次对照](../../../sycl/orca/model-m4-aligned-a770-windows-2026-10-31.json)中，Native 预填充吞吐为 +2.12%～+2.86%。那次使用 2048-token prefill、6297 槽和精确 CPU 模式，不能与原整版本表直接相减，但说明需要把 API、矩阵内核和实际工作量分别控制，不能将整段回退全部归给 L0 调度。

## 优化验证

已完成已有 [FP16 双专家实现](orca-fp16-pair-2026-10-31.md)的补充验证。同一候选 EXE／内核做 OFF／ON／OFF，固定 5771 槽、adapt0／pcie0、MTP4／min-p0.5、8192 context／prefill。每个新进程先做 Short16，再交错执行两轮 Code2K64／Long8K64；输入分别为 2048／8000 tokens。它复用两个反量化槽和现有 GEMM 工作区，两个专家使用各自权重，尾部单个专家走原路径。

以下取每臂两个样本的中位数，OFF 基线取两侧中位耗时的几何平均。prefill、TTFT 和整请求均保留首轮的真实准备成本。

| 场景 | 输入处理 OFF → ON | prefill 吞吐变化 | TTFT OFF → ON | TTFT 耗时变化 | 完整请求耗时变化 |
| --- | ---: | ---: | ---: | ---: | ---: |
| Code2K | 32.942 → 28.171 秒 | +16.93% | 33.001 → 28.230 秒 | −14.46% | −13.53% |
| Long8K | 84.117 → 76.233 秒 | +10.34% | 84.178 → 76.296 秒 | −9.36% | −9.05% |

TTFT 两侧基线漂移仅 +0.076%／+0.028%。12 条请求、768 个输出 token 全部一致，草稿接受／提议数也完全一致；无 prompt 复用，进程正常退出，冻结文件和内核哈希未变。这是一次 A/B/A、每场景两个开启样本，没有置信区间。启动报告的空闲显存为 2228／2271／2251 MiB，实际专家槽完全一致；本轮没有连续显存采样或硬件时间线，较早的独立质量／显存验收另见上述报告。

Code2K 每轮 9159 对加 22 个余项，共 18340 次专家计算；GU／Down 提交数由 36680 降为 18362。Long8K 为 9794 对加 24 个余项，GU／Down 由 39224 降为 19636。这里按日志重建矩阵提交数，不是硬件 trace；未把 SwiGLU、反量化和补齐算作 GEMM。余项仍完整执行，说明减少小矩阵提交有收益，并不需要等待尾部凑满。

### decode 单独计时，排除准备

引擎的 `DONE decode_ms` 在 prompt 处理、借出缓存回填及每请求缓冲准备完成后才开始。模型加载与 prefill 不计入 decode；首次图录制可能发生在第一个 decode 轮次。因此按用户要求，decode 另行丢弃每个场景的第一轮，只使用第二轮，引擎的 Short16 初始化也不作为速度样本。仍计入逐 token 必须执行的验证、草稿、提交和缓存操作。

| 场景 | 预热后 decode OFF → ON | 吞吐变化 |
| --- | ---: | ---: |
| Code2K | 25.02 → 25.22 tok/s | +0.81% |
| Long8K | 21.03 → 20.98 tok/s | −0.24% |

每臂每场景只剩一个预热后 decode 样本，未确认生成提速。上表不能替代充分预热的历史 Short 48.94 tok/s，也不能将 TTFT 的缩短算成 decode 提升。原始记录保持原样；公开结果单独记录追加 decode 分析的脚本散列及取样规则。

[本轮全部请求、输出、计时边界与哈希](../../../sycl/orca/prefill-pair-minp05-a770-windows-2026-10-31.json)。新工具的同 EXE 对照入口：

```powershell
python -X utf8 tools/bench_orca_prefill.py --self-check
python -X utf8 tools/bench_orca_prefill.py --config <已构建双专家内核的0.5配置.json> --pair --repeats 2 --tokens 64 --out <新目录>
```

本轮配置冻结在 `build-orca-native-20261029/profile-fp16-pair-minp05-20261031.json`；工具固定上述缓存、adapt 和 PCIe 参数。较早双专家报告中的 0.8／自动缓存结果保留，不能和这里拼成一次相同条件的实验。

**已经确认同一 Native 候选中的增量改善，尚未证明原 SYCL → Orca 的 45.96%～51.44% 回退全部消除。** Linux GPU 尚未验证。上述测速完成时未修改默认调度／主配置，默认 `spec-min-p=0.5` 保留。后续采用记录如下。

## 默认采用

后续新构建仅改 host 侧默认选择，GPU images 与上述候选完全相同。未设置开关时，仅在 A770 原生专家构建、native XMX 与 expert chain 都开启时默认采用；显式 `STRATA_PREFILL_FP16_PAIR=0` 保留原路径。SYCL／其他设备未设置开关的行为未改。Linux GPU 尚未验证。

本机主配置已切至该构建，显式开启双专家，保留 `spec-min-p=0.5`、自动缓存、adapt32、PCIe fraction 0.16、8192 context／prefill；显存预留由 1536 改为已校准的 3072 MiB。通过实际 Python `StrataEngine` 的 OFF／ON 五场景序列和未设置开关 AUTO Code2K：11 请求、704 个输出 token 全部一致，模块审计通过，进程正常退出，文件和内核哈希未变。1 Hz 物理显存采样最低空闲 **1675.168 MiB**。

自动专家槽分别为 5145／5186／4800；本轮用于功能与部署验收，不作为新增性能证据，也不把缓存容量变化算成双专家收益。未重启长期服务，下次启动采用新配置。完整前后配置、每请求输出、日志与哈希见[默认采用 JSON](../../../sycl/orca/prefill-pair-default-a770-windows-2026-10-31.json)。
