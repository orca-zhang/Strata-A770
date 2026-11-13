# Orca Direct：省掉已经完成的跨队列驱动等待

2026-11-06，Windows 10，Intel Arc A770 16 GB，驱动32.0.101.9034，oneAPI2025.3.2。
本组 L0 指 Orca L0，没有重测原生产 SYCL。

隔离候选只改 Windows／Linux 原生运行库的跨队列依赖过滤：先验证 completion 身份和序号，
再跳过 owner 或 immutable observer 的单调缓存中已确认完成的依赖。
没有新增完成查询、轮询、线程、分配、等待凑批、数学内核或 ABI／状态布局变化。
未完成依赖、重复队列的最大序号合并、Host gate、最终可见性屏障保持原逻辑。
没有生产源码接入或部署变更；本报告保留候选的原始验收门槛。

独立 BCS demo 的原版／候选各运行12波、36个完整快照和保护区检查。
24次已完成前缀提交的驱动等待从 **24次降至0次**；12次未完成尾部提交仍各保留一次。
检查覆盖 observer 缓存与 owner 缓存、重复依赖、新的被阻塞尾部、非法completion和输入改变。
这不是模型内的调用计数，也不把省掉系统调用直接折算成 tok/s。
另通过920次内存回放／460张录制、64项HC输出／CPU／保护区检查。
Linux／WSL候选构建、四项CPU检查及依赖审计通过，没有Linux GPU执行。

首版诊断编译误用clang-cl不接受的异常选项，编译失败，尚未运行GPU；
第二版检查器把上取整为64KiB的分配按4KiB计算尾部保护区，原版先失败，候选未执行。
修正版按返回容量检查；两次失败源码、命令、日志及结果全部保留，没有改写或覆盖。

整模型只重新链接原生运行库；49个kernel Host对象、引擎核心、数学pack、模型／MTP／U4资产保持相同。
两版运行库的实际编译命令已核对，编译器、优化、异常、CRT及defines相同，仅源码／include和输出路径不同。
固定实际5771槽、reserve1536MiB、U4/min16/M16、read256、adapt32/decay0.95、MTP4、
step／last min-p0.5、cumulative/suffix/cost-window/prompt-cache0；Direct BCS1、ISA／线程内存device。
顺序L0→原Direct→候选→原Direct→L0，每个为新进程，执行2K→2K→8K→8K→Prose2K→Short×15。
长请求每位置每臂一次；Short丢弃十次，保留五次中位数；两侧同后端控制取几何均值。
全部 **100/100完整协议匹配**，包含tokens、草稿采纳／提供、结束、reuse／resume。
预设控制漂移不超过3%、候选时间不回退超过3%、Short decode至少提高1%；漂移不是置信区间。
净decode排除初始化／准备／prefill；TTFT和整请求分别测量，不把TTFT加上重叠的引擎decode。

| 场景 | L0 tok/s | 原Direct | 候选 | 候选／原Direct吞吐变化 | 候选／L0吞吐变化 |
| --- | ---: | ---: | ---: | ---: | ---: |
| Code2K/first | 29.436 | 28.924 | 20.744 | -28.281% | -29.527% |
| Code2K/repeat | 36.885 | 35.955 | 35.905 | -0.139% | -2.657% |
| Long8K/first | 29.502 | 29.380 | 29.012 | -1.254% | -1.662% |
| Long8K/repeat | 33.625 | 33.552 | 33.531 | -0.064% | -0.279% |
| Prose2K | 32.913 | 32.864 | 32.913 | +0.150% | +0.002% |
| Short | 52.160 | 50.832 | 51.009 | +0.350% | -2.206% |

| 场景 | TTFT候选／原Direct耗时变化 | TTFT候选／L0耗时变化 | 整请求候选／L0耗时变化 |
| --- | ---: | ---: | ---: |
| Code2K/first | +4.699% | +7.895% | +12.822% |
| Code2K/repeat | +0.941% | -12.546% | -9.668% |
| Long8K/first | +9.669% | +3.009% | +2.858% |
| Long8K/repeat | -0.135% | -7.725% | -6.925% |
| Prose2K | -0.063% | -14.092% | -11.139% |
| Short | -0.096% | -0.165% | +1.078% |

原始门槛结果：

- `quality`: `True`
- `control_drift`: `False`
- `direct_nonregression`: `False`
- `short_decode_gain`: `False`
- `all_cases_decode_ahead_of_l0`: `False`

本组不建立整模型提速：Short为 **50.832→51.009 tok/s（+0.350%）**，未达到预设1%门槛。
L0／Direct控制漂移最大绝对值为 **4.331%／1.670%**，L0超出3%门槛。
首次2K的候选净decode吞吐低28.281%（耗时增加39.432%），TTFT多用4.699%；首次8K TTFT多用9.669%。
这些是保留的有限窗口异常观察，不能凭本组确认因果或稳定回退；也不删除异常点或追加重复以取得通过。
重复2K／8K TTFT相对原Direct分别多用0.941%／少用0.135%，没有建立稳定改善。
原Direct两侧READY为30.465／30.167秒，候选33.077秒；初始阶段也偏慢，原因尚未定位。
因此候选保持隔离，生产源码、部署EXE和默认策略不变，Orca L0仍为推荐路径。

候选相比L0的Short／重复2K／重复8K净decode差距为−2.206%／−2.657%／−0.279%。
本组重复8K控制漂移已超门槛，不能把−0.279%当作新的已确认差距，
更不能替换此前通过控制门槛的全量Host对照（重复8K为−1.690%）。
历史通用M16调用普查的Short decode跨队列GPU等待为0；它不是本组U4的计数，
但提示本候选更可能作用于预填充，不能预先当作剩余Short瓶颈的修复。

初始化READY、启动到首次TTFT／完整请求之和及首token后的交付速度保留在JSON。
资产使用明确标注的stat-cached SHA256身份校验，没有重哈希全部权重。
跨场景工作量、草稿轨迹及截断长请求的后端尾部工作区别仍限制归因。
独立demo的调用数不能证明本组模型命中了同样比例的过滤；既有Direct8K间歇故障仍未关闭。
本组不提供PPL、长期稳定性或LinuxGPU结论。

本机复核，不重新运行GPU：

```powershell
python -X utf8 build-orca-completed-dependencies-20261106/publish.py --check
```

同条件复测须保留硬件／驱动／编译器、冻结EXE／pack／资产、初始profile、输入、参数与顺序，
并使用不存在的新输出目录；不覆盖这组记录或为取得通过而追加重复。
以下命令使用同一冻结构建和输入，仅创建新的测量目录；本轮未执行它：

```powershell
python -X utf8 -c "import sys; sys.path.insert(0,'build-orca-completed-dependencies-20261106'); import model as m; m.STAGE=m.OUT/'model-repro-r1'; m.run()"
```

归档保存源码／差异、编译链接命令、原始完整协议和初始失败，仅分发文本，不含权重或二进制。
[JSON](../../../sycl/orca/direct-completed-dependencies-a770-windows-2026-11-06.json) · 原始文本归档 (`direct-completed-dependencies-a770-windows-2026-11-06.json.gz`, retained locally for privacy)
