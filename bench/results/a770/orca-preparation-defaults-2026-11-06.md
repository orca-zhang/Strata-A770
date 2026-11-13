# Orca：采用元数据提前准备与持久镜像批量回填

2026-11-06，用户接受收益幅度较小的改进后，两项优化进入 Orca L0 源码默认值，重建后生效；Direct保留显式开启。
沿用已有队列、worker、缓冲和同步接口；开关只在进程启动时读取一次。

| 优化 | 默认行为与范围 | 回退方式 |
| --- | --- | --- |
| MTP 位置元数据提前准备 | 在首个 round 提交前准备已知的后续位置，共用提交前的发布屏障；置信度仍决定是否执行后续草稿 | `STRATA_MTP_PRESTAGE=0` |
| 持久镜像批量回填 | 服务归还借用专家槽时，对持久 GGUF USM 镜像异步提交；每256笔及最终同步后再发布驻留表 | `STRATA_ORCA_MIRROR_REFILL=0` |

显式1开启、0关闭，其它值报错；Direct及原 SYCL 源码默认保持旧行为。临时环形来源、强制阻塞回填和未完成的数据依赖保留原路径。
MTP 的 round 会读取 `2*T` 行映射元数据，因此准备发生在提交之前，不能在它读取期间覆盖这些槽。
批量回填仅覆盖服务的持久镜像分支，CLI generate 未接入。

采用依据是减少重复准备／逐槽等待，接受有限的小幅改善，不发布新的整版提速百分比：

- [原 MTP 元数据实验](orca-mtp-prestage-2026-11-05.md)：90次完整协议匹配，Direct／L0净decode变化为 **+0.223%／−0.024%**；幅度接近波动，没有建立稳定提速。
- [原镜像回填实验](orca-mirror-refill-2026-11-05.md)：逐字节检查8,016槽、11,442,329,600字节及驻留表；Direct／L0的8K TTFT少用 **1.788%／0.316%**，控制漂移 **+0.444%／+0.037%**，每位置每臂仅一次。原源码接入另通过16条完整协议。
- [参数栈数组](orca-direct-argument-array-2026-11-06.md)已在源码采用；旧Short对照仅+0.472%，原≥1%收益门仍保留为失败，不重复计算本次收益。

以上来自不同冻结实验，不相加，也不把逻辑准备顺序当作已测得的CPU／GPU重叠时长。

首轮对Direct／L0共同启用默认值，已编译、链接两个后端；Direct首次及重复2K完整协议匹配，
第三条8K在预填充退出，报 `U4 expert nonfinite activation`，退出码1，未生成该请求完整输出。
保留原始源文件、构建、日志和失败；没有将它写为通过，也没有重复同一试验取得通过。
此前Direct已有8K间歇异常，本次不能确定是否由新组合引起，因此Direct默认回退为关闭。
随后收敛为主用的L0默认，使用新目录、重新生成并编译两个Host单元、链接后验证。
缺省开启与两项显式0两种设置的 **8/8条完整协议匹配**，逐条核对全部tokens、草稿采纳／提供数、
结束原因、reuse／resume及原始DONE记录。两设置均先执行首次2K→重复2K→8K，再执行Short；
没有先用Short预热8K，也没有新增GPU回读或数值内核变更。最终源码摘要与本次生成输入一致。

本次只重编实际仓库的MTP及generate两个完整Host单元／后端，链接11月6日冻结完整引擎、U4/M16数学依赖，
首轮Direct使用已校准的全局显存记账Runtime；并非本机生产SG16包的完整重建。
固定5771槽、reserve1536MiB、MTP4、step／last min-p0.5、cumulative0、U4/min16/M16、read256、
adapt32/decay0.95、suffix／cost-window／prompt-cache0，Direct BCS1。
首轮原计划Direct缺省开启／两项显式0，再L0缺省开启／两项显式0，在上述Direct失败时即停止。
后续L0按缺省开启→两项显式0；每个新进程直接运行2K→2K→8K→Short。
不添加逐算子时间戳、回读诊断或额外轮询；原始耗时保存作证据，本次不作性能比较。
准备、初始化及prefill排除在净decode之外；READY、TTFT和完整请求分别保存。

本机生产EXE／主启动配置未替换，现有HC SG16与采样设置保持。没有新增PPL或Linux GPU结论，原Direct8K间歇故障仍保留。
共享计算context、启动后重录窗口图和扩大缓冲环未随这两项采用；它们各自的原始实验单列。

本机复核，不重新运行GPU：

```powershell
python -X utf8 build-orca-preparation-adopt-20261106/l0-r2/adopt.py check
python -X utf8 build-orca-preparation-adopt-20261106/publish.py check
```

同条件复测须保留相同设备、驱动、编译器、模型／初始profile、冻结引擎与数学包、参数、输入及请求顺序，使用新输出目录。
归档保存精确源代码、前后差异、编译／链接命令、完整协议、配置及依赖摘要；模型资产使用stat-cached SHA256身份核验。
编译、GPU、重型哈希及归档持共享实验锁；不分发权重或二进制。

[JSON](../../../sycl/orca/preparation-defaults-a770-windows-2026-11-06.json) · 精确文本归档 (`preparation-defaults-a770-windows-2026-11-06.json.gz`, raw capture retained locally)
