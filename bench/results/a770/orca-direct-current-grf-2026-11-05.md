# Orca Direct：v14／M16 下的 GRF 状态命令对照

2026-11-05，Windows 10 22H2，Intel Arc A770 16 GB，驱动32.0.101.9034，oneAPI2025.3。
本轮复查[旧版 GRF 状态实验](orca-direct-grf-combined-2026-11-04.md)，基底已含 ISA 尾部缩减、
提前代码堆和队列 scratch 描述符复用。构建前逐字节核对当前源码的七个运行库文件与冻结 v14 基底一致，
不把重建同一运行库称为新增优化。Host／数学包沿用冻结通用 M16，非当前生产 U4 的完整重建。

Direct 两种策略使用同一候选 EXE，只有进程缓存的 `ORCA_DG2_COMPACT_GRF=0/1` 不同。
仅已校准 C0 且只有 GRF 状态发生变化时，候选省去内容不变的 CFE_STATE、MI_LOAD_REGISTER_IMM、
PIPELINE_SELECT；保留 STATE_COMPUTE_MODE、原依赖屏障、scratch 和所有其它状态变化时的完整编程。
初始状态、失效状态、未校准回退、堆／MOCS 联动和模式切换均保留完整前导。
CPU 检查逐字比较完整包序列，核对纯 GRF 切换及上述回退；未新增模型内层计数或时间戳。
本轮没有抓取实际模型命令缓冲，因此不沿用旧实验的整请求命令减少数。

两种策略各通过14个具名 CTest、120种 FP16/BF16 形状、840项双专家完整输出及一组队列检查，
合计28个 CTest、240种形状、1,680项双专家检查和两组队列检查，并拒绝非法 selector。
非法值测试预期退出1；命令助手打印的“FAIL invalid”是这一预期失败，不是 GPU 检查失败。

固定实际5,771专家槽、reserve1536MiB、read256、decay0.95、MTP4、step／last min-p0.5、累计0、
suffix0、cost-window0、prompt-cache0，Direct dependency-only1／BCS1。每个新进程先丢弃10次 Short28，
再保留5次；每次76个输出 token、57/57草稿，自然 stop，reuse／resume均为0。
完整 tokens、草稿与结束状态逐项匹配并重新解析原始协议；首个不匹配即停止。
净 decode 使用引擎 DONE，排除加载、准备、初始化与预填充；READY和TTFT单列。
CPU为五次保留完整请求前后的进程 FILETIME差除以五，包含预填充和decode，不是仅decode的CPU时间。
模型吞吐运行未增加 GPU 标记、回读、计时诊断或容量探针；未锁频。

## r2

顺序：full → compact → l0 → compact → full。

| 进程／策略 | READY s | Prefill ms | TTFT ms | 净 decode tok/s | CPU ms／完整请求 |
| --- | ---: | ---: | ---: | ---: | ---: |
| 0／full | 31.665 | 888.500 | 926.241 | 58.5787 | 40.625 |
| 1／compact | 31.175 | 889.700 | 927.019 | 58.5968 | 34.375 |
| 2／l0 | 26.180 | 887.400 | 922.562 | 59.8661 | 2196.875 |
| 3／compact | 30.419 | 890.800 | 928.656 | 58.6510 | 50.000 |
| 4／full | 31.695 | 890.900 | 928.785 | 58.5065 | 53.125 |

compact／full 吞吐变化 **+0.139%**；full／compact 两侧控制漂移分别为 -0.123%／+0.093%。full／compact 相对该窗口 L0 为 -2.211%／-2.075%。

进程内取五次保留请求的中位数，同策略两进程取几何均值；CPU使用总差值除以五。控制漂移不是置信区间，中央L0仅一个进程；不跨窗口混合速率。75/75完整协议匹配。

## r3

顺序：compact → full → l0 → full → compact。

| 进程／策略 | READY s | Prefill ms | TTFT ms | 净 decode tok/s | CPU ms／完整请求 |
| --- | ---: | ---: | ---: | ---: | ---: |
| 0／compact | 30.510 | 888.000 | 925.503 | 58.5336 | 59.375 |
| 1／full | 30.096 | 886.600 | 924.363 | 58.5110 | 40.625 |
| 2／l0 | 26.301 | 887.400 | 922.566 | 59.9653 | 2193.750 |
| 3／full | 30.711 | 888.500 | 926.038 | 58.2242 | 37.500 |
| 4／compact | 30.200 | 887.900 | 925.214 | 58.4391 | 25.000 |

compact／full 吞吐变化 **+0.204%**；full／compact 两侧控制漂移分别为 -0.490%／-0.161%。full／compact 相对该窗口 L0 为 -2.665%／-2.466%。

进程内取五次保留请求的中位数，同策略两进程取几何均值；CPU使用总差值除以五。控制漂移不是置信区间，中央L0仅一个进程；不跨窗口混合速率。75/75完整协议匹配。

## 结论与复现范围

两轮 compact／full 吞吐变化分别为+0.139%／+0.204%，方向均为微幅正向；
第二轮 full 控制漂移绝对值0.490%大于策略间变化，尚未建立足以改变默认的收益。
候选相对各自窗口L0仍低2.075%／2.466%，未追平L0。进程CPU比较也伴随较大控制漂移，不能据此选择策略或推导能耗。
候选保持隔离、默认关闭，未修改生产源码／EXE／配置。状态命令更少并不直接证明整模型明显更快。
这不是环容量实验，也没有测 GPU 占用、槽复用等待、缓存命中率、物理 DMA 或纯 kernel 时间。
本轮没有新增长上下文、PPL或Linux GPU结果，也不关闭原Direct8K间歇全零问题。

首次模型尝试r1在启动任何模型进程前，误把配置中的放置策略`device`当文件做哈希，因FileNotFoundError退出。
原runner、已通过的构建、准备文件和failure.json保留。修正版仅将路径哈希限定为DG2_MEMORY模块路径，
复用同一已校验运行库／二进制，在新目录r2／r3采样；没有为修正脚本错误重复GPU功能检查。
构建时的当前运行库对应关系不代表生产U4数学／Host快照。主启动JSON的既有更新独立记录，实验直接使用保存的argv／env。

```powershell
python -X utf8 build-orca-slm-async-20261104/current_grf_model.py verify-build
python -X utf8 build-orca-slm-async-20261104/current_grf_model.py verify r2
python -X utf8 build-orca-slm-async-20261104/current_grf_model.py verify r3
python -X utf8 build-orca-slm-async-20261104/publish_current_grf.py --check
```

重新采样使用未存在的目录：先`current_grf_model.py prepare repro-r1`（反向加`--opposed`），再`run repro-r1`。
需相同设备／驱动／oneAPI、模型、初始专家profile、运行库、Host／数学pack、完整参数和预热历史；实际速率会波动。
输入和命令准备在共享锁前完成，编译、GPU、重型哈希和归档持共享锁。归档保留源码、可逆diff、
构建／链接命令、两种策略的检查日志、全部原始请求协议、参数、FILETIME及依赖哈希；不分发模型／EXE。

精确JSON (`direct-current-grf-a770-windows-2026-11-05.json`, raw capture retained locally) ·
原始证据 (`direct-current-grf-a770-windows-2026-11-05.json.gz`, raw capture retained locally)
