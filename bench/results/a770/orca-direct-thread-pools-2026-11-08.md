# A770：线程池容量与录图引用

2026-11-08，Windows10／Arc A770 16GB／驱动32.0.101.9034／oneAPI2025.3.2。

沿用当前合格原生运行时、冻结HC128／MTP4／min-p0.5数学及Host对象，配置5500最大blob预算／reserve1536MiB。仅启动至READY，未发送GEN；补充读取每个线程池的容量、所有队列当前绑定及所有已保留命令列表的存储引用，不释放、不缩小、不共享或重新绑定任何内存。实际5413槽，这个独立观察不通过或放宽原5771槽性能条件，也不是当前spec6／0.6生产测试。

原始INFO：`INFO context=8192 kv=fp16 kv_resident=0 expert_slots=5413 expert_cache_mib=7433 expert_slots_primary=5413 expert_cache_primary_mib=7433 spec=4 mtp_max=0 lookup=0 vram_free_mib=1059 cvec=0 arena_mib=33812 pool_workers=21 pcie_frac=0.16 spec_min_p=0.50 conversation_cache_mib=0 conversation_cache_slots=4 conversation_cache_min_free_mib=2560 tail_role_token=-1 vram_elastic=0 engine=0.1.39-orca`。

| 池索引 | 每线程字节 | 分配MiB | 当前绑定数 | 已保留录图引用数 | 活动列表引用 |
| --- | ---: | ---: | ---: | ---: | ---: |
| 0 | 12544 | 49.125 | 0 | 1 | 0 |
| 1 | 20096 | 78.625 | 1 | 4 | 0 |
| 2 | 2048 | 8.125 | 0 | 1 | 0 |
| 3 | 4096 | 16.125 | 0 | 1 | 0 |
| 4 | 8192 | 32.125 | 1 | 5 | 0 |
| 5 | 12544 | 49.125 | 0 | 1 | 0 |
| 6 | 20096 | 78.625 | 1 | 10 | 0 |
| 7 | 8192 | 32.125 | 1 | 10 | 0 |

八个池合计344MiB，全部仍被已保留录图引用。当前两个计算队列各绑定一个20096字节／线程private池和一个8192字节／线程scratch池，共221.5MiB。旧池0／2／3／5共122.5MiB，各有一份录图引用；没有可直接回收的无引用池。仅凭当前队列不再绑定，不能认定旧分配已无效。

全部池的payload等于每线程字节乘4096，另保留首尾各64KiB保护区。查询逐项核对池索引、大小、当前绑定数与线程池分类总量；五次完整分配分区均守恒。只打印池／队列索引和计数，没有指针。分类器归一化CRLF并删除精确测试块后，逐字节恢复合格运行时。源码、产物摘要和协议归档通过检查。

下一项可验证的是先让录图使用最终容量的池，再确认旧引用消失后回收；122.5MiB是当前旧图保留量，不是已经实现的节省。之前只重录图的隔离性能试验未建立稳定收益，因此不能据此默认开启重录，也不能承诺本次内存变化会解决约2%的Short decode差距。尚无对应L0内部池分类，不把全部344MiB算成相对L0额外占用。

没有新的tok/s、TTFT、PPL或Linux GPU结果；部署配置及数学内核未改。Orca L0仍为推荐后端，Direct仍为WIP。

[前一轮内存归类](orca-direct-memory-inventory-2026-11-08.md) · [此前只重录图的隔离试验](orca-direct-scratch-recapture-2026-11-06.md)

[JSON](../../../sycl/orca/direct-thread-pools-a770-windows-2026-11-08.json) · 精确源码与原始查询日志 (`direct-thread-pools-a770-windows-2026-11-08.json.gz`, raw capture retained locally)

复核：`python -X utf8 build-orca-thread-pools-20261108/publish.py check`。新采样使用新目录，不覆盖原失败和成功记录。
