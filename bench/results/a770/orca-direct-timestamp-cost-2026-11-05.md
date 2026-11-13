# Orca Direct：空时间戳／屏障成本校准

2026-11-05（UTC+8），Windows 10 22H2，Intel Arc A770 16 GB，驱动 `32.0.101.9034`，oneAPI 2025.3。
[Short 细分诊断](orca-direct-short-router-phases-2026-11-05.md)增加标记后，观察到的阶段差异也改变。
本小 demo 在原运行库与同一薄队列接口上，只重放有序时间戳，不运行模型数学，检查诊断方法本身的成本。

固定设备缓冲，分别录制 2／16／128／512 个时间戳；每种重放十次，丢弃前三次。
顺序 Direct → L0 → Direct，共 **120 次完整检查／84 次保留采样**。
每次重放前清零活动区、填充 guard，完成后回传全缓冲并检查边界、原始 tick、单调性、
36 位回绕、首尾跨度与每间隔之和、跨重放顺序。两种时钟属性表示都用 19.2 MHz 校准。
源、输入和命令在等待锁时准备；编译、GPU 与重型哈希均持有共享实验锁。

Host 墙钟仅包围单次录制列表提交与最终等待；清零、回传、校验和打印在计时外。
GPU 每间隔耗时为首尾 tick 差除以 count−1，不含最终队列完成、系统唤醒或 Host 提交。
表中各进程先取七个保留样本的中位数，Direct 两个进程再取几何均值。

| 标记数 | Direct GPU µs／间隔 | L0 GPU µs／间隔 | Direct GPU 控制漂移 | Direct Host µs／重放 | L0 Host µs／重放 | Direct Host 控制漂移 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 2 | 0.879268 | 0.781250 | -21.053% | 138.819 | 83.800 | -40.589% |
| 16 | 1.308986 | 0.569444 | -1.579% | 116.871 | 80.600 | -4.351% |
| 128 | 1.211245 | 0.560614 | +0.034% | 280.398 | 185.800 | +10.536% |
| 512 | 1.145072 | 0.552328 | +1.851% | 753.955 | 360.500 | +4.004% |

密集空标记在 Direct 上成本更高，说明细粒度诊断不能忽略两条运行库的标记差异。
两标记结果尤其受 tick 量化、提交和等待影响，不能据此挑选引擎策略。
此处测的是空队列上下文的时间戳及其排序屏障，不是模型 kernel 时间。
有实际计算时，cache、状态切换与依赖会改变成本，不能把本表每标记成本直接从模型阶段时间扣除，
也不能据此解释全部 Short 差距、推算吞吐或声称某数学算子慢同等倍数。
下一步应使用无逐算子标记的独立 hc1／路由投影重放来复核。
生产运行库、默认策略与部署 EXE 均未修改；没有新的长上下文、PPL 或 Linux GPU 结果。

```powershell
python -X utf8 build-orca-slm-async-20261104/timestamp_cost.py self-check
python -X utf8 build-orca-slm-async-20261104/timestamp_cost.py verify r1
python -X utf8 build-orca-slm-async-20261104/publish_timestamp_cost.py --check
```

新运行先 `timestamp_cost.py prepare r2`，再 `timestamp_cost.py run r2`，使用不存在的目录并保留相同硬件、
驱动、数学 pack、运行库与环境。归档含源、命令、配置、原始 tick、检查结果和精确 JSON，不分发 EXE。

精确 JSON (`direct-timestamp-cost-a770-windows-2026-11-05.json`, raw capture retained locally) ·
原始证据 (`direct-timestamp-cost-a770-windows-2026-11-05.json.gz`, raw capture retained locally)
