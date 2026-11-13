# Orca Direct：即时执行只追加依赖

2026-11-04，Windows 10 22H2，Intel Arc A770 16 GB，驱动 `32.0.101.9034`，oneAPI 2025.3。

此前[提交普查](orca-direct-prefill-submit-census-2026-11-04.md)发现预填充有大量细碎提交。
本轮先隔离验证，再接入三个与测试候选逐字节一致的源文件。
将 16 处丢弃返回事件的 prefill `ext_oneapi_submit_barrier({event})`
生成为现有的 `depends_on` 调用。Direct 的即时执行路径只追加等待依赖，
与后续计算/拷贝一起提交；不为这次等待另外导出完成事件。
3 处用于发布完成事件的调用、图录制行为及 L0 行为保持原逻辑。
没有定时凑批，真实完成标记、队列切换、等待和回读保留提交边界。

启动时一次解析并缓存 `STRATA_ORCA_DG2_DEPENDENCY_ONLY=0/1`。
同一 Direct EXE 的关闭路径保留原逻辑；开启路径不改变数学、专家顺序或缓冲地址。
候选基于[已接入源码](orca-source-adoption-2026-11-04.md)，未叠加完成证明、主动轮询或 Point 成组分配。

## 构建与正确性

- 重编全部 69 个 Queue/adapter 依赖 Host TU；另 11 个按实际依赖复用，避免内联实现混用。
- runtime 和逐字节共用的数学 pack 不变；生成器重新生成的 prefill 与手工隔离版本一致。
- 840 项 FP16 双专家完整输出、36 组 FP16/BF16 GEMM、engine 和 queue 检查通过。
- 新策略在 BCS=0/1 下通过 queue 检查，关闭策略也通过；包括八槽缓冲、强制门控、2,048 个交错任务及跨线程完成发布。
- 干净模型五组共 **85 个请求全部匹配**；独立计数两组 **34 个请求全部匹配**完整输出、草稿工作及结束状态。
- 接入前核对三个源文件仍与基线一致；接入后与已编译测试的候选逐字节一致，仓库生成器输出也与测试版一致。
  此为源码一致性检查，不另声称一次仓库全量构建或新的性能测试。

## 无探针模型对照

顺序为 **当前源码 L0 → 同一 Direct EXE 关 → 开 → 关 → 当前源码 L0**。
每进程 15 次 Short（10 次丢弃、5 次保留），然后 2K/8K 各一次，无前缀复用。
表中 Short 取进程内中位数，两侧同策略进程取几何均值。
净 decode 排除初始化、准备及 prefill；TTFT 单列。
计时没有内部阶段探针、GPU 时间戳、遥测或并行编译；GPU 时钟未锁。

| 输入 | 路径 | prefill ms | TTFT ms | 净 decode tok/s |
| --- | --- | ---: | ---: | ---: |
| Short28 | L0 | 909.15 | 945.21 | 58.0977 |
| Short28 | Direct 关 | 910.00 | 948.80 | 56.8507 |
| Short28 | Direct 开 | 919.60 | 958.60 | 56.7715 |
| Code2K | L0 | 27813.25 | 27864.63 | 18.7193 |
| Code2K | Direct 关 | 25229.04 | 25283.29 | 18.5700 |
| Code2K | Direct 开 | 23878.10 | 23933.37 | 18.4390 |
| Long8K | L0 | 72507.85 | 72560.48 | 18.7789 |
| Long8K | Direct 关 | 71503.40 | 71559.59 | 18.5008 |
| Long8K | Direct 开 | 70016.10 | 70070.80 | 18.4273 |

时间减少为负，吞吐提高为正。

| 输入 | 比较 | prefill 时间变化 | TTFT 变化 | 净 decode 吞吐变化 |
| --- | --- | ---: | ---: | ---: |
| Short28 | Direct 开/关 | +1.0549% | +1.0328% | -0.1393% |
| Short28 | Direct 开/L0 | +1.1497% | +1.4166% | -2.2828% |
| Short28 | 关闭控制后/前漂移 | +0.0000% | -0.1196% | +0.9545% |
| Short28 | L0 控制后/前漂移 | +0.4520% | +0.4632% | +0.7751% |
| Code2K | Direct 开/关 | -5.3547% | -5.3392% | -0.7053% |
| Code2K | Direct 开/L0 | -14.1485% | -14.1084% | -1.4971% |
| Code2K | 关闭控制后/前漂移 | -0.4429% | -0.4474% | +1.3789% |
| Code2K | L0 控制后/前漂移 | -0.3618% | -0.3613% | +0.5367% |
| Long8K | Direct 开/关 | -2.0800% | -2.0805% | -0.3970% |
| Long8K | Direct 开/L0 | -3.4365% | -3.4312% | -1.8721% |
| Long8K | 关闭控制后/前漂移 | -0.0196% | -0.0200% | +1.8055% |
| Long8K | L0 控制后/前漂移 | -0.0619% | -0.0656% | +0.6594% |

控制漂移是观测值，不是置信区间；长请求每进程仅一次，未建立稳态或跨日重复性。
独立实验的百分比不累加。

这轮改善集中在预填充：Direct 开/关的 2K/8K 耗时减少 **5.35%/2.08%**，
关闭组前后漂移分别为 **−0.44%/−0.02%**；相对同数学包 L0 减少 **14.15%/3.44%**。
Short 净解码为 **56.77 tok/s**，相对关闭组 **−0.14%**，仍比 L0 **58.10 tok/s** 低 **2.28%**。
因此这次不能宣称 Short 解码提速，也不能据此将 Direct 改为推荐后端。

2K/8K 相对关闭策略分别节省约 **1.35/1.49 秒**，绝对节省接近，
但关闭组总预填充时间约 **25.23/71.50 秒**，因此长上下文中的百分比收益变小。
下方独立计数的提交降幅也接近；不能把较小的 8K 时间收益解释为批量提交优势消失。
具体的计算、访存、注意力及专家换入耗时占比仍需分开测量。

## 独立提交计数

这部分使用另一个诊断 EXE；无每次 API 调用的时钟读取或命令缓冲抓取，
但原子计数及观察器 SRW 锁仍会扰动执行。**诊断耗时不用作性能结论**。
同一诊断 EXE 开关两种策略，保留 68 个 prefill/decode 区间。
表中 Short 取第 15 次，2K/8K 各唯一一次，阶段为 Host 区间而非 GPU 原子快照。

| 请求/阶段 | 策略 | D3DKMT 提交 | Compute node5 | Copy node2 | CPU fence wait | GPU queue wait | 可见 `_Mtx_lock` |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Short28/prefill | off | 29 | 29 | 0 | 22 | 0 | 63 |
| Short28/prefill | on | 29 | 29 | 0 | 22 | 0 | 63 |
| Short28/decode | off | 97 | 97 | 0 | 77 | 0 | 112 |
| Short28/decode | on | 97 | 97 | 0 | 77 | 0 | 112 |
| Code2K/prefill | off | 67669 | 39667 | 28002 | 4529 | 27998 | 20517 |
| Code2K/prefill | on | 39671 | 25664 | 14007 | 4091 | 27998 | 20766 |
| Code2K/decode | off | 160 | 134 | 26 | 109 | 0 | 4453 |
| Code2K/decode | on | 160 | 134 | 26 | 109 | 0 | 4552 |
| Long8K/prefill | off | 81510 | 47634 | 33876 | 5388 | 33872 | 17042 |
| Long8K/prefill | on | 47638 | 30694 | 16944 | 5297 | 33872 | 16891 |
| Long8K/decode | off | 194 | 162 | 32 | 131 | 0 | 4336 |
| Long8K/decode | on | 194 | 162 | 32 | 133 | 0 | 4348 |

2K 预填充提交从 **67,669** 减至 **39,671**（**-41.3749%**）。
8K 预填充提交从 **81,510** 减至 **47,638**（**-41.5556%**）。

CPU fence wait 和 GPU queue wait 承担不同工作，不能相减当作可删除的等待。
预填充可见应用锁次数没有一致下降；Short 解码两种策略均为 97 次提交、112 次可见锁调用。
同一策略同时减少预填充提交和无探针耗时，支持削减额外完成边界有收益；提交减少比例不等于时间减少比例。
观察范围为 EXE 导入及已挂钩动态解析入口；厂商库仅观察 D3DKMT，私有/内联调用不在覆盖范围内。
计数不是物理 PCIe/显存传输量，也不证明相同 node 上多个逻辑队列同时占用多个物理引擎。

## 条件与复查

固定 5,771 专家槽、reserve1,536MiB、read256、decay0.95、margin1.5、MTP4、
spec-min-p0.5、cumulative0、suffix0、cost-window0、BCS1。此为锚定参数，不是日常平衡默认。
已接入三个测试候选源文件；Direct 内部的新策略默认开启，可设 `STRATA_ORCA_DG2_DEPENDENCY_ONLY=0` 关闭。
Direct 后端本身仍默认关闭，生产配置和部署 EXE 未改动；Orca L0 仍为推荐路径。
这轮没有 PPL 或 Linux GPU 数据，成功请求也不关闭此前 Direct 8K 间歇异常。

```powershell
python -X utf8 build-orca-slm-async-20261104/build_dependency_only.py --verify
python -X utf8 build-orca-slm-async-20261104/dependency_only_contexts.py r1 --verify
python -X utf8 build-orca-slm-async-20261104/dependency_only_census.py r1 --verify
python -X utf8 build-orca-slm-async-20261104/adopt_dependency_only.py --verify
python -X utf8 build-orca-slm-async-20261104/publish_dependency_only.py --check
```

重新采集使用尚不存在的 r2 等编号；成功和失败证据都不覆盖。
同条件复现需相同硬件、驱动、编译器、外部模型资产、数学 pack、初始专家 profile、参数和请求历史。
归档含改动源码、生成器、构建命令及依赖、精确配置、原始协议/日志与 SHA-256；不分发模型和二进制。
未变的完整源码见源码接入档案。

精确 JSON (`direct-dependency-only-a770-windows-2026-11-04.json`, raw capture retained locally) ·
原始证据 (`direct-dependency-only-a770-windows-2026-11-04.json.gz`, raw capture retained locally)
