# 输出尾部验证窗口：FP16 KV 补测（2026-10-25）

服务端与 CLI 在选择 MTP/suffix 窗口后，将验证长度限制为剩余输出数，省去无法返回的额外验证行。
本轮复用此前两处长度限制及相同 A/B EXE，补齐生产 KV 格式的验收；[此前 Q8 报告](../spec-output-limit-2026-10-25/README.md) 保持原样。
FP16 生成自己的参考输出，收益与 Q8 分开报告。生产 EXE 和配置未写入。

## Checklist

- [x] 开跑前冻结源码、两臂 EXE、生产配置及脚本哈希；同一工具链、依赖和参数，只切换 KV 格式。
- [x] 72次服务端请求通过：greedy、固定种子采样与惩罚、阈值0/0.50、1/2/3/4/6输出上限、16-token参考、取消恢复和 parked/live 会话恢复。
- [x] 两次EOS、六次CLI、两次上下文恰好装满检查通过，共82次调用；上下文用例原基线也通过。
- [x] 对应输出逐token一致；候选每个验证窗口均不超过剩余输出数。
- [x] 保留初筛与初次确认的全部样本；初次确认的6-token主测点两组均因基线漂移>3%而无效，未计为通过。
- [x] [补测前固定规则](retry-preregistered.json)：只补跑一次完整 A/B/A/B/A，不更改参数或门槛。补测确认通过：True。
- [x] 原始日志、无效配对、实际缓存、参数、接受/提议数、构建及源码哈希落盘，离线脚本重算中位数与收益。
- [ ] 长输出、四场景完整性能矩阵、非 eager 图模式和 CUDA/HIP 对应路径另测。

## 测量范围和结果

A770 16GB、Xeon E5-2696 v4、Windows、DPC++ 2025.3.2；Short提示23token，**FP16 KV**。
请求缓存4000MiB，实际固定 **4191 slots / 5760 MiB**；`--spec 4 --spec-min-p 0.5`，suffix lookup=3，内部验证容量6、MTP cap4。
使用原 Q8 实验冻结配置（`--short-read 256`），保持 A/B 比较条件；当前生产 short-read=1024，未改写。
`STRATA_IQ_MT_MIN=1`、coupled sampling、`--adapt-swaps 0 --pcie-frac 0`；四条 DeepInfer 实验路径和 widening 关闭。

GPU独占锁串行运行。性能阶段关闭trace、状态哈希和性能分析器。
每进程预热两次16-token请求，随后4/6/16输出上限各保留三次；共15个计时进程、30次预热、135次保留请求。
每测点取三次decode毫秒值的中位数；候选对比相邻两基线的几何均值。基线漂移>3%的配对保留但排除判定。

| Short 输出上限 | 初筛 decode 吞吐提升 | 初次确认（仅有效配对） | 补充确认（仅有效配对） |
|---|---:|---:|---:|
| 4 token | +8.07%～+8.37%（2组） | 无有效配对 | +7.71%～+7.93%（2组） |
| 6 token | +9.45%～+10.24%（2组） | 无有效配对 | +11.22%～+11.65%（2组） |
| 16 token | +7.49%～+8.25%（2组） | +6.59%～+8.60%（2组） | +6.87%～+7.65%（2组） |

14/18组decode配对有效；全部配对最大绝对基线漂移 **3.5824%**。
初次确认中间基线的4/6-token耗时升高，四组配对漂移超限（最大3.5824%）；两组6-token漂移分别为+3.2454%和−3.0124%，其正收益也未计入通过。
补充确认使用全新五进程，没有挑选单个请求替换。
计时完成并保存结果后，脚本打印摘要时误读不存在的 `screen_passed` 字段，报 `KeyError`。
[错误记录](retry-summary-error.txt) 和实跑脚本保持原样；[补做检查](postcheck.py) 重新核对全部落盘数据及生产/源码保护哈希，通过后才归档，没有重新采样。
`confirm_retry-replay.py` 只修复摘要打印，已检查语法，没有替换本轮样本。
补充确认的6-token进程中位decode耗时：基线 **444.0～449.3ms**，候选 **398.8～402.7ms**。

补充确认达到预定1%门槛，支持在已测 Short/FP16 短输出范围保留源码默认限制。
结论限于 **Short/FP16短输出decode**；prefill、TTFT、长输出与四场景整模型性能仍待验证，收益不与Q8相加。

[功能数据](checks/results.json)、[首次计时和无效配对](measure/results.json)、[补充确认](confirm-retry/results.json)、[汇总](summary.json)。
本轮无编译；独立候选为 `build-sycl-window-limit-20261024/candidate/strata.exe`，精确EXE哈希见 [frozen.json](frozen.json)。
[原始构建命令](reused-build.json)、[冻结依赖](reused-dependencies.json) 和 [源码差异](window-limit.patch) 保留。
同一源码此前的1,302项CPU回归见 [原日志](../spec-output-limit-2026-10-25/cpu-final.txt)，本轮未重复编译。

## 复核和复测

离线复核，无需模型或GPU：

```powershell
python -X utf8 bench/results/a770/spec-output-limit-fp16-2026-10-25/check_results.py
```

本机固定EXE复测使用共享锁，每次换新输出目录；性能脚本使用本轮已完成的FP16正确性参考：

```powershell
python -X utf8 build-sycl-deepinfer-20261023/check_bf16_arena.py build-sycl-window-limit-fp16-20261025/check.py build-sycl-window-limit-fp16-20261025/checks-repeat
python -X utf8 build-sycl-deepinfer-20261023/check_bf16_arena.py build-sycl-window-limit-fp16-20261025/measure.py build-sycl-window-limit-fp16-20261025/measure-repeat
```
