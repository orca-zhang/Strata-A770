# Orca Direct：细分 Short 的 HC1、路由与设备计划

2026-11-05（UTC+8），Windows 10 22H2，Intel Arc A770 16 GB，驱动 `32.0.101.9034`，oneAPI 2025.3。
[粗粒度诊断](orca-direct-short-phases-2026-11-05.md)的窗口输入准备约为 Direct 1.20／L0 0.80 ms／请求，
而 hc1-router-plan 区间在 Direct 上合计多约 9.00 ms／请求。本轮将这个区间细分。

沿用同一当前默认运行库、其他 Host 对象与通用 FP16 M16 pack，各后端仅重编一个 Verifier TU。
保持 `prof_on_` 为 false，保留共享／路由交错与独立投影；新增标记只使用已有事件边界。
每层读取 0／16／30／31／32／17／20／24，加 head 0／1，共 **386 个原始时间戳／窗口**。
使用已校准的 19.2 MHz／36 位时钟，保留整数／浮点属性表示，不用取整的 52 ns 缩放。
解析器拒绝缺失、零值、倒退和错误周期，检查回绕、完整计数及阶段之和。

固定实际 5771 专家槽、1536 MiB 预留、read256、decay0.95、MTP4/min-p0.5、cumulative0、
suffix0、cost-window0、prompt-cache0；Direct dependency-only1／BCS1。顺序 Direct → L0 → Direct，
各进程十次预热后保留五次，每请求生成 76 token。**45/45 请求完整匹配**，
包含完整 token、草稿采纳／提供、reuse／resume／finish、原始协议及跨后端的实际 T 序列。
每请求的窗口序号也逐项核对。输入、参考、配置与编译命令在拿 GPU 锁前准备，
构建和采样全程持有一次共享实验锁。

下表为保留五请求的算术均值，单位 ms／请求；Direct 再取两侧进程算术均值。
G／Q 分别合计 36／12 层。正差值代表 Direct 区间更长。

| 区间 | Direct ms | L0 ms | 差 ms | 差异 | Direct 控制漂移 |
| --- | ---: | ---: | ---: | ---: | ---: |
| G:attention-half | 339.9932 | 330.2096 | +9.7837 | +2.963% | -3.947% |
| G:hc1 | 131.1472 | 119.9990 | +11.1483 | +9.290% | -5.144% |
| G:router-projection | 25.9270 | 17.8797 | +8.0472 | +45.008% | -5.169% |
| G:top10 | 19.9780 | 18.8447 | +1.1333 | +6.014% | -2.422% |
| G:device-plan | 15.2776 | 14.0062 | +1.2714 | +9.077% | -8.926% |
| G:shared-routed-chain | 371.7805 | 369.9785 | +1.8021 | +0.487% | -4.640% |
| G:misses-combine | 7.6835 | 5.5822 | +2.1012 | +37.641% | -4.864% |
| G:between-layer | 1.5320 | 0.7377 | +0.7943 | +107.678% | +7.174% |
| Q:attention-half | 127.3611 | 122.5589 | +4.8023 | +3.918% | -7.030% |
| Q:hc1 | 44.7835 | 40.9383 | +3.8452 | +9.393% | -5.592% |
| Q:router-projection | 8.2640 | 5.8244 | +2.4397 | +41.887% | -3.045% |
| Q:top10 | 6.4879 | 6.0760 | +0.4119 | +6.779% | -0.246% |
| Q:device-plan | 4.9637 | 4.6513 | +0.3124 | +6.716% | -9.050% |
| Q:shared-routed-chain | 120.7787 | 120.4788 | +0.2999 | +0.249% | -5.795% |
| Q:misses-combine | 2.4088 | 1.7106 | +0.6983 | +40.821% | +1.853% |
| Q:between-layer | 0.6953 | 0.4947 | +0.2006 | +40.551% | +0.318% |
| head | 34.7358 | 34.4970 | +0.2388 | +0.692% | -8.430% |
| 全部标记跨度 | 1263.7979 | 1214.4674 | +49.3304 | +4.062% | -4.989% |

四个细分区间跨 GDN／QSA 合计的差值：

- `hc1`：+14.993 ms／请求。
- `router-projection`：+10.487 ms／请求。
- `top10`：+1.545 ms／请求。
- `device-plan`：+1.584 ms／请求。

本窗口 GDN／QSA 的 hc1 差异为 +9.29%／+9.39%，router-projection 为 +45.01%／+41.89%。总标记跨度差 +4.06%，Direct 控制漂移 -4.99%。
增加每窗口 144 个标记后，观察到的区间差异也改变；不能将它直接解释为同等幅度的数学算子变慢。下一步优先做无逐算子标记的独立 hc1／路由投影回放，并校准空时间戳／屏障成本，再选择运行库或内核修改。

`hc1` 是 FFN 前的 hyper-connection 读取／归一化／投影，不是 CPU 输入准备。
`router-projection` 还包含该边界前并行的激活量化，不能当作独占 router GEMV 时间；
`top10` 包含 Top-K 选择，单行回退路径可能已有计算落在前一区间；
`device-plan` 包含 resident plan 和路由 ID 回传。其余区间定义与粗粒度报告相同。

这些是插桩区间，包含 GPU 工作及可能的依赖空隙。新增 barrier、每窗口 D2H 和日志会改变时序，
不同粒度报告之间的绝对值不能直接相减；也不能当作独占 kernel 时间、GPU 利用率或无插桩吞吐。
控制漂移不是置信区间，L0 只有一个进程。此诊断用于缩小下一项隔离实验的范围，
不直接选择生产策略，也不替代无插桩整模型性能对照。
本轮没有新的长上下文、PPL 或 Linux GPU 数据，不关闭 Direct 8K 间歇问题。

```powershell
python -X utf8 build-orca-slm-async-20261104/short_fine.py self-check
python -X utf8 build-orca-slm-async-20261104/short_fine.py verify-build
python -X utf8 build-orca-slm-async-20261104/short_fine.py verify r1
python -X utf8 build-orca-slm-async-20261104/publish_short_fine.py --check
```

新采样用不存在的编号，例如先 `short_fine.py prepare r2`，再 `short_fine.py run r2`。
保持硬件、驱动、模型、初始 profile、参数、pack 与请求历史相同；保留旧运行。
归档含脚本、生成源、原始源、命令、配置、原始 tick、完整协议及逐请求 JSON，不分发模型／EXE。

精确 JSON (`direct-short-router-phases-a770-windows-2026-11-05.json`, raw capture retained locally) ·
原始证据 (`direct-short-router-phases-a770-windows-2026-11-05.json.gz`, raw capture retained locally)
