# A770：当前合格 Direct／L0 运行时对照

2026-11-08，Windows10，Arc A770 16GB，驱动32.0.101.9034，oneAPI2025.3.2。

补齐此前版本边界：Direct直接链接通过19项检查的纯原生运行时CMake对象，源码与准备时已采用版本逐字节一致；包含原生Host分配／页面导入和显式安全trim API，但没有调用trim、自动重录、倒序录图或测试内存审计包装。L0控制为已通过六类查询故障和八请求功能检查的全局驻留统计修正版。

继续冻结HC128／MTP4／min-p0.5／reserve1536MiB、数学包、其他Host对象、profile和资产身份缓存；两端参数完全一致，环境仅差四个既有DG2设置。不是当前spec6／0.6生产版本，也不是整个最新工作树的性能。部署EXE、默认参数、推荐L0及Linux GPU待测状态保持。

预先固定原始84请求：L0／Direct／L0与Direct／L0／Direct；每进程依次2K×2、Short×10、8K×2，Short取后5次中位数。每个进程要求实际5771槽／7922MiB缓存及完整token／8字段一致、退出0；所有指标控制漂移≤3%，无回退门为所有TTFT／decode／整请求耗时增加≤1%。任何容量／质量／驱动故障停止，不重试、不降低reserve、不覆盖此前失败。两个顺序按固定计划执行，速度差本身不触发追加采样。

首轮链接遗漏已记录的oneAPI环境，无法找到libircmt.lib，在启动任何引擎前停止；没有GPU请求、容量判断或性能样本。这不是容量／数值门失败。本轮仅为链接调用补回既有ENV，在独立r2目录继续原计划，首次源码、错误及归档完整保留。

复用合格对象，不重复编译或运行已成功的19项检查。新增DLL检查放在每个引擎全部计时请求之后；Direct要求未加载SYCL／UR／Level Zero／IGC／GMM，L0要求实际加载Level Zero。没有增加逐kernel、逐token或U4诊断计时；准备保留在TTFT／整请求，排除在decode_ms之外。

原生对象SHA256：`f78ecceb3920c07aba62185221daa63a9cba37e3cc3f20c5c629c19752222213`。L0 EXE SHA256：`1e729da29b42687fbc30ca968c91ca01cd628668c3d7d7e49302b7df92b15759`。

完整请求84条／输出7272个，独立原始协议及计算复核通过。

| Decode位置／顺序 | L0 tok/s | Direct tok/s | Direct吞吐变化 | decode控制耗时漂移 |
| --- | ---: | ---: | ---: | ---: |
| first2K／forward | 29.438 | 29.469 | +0.106% | -0.303% |
| repeat2K／forward | 36.828 | 36.626 | -0.548% | -0.319% |
| warmShort／forward | 52.866 | 51.975 | -1.686% | -0.362% |
| first8K／forward | 26.871 | 26.440 | -1.606% | -0.078% |
| repeat8K／forward | 33.819 | 33.669 | -0.442% | -0.174% |
| first2K／reverse | 29.529 | 29.374 | -0.524% | -0.350% |
| repeat2K／reverse | 36.890 | 36.409 | -1.302% | +0.536% |
| warmShort／reverse | 53.002 | 51.911 | -2.058% | +0.260% |
| first8K／reverse | 26.809 | 26.863 | +0.203% | +0.065% |
| repeat8K／reverse | 33.525 | 33.682 | +0.466% | +0.163% |

| 场景／顺序／指标 | L0 ms | Direct ms | Direct耗时变化 | 控制耗时漂移 |
| --- | ---: | ---: | ---: | ---: |
| first2K／forward／ttft_ms | 26008.363 | 23669.713 | -8.992% | +0.738% |
| first2K／forward／decode_ms | 4348.095 | 4343.500 | -0.106% | -0.303% |
| first2K／forward／request_ms | 30298.771 | 27963.136 | -7.709% | +0.601% |
| repeat2K／forward／ttft_ms | 15316.388 | 12575.870 | -17.893% | +0.371% |
| repeat2K／forward／decode_ms | 3475.646 | 3494.800 | +0.551% | -0.319% |
| repeat2K／forward／request_ms | 18753.278 | 16032.014 | -14.511% | +0.242% |
| warmShort／forward／ttft_ms | 705.507 | 636.845 | -9.732% | -0.034% |
| warmShort／forward／decode_ms | 1324.098 | 1346.800 | +1.715% | -0.362% |
| warmShort／forward／request_ms | 1998.156 | 1953.904 | -2.215% | -0.031% |
| first8K／forward／ttft_ms | 33745.164 | 31053.420 | -7.977% | -0.497% |
| first8K／forward／decode_ms | 4763.450 | 4841.200 | +1.632% | -0.078% |
| first8K／forward／request_ms | 38468.129 | 35856.782 | -6.788% | -0.445% |
| repeat8K／forward／ttft_ms | 33749.621 | 30916.047 | -8.396% | -0.119% |
| repeat8K／forward／decode_ms | 3784.899 | 3801.700 | +0.444% | -0.174% |
| repeat8K／forward／request_ms | 37496.195 | 34680.146 | -7.510% | -0.125% |
| first2K／reverse／ttft_ms | 25984.230 | 23292.898 | -10.358% | +0.248% |
| first2K／reverse／decode_ms | 4334.700 | 4357.543 | +0.527% | -0.350% |
| first2K／reverse／request_ms | 30263.106 | 27600.404 | -8.799% | +0.155% |
| repeat2K／reverse／ttft_ms | 15109.777 | 12598.906 | -16.618% | +0.957% |
| repeat2K／reverse／decode_ms | 3469.800 | 3515.587 | +1.320% | +0.536% |
| repeat2K／reverse／request_ms | 18541.291 | 16075.526 | -13.299% | +0.864% |
| warmShort／reverse／ttft_ms | 703.306 | 631.669 | -10.186% | -0.222% |
| warmShort／reverse／decode_ms | 1320.700 | 1348.449 | +2.101% | +0.260% |
| warmShort／reverse／request_ms | 1993.721 | 1950.913 | -2.147% | -0.039% |
| first8K／reverse／ttft_ms | 33720.419 | 30786.786 | -8.700% | +0.228% |
| first8K／reverse／decode_ms | 4774.500 | 4764.850 | -0.202% | +0.065% |
| first8K／reverse／request_ms | 38454.789 | 35514.100 | -7.647% | +0.206% |
| repeat8K／reverse／ttft_ms | 33821.905 | 30832.015 | -8.840% | +0.064% |
| repeat8K／reverse／decode_ms | 3818.000 | 3800.299 | -0.464% | +0.163% |
| repeat8K／reverse／request_ms | 37600.779 | 34594.809 | -7.994% | +0.075% |

实际条件门：`{"quality": true, "capacity": true, "drift": true, "no_regression": false}`。

forward：Short吞吐变化-1.686%，重复8K为-0.442%；绝对差距缩小+1.244个百分点。

reverse：Short吞吐变化-2.058%，重复8K为+0.466%；绝对差距缩小+1.592个百分点。

以上只在各条件门覆盖的范围内解释。未通过的漂移／回退条件不会因完成全部请求而转为通过；上下文、草稿轨迹和工作集随场景同时变化，不能单凭长上下文差距推导API、锁或某个kernel为因果根源，也不能累乘历史优化收益。

原始INFO：

- 0／l0：`INFO context=8192 kv=fp16 kv_resident=0 expert_slots=5771 expert_cache_mib=7922 expert_slots_primary=5771 expert_cache_primary_mib=7922 spec=4 mtp_max=0 lookup=0 vram_free_mib=1507 cvec=0 arena_mib=33812 pool_workers=21 pcie_frac=0.16 spec_min_p=0.50 conversation_cache_mib=0 conversation_cache_slots=4 conversation_cache_min_free_mib=2560 tail_role_token=-1 vram_elastic=0 engine=0.1.39-orca`
- 1／native：`INFO context=8192 kv=fp16 kv_resident=0 expert_slots=5771 expert_cache_mib=7922 expert_slots_primary=5771 expert_cache_primary_mib=7922 spec=4 mtp_max=0 lookup=0 vram_free_mib=1212 cvec=0 arena_mib=33812 pool_workers=21 pcie_frac=0.16 spec_min_p=0.50 conversation_cache_mib=0 conversation_cache_slots=4 conversation_cache_min_free_mib=2560 tail_role_token=-1 vram_elastic=0 engine=0.1.39-orca`
- 2／l0：`INFO context=8192 kv=fp16 kv_resident=0 expert_slots=5771 expert_cache_mib=7922 expert_slots_primary=5771 expert_cache_primary_mib=7922 spec=4 mtp_max=0 lookup=0 vram_free_mib=1475 cvec=0 arena_mib=33812 pool_workers=21 pcie_frac=0.16 spec_min_p=0.50 conversation_cache_mib=0 conversation_cache_slots=4 conversation_cache_min_free_mib=2560 tail_role_token=-1 vram_elastic=0 engine=0.1.39-orca`
- 3／native：`INFO context=8192 kv=fp16 kv_resident=0 expert_slots=5771 expert_cache_mib=7922 expert_slots_primary=5771 expert_cache_primary_mib=7922 spec=4 mtp_max=0 lookup=0 vram_free_mib=1201 cvec=0 arena_mib=33812 pool_workers=21 pcie_frac=0.16 spec_min_p=0.50 conversation_cache_mib=0 conversation_cache_slots=4 conversation_cache_min_free_mib=2560 tail_role_token=-1 vram_elastic=0 engine=0.1.39-orca`
- 4／l0：`INFO context=8192 kv=fp16 kv_resident=0 expert_slots=5771 expert_cache_mib=7922 expert_slots_primary=5771 expert_cache_primary_mib=7922 spec=4 mtp_max=0 lookup=0 vram_free_mib=1447 cvec=0 arena_mib=33812 pool_workers=21 pcie_frac=0.16 spec_min_p=0.50 conversation_cache_mib=0 conversation_cache_slots=4 conversation_cache_min_free_mib=2560 tail_role_token=-1 vram_elastic=0 engine=0.1.39-orca`
- 5／native：`INFO context=8192 kv=fp16 kv_resident=0 expert_slots=5771 expert_cache_mib=7922 expert_slots_primary=5771 expert_cache_primary_mib=7922 spec=4 mtp_max=0 lookup=0 vram_free_mib=1163 cvec=0 arena_mib=33812 pool_workers=21 pcie_frac=0.16 spec_min_p=0.50 conversation_cache_mib=0 conversation_cache_slots=4 conversation_cache_min_free_mib=2560 tail_role_token=-1 vram_elastic=0 engine=0.1.39-orca`

[此前冻结运行时对照](orca-direct-native-host-2026-11-08.md) · [原生运行时资格](orca-direct-thread-trim-2026-11-08.md) · [L0统计资格](orca-direct-memory-budget-2026-11-08.md)

[JSON](../../../sycl/orca/direct-qualified-contexts-r2-a770-windows-2026-11-08.json) · 精确源码、计划、构建命令与原始协议 (`direct-qualified-contexts-r2-a770-windows-2026-11-08.json.gz`, raw capture retained locally)

离线复核：`python -X utf8 build-orca-qualified-contexts-20261108/publish.py check`。新采样用新目录，沿用固定配置和资产身份缓存，不覆盖本次数据。
