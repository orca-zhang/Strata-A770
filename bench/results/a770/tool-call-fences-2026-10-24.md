# Markdown 围栏内的工具示例保持为文本（2026-10-24）

四个反引号包住 Markdown 文档、文档里又含三个反引号的 XML 示例时，旧解析器会提前关闭外层围栏，把示例中的 `<tool_call>` 当作真实调用。六个及以上的围栏标记也可能未被识别；带正文的行（例如三个反引号后接 `example`）会被误当作结束行。本地假 MCP 回归确认，旧代码实际调用了示例里的 `echo(text="quoted")`。

`OutputParser._track` 现在保存开头完整的围栏标记。结束行必须使用同一种标记、长度不少于开头，且后面只有空白；反引号围栏的信息部分不能再含反引号。沿用原有状态和字符串操作，没有新增依赖。

修复前，增强后的三项检查出现 57 个失败（含子用例），没有错误；修复后 49 项针对性检查全部通过。检查覆盖反引号及波浪号、3/4/6 个标记、较短结束行、带正文的伪结束行、等长及更长的合法结束行，以及不同分块。OpenAI Chat Completions、Anthropic Messages 和 OpenAI Responses 的流式与非流式 HTTP 检查均通过。本地假 MCP 确认围栏内示例原样留在文本中，仅执行围栏外的真实调用。

完整服务端回归运行 470 项：461 通过、1 失败、8 跳过。失败的是 `Slots.test_slot_work_counts_as_activity_and_shows_in_status`：观察线程固定等待 0.15 秒，可能在 HTTP 保存请求进入处理前就读取 `busy=False`。测试现在在实际进入 `session_file` 时取样，继续调用原来的假引擎方法，保留忙碌状态、阶段、活动时间和空闲卸载断言；删除了观察线程和固定等待。此后会话模块 32 项复测：31 通过、1 项 Windows 符号链接权限跳过。没有重新执行整套 470 项，不能将首次完整运行记为全绿。其他跳过项为 5 项缺少 pack tokenizer 和 2 项缺少 jsonschema。

环境为 Windows、Python 3.11.9、Intel Xeon E5-2696 v4；使用 CPU MockEngine 和本地假 MCP。原始日志、前后源码快照及回归脚本位于 `build-sycl-a770-tool-fences-20261024/`。完整检查及 SHA-256 见[记录](tool-call-fences-2026-10-24.json)。

```powershell
python -X utf8 -m unittest serve.test_reasoning_rescue serve.test_mcp
python -X utf8 -m unittest discover -s serve -t .
python -X utf8 build-sycl-a770-tool-fences-20261024/check.py
```

本轮为正确性修复，没有新增模型 prefill、decode 或 HTTP 吞吐收益数据，README 性能表未调整。
