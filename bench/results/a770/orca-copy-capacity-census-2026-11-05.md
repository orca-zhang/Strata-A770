# DG2 复制描述符容量：真实模型调用计数

2026-11-05，Windows 10 22H2，Arc A770 16 GB，驱动32.0.101.9034，oneAPI2025.3。
检查64项复制描述符暂存数组是否限制当前路径。只增加诊断计数，容量仍为64，
`STRATA_ORCA_DG2_BATCH_OPS=64`；没有改变提交、同步、队列或数学内核。

重编译全部69个原生适配头使用者、XMX及四个检查器，共74个一致的TU，保留11个不使用此头的对象。
四个GPU检查器通过。真实服务入口分别以持久镜像回填关闭／开启启动新进程，
各直接2K→2K→8K→Short；八次完整输出、草稿采纳／提供、结束原因与无prefix复用均匹配。
使用实际源码接入后的服务TU及冻结的通用M16数学pack／运行库；不是生产U4完整重建。
固定5771槽、reserve1536MiB、MTP4、step/last min-p0.5、累计0、read256、decay0.95、
suffix0、cost-window0和prompt-cache0。镜像／临时源的区别见[回填对照](orca-mirror-refill-2026-11-05.md)。

首次构建的74项编译、链接和三个GPU检查通过；随后router检查器因本实验漏传既有时钟校准参数退出，
模型尚未开始。原失败目录与日志保留。独立续跑复用原构建对象，检查器传入既有校准值
`STRATA_ORCA_DG2_TIMESTAMP_HZ=19200000`、`STRATA_ORCA_DG2_TIMESTAMP_BITS=36`，
四项GPU检查重新通过后才运行模型。未修改模型环境、时钟校验条件、容量或调度来绕过失败。
续跑的默认回填四次协议通过，但随后解析器预期全局析构输出的final快照而失败。
服务原本使用 `std::_Exit(0)` 正常结束，不调用全局析构。第二次失败目录同样保留；
独立审计逐项核对已有四个DONE边界，并另启相同EXE／参数的镜像回填进程。
统计范围明确止于DONE边界，不包含退出清理，不补造final快照，也没有修改退出行为。

## 计数范围

只统计非零字节的 `Stream::copy` 调用及对应暂存flush，不覆盖fill或绕过该入口的strided copy_2d。
READY前关闭计数，排除初始化和预热录图；每次DONE前输出累计快照。
区分eager调用与recording期间调用。一次已录列表的GPU重放不会重新经过此Host API，
因此不是实际GPU复制执行次数／物理传输量，也不是GPU队列中尚未完成的复制数量。
cap指暂存达到64项、触发flush的Host事件；flush本身不等于GPU等待。
普通调用中的同步、队列切换等仍可提前flush。快照不是跨线程事务；相邻DONE的差值可能包含异步尾部，
仅按观测边界列出，不作独占阶段归因。逐次核对八个DONE边界的计数守恒，不包含退出清理阶段。

| 回填开关 | DONE后位置 | eager入暂存增量 | eager容量触发增量 | eager最大已flush批次 | recording入暂存增量 | recording容量触发增量 | recording最大已flush批次 |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 0 | Code2K | 16528 | 0 | 32 | 0 | 0 | 0 |
| 0 | Code2K | 16208 | 0 | 32 | 0 | 0 | 0 |
| 0 | Long8K | 20637 | 0 | 32 | 0 | 0 | 0 |
| 0 | Short28 | 695 | 0 | 32 | 0 | 0 | 0 |
| 1 | Code2K | 16528 | 11 | 64 | 0 | 0 | 0 |
| 1 | Code2K | 16208 | 11 | 64 | 0 | 0 | 0 |
| 1 | Long8K | 20637 | 40 | 64 | 0 | 0 | 0 |
| 1 | Short28 | 695 | 0 | 32 | 0 | 0 | 0 |

## 第四个DONE边界的累计计数

| 回填开关 | Host copy调用 | Host API字节参数之和 | 绕过暂存的调用 | eager入暂存／容量触发／峰值 | recording入暂存／容量触发／峰值 |
| --- | ---: | ---: | ---: | ---: | ---: |
| 0 | 54,068 | 78,108,539,952 | 0 | 54068 / 0 / 32 | 0 / 0 / 0 |
| 1 | 54,068 | 78,108,539,952 | 0 | 54068 / 62 / 64 | 0 / 0 / 0 |

八个DONE边界分别核对：调用数＝eager入暂存＋recording入暂存＋绕过；各模式的入暂存数等于flush大小直方图的加权和，
容量触发数等于64项flush桶计数，峰值等于最大非空flush桶。保留全部65项原始直方图及每次快照。
原子计数与日志会改变执行时序，**不发布诊断进程的吞吐／TTFT收益**；也未测每次容量触发的阻塞时间。
这不是1024项HostTask环、512项临时专家环、16项Stager环、GPU专家staging环、FP16双缓冲或驱动硬件队列的占用测量。
源码／生产配置与已部署EXE未改，候选诊断保持隔离；Linux GPU与原8K间歇故障仍未关闭。

当前routed预填充的[GPU专家staging](../../../sycl/src/prefill/prefill.cpp#L111)仍为8槽，
[FP16反量化缓冲](../../../sycl/src/prefill/prefill.cpp#L204)为2槽；持久镜像不会绕过它们。
`STRATA_PREFILL_RING`只在stream-all条件满足时改变大环；当前条件下仍返回STAGE=8。
强制stream-all会额外搬运未被路由选中的专家，不是只改routed环容量的公平对照。
因此不能由本报告的Host描述符计数排除GPU预取深度不足；GPU槽复用的等待／重叠尚未测量。

```powershell
python -X utf8 build-orca-slm-async-20261104/copy_capacity_boundary_audit.py verify
python -X utf8 build-orca-slm-async-20261104/publish_copy_capacity_census.py --check
```

以上命令核对本机冻结证据；重测须在新目录按归档中的源树／编译命令重建，并在相同输入历史的两个新进程中分别关闭／开启回填。
原prepare/run及两次失败均原样保留，原run仍包含已披露的诊断假设错误；独立边界审计复核默认臂并补跑开启臂。
输入／参考／源变更／命令在锁前准备；
复制冻结构建树、编译、GPU、重型哈希与归档持共享实验锁。同条件需相同模型、输入顺序、
设备／驱动、数学pack／运行库和完整参数。
JSON (`copy-capacity-census-a770-windows-2026-11-05.json`, raw capture retained locally) ·
原始证据 (`copy-capacity-census-a770-windows-2026-11-05.json.gz`, raw capture retained locally)

默认回填的四次请求未观察到64项容量触发；扩大该Host数组不能消除本轮不存在的容量触发提交。
异步镜像回填的四次请求观察到62次64项容量触发，说明该路径确实能用满暂存；这尚未证明GPU等待或扩容有吞吐收益。
