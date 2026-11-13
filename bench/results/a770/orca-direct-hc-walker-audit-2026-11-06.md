# 当前 HC：完整派发命令与同步方式核对

2026-11-06，Windows 10，Intel Arc A770 16 GB，驱动 32.0.101.9034，oneAPI 2025.3.2。
离线复用此前同一 GRF128 数学包的独立 HC 抓取，本轮没有运行 GPU 或改产品源码。
Direct 的 1,514 条和 Orca L0 的 2,096 条 walker 全部按真实命令边界解码并逐字核对原文件。
总数包含各后端自己的内存操作，不能当成同一模型请求的算子数量对比。

两后端分别选中 160 次归一化与 80 次融合 Q8 up，覆盖 T1～8。
核对完整 walker 的前 31 个 DWORD，包含保留位；参数堆偏移、ISA 入口偏移及 32 字节 inline 参数单独排除。
本检查没有逐参数等价或机器码重编结论；既有 128 项完整 HC 输出检查仍属于原抓取实验。

扣除已知 indirect 长度、preferred SLM 和独立列出的 post-sync 区域后，所有目标派发的其它控制位一致。
不能只看此前列出的 SIMD／SLM 字段，就认为完整派发命令已经相同。

| 配置 | Direct | Orca L0 |
| --- | --- | --- |
| walker 内完成操作 | 不写入 | 写立即数／队列完成计数 |
| walker 内 dataport pipeline flush | 关闭 | 开启 |
| walker 内 dataport subslice cache flush | 关闭 | 开启 |
| post-sync 控制 DWORD | `0x00000000` | `0x00001025` |
| post-sync MOCS | 未启用写入 | 2 |
| post-sync system-memory fence request | 关闭 | 关闭 |

Direct 仍通过后续 PIPE_CONTROL 和列表末尾完成通知保持依赖，不能把第一列解释为“没有同步”。
L0 把刷新和完成计数放在 walker 内，是新的可验证同步机制差异；它还不是差距成因或提速证据。
单纯删除 Direct 的刷新也不能据此称为等价实现，需要独立依赖／跨队列／Host 可见性及模型检查。
SLM 与参数长度的既有未胜出实验保持原结论，本轮没有重跑或覆盖它们。

CPU 自检核对解析范围、地址／inline 排除，以及保留位和 post-sync 位变化的敏感性。
实际解析另以每个 batch 的原抓取索引核对 walker 数量、唯一偏移及全部 156 字节，避免把 inline 数据误认成命令。
检查和归档持共享实验锁；归档保存精确源码、命令、输出、原始命令包和每份 SHA-256，逐文件回译复核。

```powershell
python -X utf8 build-orca-hc-walker-audit-20261106/audit.py self-check
python -X utf8 build-orca-hc-walker-audit-20261106/publish.py check
```

无新的整模型吞吐、TTFT、PPL 或 Linux GPU 结论；Orca L0 仍为推荐后端，Direct 仍为 WIP。

[精确 JSON](../../../sycl/orca/direct-hc-walker-audit-a770-windows-2026-11-06.json) · 完整原始证据 (`direct-hc-walker-audit-a770-windows-2026-11-06.json.gz`, raw capture retained locally)
