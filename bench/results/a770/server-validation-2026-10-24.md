# 服务端回归检查（2026-10-24）

本次只验证正确性，没有改动生产源码。

| 检查范围 | 运行 | 通过 | 跳过 | 失败 / 错误 |
| --- | ---: | ---: | ---: | ---: |
| 原生扫描器开启，全部 21 个服务端测试模块 | 499 | 498 | 1 | 0 / 0 |
| 原生扫描器关闭，解析器与协议相关检查 | 177 | 174 | 3 | 0 / 0 |
| 最新 Anthropic 优化的随机差分 | 5,000 组 | 全部一致 | 0 | 0 / 0 |

全量检查唯一跳过的是 `serve.test_slots.Slots.test_restore_of_a_link_reaches_the_engine_which_decides`：当前 Windows 环境没有创建符号链接的权限。回退检查的 3 项跳过均为原生扫描器专用检查；这些检查在全量测试中通过。回退测试运行 6 个完整模块及 `test_server` 中的 13 个相关测试类，具体名单保存在机器可读记录中，两轮存在重叠。

覆盖实际 tokenizer 的编码/解码一致性、长文本 BPE、流式解码、SIMD/Unicode/分段边界、工具参数及截断、多内容块、OpenAI Chat Completions / Responses / Anthropic Messages、SSE/gzip、JSON Schema、认证与 Host 校验、并发、取消、引擎异常恢复和 Windows 子进程回收。HTTP 检查使用随机本地端口和模拟引擎；没有启动或重配生产引擎。

环境：Windows 10 19045，Xeon E5-2696 v4，CPython 3.11.9 x64。使用本机 IQ2_XS 模型包的 tokenizer；JSON Schema 使用既有隔离目录中的 `jsonschema 4.26.0`，没有安装新依赖。全量 unittest 耗时约 723 秒，回退约 54 秒；这些是检查运行时间，不是性能基准。

随机差分重新比较优化前后的完整 Anthropic 协议事件、逐字节 SSE 和最终非流式响应，并检查输入未被修改及不支持的事件仍被拒绝。最近一轮性能证据的 76 个文件哈希、5,000 组既有差分、28 项检查记录及 A/B/A 算术复核通过。38 项源码及受保护文件、3 项 tokenizer 文件与测试前基线一致，包括模型配置、生产 EXE、原生扫描器和 README。`git diff --check -- serve tools/strata_tokenizer.py` 通过。

本次没有测量真实模型 prefill、decode 或推理端到端收益。

[机器可读结果及 SHA-256](server-validation-2026-10-24.json)。原始日志、结果和运行脚本位于 `build-sycl-a770-server-validation-20261024/`。复跑命令如下；脚本拒绝覆盖已有结果，复跑前应在新的证据目录准备基线。

```powershell
python -X utf8 build-sycl-a770-server-validation-20261024/run.py full
python -X utf8 build-sycl-a770-server-validation-20261024/run.py fallback
python -X utf8 build-sycl-a770-anthropic-events-20261024/measure.py differential-validation.json
python -X utf8 build-sycl-a770-anthropic-events-20261024/publish.py --check --live
```

差分脚本同样拒绝覆盖，复跑需换用新的 `differential-*.json` 文件名。`--live` 会检查当前源码是否仍与最近一轮冻结版本一致。
