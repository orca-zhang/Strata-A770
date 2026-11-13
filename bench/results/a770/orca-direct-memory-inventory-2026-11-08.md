# A770：Direct 的 READY 显存分配归类

2026-11-08，Windows10／Arc A770 16GB／驱动32.0.101.9034／oneAPI2025.3.2。

固定冻结HC128／MTP4／min-p0.5／配置5500最大blob预算／reserve1536MiB，使用当前19项检查合格的原生运行时。只在memory_info打印现有逻辑分配的所有权，不增加GPU命令，不发送GEN，也不重复已成功的模型输出资格检查。实际5569槽；这是独立容量诊断，未通过或放宽原5771槽性能条件。部署、数学核及Linux GPU状态不变。

原始INFO：`INFO context=8192 kv=fp16 kv_resident=0 expert_slots=5569 expert_cache_mib=7646 expert_slots_primary=5569 expert_cache_primary_mib=7646 spec=4 mtp_max=0 lookup=0 vram_free_mib=1095 cvec=0 arena_mib=33812 pool_workers=21 pcie_frac=0.16 spec_min_p=0.50 conversation_cache_mib=0 conversation_cache_slots=4 conversation_cache_min_free_mib=2560 tail_role_token=-1 vram_elastic=0 engine=0.1.39-orca`。

| READY 存储类别 | 本地GPU MiB | Host MiB | 本地／Host分配数 |
| --- | ---: | ---: | ---: |
| application | 12107.562 | 34336.938 | 335／93 |
| isa | 4.000 | 1.000 | 1／1 |
| module_data | 1.625 | 0.000 | 26／0 |
| thread_pools | 344.000 | 0.000 | 8／0 |
| scratch_surfaces | 0.250 | 0.000 | 4／0 |
| parameter_source | 0.000 | 2.000 | 0／2 |
| parameter_mirror | 2.000 | 0.000 | 2／0 |
| other_internal | 0.000 | 543.875 | 0／29 |

内部本地GPU分配合计351.875MiB，其中scratch／private线程池344MiB，占97.76%。参数镜像2MiB，ISA4MiB，模块数据1.625MiB，scratch描述面0.25MiB。每个底层分配句柄只归入一个类别，重复／失效／交叠角色均拒绝；五次查询全部分区计数及字节数守恒。

首次查询只有4MiB本地ISA；专家缓存计算前为4412.938MiB，本地线程池尚为零；READY时为12459.438MiB，其中应用分配12107.562MiB、内部351.875MiB。观测限定于启动阶段，应用类别包含模型、KV、专家缓存等，不能直接称作单一张量。Host字节是已跟踪逻辑大小；本表不证明物理页大小、驻留页分布、PCIe传输量或GPU缓存命中率。

这些结果把后续内存排查集中到线程scratch／private池的容量及保留策略，而不是参数镜像。尚未采集对应L0内部类别，因此不能把344MiB全算作相对L0的额外占用，更不能认定它造成约2%的Short decode差距。没有新的tok/s、TTFT或PPL数据。

固定commit的Intel参考源码按最大物理线程空间计算scratch容量；扩容时将旧分配交给TEMPORARY_ALLOCATION延迟回收。Orca当前也按物理线程ID空间留4096槽，但旧池仍可被已录图引用，不能直接缩小槽数或释放。参考源码不是本机已安装驱动的反编译证明。后续应检查各队列及图的实际池引用，区分必需并发存储和可回收历史池。

源码归一化仅将CRLF还原为LF并删除精确测试打印块，恢复合格运行时的逐字节摘要；其他源及数学对象固定。原始查询误对blob:none仓库作广泛git grep，引发按需抓取，已停止自己的查询进程树并保留退出1记录；改为六个固定commit文件，按SHA256落盘缓存，不重复联网。未停止任何外部GPU测试。

参考：

- [shared/source/helpers/gfx_core_helper_tgllp_to_dg2.inl](https://raw.githubusercontent.com/intel/compute-runtime/9e866e5f9b8b3281cb6d8217a925594caa67cf76/shared/source/helpers/gfx_core_helper_tgllp_to_dg2.inl)
- [shared/source/helpers/gfx_core_helper_xehp_and_later.inl](https://raw.githubusercontent.com/intel/compute-runtime/9e866e5f9b8b3281cb6d8217a925594caa67cf76/shared/source/helpers/gfx_core_helper_xehp_and_later.inl)
- [shared/source/helpers/gfx_core_helper_base.inl](https://raw.githubusercontent.com/intel/compute-runtime/9e866e5f9b8b3281cb6d8217a925594caa67cf76/shared/source/helpers/gfx_core_helper_base.inl)
- [shared/source/command_stream/scratch_space_controller_base.cpp](https://raw.githubusercontent.com/intel/compute-runtime/9e866e5f9b8b3281cb6d8217a925594caa67cf76/shared/source/command_stream/scratch_space_controller_base.cpp)
- [shared/source/command_stream/scratch_space_controller_xehp_and_later.cpp](https://raw.githubusercontent.com/intel/compute-runtime/9e866e5f9b8b3281cb6d8217a925594caa67cf76/shared/source/command_stream/scratch_space_controller_xehp_and_later.cpp)
- [shared/source/xe_hpg_core/hw_info_xe_hpg_core.cpp](https://raw.githubusercontent.com/intel/compute-runtime/9e866e5f9b8b3281cb6d8217a925594caa67cf76/shared/source/xe_hpg_core/hw_info_xe_hpg_core.cpp)

[JSON](../../../sycl/orca/direct-memory-inventory-a770-windows-2026-11-08.json) · 精确源码、查询日志与参考文件 (`direct-memory-inventory-a770-windows-2026-11-08.json.gz`, raw capture retained locally)

复核：`python -X utf8 build-orca-memory-inventory-20261108/publish.py check`。源码／输出／归档逐字节验证；新采样使用新目录，不覆盖原结果。
