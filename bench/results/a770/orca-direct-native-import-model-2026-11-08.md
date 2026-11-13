# A770：当前原生运行时的完整模型验证

2026-11-08，Windows10，Arc A770 16GB，驱动32.0.101.9034，oneAPI2025.3.2。

本轮把通过19项独立检查的原生Host分配／调用者页面导入运行时，接入冻结的完整模型。固定HC128／MTP4／min-p0.5／配置5500最大blob预算／reserve1536MiB，复用原数学包、Host对象及权重身份缓存；不是当前spec6／0.6生产版本。部署EXE、配置与数学核未改，Linux GPU仍待验证。

八条完整请求依次为Code2K×2、Short×2、Long8K×2、Prose2K、Short；所有输出token和8项协议字段与冻结参考完全一致，进程正常退出。模块检查未加载SYCL、UR、Level Zero、IGC或GMM DLL。此处证明当前运行时可以支撑完整模型，不产生新tok/s、TTFT或PPL结论。

八请求原始INFO：`INFO context=8192 kv=fp16 kv_resident=0 expert_slots=5577 expert_cache_mib=7656 expert_slots_primary=5577 expert_cache_primary_mib=7656 spec=4 mtp_max=0 lookup=0 vram_free_mib=1047 cvec=0 arena_mib=33812 pool_workers=21 pcie_frac=0.16 spec_min_p=0.50 conversation_cache_mib=0 conversation_cache_slots=4 conversation_cache_min_free_mib=2560 tail_role_token=-1 vram_elastic=0 engine=0.1.39-orca`。

实际5577槽、7656MiB专家缓存；未通过原5771槽性能条件。reserve配置未降低，实时容量约束仍开启，功能通过不能转记为同槽性能通过。

首次分配观测把汇总放在全局析构函数；模型以std::_Exit(0)退出，跳过析构，日志没有汇总。因此首次八请求不能证明分配计数，也不能把无日志解释成某路径未使用。原始缺口保留。只修正测试观察器为每次成功分配后写出并刷新累计计数，另跑Code2K和Long8K两条诊断；原成功八请求没有重跑。两条输出及协议字段也完全一致，退出0。

补充观测原始INFO：`INFO context=8192 kv=fp16 kv_resident=0 expert_slots=5567 expert_cache_mib=7643 expert_slots_primary=5567 expert_cache_primary_mib=7643 spec=4 mtp_max=0 lookup=0 vram_free_mib=1082 cvec=0 arena_mib=33812 pool_workers=21 pcie_frac=0.16 spec_min_p=0.50 conversation_cache_mib=0 conversation_cache_slots=4 conversation_cache_min_free_mib=2560 tail_role_token=-1 vram_elastic=0 engine=0.1.39-orca`。

| 运行中观测项 | 计数 |
| --- | ---: |
| 成功原生CreateAllocation调用 | 1143 |
| 调用者页面分配调用 | 0 |
| 调用者页面分段数 | 0 |
| 最大调用者页面请求，字节 | 0 |
| 标准分配调用 | 0 |

这两条冻结模型请求没有使用调用者页面路径。外部导入和大于2GiB的自有Host分配已由独立检查覆盖，但这次模型没有实际触发；补齐这些路径不能解释既有Short decode差距。PREFILL_EXPERT_U4 host_bytes是CPU资产的总量，不能据此认定存在单个约26GB原生导入。

运行中打印改变了测试进程的Host行为，补充观测只用于路径确认。此前严格同条件对照的Short约2%差距和长历史下缩小的现象仍未定位；本轮不覆盖原失败，不放宽原性能门槛。L0仍为推荐后端，Direct仍为WIP。

[JSON](../../../sycl/orca/direct-native-import-model-a770-windows-2026-11-08.json) · 精确源码与协议日志 (`direct-native-import-model-a770-windows-2026-11-08.json.gz`, raw capture retained locally)

复核：`python -X utf8 build-orca-import-model-20261108/publish.py check`。归档包含运行脚本、精确运行时源码、链接命令、模块列表、协议日志及原始观察缺口；不包含模型资产或EXE。新GPU采样使用新目录，不覆盖本次结果。
