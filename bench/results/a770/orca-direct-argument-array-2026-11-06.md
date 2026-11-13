# Orca Direct：全量 Host 参数栈数组的模型对照

2026-11-06，Windows 10，Intel Arc A770 16 GB，驱动 32.0.101.9034，oneAPI 2025.3.2。
L0 指 Orca L0；本报告没有重新测量原生产 SYCL。

重编全部 49 个 kernel Host 单元，替换 Direct 运行库，移除临时参数描述符 vector 分配。
引擎核心对象、数值内核 pack、模型／MTP／U4 资产、输入和策略保持冻结。
旧 vector 接口仍可链接；全量 Host 的 64 项 HC 输出／CPU／边界检查通过。
初版构建清单漏掉生成源码，在编译／GPU 执行前取消并保留记录；修正版通过冻结原始源码及转换脚本逐项推导比对，不改写旧结果。

固定实际 5771 专家槽、1536 MiB 预留、U4/min16/M16、read256、adapt32/decay0.95、
MTP4、step／last min-p0.5、cumulative/suffix/cost-window/prompt-cache0；Direct BCS1，
ISA／线程内存 device。顺序 L0→旧 Direct→新 Direct→旧 Direct→L0，各为新进程。
每进程 2K→2K→8K→8K→Prose2K→Short×15；最多输出128，Short自然结束于70。
丢弃前十次 Short，保留后五次中位数；长请求各位置每臂仅一次。
两侧相同后端取几何均值。全部 **100/100 协议匹配**，包括 token、草稿采纳／提供和结束状态。
资产前后均通过明确标注的 stat-cached SHA256 检查，没有重哈希89.8 GiB资产。
模型计时没有内层 profiler 或 GPU 时间戳；净 decode 排除准备／初始化／prefill。

| 场景 | L0 tok/s | 旧 Direct | 新 Direct | 新／旧 Direct 吞吐变化 | 新 Direct／L0 吞吐变化 |
| --- | ---: | ---: | ---: | ---: | ---: |
| Code2K/first | 29.481 | 28.940 | 29.059 | +0.408% | -1.433% |
| Code2K/repeat | 36.950 | 35.905 | 36.110 | +0.572% | -2.272% |
| Long8K/first | 30.082 | 29.581 | 29.557 | -0.081% | -1.745% |
| Long8K/repeat | 34.331 | 33.746 | 33.751 | +0.013% | -1.690% |
| Prose2K | 33.742 | 32.977 | 32.998 | +0.066% | -2.203% |
| Short | 52.789 | 50.985 | 51.226 | +0.472% | -2.961% |

| 场景 | L0／新 Direct TTFT 秒 | TTFT耗时变化 | L0／新 Direct 整请求秒 | 整请求耗时变化 |
| --- | ---: | ---: | ---: | ---: |
| Code2K/first | 25.197 / 26.261 | +4.224% | 29.482 / 30.615 | +3.843% |
| Code2K/repeat | 14.694 / 12.722 | -13.419% | 18.119 / 16.227 | -10.441% |
| Long8K/first | 33.428 / 31.216 | -6.619% | 37.646 / 35.508 | -5.679% |
| Long8K/repeat | 33.452 / 31.255 | -6.566% | 37.143 / 35.010 | -5.742% |
| Prose2K | 14.281 / 12.375 | -13.347% | 18.034 / 16.212 | -10.100% |
| Short | 0.715 / 0.719 | +0.522% | 2.008 / 2.055 | +2.332% |

两后端控制漂移最大绝对值分别为 L0 **1.946%**、Direct **1.263%**，通过预设3%门槛；
这不是置信区间。质量和新 Direct 时间不回退门槛通过，但 Short 至少+1%收益、
所有场景decode超过L0两项门槛失败，不增加重复以取得通过。
新／旧 Direct Short 为+0.472%，不能建立稳定提速；全部描述符分配并非剩余Short差距的主要解释。

新 Direct 相比 L0 的净 decode 差距从 Short −2.961%缩小到重复2K／8K的−2.272%／−1.690%；
排除首token后的实际交付速度也分别低2.983%、2.302%、1.726%，所以差距不是单纯首token计时边界。
跨场景工作量和草稿轨迹不同，这个趋势不能单独证明固定API开销或锁是根因。
旧核心仍有后端相关的输出尾部草稿cap区别；长度截断的长请求不证明丢弃工作完全相同。
自然结束的Short不触发该区别；同后端新旧Direct使用完全相同的核心。

初始化也单列：旧／新Direct READY为30.214／30.722秒（+1.682%），首次2K TTFT为
26.709／26.261秒（−1.677%）。READY＋首次TTFT为56.922／56.983秒（+0.106%），
READY＋首次完整请求为61.294／61.336秒（+0.069%）。这是顺序阶段之和，排除控制器间隙；
没有新的冷启动整体提速证据，也不从本组建立初始化前移的因果关系。
整请求时间直接测量；不能把TTFT和存在首窗口重叠的引擎decode相加。

源码保持栈数组实现，部署EXE／启动配置未改。Orca L0仍为推荐路径，Direct仍为WIP。
既有8K间歇故障、广泛PPL、长期稳定性和Linux GPU均不由本组关闭。
此前Linux/WSL原生核心构建的四项CPU检查和依赖审计通过，不等于Linux完整前端或GPU验证。

本机独立归档检查：

```powershell
python -X utf8 build-orca-argument-array-20261106/publish.py --check
```

保持同一设备／驱动／编译器、冻结EXE／内核／资产、初始profile、参数和请求顺序才是同条件复测。
下面使用新目录，拒绝覆盖已有尝试；本轮未为取得通过而执行它：

```powershell
python -X utf8 -c "import sys; sys.path.insert(0,'build-orca-argument-array-20261106'); import all_host_model as m; m.STAGE=m.OUT/'model-repro-r1'; m.run()"
```

归档保留精确输入／相关env、完整原始协议、源代码与编译／链接命令及摘要；不分发权重或二进制。
[JSON](../../../sycl/orca/direct-argument-array-a770-windows-2026-11-06.json) · 原始文本归档 (`direct-argument-array-a770-windows-2026-11-06.json.gz`, raw capture retained locally)
