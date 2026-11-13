# Orca Direct：L1 WBP／WB 的 Short 对照

2026-11-04，Windows 10 22H2，Intel Arc A770 16 GB，驱动 `32.0.101.9034`，oneAPI 2025.3。
本轮在当前通用 FP16 M16 数学下检查 DG2 L1 策略能否缩小 Short 差距。
复用此前隔离 L1 候选源码，先逐文件核对它与当前冻结 runtime 基底：只有编码头和对应 CPU 检查不同。
在独立目录重新编译运行库，再链接当前 Host、默认 M16 wrapper 和相同 pack。Direct 的 WBP／WB 使用同一个 EXE，
只切换进程缓存的 `ORCA_DG2_L1_WB=0/1`：SBA 编码值分别为 0／2。
L0 使用上一轮新默认 EXE 与同一数学 pack，未修改厂商驱动。

两种策略各通过 14 项实际命名 CTest、120 个 FP16/BF16 形状、840 项双专家完整输出检查和一组 BCS 队列检查；
合计 **28 项 CTest、240 个形状、1680 项双专家检查、两组队列检查**，另拒绝非法 selector。
这项修改只保留在隔离实验中，源码默认、生产配置与部署 EXE 均未改变。

初次构建 `l1-short-build-r1` 误复用了旧 CTest 目录，CTest 改写 `CTestCostData.txt` 与 `LastTest.log`，
因此证据核对失败，未进行模型性能采样。旧源码、运行库、原始 CTest stdout 及结果 JSON 的哈希均保持原值，
两份临时文件的原始字节未被旧归档收录，不能宣称旧目录的全量 live 哈希检查仍通过。
失败构建和两份文件的原始／当前哈希保留在归档 incident 记录中。
本报告只使用随后独立重建的 `l1-short-build-r2` 和新请求窗口；未改写旧结果来掩盖失败。

输入、精确参考、配置与重放命令在等 GPU 锁前准备。固定实际 5771 专家槽、1536 MiB 预留、
read256、decay0.95、MTP4/min-p0.5、cumulative0、suffix0、prompt-cache0，Direct dependency-only1／BCS1。
每进程 Short28 十次预热后保留五次，每请求生成 76 token；原始协议和完整输出、
草稿采纳／提供、reuse／resume／finish 均逐项核对。每窗口五个进程，共 75 请求。

READY 单列，净 decode 排除启动、准备与预填充。CPU 仅在保留五请求前后读取进程 FILETIME，
表示包含预填充与 decode 的所有线程 CPU 时间，排除初始化和预热；不是 decode-only CPU 或能耗。
100 ns 是计数单位，不能据此声称同等统计分辨率；采样没有新增 GPU 标记或内部计时诊断。

## r3

顺序：wbp → wb → l0 → wb → wbp。

| 进程／策略 | READY s | Prefill ms | TTFT ms | 净 decode tok/s | CPU ms／完整请求 |
| --- | ---: | ---: | ---: | ---: | ---: |
| 0／wbp | 32.884 | 924.300 | 962.630 | 56.6024 | 46.875 |
| 1／wb | 32.830 | 923.600 | 965.869 | 56.4804 | 25.000 |
| 2／l0 | 28.240 | 929.100 | 966.140 | 57.3585 | 2228.125 |
| 3／wb | 32.961 | 925.600 | 964.038 | 56.5266 | 43.750 |
| 4／wbp | 32.218 | 927.400 | 966.128 | 56.0183 | 56.250 |

各进程使用五次保留请求的中位数；同策略两侧进程取几何均值。

| 指标 | WBP | WB | L0 | WB／WBP − 1 | WBP 控制漂移 | WB 控制漂移 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| decode_tok_s | 56.3096 | 56.5035 | 57.3585 | +0.344% | -1.032% | +0.082% |
| prompt_ms | 925.8487 | 924.5995 | 929.1000 | -0.135% | +0.335% | +0.217% |
| decode_ms | 1349.6818 | 1345.0499 | 1325.0000 | -0.343% | +1.043% | -0.082% |
| ttft_ms | 964.3774 | 964.9534 | 966.1399 | +0.060% | +0.363% | -0.190% |
| cpu_ms_per_request | 51.3490 | 33.0719 | 2228.1250 | -35.594% | +20.000% | +75.000% |

**75/75 次请求完整匹配**，原始协议复查通过。CPU 行使用保留段 CPU 总和除以五，不取单请求中位数。

## r4

顺序：wb → wbp → l0 → wbp → wb。

| 进程／策略 | READY s | Prefill ms | TTFT ms | 净 decode tok/s | CPU ms／完整请求 |
| --- | ---: | ---: | ---: | ---: | ---: |
| 0／wb | 32.030 | 922.400 | 961.217 | 56.5729 | 21.875 |
| 1／wbp | 31.898 | 921.400 | 960.617 | 56.3840 | 40.625 |
| 2／l0 | 27.548 | 922.600 | 959.189 | 57.7683 | 2253.125 |
| 3／wbp | 32.670 | 926.200 | 965.166 | 56.7334 | 46.875 |
| 4／wb | 32.280 | 926.200 | 964.949 | 56.6995 | 46.875 |

各进程使用五次保留请求的中位数；同策略两侧进程取几何均值。

| 指标 | WBP | WB | L0 | WB／WBP − 1 | WBP 控制漂移 | WB 控制漂移 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| decode_tok_s | 56.5584 | 56.6361 | 57.7683 | +0.137% | +0.620% | +0.224% |
| prompt_ms | 923.7969 | 924.2980 | 922.6000 | +0.054% | +0.521% | +0.412% |
| decode_ms | 1343.7436 | 1341.8992 | 1315.6000 | -0.137% | -0.616% | -0.223% |
| ttft_ms | 962.8888 | 963.0815 | 959.1889 | +0.020% | +0.473% | +0.388% |
| cpu_ms_per_request | 43.6383 | 32.0217 | 2253.1250 | -26.620% | +15.385% | +114.286% |

**75/75 次请求完整匹配**，原始协议复查通过。CPU 行使用保留段 CPU 总和除以五，不取单请求中位数。

## 结论范围与复现

r3：WB 相对 WBP 的净 decode 吞吐变化为 +0.344%；WBP／WB 相对 L0 分别为 -1.829%／-1.491%。

r4：WB 相对 WBP 的净 decode 吞吐变化为 +0.137%；WBP／WB 相对 L0 分别为 -2.094%／-1.960%。

两种顺序中，WB／WBP 的净 decode 吞吐差异都小于对应的 WBP 控制漂移，
因此未建立稳定收益，不采用 WB。CPU 策略间的差异也伴随很大的控制漂移和粗粒度计账，
不据此选择 WB 或宣称节能。

应结合两种顺序与控制漂移判断；控制漂移不是置信区间，单窗口的 L0 仅有一个进程。
这是 Short 的缓存策略隔离，不将独立优化的百分比相加。未测量实际 L1 命中率、带宽或引擎利用率。
本轮没有新长上下文、PPL 或 Linux GPU 数据，也不关闭 Direct 8K 间歇问题。
默认仍保持 WBP；需要充分的整模型数据后才能改变生产策略。

```powershell
python -X utf8 build-orca-slm-async-20261104/l1_short.py check-preparation r3
python -X utf8 build-orca-slm-async-20261104/l1_short.py verify-build
python -X utf8 build-orca-slm-async-20261104/l1_short.py verify r3
python -X utf8 build-orca-slm-async-20261104/l1_short.py verify r4
python -X utf8 build-orca-slm-async-20261104/publish_l1_short.py --check
```

新采样使用不存在的编号，例如先 `l1_short.py prepare r5` 或加 `--opposed`，再 `l1_short.py run r5`。
保留相同硬件、驱动、oneAPI、模型、初始 profile、参数、pack 与请求历史；新编译程序另立结果，
不混入冻结 EXE 的窗口。数学／Host 基底见[通用 M16 报告](orca-generic-projection-m16-2026-11-04.md)。
归档含候选 runtime 源码、构建／链接命令、配置、完整协议、原始 FILETIME 与逐请求 JSON，
不分发模型或 EXE。

精确 JSON (`direct-l1-short-a770-windows-2026-11-04.json`, raw capture retained locally) ·
原始证据 (`direct-l1-short-a770-windows-2026-11-04.json.gz`, raw capture retained locally)
