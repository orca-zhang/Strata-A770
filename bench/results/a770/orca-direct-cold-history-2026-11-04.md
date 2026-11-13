# Orca Direct：冷历史与复用 scratch 正确性检查

2026-11-04，Windows 10 22H2，Intel Arc A770 16 GB，驱动 `32.0.101.9034`，oneAPI 2025.3。

本轮专门追查[此前保留的 8K 异常](orca-direct-current-contexts-2026-11-04.md)：
新进程先成功运行两次 2K，再运行 8K 时，生成的 token ID 从首个开始为 0，草稿为 0/0。
**这是生成内容错误，不是生成了 0 个 token。** 当时裸 Direct 失败，L0 和诊断 Direct 匹配，合计 8/9；
后续成功重试仍不足以确定原因。本轮不声称修复、性能收益、PPL 等价或 Linux GPU 正确性。

## 无探针冷历史

五个全新进程均直接执行 **Code2K → Code2K → Long8K**，没有 Short 预热或前缀复用。
旧 Direct 复用原失败时的 EXE、配置和数学 pack，仅作为历史正确性对照。
当前开／关组复用同一依赖合并版 EXE，以 `STRATA_ORCA_DG2_DEPENDENCY_ONLY=1/0` 切换；
当前 L0 与当前 Direct 共用 M16 数学 pack。旧／新版本之间不作单变量因果或性能比较。
没有内层计时、logits 回读、状态 hash 或新增诊断 GPU 工作。

| 进程 | 2K 第一次 | 2K 第二次 | 8K 第一次 | 8K 首四 token | 8K 草稿采纳／提供 |
| --- | --- | --- | --- | --- | ---: |
| 0: 旧 Direct | 匹配 | 匹配 | 匹配 | [760, 3766, 1414, 17855] | 32/44 |
| 1: 当前 Direct 开 | 匹配 | 匹配 | 匹配 | [760, 3766, 1414, 17855] | 32/44 |
| 2: 当前 Direct 关 | 匹配 | 匹配 | 匹配 | [760, 3766, 1414, 17855] | 32/44 |
| 3: 当前 Direct 开 | 匹配 | 匹配 | 匹配 | [760, 3766, 1414, 17855] | 32/44 |
| 4: 当前 L0 | 匹配 | 匹配 | 匹配 | [760, 3766, 1414, 17855] | 32/44 |

本轮 **15/15** 请求完整匹配 tokens、生成数量、草稿数量、结束条件和复用状态。
2K／8K 参考首四 token 分别为 `[760, 3766, 1970, 41643]`／`[760, 3766, 1414, 17855]`，
参考草稿均为 38/44／32/44。保存全部原始请求和输出，未挑选成功样本。

## 每半层 NaN scratch 检查

另开 L0、当前 Direct 两个进程，各跑同样三次请求，使用已有
`STRATA_PREFILL_FINITE_ASYNC=poison`：每个 attention／MoE 半层开始前，将两者共用的
scratch region 填为 `0xff`，使 FP16／BF16／FP32 未写入的浮点内容成为 NaN。
该区域与 embedding、residual、norm 输出、GEMM workspace、权重及 session／循环状态分开；
每半层结束后扫描全行 residual 的非有限值。此检查针对复用 scratch 中的潜在未写先读，
不覆盖所有内存、整数索引或所有竞态，也不能把通过视为排除了原故障。

同时启用已有 `STRATA_VERIFY_ZERO_DIAG=1`，仅在一轮输出全部为 token 0 **之后**回读 logits 和 residual。
它并非默认运行开销或本轮新增功能。有限值扫描与 NaN 填充会改变 GPU 工作和调度，
前两次 2K 也受到诊断影响，因此不等同于原故障的无探针前史。

| 路径 | 完整匹配 | 2K／2K／8K 非有限 residual 半层数 | 首次全零诊断 |
| --- | ---: | --- | --- |
| 当前 L0 | 3/3 | 0/0/0 | 未触发 |
| 当前 Direct 开 | 3/3 | 0/0/0 | 未触发 |

诊断共 **6/6** 请求完整匹配。每条请求检查 48 层 × 2 半层；
共 576 个半层扫描点。扫描统计 residual 的非有限值，不直接检查所有中间 tensor。

## 结论与复查

本轮只增加了有界冷历史和 scratch 检查的证据；**原 Direct 8K 间歇异常保持未解决**。
未更改运行时、数学模块、默认配置或部署 EXE；Orca L0 仍为推荐路径。
不以这些请求耗时作新的性能数据，也不从有限成功重试计算故障率。
之后若再次出现全零输出，应先保留首个失败的 logits／residual 诊断，再缩小到前一正常阶段，
避免用插桩后重试成功代替定位。

固定实际 5,771 专家槽、reserve 1,536 MiB、read256、decay0.95、MTP4、
step／last min-p0.5、cumulative0、suffix0、cost-window0、prompt-cache0。
当前 Direct 使用 BCS1，旧对照沿用原配置；所有完整配置和数学 pack 哈希均保留。
同条件复查需要相同硬件、驱动、模型资产、初始 profile、EXE、数学 pack、参数和请求历史。
归档包含原失败与本轮原始协议、日志、脚本、配置、源码及 SHA-256，不分发模型和可执行文件。

```powershell
python -X utf8 build-orca-slm-async-20261104/cold_history_check.py r1 --verify
python -X utf8 build-orca-slm-async-20261104/scratch_poison_check.py r1 --verify
python -X utf8 build-orca-slm-async-20261104/publish_cold_history.py --check
```

重新采集冷历史使用未存在的 r2 等编号。scratch runner 固定核验 r1 冷历史作为依赖；
成功与失败目录都保留，不能覆盖。

精确 JSON (`direct-cold-history-a770-windows-2026-11-04.json`, raw capture retained locally) ·
原始证据 (`direct-cold-history-a770-windows-2026-11-04.json.gz`, raw capture retained locally)
