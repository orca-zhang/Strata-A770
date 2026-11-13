# A770：重录后线程池引用观察

2026-11-08，Windows 10／Arc A770 16GB／驱动32.0.101.9034／oneAPI2025.3.2。

冻结HC128／MTP4／min-p0.5／reserve1536，仅替换当前Q2-fork Verifier 的 warm()，使用隔离环境变量启用此前重录补丁；其他Host对象、数学内核和配置保留。只启动到READY，没有GEN，也没有释放、缩小或共享任何线程池。

实际容量以原始INFO为准：`INFO context=8192 kv=fp16 kv_resident=0 expert_slots=5586 expert_cache_mib=7668 expert_slots_primary=5586 expert_cache_primary_mib=7668 spec=4 mtp_max=0 lookup=0 vram_free_mib=1060 cvec=0 arena_mib=33812 pool_workers=21 pcie_frac=0.16 spec_min_p=0.50 conversation_cache_mib=0 conversation_cache_slots=4 conversation_cache_min_free_mib=2560 tail_role_token=-1 vram_elastic=0 engine=0.1.39-orca`。本次为5586槽的引用诊断，不通过或放宽原5771槽性能条件。

八个线程池仍占344MiB。重录后旧池0、2、3的当前队列绑定、保留列表引用、活动列表引用均为零，合计 **73.375MiB**；旧池5仍被一份录图引用，占49.125MiB，不能释放。当前四个绑定池仍为221.5MiB。参数镜像由此前2MiB变为4MiB；这不是整体显存下降的结果。

这提供了回收部分旧池的前置条件，尚未实现回收。此前只重录的隔离性能试验decode为−0.352%／−0.582%，不能由本次引用变化推导速度提升或默认启用。没有新的tok/s、TTFT、PPL或Linux GPU结论；部署不变，L0仍为推荐后端，Direct仍为WIP。

首次准备因旧辅助模块导入冲突失败，发生在编译／GPU执行前；原记录保留。r2改为只读取旧补丁文字，验证仅warm()发生改变。运行时去除精确观察块并归一化换行后逐字节恢复合格源码；完整句柄／字节分区和每池引用已核对。

[重录前的引用表](orca-direct-thread-pools-2026-11-08.md) · [此前只重录的性能试验](orca-direct-scratch-recapture-2026-11-06.md)

[JSON](../../../sycl/orca/direct-pool-recapture-a770-windows-2026-11-08.json) · 源码、原始记录及准备失败归档 (`direct-pool-recapture-a770-windows-2026-11-08.json.gz`, raw capture retained locally)

复核：`python -X utf8 build-orca-pool-recapture-20261108/publish.py check`。新采样使用新目录，保留原记录。
