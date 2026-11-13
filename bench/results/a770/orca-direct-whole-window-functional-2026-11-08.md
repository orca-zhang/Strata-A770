# 完整窗口全状态恢复：独立功能验证

2026-11-08，Windows10／Arc A770 16GB／驱动32.0.101.9034／oneAPI2025.3.2。

首次只恢复PLE的隔离回放在QSA状态检查处拒绝。随后完整恢复PLE／QSA／GDN的固定5771槽尝试，在READY实际只取得5757槽，0条GEN即停止；两个失败均完整保留。

本轮复用已编译的同一L0／Direct测试EXE，不重编、降预留或改模型参数。范围明确为功能验证：每后端10次Short，无2K预热，在第10次的pos23／T4窗口额外回放10次；两后端先后执行，不计算性能对照或发布tok/s。HC128／MTP4／min-p0.5／reserve1536保留，实际容量见INFO。

每次额外回放前完整恢复原窗口输入的GDN状态、QSA arena与PLE历史，重置原flag／事件；回放后逐字节比较全部GDN／QSA状态、PLE结果、全部历史快照、完整logits和采样ID，与首次正常执行一致才继续。恢复和全状态回读均在原诊断计时区间外，但会改变缓存，所以原始时间仅作诊断记录，不解释为正常模型速度。

已完成请求20条／输出1400个；全部功能检查完成：True。

- l0：`INFO context=8192 kv=fp16 kv_resident=0 expert_slots=5748 expert_cache_mib=7891 expert_slots_primary=5748 expert_cache_primary_mib=7891 spec=4 mtp_max=0 lookup=0 vram_free_mib=1393 cvec=0 arena_mib=33812 pool_workers=21 pcie_frac=0.16 spec_min_p=0.50 conversation_cache_mib=0 conversation_cache_slots=4 conversation_cache_min_free_mib=2560 tail_role_token=-1 vram_elastic=0 engine=0.1.39-orca`；退出0。

- native：`INFO context=8192 kv=fp16 kv_resident=0 expert_slots=5771 expert_cache_mib=7922 expert_slots_primary=5771 expert_cache_primary_mib=7922 spec=4 mtp_max=0 lookup=0 vram_free_mib=1079 cvec=0 arena_mib=33812 pool_workers=21 pcie_frac=0.16 spec_min_p=0.50 conversation_cache_mib=0 conversation_cache_slots=4 conversation_cache_min_free_mib=2560 tail_role_token=-1 vram_elastic=0 engine=0.1.39-orca`；退出0。

功能通过也不满足原5771槽对照条件，不关闭Short根因、原Direct长上下文问题或Linux GPU待测项。生产源码、默认推荐L0和部署EXE保持。

[PLE-only失败](orca-direct-whole-window-replay-2026-11-08.md) · [完整恢复的容量失败](orca-direct-whole-window-replay-reset-2026-11-08.md) · [最新合格性能对照](orca-direct-qualified-contexts-r2-2026-11-08.md)

[JSON](../../../sycl/orca/direct-whole-window-functional-a770-windows-2026-11-08.json) · 精确源码、原始协议和编译头 (`direct-whole-window-functional-a770-windows-2026-11-08.json.gz`, raw capture retained locally)

离线复核：`python -X utf8 build-orca-window-replay-functional-20261108/publish.py check`。
