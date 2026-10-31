# 缺少工具调用尾标签时保留完整参数（2026-10-24）

## 问题与修复

模型已经写完 `</function>`、但没有写完最后的 `</tool_call>` 时，解析器按已有规则把函数视为完整调用。流式 JSON 包含参数，但最终 `tool_call` 事件复用了一个参数仍为 `{}` 的对象。服务端 MCP 工具循环读取最终事件，因而会把空参数交给工具。

复现输入：

```text
<tool_call><function=write><parameter=path>notes.txt</parameter><parameter=count>3</parameter></function>
```

修复前，流式参数为 `{"path":"notes.txt","count":3}`，最终事件参数却是 `{}`。现在在返回最终事件前复用已有的 `parse_tool_call`，按工具 schema 解析已完成的函数正文，并填入原调用对象。调用 ID 保留，解析范围截止于扫描器确认的 `</function>`，不会把后面的残缺参数标签当成正文。

生产代码增加两行。函数正文仍未结束的调用沿用已有的未完成处理；MCP 的对应回归确认不会执行这类调用。

## 正确性验证

环境为 Intel Xeon E5-2696 v4、Windows、Python 3.11.9，使用 CPU MockEngine 和本地假 MCP 服务。机器同时运行其他任务，测试总耗时不作为性能收益。

- 先增强已有解析检查并新增 MCP 回归，修复前两项测试共触发 10 个失败（含子用例），确认能捕获空参数问题。
- 解析检查覆盖完整尾标签、完全缺少尾标签、尾标签只输出一部分、函数后残留不完整参数标签，以及 schema 声明为字符串的 `"123"`。按 1、7、10,000 字符分块，验证最终参数等于流式 JSON，且调用 ID 一致。
- 本地 MCP 的 HTTP 回归使用假的 `add` 工具：函数完整但缺少尾标签，收到 `{"a":2,"b":3}`，返回 `5`，同一调用 ID 贯穿 start/call/result，结果进入模型的下一轮提示。
- 2,000 组随机输入、4,000 次解析（种子 20261024）验证完整尾标签与缺少／部分尾标签的调用均保留期望参数。覆盖 1～3 个连续调用、字符串／整数／嵌套对象、Unicode、字面格式标签、随机分块、空片段及不同结束原因；最终参数、流式 JSON 和调用 ID 检查通过。
- 额外完成 12 次真实 localhost HTTP 请求，使用 MockEngine，覆盖下表。这里只验证响应数据，不测性能。

| API | 输出形式 | 模型尾标签 | 验证 |
| --- | --- | --- | --- |
| OpenAI Chat Completions | 流式、非流式 | 完整、缺少 | 参数正确，结束原因为 tool_calls |
| Anthropic Messages | 流式、非流式 | 完整、缺少 | 参数正确，结束原因为 tool_use |
| OpenAI Responses | 流式、非流式 | 完整、缺少 | 参数正确，函数调用状态为 completed |

OpenAI/Anthropic 的响应收集器使用流式参数组装 JSON，本次明确复现的错误发生在最终解析事件及读取该事件的 MCP 路径。

## 记录与复核

首次全量运行完成 468 项，耗时 714.084 秒，458 通过、1 失败、1 错误、8 跳过。没有把这轮记为全绿：

- gzip Unicode 功能检查的客户端在 `response.read()` 触发 3 秒 socket 读超时。测试客户端等待上限改为与其他 HTTP 检查相同的 30 秒。
- 会话测试假定消费者会在生产者 0.8 秒等待期间处理 SESSION 并开始超时计时；负载下消费者晚到，完成响应已经排队，得到合法的 200。改为在 SESSION 后直接注入 `queue.Empty`，同时检查传给队列的等待期限依次为 0.3、2.0、0.3 秒，以及 HTTP 500 和引擎结束状态。

这两处修改只涉及测试。相关 37 项复查完成：36 通过、1 项 Windows 符号链接权限条件跳过。前一轮的重启和会话时序修复也已在本次全量运行中通过。

修复测试后，重新完整运行 **468 项**，耗时 **488.250 秒**：**460 通过、8 跳过，无失败或错误**。所有服务端 Python 源码在运行期间的哈希保持一致。跳过项为缺少 pack tokenizer 的 5 项、Windows 符号链接权限条件的 1 项、缺少 jsonschema 的 2 项。

原始检查结果及冻结文件 SHA-256：[完整记录](tool-call-final-arguments-2026-10-24.json)。生产配置和 EXE 的 SHA-256 与本轮开始时一致，`git diff --check` 通过。

本机快照及日志位于 `build-sycl-a770-final-call-20261024/`：`before/`、`after/`、`after-final/`、`before-fix.log/json`、`after-fix.log/json`、`arguments-check.py/json`、`http-arguments.log/json`、`http-responses.log/json`、`regression.log/json`、`timing-check.log/json` 和 `regression-final.log/json`。脚本保留原记录并拒绝覆盖。

```powershell
python -X utf8 -m unittest serve.test_server.UnfinishedToolCall serve.test_mcp.ToolLoop.test_call_without_outer_close_keeps_arguments serve.test_mcp.ToolLoop.test_a_call_the_output_ends_inside_is_not_run
python -X utf8 build-sycl-a770-final-call-20261024/check.py --check
```

本轮属于正确性修复，没有新的模型 prefill、decode 或 HTTP 端到端计时数据，未调整 README 性能数据。
