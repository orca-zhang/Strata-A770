# MTP 零草稿轮次摘要修复（2026-10-24）

CLI 首个验证窗口就达到输出上限或 EOS 时，验证轮数为 1，草稿轮数为 0。
原摘要以验证轮数控制打印，却除以草稿轮数，显示 `-nan(ind) ms/round drafting (0 rounds)`。
SYCL 和 CUDA/HIP 共用入口各改一行：没有草稿时显示 `0.000`，保留轮数、prompt 耗时和 VRAM 信息；
有草稿时使用原计算。修改仅在解码后的摘要，不改变生成、采样、默认开关或服务端路径。

- [x] 冻结已有源码与依赖；复用上轮干净候选作为基线，仅编译一个新 SYCL 入口。头文件、MTP、sampler、verifier 与依赖库相同。
- [x] 复用已有入口 CPU 检查：两种后端共 216 项通过，覆盖位置 0/7、禁用推测解码、输出上限及 EOS。没有新增测试框架。
- [x] 9 次 A770/SYCL CLI 调用通过：基线复现零轮次 NaN，并检查正常多轮摘要；候选覆盖阈值 0/0.50 的上限 1/6、首 token EOS，以及 0.50 的固定种子采样。
- [x] 候选四次零草稿轮次均显示 `0.000`，三次有草稿的均值有限且非负，完整 stdout 不含 NaN/Inf；全部输出逐 token 等于同参数的上一轮参考。
- [x] 请求缓存参数为 5500 MiB，实际 CLI 缓存为 5771 slots / 7.74 GiB；Q8 KV、固定专家计算配置，关闭自适应换入与 PCIe 专家分流。
- [x] 原始输出、源码差异、构建命令和哈希归档；生产 EXE 与配置哈希未变，上一轮证据未改写。
- [ ] CUDA/HIP 完整构建与实机运行、CLI 多 token 原生 prefill：本轮不验收。

没有测量或宣称性能提升。零轮次的 `0.000` 表示未执行草稿，不是草稿耗时测量。
本轮开启 trace，日志中的时间只用于回归溯源；整模型采用门槛保持 1%，算子正收益可保留。
[results.json](results.json) 包含实际参数与摘要，[mtp-summary.patch](mtp-summary.patch) 仅包含两行源码修正。
参考输出、参数与基线 EXE 哈希来自上一轮 [CLI 入口报告](../cli-decode-entry-2026-10-24/README.md)，原始结果副本为 [reference.json](reference.json)。

离线审计（不使用 GPU）：

```powershell
python -X utf8 bench/results/a770/mtp-summary-2026-10-24/check_results.py
```

对保留的冻结构建重新执行回归，每次指定新结果目录，使用共享锁串行执行：

```powershell
python -X utf8 build-sycl-deepinfer-20261023/check_bf16_arena.py build-sycl-mtp-summary-20261024/check.py build-sycl-mtp-summary-20261024/checks-repeat
```

独立候选为 `build-sycl-mtp-summary-20261024/candidate/strata.exe`，基线为
`build-sycl-cli-entry-20261024/candidate/strata.exe`。归档的 `run.py` 和 `check.py` 是实际脚本副本，路径按私有构建目录解析。
生产 EXE 和配置没有替换；本轮不重复宣称上一轮的服务端、长输入或其他验收范围。
