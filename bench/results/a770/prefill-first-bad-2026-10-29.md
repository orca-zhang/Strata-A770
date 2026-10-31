# A770：FP16 候选的专家阶段异常定位

2026-10-29，A770 16GB / PCIe 3.0 / Windows / oneAPI 2025.3。独立诊断已完成，在第二个引擎复现真实失败。最早已覆盖的异常阶段是第27层注意力输出投影，之后传播到GU/H/Down；不能归因于SwiGLU半精度转换。没有新增可采用的性能结果。

`build-sycl-a770-value-fp16-after-desktop-20261029` 的 B2 最后一条 Prose 请求失败：第 24 层 Down 完整扫描发现 614400 个非有限值，失败后的 FP16 H 扫描发现 153600 个非有限值。该 H 记录发生在 Down 之后，无法证明异常在 Down 之前已存在。H 是 FP16，不能据此得出 BF16 路径有问题。第一轮 Prose 的 +11.42% 不能绕过第二轮失败、完整质量和稳定性验收。

新入口 `build-sycl-a770-prefill-first-bad-20261029/run.py` 仅做诊断，使用新的 prefill 和 GEMM 对象。保留原 XMX expert chain、batch/finish 参数、矩阵运算和 SwiGLU 表达式，在 chain 内部以及原 fallback 中分别检查：

1. GU 计算后、SwiGLU 之前的完整 FP32 输出；
2. SwiGLU 后、Down 之前的完整 FP16 H；
3. Down 后的完整 FP32 输出。

每个 chunk/layer/expert 有独立的三个计数位置。原 49 阶段全数组计数也保留。未提交的位置用哨兵区分，不能当成已检查且为零；计数在正常退出和 C++ 异常展开时均输出。诊断只增加有序读取和计数，计数 kernel 会改变调度，其耗时不参与性能结论。即使没有复现，也不表示原问题已修复。

运行先验证实际 helper 的 FP32/FP16/BF16 共 36 项计数检查，以及哨兵、专家索引和真实异常展开的打印检查。随后最多启动两个独立 FP16 引擎，各自完整重放原 Short16、交错的八条 Code/Prose 2K/full128 请求；保留输出、accepted/offered、错误和实际退出码。成功请求必须匹配原 B1，不能只运行最后一条失败请求。固定实际 6297 槽、READY 空闲显存至少 1024 MiB、实验 min-p 0.5，生产配置不改。记录实际子进程 DLL，并保留编译启动监视。

CPU 自检已通过两对象源码可逆差异、原矩阵运算保留、检查点顺序、FP16 类型、索引边界、作用域清理，以及监督进程实际非零退出码检查。

20:25:10 +08:00，实际fixture exit0：`PASS stage_trace checks=36 FP32/FP16/BF16 full/tail guards and read-only inputs`，以及 `PASS failure trace sentinel coverage,expert indexing and actual exception unwind`。fixture特意注入的非有限值打印不代表模型失败。随后完整重放D1真实子进程 PID25208 / 创建毫秒1793276745534已启动，READY为6297槽、1553MiB空闲，Short16完整输出和accepted/offered匹配原B1，正在第一条Code2K。仍须检查全部历史，不能从fixture通过推断模型稳定。

20:35 +08:00，D1已完整完成Short16及原八条交错2K/full128历史，输出和accepted/offered均匹配原B1，实际exit0。八条完整阶段摘要的bad_stages和bad_experts全部为0。D2已启动并进入第一条Code2K，尚未完成。计数kernel改变了调度；这次尚未复现不能抵消原FP16候选的真实失败，也不能将诊断运行当作性能样本。

20:38:25 +08:00，worker实际exit0，`state.json complete=true,reproduced=true`。这表示诊断完成并捕获预期失败，**不是模型成功**：D2真实模型exit3221225477（0xc0000005），首异常为 `ERR down retry scan rejected layer=27 ... gpu_count=409600 cpu_nonfinite=409600 reason=overflow`。D2完成Short及前四条长请求，失败在第五条长请求Code2K。READY仍为6297槽、1724MiB空闲；compiler guard完整通过、events=[]。原全部失败保留。

| 第27层已覆盖阶段 | 非有限元素 |
| --- | ---: |
| projection_attn | 40960 |
| residual_attn | 163840 |
| ffn_mixed | 40960 |
| expert_gather_half | 409600 |
| 各失败专家GU（expert0–9） | 每个20480 |
| 各失败专家H，在Down之前（expert0–9） | 每个10240 |
| 各失败专家Down（expert0–9） | 每个40960 |

D2失败摘要为stage_samples1262、bad_stages20、expert_samples30435、bad_experts30、unwinding1。H在Down之前确已异常，但GU和注意力输出投影更早出现非有限值，所以仅重算Down或钳位SwiGLU不能解决这次来源。此前49阶段检查没有覆盖QSA注意力原始输出及gate后的half输入；未执行格仍为哨兵，不能称这些输入已证明有限。当前不能区分QSA注意力、门控/转换和输出投影中的具体起点，也不能从通用reason=overflow推断一定是FP16数值溢出。

新 `build-sycl-a770-qsa-first-bad-20261029/run.py` 复用原完整历史和异常展开诊断，只增加六个QSA检查点：K/V/Q投影FP32、规范化后的Q、attention的FP32输出、gate后的FP16输入。55阶段与原专家三格使用不重叠索引；保留原全部矩阵运算、注意力实现、量化和原XMX chain。CPU自检通过原运算保留、变换可逆、fixture索引适配及监督进程实际exit7检查。新入口尚未编译或运行GPU；不是修复版本或性能样本。

20:43:47 +08:00，supervisor PID2608 / 创建毫秒1793277827094，等待MTP复测worker12936 / 1793276771763后再拿共享锁，不在现有测速期间编译。

```powershell
python -X utf8 build-sycl-a770-qsa-first-bad-20261029/run.py --self-check
python -X utf8 build-sycl-a770-qsa-first-bad-20261029/supervise.py --self-check
python -X utf8 bench/results/a770/check_prefill_first_bad_20261029.py
```

独立离线检查已exit0：从原协议重新构造D1九次和D2五次完成输出，核对全部草稿计数、真实失败退出、完整编译监视、八次/四次干净摘要及失败20/30个阶段记录。该检查不启动GPU或编译器，也不读取或重新哈希权重/EXE，不能代替获锁的计算输入摘要核查。

19:34:28 +08:00 原生查询确认 supervisor PID 3988 / 创建时间 1793273668279 ms，worker PID 22780 / 1793273668472 ms。worker 精确等待现有质量接续 PID 20864 / 1793271973212 ms 和 GR/runtime 确认 PID 11824 / 1793272287025 ms，再获取共享 GPU/编译锁。大型计算输入、对象和 EXE 的读取及摘要验证均位于锁内。未终止桌面程序或其他任务。

```powershell
python -X utf8 build-sycl-a770-prefill-first-bad-20261029/run.py --self-check
python -X utf8 build-sycl-a770-prefill-first-bad-20261029/supervise.py --self-check
```

本诊断不改变采用门槛：同场景两轮至少 1% 收益、相邻基线耗时漂移不超过 3%、吞吐绝对相对变化低于 0.3% 视为噪音；完整输出、质量及稳定性仍需单独通过。35 tok/s 目标未达到。
