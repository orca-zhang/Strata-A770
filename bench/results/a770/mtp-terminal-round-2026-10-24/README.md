# SYCL MTP 末轮统计与取消回归（2026-10-24）

修复 `sycl/src/program/generate.cpp` 的两个结束路径：达到输出上限、EOS 或已收到取消时，
跳过下一链草稿与其历史准备；没有执行草稿的末轮不再训练包含 verify、commit、draft 的完整轮次成本。
CLI 的输出上限轮次也排除在观测外。适用于本轮 SYCL 路径；取消在末轮判断之后才到达时，仍须等待已经提交的工作。
未修改采样精度、设备数学顺序、默认实验开关、生产 EXE 或配置。

- [x] 冻结修改前源码、候选、头文件和依赖哈希；4 个独立引擎使用同一工具链、FP32 sampler、verifier 与依赖库。同类 A/B 共用 MTP 对象，诊断组仅在私有副本增加日志。
- [x] 两个私有诊断构建输出真实 drafter/observer 调用；两个干净构建检查原始输出。诊断代码不进入项目运行时源码。
- [x] 最终服务端矩阵 46 次请求，核对 317 个诊断轮次；阈值 0 和 0.50、1/4/6 token 上限、固定种子采样与惩罚、EOS、取消和恢复均通过。另留首轮已完成请求及中止记录。
- [x] 两种阈值及基线/候选的 greedy 与采样输出逐 token 相同；取消后的新请求输出也与未取消参考相同。
- [x] 验证期间发送 STOP：基线仍进入 drafter，候选两次分别省去一条草稿链，末轮不再更新策略。
- [x] 原有 DraftPolicy 二进制的 53 项检查通过；该策略源码本轮未改动，检查原始输出及二进制哈希已保存。
- [x] CLI 新增条件直接从基线/候选源码抽取，编译后调用真实 DraftPolicy：54 对、108 项 CPU 检查通过，包含首轮、完整轮次及达到/超过 1/4/6 输出上限，复现并消除 18 种末轮观测差异。
- [ ] CLI 整模型/prefill 验收：已有基线在多 token 提示阶段未完成；单 token 提示直接返回 0 输出。不能把 CPU 调用条件检查当作 CLI 整模型通过。
- [x] 独立原始日志审计、源码/构建哈希及生产文件保护检查；结果不包含其他会话的前端或 XMX 修改。

本轮是结束路径的正确性与调用回归，**没有做整模型性能验收，也没有声明百分比提速**。
日志中的 decode、prompt、TTFT、接受/提议数仅用于回归溯源；带诊断的读数不用于性能比较。
整模型采用门槛仍为 1%，算子正收益可保留；本轮没有新增算子计时。
服务端窗口读取提示路径、Q8 KV、`--adapt-swaps 0 --pcie-frac 0`、`STRATA_IQ_MT_MIN=1`、耦合采样；
完整参数、实际缓存容量见 [results.json](results.json) 的 `args` 和 `runtime_info`，不可用请求缓存 MiB 代替实际分配。

初次独立链接超出了脚本设置的 360 秒超时；后台编译器继续完成了输出。保留了首轮源码、命令、日志和对象哈希，
重试统一使用原生 `-fsycl-max-parallel-link-jobs=8`，超时调整为 1200 秒；全部独立构建及之后的功能回归通过。
重试复用经哈希确认的已编译对象，不改编译优化、设备运算或依赖内容；首轮输出未参与回归。
首轮功能脚本在切换到 0.50 时触发日志文件名冲突，已完成的 22 次请求未发现模型错误；
保留 [首轮记录](raw/functional-attempt1/results.json)，修正小数阈值文件名后在新目录完整复测。
CLI 首次检查误用未验收的批量 prefill，停留于提示处理而未产生输出；仅中止该自建子进程并保留日志，
之后保留已通过的 44 次诊断请求。原生 IQ CLI 在启动前拒绝 `--prefill 0`（exit 2）；
`--prefill 1`、23-token 提示同样未进入 decode（约两分钟后中止该自建进程）。单 token 提示正常退出却输出 0 token：
基线的 `spec_pos=0` 不进入已有的 `spec_pos>0` 解码循环，修改前后源码均保留这一既有行为。
因此本轮 CLI 仅完成新增调用条件的 CPU 回归，未声明 CLI 整模型/prefill 通过。
两组干净服务端引擎输出对照已完成；全部中止/拒绝/空输出日志均不作为正确性或性能通过记录。

独立构建留在 `build-sycl-mtp-terminal-20261024/{baseline,candidate,baseline-debug,candidate-debug}/strata.exe`。
归档的 [run.py](run.py) 和 [check.py](check.py) 为实际构建/回归脚本副本，工作目录按私有构建路径解析。
源码变化见 [terminal-round.patch](terminal-round.patch)，构建命令见 [build.json](build.json)，所有原始输出在 [raw](raw/)。
编译器和 CPU 检查日志以原始字节保存在 `compile-logs/*.txt`，只更改扩展名以避免仓库忽略 `*.log`。

CPU 离线复核（不加载模型）：

```powershell
python -X utf8 bench/results/a770/mtp-terminal-round-2026-10-24/check_results.py
```

对保留的冻结引擎重新执行 GPU 回归，每次指定新结果目录；使用共享锁串行运行：

```powershell
python -X utf8 build-sycl-deepinfer-20261023/check_bf16_arena.py build-sycl-mtp-terminal-20261024/check.py build-sycl-mtp-terminal-20261024/checks-repeat
```

重建并执行 CLI 调用条件的 CPU 检查（复用已通过的服务端请求记录，不重新运行模型）：

```powershell
python -X utf8 build-sycl-deepinfer-20261023/check_bf16_arena.py build-sycl-mtp-terminal-20261024/finalize.py
```
