# 真实 HC→Router：逐层交错与分阶段回放

2026-11-07，Windows 10，Arc A770 16GB，驱动 32.0.101.9034，Xeon E5-2696 v4，oneAPI 2025.3.2。
Orca L0 仍为推荐后端，Direct 为 WIP。本轮用于定位，不改变生产默认或 README 主成绩。

从 frozen full-current Direct 的一个 T4 窗口捕获全部 48 层 HC1、Router 投影、TopK、BF16 与 native-Q8 数据及权重。
位置 0；完整 Short 的 70 tokens、草稿 42/58、stop、reuse/resume 与参考一致。
48 份 HC 文件与上轮逐字节相同；新增 Router 文件也属于同一窗口。
96 份有效私有文件合计 791,709,696 字节；含模型权重，公开归档仅存哈希。
这次记录确认实际 43 层使用早量化并发，5 层（6、7、11、22、30）回退。
非并发层的 shared Q8 scratch 在内部被复用，因此文件中该字段使用捕获的 native-Q8 字节作为独立量化回放参考；
没有把它称为实际 shared scratch 捕获。回放另行验证 native quantizer 的逐位一致性。

独立回放保持同一组输入、权重、地址与 8 个 kernel/case：HC norm/down/reduce/up，
以及并发的 BF16/Q8 量化、独立 Q8 量化、Router GEMV，最后 TopK。
所有回放 case 都保留这组三路并发；真实模型 5 个回退层的完整调度未被复现。
每个 case 读取固定 R0，写独立 R1，省略原地跨层递归和周围算子。
逐层交错为 `HC0→Router0→HC1→Router1…`；分阶段为每 48 cases 先全部 HC、再全部 Router。
两种顺序都逐块遍历同样的 48 组真实权重，不把有依赖的真实模型层直接重排。

每个进程录制 4 张图：48/192 case，也就是 384/1536 kernel/list。
固定 L0→Direct→L0 顺序；每臂 4 个预热 work 后保留 12 个固定交错 work，每 cell 3 个样本，全部保留，无补跑筛样本。
每个保留样本完成 3,072 case / 24,576 kernel。两端 L0 中位数的几何均值作为对照。
计时仅为 Host replay 与每列表 wait，初始化、重置、回读、全部校验和打印在外；无 GPU timestamps/profiler。

| 顺序 | kernel/list | L0 Host µs/case | Direct Host µs/case | Direct 耗时变化 | L0 首尾耗时漂移 |
| --- | ---: | ---: | ---: | ---: | ---: |
| 逐层交错 | 384 | 128.727861 | 129.606673 | +0.683% | -0.321% |
| 分阶段 | 384 | 107.872800 | 105.421647 | -2.272% | -7.120% |
| 逐层交错 | 1536 | 126.127443 | 125.315462 | -0.644% | -2.192% |
| 分阶段 | 1536 | 103.449299 | 101.473861 | -1.910% | -3.306% |

同一后端中，分阶段相对逐层交错的耗时变化：

| 进程 | 384 kernel/list | 1536 kernel/list |
| --- | ---: | ---: |
| 0 l0 | -13.188% | -17.509% |
| 1 direct | -18.660% | -19.025% |
| 2 l0 | -19.109% | -18.449% |

48/48 work 协议、1,032,192 次回放 kernel 派发全部通过。
三个进程共 144 个层 case 完成 FP64 HC/Router、BF16 RNE、Q8、TopK 与完整输入/输出/guards 校验；
所有 GPU 输出逐位匹配捕获，hash `ccf0bb7e5263f874`。
三臂 HC 最大绝对误差均为 `4.59847678371e-05`，Router 均为 `2.01617236684e-06`。
FP64 固定容差仍为 `2e-5 + 2e-4*abs(reference)`；未为通过而放宽。
TopK 独立 CPU 检查允许预声明的浮点相邻概率 tie，但 GPU ID、顺序和权重仍必须与捕获逐位一致。

分阶段在两个后端上均更快，说明独立 case 的执行顺序有明显影响。
它同时改变 cache、内存、依赖和状态历史，不能单独归因指令缓存。
Direct 的几个小幅优势均低于各自 L0 对照漂移；分阶段短列表漂移达到 7.120%。
这些是单一后端顺序的诊断结果，不是稳定模型收益或置信区间，也没有完整复现模型中的 Direct 差距。
不能把表中数值称为纯 GPU 时间或模型 tok/s，不能据此跨越真实模型依赖或采用分阶段生产默认。
没有新增 PPL、TTFT、长上下文或 Linux GPU 数据，历史 Direct nonfinite/驻留问题仍未关闭。

前两次诊断捕获失败均保留：首版错误要求所有 T4 图都满足并发条件，第二版跳过后又漏掉实际回退层。
第三版将捕获点移到 BF16/native-Q8 数据就绪处，并记录真实分支；完整协议与上轮 HC 原字节校验通过。
没有删除、覆盖失败或补跑任何性能样本。
原生数学内核没有重编译；仅编译模型捕获钩子和独立 Host 回放程序，复用 frozen Host/运行库、驻留修复与共同 HC128 ZEBIN 包。

离线核验（需要本机私有数据及 frozen 构建输入）：

```powershell
python -X utf8 build-orca-hc-router-20261107/run_r3.py selfcheck
python -X utf8 build-orca-hc-router-20261107/report.py selfcheck
python -X utf8 build-orca-hc-router-20261107/report.py check
```

同条件复测需保留硬件、驱动、编译参数、模型/输入历史、frozen HC128 包、5771 实际专家槽、MTP4、spec-min-p=0.5 和完整输出协议；
按 `capture_r3.py run`、`run.py build`、`run_r3.py run` 顺序在新的实验输出目录重建，不能覆盖本次证据。
公开归档包含脚本、实际编译/链接命令、源文件/头文件 pins、全部原始文本协议和失败记录；不含权重、捕获 payload、EXE、OBJ 或 LIB。

[真实 HC 对照](orca-direct-hc-real-2026-11-07.md) · [模型 Router 顺序化](orca-direct-router-serial-2026-11-07.md) ·
[JSON](../../../sycl/orca/direct-hc-router-a770-windows-2026-11-07.json) · 文本证据 (`direct-hc-router-a770-windows-2026-11-07.json.gz`, raw capture retained locally)
