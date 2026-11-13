# Orca Direct：用原生全局驻留量约束显存预算

2026-11-06，Windows10／Arc A77016GB／驱动32.0.101.9034／oneAPI2025.3.2。
补齐Direct与L0的容量检查差异；这是显存记账修正，不是decode提速试验，也未定位首次驻留失败的唯一原因。
Direct使用D3DKMTQueryStatistics读取本地段驻留量，空闲量为CommitLimit−BytesResident，排除aperture／系统内存段。
适配器LUID和本地段ID只发现一次；容量检查更新驻留量，并取进程预算、自身已跟踪分配和全局空闲量的最小值。
内核派发与成功分配路径未增加查询、线程或锁；容量查询失败不伪造可用量。错误诊断失败也不掩盖原驻留错误及回滚。

空闲观察进程：预算15489.000 MiB，全局空闲14713.887 MiB，原差额775.113 MiB。
另一进程申请256 MiB后，全局驻留量上升，观察进程自身用量仍为0；释放后全局驻留量下降。全部固定采样保留。

| 状态 | 原生空闲量：前／后 MiB | Sysman空闲量 MiB |
| --- | ---: | ---: |
| baseline | 14712.570／14708.500 | 14708.484 |
| foreign-held | 14452.355／14452.355 | 14452.340 |
| released | 14708.500／14727.410 | 14727.395 |

Sysman仅在独立校准进程中作为原L0口径的对照；Direct不链接Level Zero、SYCL或UR。
前后括号允许64 MiB桌面／驱动占用变化，不把相隔时刻的查询当作原子快照。
分别启动的Runtime检查中，原可用量15489.000 MiB，新可用量14589.316 MiB；这不是相同时刻测量。

旧Runtime负控预期失败；新版通过17次受控驻留拒绝及重试、双块分配、同步释放、既有数据保护、两类终止失败。
另通过动态计数／静态拓扑缓存、三种全局查询错误及恢复，以及诊断失败不掩盖原错误的检查。
只重编Runtime对象，并重链接上一轮已检查的完整Direct引擎，数学包、核心与Host对象保留。
固定MTP4、step／last min-p0.5、cumulative／suffix／cost-window／prompt-cache0；实际5771槽、reserve1536 MiB、U4/min16/M16、read256、adapt32/decay0.95、BCS1。
完整模型7条协议匹配：2K→2K→8K→8K→Prose2K→Short→Short，全部token及草稿计数与冻结参考一致。
启动原始信息：`INFO context=8192 kv=fp16 kv_resident=0 expert_slots=5771 expert_cache_mib=7922 expert_slots_primary=5771 expert_cache_primary_mib=7922 spec=4 mtp_max=0 lookup=0 vram_free_mib=1797 cvec=0 arena_mib=33812 pool_workers=21 pcie_frac=0.16 spec_min_p=0.50 conversation_cache_mib=0 conversation_cache_slots=4 conversation_cache_min_free_mib=2560 tail_role_token=-1 vram_elastic=0 engine=0.1.39-orca`。
没有减少缓存或调低预留来取通过；本轮未替换生产EXE／配置，不报告新吞吐、PPL或Linux GPU结论。
Windows桌面与其它进程仍可在查询后改变占用；原首次7.74 GiB驻留失败及历史8K间歇故障均未关闭。

首次探针因虚拟显示适配器复用设备ID而在分配前停止，已按Runtime现有规则过滤软件／间接显示设备；原失败保留。
Runtime检查准备期间误复用已冻结探针，被完整性检查在编译前拦截；已恢复原文件并单独保存新检查，未重新采样以取得通过。
归档只含精确文本、参数、命令、协议与摘要，不分发模型、EXE或zebin。编译／GPU／哈希／归档串行持共享锁。

本机复核，不运行GPU：

```powershell
python -X utf8 build-orca-global-memory-20261106/finish.py check
```

[JSON](../../../sycl/orca/direct-global-memory-a770-windows-2026-11-06.json) · 精确文本归档 (`direct-global-memory-a770-windows-2026-11-06.json.gz`, raw capture retained locally)
