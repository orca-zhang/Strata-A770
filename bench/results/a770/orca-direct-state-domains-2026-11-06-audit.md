# 状态命令普查：发布校验补充

两条完整模型协议、原始命令及采集结果均保留，未重跑GPU。
首次发布在写入归档前因JSON掩码对象键的整数／字符串比较失败；原脚本与失败记录已归档。
第二次已写出报告／JSON／精确归档，最终校验又因systolic直方图键的同类JSON转换失败。
原发布器、报告、采集数据及归档均未改写；不能把原发布器退出1记为通过。

补充审计只把重新计算的details转换成其JSON存储表示，逐值比较全部内容；不更改任何计数或门。
它重新解析两条原始token／DONE协议、全部列表／命令DWORD，执行四项损坏负控，并核对精确归档及源／依赖摘要。
补充检查已通过，不新增吞吐、PPL、长上下文或Linux GPU结论。

原报告中的publish.py check保留其历史失败；本机独立复核改用：

```powershell
python -X utf8 build-orca-state-domains-20261106/audit.py check
```

[原始报告](orca-direct-state-domains-2026-11-06.md) · [补充审计JSON](../../../sycl/orca/direct-state-domains-a770-windows-2026-11-06-audit.json)
