# A770：最大窗口优先录图

2026-11-08，Windows10，Arc A770 16GB，驱动32.0.101.9034，oneAPI2025.3.2。

仅将冻结Q2-fork Verifier::warm的录图循环由T=1…4改为4…1。不增加录图遍数，不释放、共享或重绑线程池。HC128／MTP4／min-p0.5／reserve1536MiB、数学包、其他Host对象和完整运行时源码均保持；与当前spec6／0.6默认版本分开。

| 原生线程存储 | 顺序录图 | 倒序录图 |
| --- | ---: | ---: |
| 线程池总量 | 344MiB | 332MiB |
| 线程池数量 | 8 | 8 |
| 较早的scratch池，每线程 | 2048／4096B | 1024／2048B |
| 当前四个绑定池合计 | 221.5MiB | 221.5MiB |
| 两块旧private池合计 | 98.25MiB | 98.25MiB |

实测避免分配12MiB（线程池总量下降3.488%），来自两块较小scratch池，不是消除了旧private池。全部句柄／字节分区和队列／录图引用已复核。不同启动的实际专家槽受实时全局驻留限制，不能把槽数变化直接算成本补丁收益，也不能把332MiB全部当作相对L0的额外占用。

READY原始INFO：`INFO context=8192 kv=fp16 kv_resident=0 expert_slots=5705 expert_cache_mib=7832 expert_slots_primary=5705 expert_cache_primary_mib=7832 spec=4 mtp_max=0 lookup=0 vram_free_mib=1240 cvec=0 arena_mib=33812 pool_workers=21 pcie_frac=0.16 spec_min_p=0.50 conversation_cache_mib=0 conversation_cache_slots=4 conversation_cache_min_free_mib=2560 tail_role_token=-1 vram_elastic=0 engine=0.1.39-orca`。

完整模型原始INFO：`INFO context=8192 kv=fp16 kv_resident=0 expert_slots=5771 expert_cache_mib=7922 expert_slots_primary=5771 expert_cache_primary_mib=7922 spec=4 mtp_max=0 lookup=0 vram_free_mib=1148 cvec=0 arena_mib=33812 pool_workers=21 pcie_frac=0.16 spec_min_p=0.50 conversation_cache_mib=0 conversation_cache_slots=4 conversation_cache_min_free_mib=2560 tail_role_token=-1 vram_elastic=0 engine=0.1.39-orca`。

同一EXE另测Code2K×2、Short×2、Long8K×2、Prose2K、Short，共8请求／850输出；全部输出token及8项协议字段匹配冻结参考，进程退出0。独立读取原始协议再次复核；未加载SYCL、UR、Level Zero、IGC或GMM DLL。没有新增PPL或Linux GPU结论。

随后单独做Short性能对照。控制组和候选的运行时源码逐字节相同，归一化链接参数一致，仅Verifier对象的warm循环顺序变化。每个引擎4条Short，首条用于准备／热身，后三条取中位数；decode使用引擎decode_ms，准备保留在TTFT和整请求。预先固定实际5771槽、全部输出／8字段、各指标控制漂移绝对值≤1%的条件；正序失败或decode回退即停止，不反序、不重试。

| 顺序／指标 | 控制时间ms | 候选时间ms | 候选耗时变化 | 两端控制漂移 |
| --- | ---: | ---: | ---: | ---: |
| forward／decode_ms | 1712.237 | 1715.900 | +0.214% | +0.780% |
| forward／ttft_ms | 687.722 | 688.832 | +0.161% | +0.588% |
| forward／request_ms | 2363.076 | 2367.670 | +0.194% | +0.723% |

本次容量、输出与正序漂移条件均通过，但decode耗时表观增加约0.21%，所以未进入反序。该幅度可能属于噪声，不能据此确认真实回退；同样没有建立速度收益。本次未通过预先固定的采用条件，原始数据和所有门槛保持，不追加采样找好结果。

候选未移入生产源码，部署EXE／参数及默认后端保持；L0仍为推荐路径，Direct仍为WIP。这个初始化内存结果不能解释此前Short约2%的差距。此前同条件长历史对照中，Direct的Short吞吐低约2.254%／2.077%，Repeat8K低约1.359%／0.308%；差距随历史变小，因果根源仍未证明。本轮不是新的Direct／L0速度比较。

[原线程池引用](orca-direct-thread-pools-2026-11-08.md) · [此前同槽完整对照](orca-direct-native-host-2026-11-08.md)

[JSON](../../../sycl/orca/direct-warm-order-a770-windows-2026-11-08.json) · 精确源码、构建命令与原始协议 (`direct-warm-order-a770-windows-2026-11-08.json.gz`, raw capture retained locally)

离线复核：`python -X utf8 build-orca-warm-order-20261108/publish.py check`。新采样使用新目录和归档中的固定配置，不覆盖原始成功或失败；模型资产沿用文件身份缓存，不重新全量哈希。
