# 行内代码里的工具示例保持为文本（2026-10-24）

可见回答用两个反引号包住 `<tool_call>` 示例时，旧解析器会把示例当作真实调用。旧判断只看反引号总数的奇偶，不能识别 Markdown 的连续反引号分隔符；本地假 MCP 回归确认，示例里的 `echo(text="quoted")` 曾被执行。

`OutputParser` 现在记录打开行内代码的连续反引号长度，仅由同样长度的分隔符关闭。完整行及尚未完成的行使用同一判断，保留跨行和任意分块的行为。进入代码围栏时清除前一段的行内状态，避免阻止围栏后的真实调用。逐位置分块还复现了工具标签前空格被丢弃的问题：前缀只含空格时，旧判断把它一并删除；现在仅丢弃标签前的换行，保留文字末尾空格。没有新增依赖。

增强后的三项检查在修复前出现 39 个失败（含子用例），没有错误。第一版行内判断修正后，64 项检查出现 12 个失败，全部来自逐位置检查发现的前缀空格差异；原始失败记录保留。最终 64 项针对性检查全部通过，覆盖：

- 1/2/3/4/6 个连续反引号、匹配及不匹配的结束符、未闭合行内代码，以及代码围栏打断前一段的情况。
- 2/3/6 个反引号的示例与后续真实调用，在每个分块位置验证有无 thinking、是否流式发送工具参数的四种组合；示例正文及前缀空格保持完整，真实调用恰好一个，参数正确。
- OpenAI Chat Completions、Anthropic Messages、OpenAI Responses 的流式与非流式 HTTP 路径。
- 本地假 MCP：示例留在回答文本中，只执行示例外的 `echo(text="actual")`，返回正确结果。

随后完整执行服务端发现的 **473 项检查：465 通过、8 跳过，没有失败或错误**；耗时 651.456 秒（含收集）。跳过项为 5 项缺少 pack tokenizer、1 项 Windows 符号链接权限，以及 2 项缺少 jsonschema。运行期间全部服务端 Python 源码哈希保持一致，上一轮的会话活动状态及超时测试均在此完整运行中通过。最终源码与快照一致，生产配置和 EXE 未改变；`git diff --check` 通过。

环境为 Windows、Python 3.11.9、Intel Xeon E5-2696 v4，使用 CPU MockEngine 和本地假 MCP。原始日志及前后快照位于 `build-sycl-a770-inline-calls-20261024/`，检查及 SHA-256 见[记录](inline-tool-calls-2026-10-24.json)。

```powershell
python -X utf8 -m unittest serve.test_reasoning_rescue serve.test_mcp
python -X utf8 -m unittest discover -s serve -t .
python -X utf8 build-sycl-a770-inline-calls-20261024/check.py
```

本轮为正确性修复，没有新增模型 prefill、decode 或 HTTP 吞吐收益数据，README 性能表未调整。
