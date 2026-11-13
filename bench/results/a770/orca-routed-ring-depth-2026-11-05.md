# GPU 专家预取深度：固定预留16槽，实际8／16槽对照

2026-11-05，Windows 10 22H2，Arc A770 16 GB，驱动32.0.101.9034，oneAPI2025.3。
隔离候选只改变routed预填充提前准备的深度；两臂均预留16个GPU专家槽，
实际循环复用／lookahead分别为8或16。保持同一借槽／显存布局，未强制stream-all，
未新增未被路由选中的专家。FP16反量化仍为双缓冲，Host复制描述符仍为64项。

重编两个私有prefill TU，各归档／链接一次；Host主程序、运行库和通用M16数学pack保持一致。
Direct按8→16→8启动新进程，各直接执行2K→2K→8K→Short，共12次完整输出、
草稿采纳／提供、结束原因及无prefix复用检查。已有双专家pairs／leftover／workspace日志
在三臂逐请求匹配，没有增加计时探针或复制回读。不同进程仍可能有GPU VA／系统状态差异。

固定5771专家槽、reserve1536MiB、MTP4、step/last min-p0.5、累计0、read256、decay0.95、
suffix0、cost-window0、prompt-cache0，镜像异步回填显式关闭。
两臂共同多预留8个槽，因此本报告隔离的是预取深度，不能直接作为生产8槽到16槽的完整资源变化对照。
这是冻结通用M16依赖下的Direct试验，不是最新生产U4重建或L0对照。准备／初始化不计入decode。

| 请求位置 | 第一次8槽 TTFT ms | 16槽 TTFT ms | 第二次8槽 TTFT ms | 16槽相对两侧8槽均值 | 8槽控制漂移 |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1: Code2K | 15833.138 | 15700.036 | 15721.533 | -0.490% | -0.705% |
| 2: Code2K | 15581.755 | 15770.484 | 15749.182 | +0.670% | +1.075% |
| 3: Long8K | 38206.880 | 38344.035 | 38096.176 | +0.505% | -0.290% |
| 4: Short28 | 907.639 | 897.949 | 902.845 | -0.806% | -0.528% |

负百分比表示TTFT更短。每请求位置每臂一次，仅一个包围顺序；未建立跨窗口稳定性或置信区间。
完整prompt／decode／TTFT及草稿计数在JSON与原始协议中，不把单个位置当作稳态吞吐。
这不是GPU环占用、槽复用等待时长或物理传输量测量。预填充环结果不能直接解释Short净decode的后端差距。
源码默认／生产配置与部署EXE未由本实验修改；候选保持隔离，Linux GPU与原Direct 8K间歇故障仍待处理。

```powershell
python -X utf8 build-orca-slm-async-20261104/routed_ring_depth.py verify r3
python -X utf8 build-orca-slm-async-20261104/publish_routed_ring_depth.py --check
```

输入／参考／候选源码和命令在锁前准备，编译、GPU、重型哈希和归档持共享实验锁。
prepare新目录后run可在本机冻结依赖上重测；保留所有准备目录和运行尝试。
同等条件需要相同模型、输入历史、配置、数学pack、运行库、设备和驱动；归档包括完整候选TU、
源差异、编译／链接命令、协议、配置与依赖哈希。未重新测量PPL。
[JSON](../../../sycl/orca/routed-ring-depth-a770-windows-2026-11-05.json) ·
原始证据 (`routed-ring-depth-a770-windows-2026-11-05.json.gz`, raw capture retained locally)
