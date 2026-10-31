# 服务解析检查（2026-10-24）

Xeon E5-2696 v4、Windows、Python 3.11.9。此次只测 CPU 解析；未启动模型、占用 GPU 或修改生产 EXE、配置。

`serve/server.py` 的批次解析改用标准库 `split` / `int`，删除手写整数扫描；返回任何 token 前先验证整个批次。`serve/frontend.py` 在尚未收到 `</tool_call>` 时提前返回，避免重复遍历参数。完整解析仍检查参数内的字面结束标签。

另修复 `parallel_args` 缓存 `STRATA_BATCH_MTP` 导致的警告行为回归，恢复在调用时读取环境变量。此项属于正确性修复。

## CPU 阶段计时

同进程 A/B/A，每臂 7 个样本，取中位数；对照耗时取两臂 A 中位数的几何平均。批次每个样本解析 5,000 次；工具流通过实际 `OutputParser`，以 8 字符增量输入约 26K 字符参数，逐次验证完整参数与流式 JSON 一致。

两次测量均保留；以两臂 A 漂移绝对值不超过 3% 作为本次比较的接纳条件。没有模型吞吐量或 HTTP 计时。

| 复测项目 | 基线 | 修改后 | 耗时下降 | A 臂漂移 | 判断 |
| --- | ---: | ---: | ---: | ---: | --- |
| 4-token 批次 | 18.880 µs | 1.621 µs | — | -21.12% | 漂移过大，排除 |
| 6-token 批次 | 23.590 µs | 1.907 µs | 91.92% | +1.37% | 接纳 |
| 26K 字符工具流 | 35.934 ms | 23.838 ms | 33.66% | -0.74% | 接纳 |

首轮仅 6-token 批次满足漂移条件（25.072 → 1.959 µs，下降 92.19%，漂移 +0.47%）；其余两组漂移分别为 -18.51% 和 +24.67%，排除。共享机器负载未受控；工具流仅有一组满足条件的 A/B/A，不能据此推断其他长度、分块或实际请求的收益。提前返回仍需搜索结束标签，不改变整个解析器的复杂度上限。

原始记录：[首轮](service-parser-2026-10-24-screen.json)、[复测](service-parser-2026-10-24.json)，均含全部样本、代码和基线快照 SHA-256。复测后已通过计算与源文件哈希核验：

```powershell
python -X utf8 build-sycl-a770-service-check-20261024/bench.py --check
```

本轮没有新增等效 prefill、decode 或 HTTP 端到端测量，因此不把解析阶段的百分比加入 README 性能表。此前的[窗口阈值 HTTP 测量](window-reader-http-2026-10-24.md)是另一项独立实验。

## 回归检查

当前服务发现 462 项测试，采用分段验证，8 项因依赖或系统条件跳过：

- 完整发现运行在第 413 项因 HTTP 整体耗时 5.031 秒超过测试的 5 秒上限而停止。前 411 项（会话模块之前）无失败，含 5 项跳过；生产源文件在运行中未变。
- 将两个易受调度影响的 HTTP 上限断言改为直接检查引擎队列等待期限，覆盖普通静默、阻塞步骤的独立期限、进度后的静默期限恢复；HTTP 错误响应、引擎终止、锁释放检查仍保留。生产超时实现和限值未改。
- 修改后的全部 31 项会话检查通过，含 1 项跳过（9.517 秒）；末尾 structured / VRAM / Windows job 的 20 项检查通过，含 2 项因缺少 `jsonschema` 跳过（10.351 秒）。三段合计覆盖 462 项；并非一次完整运行全绿。
- 新增畸形批次零 token 交付，以及含 Unicode、字面嵌套标签的大工具参数流式一致性检查。14 项解析和并发参数定向检查、10 项先前失败相关复查均通过。
- SYCL、Intel Windows、配置及版本固定相关的 53 项 setup 检查通过；本轮四个修改文件的 `git diff --check` 通过。

本机日志与改前快照保存在 `build-sycl-a770-service-check-20261024/`，包括 `service-suite-after.log/json`、`remaining-tests.log/json`、`failure-recheck.log/json`、`setup-tests.log` 和 `before/serve/`。复测的计算及哈希核验也通过。

生产文件 SHA-256 与本轮开始时一致：

```text
strata-iq2_xs.json
1a755372f0e6c90c0bfa2af865d67b51f8df49c10aa1613b041b08da8184ca88
build-sycl-a770-unified-operators/bin/strata-unified-all-operators.exe
8f0d866ded297cf5a409e84cf5dd61f437c56c8a89aaeb2a85569265b6a3e122
```
