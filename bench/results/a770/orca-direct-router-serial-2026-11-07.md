# 保留早量化，仅隔离 Router / Quant 并发

2026-11-07，Windows 10，Arc A770 16GB，驱动 32.0.101.9034，Xeon E5-2696 v4，oneAPI 2025.3.2。
Orca L0 为推荐后端，Direct 仍为 WIP。

本轮两版 Direct 使用同一 full-current 冻结源码、Host 库、驻留修复对象和 HC128 原生数学包。
parallel 保留 Router/Quant 的 independent 包装；serial 仅将这个包装改为顺序 lambda。
早量化的位置、内核调用和参数保持一致，其他并发分支保留。L0 也使用同一 HC128 包。
这不是关闭早量化，也不是原生产 SYCL 与 Orca L0 的新对照。

固定 Short 23 输入 tokens、自然 EOS 输出 70 tokens；spec=4，step/last min-p=0.5，cumulative=0，
5771 专家槽、显存预留 1536 MiB，U4/min16/M16。每臂新进程，15 次请求，预热 10 次、保留后 5 次。
八臂顺序预先固定，120/120 完整协议、草稿采纳/提供、结束原因和 reuse/resume 一致。
原始 T/TB/DONE 单独回读；无 profiler、GPU 时间戳或内部计时开关。
净 decode 使用 generated/decode_ms，排除准备、初始化和 prefill；TTFT 与整请求单列。

| 臂 | 净 decode tok/s | TTFT ms | 整请求 ms |
| --- | ---: | ---: | ---: |
| 0-l0 | 51.286 | 756.15 | 2080.20 |
| 1-parallel | 49.705 | 766.73 | 2136.85 |
| 2-serial | 49.026 | 778.46 | 2170.15 |
| 3-parallel | 49.776 | 771.25 | 2126.10 |
| 4-serial | 49.551 | 769.91 | 2141.12 |
| 5-parallel | 49.815 | 763.10 | 2122.44 |
| 6-serial | 49.593 | 760.55 | 2131.76 |
| 7-l0 | 51.706 | 756.16 | 2070.05 |

每个三臂窗口使用两端控制中位数的几何均值。下表为 serial 相对 parallel：
decode 正值表示更快，TTFT/整请求正值表示更慢。

| 顺序 | decode 变化 | TTFT 耗时变化 | 整请求耗时变化 | 控制 decode 漂移 |
| --- | ---: | ---: | ---: | ---: |
| parallel → serial → parallel | -1.436% | +1.232% | +1.815% | +0.142% |
| serial → parallel → serial | -0.489% | +0.277% | +0.660% | +0.085% |

外层 L0 首尾漂移：decode +0.820%，
TTFT +0.002%，整请求 -0.488%。
预设筛选通过：**False**；各项门槛及每臂相对 L0 的数值见 JSON。
两个顺序中 serial 的净 decode 均更慢，本轮保留这个并发分支。单独将它顺序化没有缩小 Direct 的性能差距。
外层 L0 跨越整轮，只作参考，不能代替每个 Direct 窗口的局部对照。
保留所有请求、反向结果和漂移；没有补跑筛选样本。控制漂移不等于置信区间。

最初 build-r1 在链接输入校验阶段停止，未编译、未运行 GPU；原脚本和失败记录保留。
build-r2 按编译环境查找真实 Windows SDK 链接库，并排除输出选项。
首次离线发布器误按 9 字段解析 DONE，冻结引擎实际输出 16 字段；修正后逐项核对扩展计数及完整 23-token prompt 读取。
原发布脚本和失败记录保留，未重跑 GPU。
模型大权重复核已有 SHA256 与文件身份缓存，没有重复全量哈希。

本轮未改生产默认、产品 EXE 或 README 主成绩；没有新 PPL、长上下文或 Linux GPU 结果。
Short 筛选不能关闭 Direct 历史 8K/nonfinite 和驻留问题。
同条件复测需保留硬件、驱动、工具链、冻结源码/库/原生包、模型资产、参数和完整请求历史，使用新输出目录。
归档仅含文本证据，不分发模型、二进制或继承环境。

```powershell
python -X utf8 build-orca-router-serial-20261107/experiment.py selfcheck
python -X utf8 build-orca-router-serial-20261107/report.py check
```

[精确 JSON](../../../sycl/orca/direct-router-serial-a770-windows-2026-11-07.json) · 文本证据 (`direct-router-serial-a770-windows-2026-11-07.json.gz`, raw capture retained locally)
