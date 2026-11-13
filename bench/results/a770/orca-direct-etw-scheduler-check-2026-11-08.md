# GPU调度追踪：离线复核命令勘误

原报告归档已经完整生成，但末尾复核把Python的整数packet-type键与JSON读取后的字符串键直接比较，触发AssertionError。修正复核器先统一JSON表示；完整原始协议、独立区间／配对复算、源码和归档逐字节检查全部通过。原报告、JSON、归档、源码及所有GPU样本保留，没有新增GPU请求。

正确离线命令：`python -X utf8 build-orca-etw-scheduler-20261108/check.py`。原报告所列`publish.py check`保留为失败记录。

[原调度报告](orca-direct-etw-scheduler-2026-11-08.md) · [勘误、原文件摘要与精确复核源码](../../../sycl/orca/direct-etw-scheduler-check-a770-windows-2026-11-08.json)
