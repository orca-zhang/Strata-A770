# A770：host USM 原子能力核对

A770 16GB（PCI ID `8086:56a0`）/ Windows / driver `32.0.101.9034` / DPC++ 2025.3.2。**当前驱动为系统内存只声明普通读写，为显存声明读写与原子操作。原扫描器的 host USM 原子计数用法未获这套平台的能力声明支持；尚未证明它是此前 NaN 或扫描拒绝的根因。**

最初 SYCL 检查返回 `usm_host=1`、`usm_atomic_host=0`，检查程序按预设保护条件退出 2，未发射扫描内核。这个退出是检查程序主动拒绝，不能当作 GPU 原子操作已执行失败的证据。

进一步核对发现，Intel 的 [SYCL 实现](https://github.com/intel/llvm/blob/sycl/sycl/source/detail/device_impl.hpp)将 `usm_atomic_host_allocations` 映射到 `UR_DEVICE_USM_ACCESS_CAPABILITY_FLAG_ATOMIC_CONCURRENT_ACCESS`。该位涉及 CPU/GPU 并发原子访问，**单凭它为 0，不足以断言 GPU 单独执行的 host 原子操作不受支持**。因此又通过 `zeDeviceGetMemoryAccessProperties` 单独读取普通原子与并发原子位。

| 本机分配类型 | 原始能力位 | 普通读写 | 普通原子 | 并发访问 | 并发原子 |
| --- | ---: | --- | --- | --- | --- |
| host | `0x1` | 支持 | 未声明 | 未声明 | 未声明 |
| device | `0x3` | 支持 | 支持 | 未声明 | 未声明 |
| shared，单设备 | `0x3` | 支持 | 支持 | 未声明 | 未声明 |
| shared，跨设备 / system | `0x0` | 未声明 | 未声明 | 未声明 | 未声明 |

这次查询只读取驱动元数据，没有创建上下文、分配 GPU 内存、提交命令或启动内核。结果和 ABI/位解码自检在本机 `build-sycl-a770-down-scan-context-20261028/capabilities.json` 与 `capabilities.py`，SHA256 包含查询脚本、SDK 头文件、实际加载的 `System32/ze_loader.dll` 及此前 SYCL 检查记录。

公开驱动源码也有对应的 DG2 专用定义。在冻结的 Intel compute-runtime commit `3c7d20d781da468dc60a8204f0735004918e0940` 中，[DG2 的实现](https://github.com/intel/compute-runtime/blob/3c7d20d781da468dc60a8204f0735004918e0940/shared/source/xe_hpg_core/dg2/os_agnostic_product_helper_dg2.inl#L211)为：

```cpp
uint64_t ProductHelperHw<gfxProduct>::getHostMemCapabilitiesValue() const {
    return (UnifiedSharedMemoryFlags::access);
}
```

[通用实现](https://github.com/intel/compute-runtime/blob/3c7d20d781da468dc60a8204f0735004918e0940/shared/source/os_interface/product_helper.inl#L895)则返回 `access | atomicAccess`。公开代码的 DG2 定义与本机结果一致；它不是对已安装 Windows 驱动二进制的源码同一性证明，也没有解释更深的硅片、PCIe 或驱动设计原因。不能只修改返回能力位就宣称原子操作已正确实现。源码地址、行号、摘录和完整文件 SHA256 另存于 `source-audit.json`。

原扫描器使用 `malloc_host<int>(514, q)`，GPU 对其中的计数器执行 `atomic_ref::fetch_add`。`memory_scope::device` 限定原子的作用范围，不改变指针所指内存的分配类型；CPU 等待内核结束，也不能补上底层未声明的原子能力。

后续隔离版本把计数器放进 device USM，保留普通 host 快照，用同一 in-order 队列显式上传计数器初值并读回结果，每次两笔各2056字节拷贝。原扫描内核、512 上限、模型数学及失败时的独立 CPU 核对保留。GPU 边界检查、原版和裁剪候选重链接均已完成，证据位于 `build-sycl-a770-down-device-counter-20261028/build.json`。扫描器只在 prefill 使用，此修改本身没有 decode 提速结论。

同一新扫描器的 A1/B1/B2/A2 四进程质量检查已完成：九场景、每进程758个固定目标的必需评分均有限；A1/A2精确匹配冻结原版。候选B1/B2仅首次 followup 的6个评分相同，其余752个logprob不同，其中9个评分位置的top token不同。候选重复性未通过，按预设协议停止于质量阶段，没有启动新测速，也未采用候选。`quality/results.json`、`quality-differences.json`、`validation.json`保留完整结果；这些差异尚未归因于扫描器或驱动，先前真实异常输出与失败记录也全部保留。

源码复核确认，冻结cap10策略只考虑原top10的第8/9位（从0计数），且仅裁剪其中当时未驻留的专家；前8位必须有一个驻留donor，删除的权重质量不超过总量10%，才归一化剩余权重并以零权重donor填回原K10槽。因此数值决策本身依赖驻留表，并不是无条件删除末两位，也不是从所有缺失专家中任选两个。相同router结果遇到不同驻留表可以得到不同裁剪结果；后续诊断需核对实际路由与驻留历史。这是源码上的依赖关系，尚未证明上述两次评分差异确由驻留表变化引起。

两份原始候选协议的进一步核对：各有Short16初始化及九条评分请求，十次`RESUME`均为0；九条评分请求均生成1个token，接受数/提议数均为0。评分前缀没有实际命中复用，也没有评分请求之间的草稿接受数差异，不能用这两项解释752个评分不同。初始化的接受/提议数两次同为11/16。此检查仍未记录GPU执行瞬间的驻留表，也没有证明预填、裁剪或同步中的具体根因。

进一步只读定位已完成，新增`quality-prefix-audit.py`及独立`--check`均通过。四臂各十条请求完整对齐，六条长请求的Down扫描均为`checks=48 experts=0 trigger_tiles=0`，没有触发重算；逐请求换入数量序列也完全相同，共35次、每次32个专家。这些数量相同不证明换入的专家ID相同。

唯一不同的prefill-tail摘要出现在B2的第二条长评分请求`long`：起点7993、tail64、质量122880保持，但直方图digest从A1/A2/B1的`8edf159c6787eafb`变为`388f3db3a97de193`。其他五条长请求的摘要相同。B1/B2各982行原始评分的前63行相同，首个不同值恰为`long`的7993位置、目标74455，logprob为−0.033893756/−0.047160989。即此请求进入首个已记录评分前，预填尾段路由直方图已经不同；后续诊断应同时检查这段预填与当时缓存驻留状态，不能只对最终采样器排查。

此定位没有新增GPU运行或改写旧结果。直方图是在CPU侧从已等到队列完成的路由ID累计，不是扫描器的原子计数；仍未保存逐层路由ID、完整驻留表或有限值中间张量，不能据摘要认定具体竞态、扫描器/驱动根因或修复。复核：`python -X utf8 build-sycl-a770-down-device-counter-20261028/quality-prefix-audit.py --check`。

首版独立重读因Python元组写入JSON后变为数组而比较失败，源码与输出保留在`quality-prefix-attempt1/`。修正版只统一比较时的序列化形式，对同一组原始记录重新生成并独立重读通过；没有重测或变更模型数据。

基于这个已发生的差异，新增不编译内核的残差诊断`build-sycl-a770-down-prefix-repeat-20261029/run.py`。离线自检通过，进程28108（创建时间1793233464502）依次等待既有热评分修正9776和INT8 v2的27224结束，再申请共享锁，不抢占当前任务。四个新进程仍为A1/B1/B2/A2，各重放原Short16及`followup`、`long`两条教师强制评分请求，保留原CLI、30秒启动静置及逐臂原版/cap10开关；这只是故障定位，不替代九场景758目标或完整2K性能验收。

诊断仅增加已有`STRATA_PREFILL_DUMP_R`，从每条长请求的预填最终残差中保留每64位置一行、每行10240个FP32值，分别保存后再提交下一条请求，避免原开关覆盖前一份快照。解析要求精确位置和长度、有限值，另核对197行完整评分、原版评分精确匹配冻结前缀、6297槽/READY≥1024MiB、真实退出及两组48层Down扫描。候选间评分、路由摘要和残差是否重复都单独报告，差异作为诊断结果保留，不判为质量或性能通过。

环境使用原冷采集器同一段构造代码，并核对其完整来源清单；当前完整环境摘要与历史记录不相等，因此只记录这一差异，不声称历史完整环境一致。进一步只读核对确认，历史`quality/protocol.json`和`results.json`只保存`runtime_sha256`，没有完整环境值，无法逐键追溯差异。当前构造器会清除继承的IGC/NEO/Override/CFE设置，再应用冻结配置与覆盖项；这不等于证明历史完整环境相同，也没有建立环境差异是评分分歧的原因。残差读回会增加等待，未复现也不证明修复；每64位置的最终层样本不等于逐层或全部位置捕获，尚未取得新的残差或GPU诊断结果。自检：`python -X utf8 build-sycl-a770-down-prefix-repeat-20261029/run.py --self-check`；入口已排队，不重复启动或修改冻结脚本。

运行时提交、事件和 graph 路径的后续核对见[compute-runtime / UR 优化空间](runtime-paths-2026-10-29.md)。

离线核对，不重新运行 GPU 测试：

```powershell
python -X utf8 build-sycl-a770-down-scan-context-20261028/capabilities.py --check
```
