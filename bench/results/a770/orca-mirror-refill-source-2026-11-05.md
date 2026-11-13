# 持久镜像批量回填接入服务源码（可选）

2026-11-05，Windows 10 22H2，Arc A770 16 GB，驱动32.0.101.9034，oneAPI2025.3。
`sycl/src/program/generate.cpp` 已接入 `STRATA_ORCA_MIRROR_REFILL=0/1`，默认关闭。
只在服务 `refill_issue` 中，对已有持久 GGUF USM 镜像复用异步 `fill_slot`；
临时源、其它来源、强制阻塞、每256笔同步及最终同步／驻留表发布保留原行为。
不新增副本、线程、内存池、队列或数学内核。开关在启动时读取一次；无回读诊断或回填计数日志。
CLI generate 与弹性显存回填尚未接入；Linux GPU 尚待用户验证。

本次从实际仓库源码重新生成并编译两个完整 generate Host TU，保留冻结的通用M16数学pack、
运行库及其余对象。Direct／L0 各以开关缺省和显式1启动新进程；各直接执行2K→2K→8K→Short，
共16次完整输出、草稿采纳／提供、结束原因及无prefix复用核对通过。Short在长请求之后，未用它预热8K。
固定5771槽、reserve1536MiB、MTP4、step/last min-p0.5、累计0、read256、decay0.95、
suffix0、cost-window0、prompt-cache0。该检查不作为性能比较，也不是最新生产U4的完整重建。
生产EXE与启动配置未替换。原Direct 8K间歇故障仍未关闭，未宣称广泛PPL验证。

此前隔离候选的字节审计及有限计时结果见[回填对照](orca-mirror-refill-2026-11-05.md)；
不将其成绩标成这次源码重建的成绩。

```powershell
python -X utf8 build-orca-slm-async-20261104/mirror_refill_source.py verify r1
python -X utf8 build-orca-slm-async-20261104/mirror_refill_source.py check r1
```

重测使用prepare新目录后run，保留旧尝试。输入、参考、配置与完整源码在等待锁前准备；
编译、GPU、重型哈希、归档持共享实验锁。同条件重现需相同模型、输入／历史、数学pack、运行库、
设备／驱动和参数。归档含源变更、实际生成的完整TU、命令、协议、配置及依赖哈希。
[JSON](../../../sycl/orca/mirror-refill-source-a770-windows-2026-11-05.json) ·
原始证据 (`mirror-refill-source-a770-windows-2026-11-05.json.gz`, raw capture retained locally)
