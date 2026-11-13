# Orca：默认提前预读下一窗口首 token 的 PLE

2026-11-05，Windows 10 22H2，Arc A770 16 GB，驱动32.0.101.9034，oneAPI2025.3。
`sycl/src/program/generate.cpp` 的单设备服务路径默认采用：提交当前 commit，
计算已确定的下一窗口首 token 的16个 n-gram 行，交给现有 PLE reader 预读，再执行 MTP；
下一窗口原 gather 消费前仍先 collect。两个前驱来自已采纳输入历史 consumed，
不能使用尚未等完 commit 的 ss.ple_prev。只在 MTP 启用、请求未结束、有下一位置、
PLE 就绪且无 layer-split 时启动这项准备。不猜未来 token，也不提前处理未知路由。

复用 reader 私有缓冲和 I/O worker，不增加线程、内存池、GPU拷贝或数学内核，
不覆盖 verifier／MTP 使用中的映射输入。提前预读只能覆盖已知首行；
后续多行 gather 仍可能先 collect 后重新组批，不能保证减少一个 reader ticket。
开关 `STRATA_ORCA_PLE_ROW0` 在进程启动时读取一次：缺省／1开启，0回退，其他值拒绝。
不增加热循环环境查询、计数日志或时钟；不为凑批等待未来任务。
prompt 双缓冲、专家 Stager 和自适应缓存已有的依赖与回收等待均保留。

本次从实际当前仓库源码重新生成、编译并链接 Direct／L0 两份完整 generate Host TU，
初次 Direct 缺省开启运行先通过两次2K，随后8K输出全零，检查立即停止；失败保留。
日志显示首个7999位置的验证窗口已有248320个非有限logits和10240个非有限residual，
发生在该请求首次新增预读之前，仍不能排除前序请求状态的影响。
这与原Direct 8K间歇故障表征一致，不能宣布该故障解决，也不因后续通过而删除这次失败。

后续复用这两份完全相同的EXE，不再重编译。固定顺序为 Direct关→L0缺省→L0关→L0开→Direct缺省→Direct开，
每进程执行2K→2K→8K→Short，共24次；逐token、生成数、草稿提供／采纳、结束原因及reuse/resume均核对。
后续检查保留所有不匹配，只有相同8K全零表征允许继续采集，其他差异会拒绝证据检查。
每次长历史由2K开始；未用Short预热8K。缺省、开启和回退均保留原有 min-p0.5。
固定5771专家槽、reserve1536MiB、MTP4、累计门槛0、read256、decay0.95、suffix0、
cost-window0、prompt-cache0，未启用逐层profiling或GPU时间戳；既有全零条件诊断保留。

| 后续进程 | 设置 | Code2K第一次 | Code2K第二次 | Long8K | Short28 |
| --- | --- | --- | --- | --- | --- |
| direct | 0 | 匹配 | 匹配 | 匹配 | 匹配 |
| l0 | 缺省 | 匹配 | 匹配 | 匹配 | 匹配 |
| l0 | 0 | 匹配 | 匹配 | 匹配 | 匹配 |
| l0 | 1 | 匹配 | 匹配 | 匹配 | 匹配 |
| direct | 缺省 | 匹配 | 匹配 | 匹配 | 匹配 |
| direct | 1 | 匹配 | 匹配 | 匹配 | 匹配 |


这是当前服务源码接入检查，链接其余冻结通用M16数学pack与运行库；不是最新生产U4的完整重建，
部署EXE和启动JSON未替换，重新构建服务后生效。CLI generate、多设备、所有采样组合与Linux GPU尚未验证。
未把这些功能请求作为新的性能成绩，也未关闭原Direct 8K间歇故障。
[此前隔离候选](orca-ple-row0-2026-11-05.md)的90次协议通过、Short变化为Direct +0.165%／L0 +0.071%，
尚未建立稳定加速；这些旧数字不能标成当前源构建的性能。

```powershell
python -X utf8 build-orca-slm-async-20261104/ple_row0_source_followup.py verify r2
python -X utf8 build-orca-slm-async-20261104/publish_ple_row0_source.py --check
```

后续首次准备r1只生成了文件，未编译／运行，随后用r2改为复用原EXE；原准备与说明保留。
重测先prepare新目录再run，保留旧尝试。同条件复现要求相同模型、设备、驱动、数学pack、
运行库、argv/env与请求顺序；归档提供精确输入、参考、当前源码、差异、生成TU、命令、协议及依赖哈希。
源码、命令与配置在等待共享实验锁前准备；编译、GPU、重型哈希与归档持锁。
精确JSON (`ple-row0-source-a770-windows-2026-11-05.json`, raw capture retained locally) ·
原始证据 (`ple-row0-source-a770-windows-2026-11-05.json.gz`, raw capture retained locally)
