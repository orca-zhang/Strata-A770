# Orca：对齐显存统计与专家槽数

2026-11-08，Windows10，Arc A770 16GB，驱动32.0.101.9034，oneAPI2025.3.2。

本轮仅启动到 READY，不发送 GEN，不产生吞吐、TTFT 或 PPL 结论。固定原有 HC128 / MTP4 / min-p0.5 / 5771实际槽 / reserve1536MiB；数学包、Host对象和权重身份保持冻结。原生侧使用通过19项检查的当前运行时，加上显存查询打印；L0第一轮使用原冻结EXE。

| 顺序 | 引擎 INFO 空闲 MiB | 独立 Sysman 空闲 MiB | 独立 WDDM 前／后 MiB |
| --- | ---: | ---: | ---: |
| l0 | 2167 | 1533.977 | 1538.059 / 1533.992 |
| native | 1247 | 1247.141 | 1247.156 / 1247.156 |
| l0 | 2179 | 1534.930 | 1534.945 / 1534.945 |

三个进程均实际5771槽、7922MiB专家缓存并正常退出。原生返回值逐次等于进程预算余量、自身本地分配余量、全局驻留余量的最小值；最后由全局驻留余量约束，并与独立Sysman查询一致。观察进程自身usage=0，不能当成模型进程的用量。

L0的INFO高于随后独立查询约633／644MiB。两种外部全局查询彼此一致；但这些采样并非同一原子快照，本轮不能仅凭差值确定驱动占用、统计刷新或查询失效的原因。按外部全局口径，本轮原生比两侧L0少约287MiB空闲，不能把INFO的约926MiB差值全算成额外占用。

离线重放原有sized-slot计算：显式5500个最大blob的预算得到5771个按层大小切分的槽。旧5763槽记录相差8个槽，合计11.206MiB；旧启动日志没有缩容重试，因此对应free_room容量上限触发。其当时可用量仅能反推区间，旧记录没有分项计数，根因仍未确定。保留旧失败，未降低reserve、减少对照槽数或关闭全局保护。

第二轮仅重编冻结L0的Host查询对象，增加内部Sysman与DXGI分项打印；其他冻结对象、库与数值核保留。三个READY协议和全部查询公式再次检查通过。两侧L0所有内部查询均have_sysman=0；因此实际返回DXGI进程预算余量，未施加全局空闲量约束。这直接解释本轮INFO的统计路径缺口；Sysman内部具体在哪个分支失败尚未细分。

L0内部最后一次查询：have_sysman=0，Sysman free=0.000MiB，INFO=2156MiB，独立Sysman=1527.023MiB。

L0内部最后一次查询：have_sysman=0，Sysman free=0.000MiB，INFO=2155MiB，独立Sysman=1532.402MiB。

修复候选保留DXGI／Sysman原有约束，并在Windows容量查询中追加原生D3DKMT全局驻留上限。适配器身份／本地段拓扑仅发现一次；每次容量检查刷新驻留量，不进入kernel派发循环。不新增库依赖。

实际Host代码通过六类查询故障／恢复、拓扑缓存／驻留刷新检查，未提交GPU命令。严格5771槽检查的首次模型启动仅得到5734槽，已停止并保留失败；未调低reserve或关闭保护。随后独立功能验证复用同一EXE，按实时容量继续完成2K×2、Short×3、8K×2、Prose2K共八条请求，输出及草稿计数与冻结参考全部一致，正常退出。该功能验证不能使原同槽数性能门槛转为通过，也不构成速度或PPL结论。

功能验证原始INFO：`INFO context=8192 kv=fp16 kv_resident=0 expert_slots=5673 expert_cache_mib=7788 expert_slots_primary=5673 expert_cache_primary_mib=7788 spec=4 mtp_max=0 lookup=0 vram_free_mib=1461 cvec=0 arena_mib=33812 pool_workers=21 pcie_frac=0.16 spec_min_p=0.50 conversation_cache_mib=0 conversation_cache_slots=4 conversation_cache_min_free_mib=2560 tail_role_token=-1 vram_elastic=0 engine=0.1.39-orca`。配置的1536MiB reserve保留；显式缓存初始化后仍会分配图缓冲，不能把配置值当成READY时的实际空闲量保证。

已将这份精确检查过的Host源码移入sycl/orca/engine.cpp，并冻结采用时的源码及差异。仅源码采用；部署EXE、配置、README吞吐标题与Linux GPU状态均未改。重编译后生效。

Intel参考源码的Windows Sysman getState使用PDH计数器取得Dedicated Usage；本轮内部have_sysman=0，不能把PDH采样滞后说成已证实原因。缓存源码commit：`9e866e5f9b8b3281cb6d8217a925594caa67cf76`。

准备阶段曾误把冻结源码清单的metadata字典当成摘要字符串，在编译前停止；原脚本、源文件和失败保留，随后按既有清单的sha256字段读取。新增诊断最初命名inspect.py，遮蔽Python标准库inspect，导致后续导入失败；已更名为check_ready.py并保留失败记录。Linux GPU未测试，部署EXE和配置未改。

[JSON](../../../sycl/orca/direct-memory-budget-a770-windows-2026-11-08.json) · 精确源码与日志归档 (`direct-memory-budget-a770-windows-2026-11-08.json.gz`, raw capture retained locally)

复核：`python -X utf8 build-orca-memory-budget-20261108/publish.py check`。新硬件采样必须使用新目录，不覆盖已有记录。
