# A770：五项未达单项门槛的小优化叠加

把此前未达到整模型独立 1% 收益门槛的兼容改动叠加为一个候选版本，直接测组合效果。硬件沿用 A770 16GB、PCIe 3.0、Windows、oneAPI 2025.3；结果不由历史百分比相加或相乘得到。

**已按用户“感觉可采用”的意见提供独立可选配置 `strata-iq2_xs-small-stack.json`：两次冷固定目标评分与原版精确一致，实际服务器引擎输出及退出检查通过。性能与显存原始判定不变：未确认同一场景两轮至少 1% 的收益，采样显存余量也未通过。**

| 场景，2048 输入 / 完整 128 输出 | 首轮配对吞吐变化 | 独立第二轮变化 | 第二轮候选中位 tok/s | 两轮基线耗时漂移 |
| --- | ---: | ---: | ---: | ---: |
| Code2K | +1.1091% | +0.1776%，噪声范围 | 29.3497 | −2.3256% / −0.2081% |
| Prose2K | +0.5606% | −0.8197% | 26.8987 | −0.9648% / −0.2751% |

Code 首轮触发独立确认，但第二轮没有重复；Prose 两轮均未达到 1%。`confirmed_scenes=[]`、`passes_confirmation=false`、`speed_target35=false`。这些是当次容量条件下的观测结果，不能宣布已经证明组合在充足显存下无效。

六个引擎均正常 exit0，48 条长请求与六条 Short16 的完整输出共 6240 tokens 匹配冻结参考。五项候选启用见证、每进程八组有序 48 层扫描、原始协议及来源核对通过；编译监视器正常结束、events=[]。收集器末尾完整复核通过。

READY 空闲显存为 1318–1481MiB，完整请求期间却均未满足 1024MiB 采样门槛：A1/B1/A2/A3/B2/A4 最低分别为 778.719 / 796.164 / 524.875 / 649.301 / 855.152 / 886.914MiB，`sampled_memory_passed=false`。没有据这些采样认定已发生换页，也不能排除容量压力的干扰。当时未继续执行冷/热固定目标质量；用户接受后补做的冷评分见下文，完整贪心输出不替代通用质量验收。

| 叠加项 | 旧实验结果 | 本次接入 |
| --- | --- | --- |
| compact IQ4_XS GEMV | 独立 Code +0.041%、Prose +0.158% | 复用九个已确认分派的原生对象 |
| GU/Q8 小窗口融合 | Code +0.192%、Prose +0.460% | 开启原有 chunk2 开关 |
| 共享专家 gate/up 合并 | V4 与 FP16 均未独立达到 1% | 复用已确认格式/列数的共享专家对象 |
| GDN Q8 尾段融合 | Code +0.194%、Prose −0.882% | 复用 156 组 parity、24 次 graph 检查的尾段对象 |
| QSA gate/Q8 尾段融合 | 独立 Code +0.108%、Prose +0.138% | 复用 288 组 eager、288 次 graph 检查的尾段对象 |

仅重编同时包含两种尾段分派的 verifier。两项原始变换各自检查可逆性；链接对象差异检查限定为 verifier 替换及上述已冻结对象。标量 SG16 与 compact 是相同分派的替代实现，不能当作两项重复叠加。已确认约 3% 的 GR/runtime 改动不混入此次增量；精度、KV、缓存及草稿策略保持原协议。

入口为 [tools/bench_small_stack.py](../../../tools/bench_small_stack.py)，产物与原始数据保存在 `build-sycl-a770-small-stack-v3-20261030/`。执行与编译使用原共享 GPU 锁。

测量沿用原始 Code/Prose 各 2048 输入、完整 128 输出、6297 专家槽、tail64、min-p0.5、物理 V4、默认 runtime。每个新进程先 Short16，然后两场景各一次预热及三轮交错保留请求，完整输出必须匹配冻结参考。A1/B1/A2 任一场景达到 1%、相邻基线耗时漂移不超过 3%，才进入独立 A3/B2/A4；只有同一场景两轮通过才确认。绝对吞吐变化小于 0.3% 按噪声解释。保留所有请求、退出码、48 层扫描、五项启用见证、编译事件及显存/频率采样；采样空闲显存要求至少 1024MiB。

性能阶段只测试独立候选，未修改默认生产配置；用户接受后的独立配置验收见下文。

```powershell
python -X utf8 tools/bench_small_stack.py --self-check
python -X utf8 tools/bench_small_stack.py
python -X utf8 tools/bench_small_stack.py --check
```

首次预检在旧清单引用的工作区 `sycl/src/kernels/cuda/iq_kernels.dp.cpp` 摘要处停止，没有编译、链接或模型请求；记录保留于 `build-sycl-a770-small-stack-20261030/preflight-failure.json`。该源码已被后续实验修改，本次仅使用它以前编译出的冻结对象，不重新编译它。新目录把未参与编译的工作区源码变更列为历史来源差异；复用的对象、库及实际使用的冻结源码/头文件仍逐项严格核对，两个来源集合不混同。

V2 已通过实际冻结输入检查并编译 verifier。外层与批处理内层同时打开 `link.log` 导致 Windows 文件占用，未产出模型 EXE；原日志、未完成 build.json 与原脚本保留。V3 使用独立 `link-wrapper.log`，在逐字节核对 verifier 源码后直接复用 V2 对象，不重编。

V3 已编译、链接成功，并实际开始 A1/B1/A2。A1、B1 均完成 Short16 及八条长请求，完整输出匹配、正常 exit0；B1 五项启用见证均出现。A2 只完成 Code 预热，随后 Prose prefill 的 layer35 Down 被扫描器拒绝：GPU/CPU 均为 204800 个非有限值，`reason=overflow`，引擎 exit3221225477。编译监视器通过、没有事件。该组 `complete=false`，没有末端基线，不计算任何提速，也不把错误归因于叠加候选。

一次全新复测复用 V3 的两个精确 EXE、参数和完整原始协议，结果单独写 `build-sycl-a770-small-stack-retry-20261030/`；不重编、不拼接 V3 请求、不放宽非有限值或显存检查。入口：

```powershell
python -X utf8 tools/bench_small_stack_retry.py --self-check
python -X utf8 tools/bench_small_stack_retry.py
python -X utf8 tools/bench_small_stack_retry.py --check
```

当前状态：复测完整结束，收集器完整终态复核通过；另从原始协议和逐请求 decode_ms 独立重算 6240 tokens、六个退出码与两轮中位数/配对百分比，CPU 检查通过。重复的完整 `--check` 在尚未持锁、未执行审计时按精确进程身份结束，避免再次占用队列，不声明这条重复命令通过。原生产配置未修改。

## 用户接受后的独立配置

用户接受本次已测版本，解除本次采用的 1% 性能和 1024MiB 采样余量前置条件。原始 `passes_confirmation=false`、`sampled_memory_passed=false` 及全部样本不变，不回填为通过，也不新增提速声明。

`tools/adopt_small_stack.py` 复用现成 EXE 和完整测量参数，两个新候选进程各评分原九场景/758固定目标，要求必需概率有限、评分精确等于冻结原版并彼此重复。再通过实际 `server.StrataEngine` 跑 Short16、Code2K128、Prose2K128，核对完整输出与正常退出。全程使用共享锁和编译启动监视器，不重跑性能、不重编、不加入 GR/runtime 等其他候选。

已生成独立 [strata-iq2_xs-small-stack.json](../../../strata-iq2_xs-small-stack.json)。配置使用实际测量的 min-p0.5、cache6000、V4、graph、batchOFF 和五项开启标志；默认配置原有 min-p0.8/auto cache 等参数不混入这次验证。`STRATA_SYCL_BIN` 同步指向候选 EXE。此前六进程完整历史输出检查继续作为该历史的输出证据；本次新增的是两次冷固定目标评分和实际服务器启动，不声称补齐未执行的匹配热历史758评分。

采用检查完成，记录位于 `build-sycl-a770-small-stack-adopt-20261030/state.json`：`complete=true`、`adopted=true`。B1/B2 各九场景、758 固定目标评分精确匹配冻结原版且彼此重复，必需评分列均有限；两个进程正常 exit0。实际 `server.StrataEngine` 的 Short16、Code2K128、Prose2K128 共272个输出 token 全部匹配参考，正常 exit0。编译启动监视器完整结束、零事件；最终 `--check` 通过，包含默认配置未变的摘要检查。没有留下运行中的服务器。

```powershell
python -X utf8 tools/adopt_small_stack.py --check
python -m serve.server --engine strata --config strata-iq2_xs-small-stack.json --host 127.0.0.1 --port 8080
```
