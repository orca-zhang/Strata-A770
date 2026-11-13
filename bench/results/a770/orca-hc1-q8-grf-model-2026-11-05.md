# HC 融合 Q8 的 256 GRF：冻结 Short 整模型对照

2026-11-05，Windows 10 22H2，Arc A770 16 GB，驱动32.0.101.9034，oneAPI2025.3。
沿用[单映像组件检查](orca-hc1-q8-grf-2026-11-05.md)已通过960次完整输出／guard检查的候选，
只切换 `fused_gr/image_3.zebin` 的128／256 GRF版本；同后端使用同一EXE、Host逻辑、
其余数学pack、运行库、输入与参数。128 GRF重编译与原映像逐字节一致，参数payload布局不变。
256 GRF的报告spill_size为0，原为2208字节／4KiB scratch；global private分配仍存在。

固定Short28输入、贪心、MTP4、step／last min-p0.5、累计门槛0、5771专家槽、reserve1536MiB、
read256、decay0.95、suffix0、cost-window0、prompt-cache0。
顺序为D128→D256→L128→L256→D256→D128→L256→L128；D为Direct，L为L0。
每个新进程15次相同请求，丢弃前10次，保留5次；120次完整token、生成数、草稿提供／采纳、
结束原因与reuse/resume均匹配原参考，进程正常退出。未加内部时钟、逐层profiling或GPU时间戳。
使用历史冻结通用M16 Host／pack，而非最新生产U4；该EXE没有本轮默认PLE预读修改。

净decode不含加载、准备、初始化和prefill，TTFT另报。每进程取5份保留样本中位数，
同策略的两进程再取几何均值；正向／反向分别列出，控制漂移不是置信区间。

| 后端 | 正向128→256 tok/s | 正向变化 | 反向128→256 tok/s | 反向变化 | 汇总128→256 tok/s | 汇总变化 | 128／256漂移 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| direct | 57.984283→59.701493 | +2.962% | 57.887120→59.598494 | +2.956% | 57.935681→59.649971 | +2.959% | -0.168%／-0.173% |
| l0 | 59.565797→60.702875 | +1.909% | 59.584477→60.712574 | +1.893% | 59.575136→60.707724 | +1.901% | +0.031%／+0.016% |

| 后端 | 净decode 128／256 ms | Prefill 128／256 ms | TTFT 128／256 ms |
| --- | ---: | ---: | ---: |
| direct | 1311.800／1274.100 | 895.049／881.397 | 932.778／916.714 |
| l0 | 1275.700／1251.900 | 891.993／880.549 | 927.377／915.155 |

相同128 GRF时Direct相对L0为-2.752%；相同256 GRF时为-1.742%。

这是单日单组包围对照，不能外推当前生产U4、长上下文、全词表PPL或Linux GPU。
两后端共享这项内核资源优化，收益不能全部算成降低API层的收益，也不与其他独立试验百分比相加。
候选仍隔离，未修改默认kernel源码、生产EXE或启动配置。原Direct 8K间歇故障仍未关闭。

```powershell
python -X utf8 build-orca-slm-async-20261104/hc1_grf_model.py verify r1
python -X utf8 build-orca-slm-async-20261104/publish_hc1_grf_model.py --check
```

重测先prepare新目录再run，保留旧尝试。同条件复现要求相同硬件／驱动、模型、数学pack、EXE、
运行库、精确argv/env与请求历史。归档保留输入、参考、源码、协议、配置、每次样本及依赖哈希，
单映像编译证据见组件报告，不分发模型或EXE。准备在锁前完成，GPU／重型哈希／发布持共享实验锁。
[精确JSON](../../../sycl/orca/hc1-q8-grf-model-a770-windows-2026-11-05.json) ·
原始证据 (`hc1-q8-grf-model-a770-windows-2026-11-05.json.gz`, raw capture retained locally)
