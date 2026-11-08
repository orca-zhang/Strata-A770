# Orca：等待之前准备 MTP 后续位置元数据

2026-11-05，Windows 10 22H2，Intel Arc A770 16 GB，驱动32.0.101.9034，oneAPI2025.3。
后续草稿的位置由当前 p/a 确定，不依赖尚未生成的 token；本次把它们一次填到已有映射槽，
放在首个 round 提交之前，复用该提交前的发布屏障，省去后续逐步填表与发布屏障。
准备时主模型 commit 可能仍在另一个队列执行；本次没有测量两者实际重叠时长。
round 会复制 2*T 行映射数据，因此不能简单把写入挪到 round 的等待中。
没有新增线程、分配、队列或 kernel，没有预跑低置信度草稿，也没有移除结果等待。
GPU读取映射内存及复制到device的次数不变；这里只前移CPU填表，减少相邻草稿之间的Host准备。

这是隔离候选，开关 STRATA_MTP_PRESTAGE=0/1，默认关闭。
同一 EXE 按 Direct关→开→关、L0关→开→关运行；每个进程15次Short28请求，丢弃前十次、保留五次。
90次请求逐条核对完整tokens、生成数、草稿采纳/提供数、reuse/resume与结束原因，并重放原始T/TB/DONE协议。
只重编每个后端的一个MTP Host TU，使用历史冻结的通用M16数学pack与运行库；不是新生产U4的对照。
固定5771实际专家槽、reserve1536MiB、MTP4、min-p0.5、累计门槛0、read256、decay0.95、
suffix0、cost-window0、prompt-cache0，不增加GPU时间戳或逐层profiling。
净decode排除初始化、请求准备与prefill；TTFT独立报告。

| 后端 | 净decode tok/s：关→开→关 | 开相对关 | 两侧关漂移 | Prefill关/开ms | TTFT关/开ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| direct | 58.421093／58.542597／58.403135 | +0.2234% | -0.0307% | 889.300/893.400 | 926.782/930.968 |
| l0 | 59.880240／59.823678／59.795437 | -0.0236% | -0.1416% | 889.750/886.500 | 925.059/922.038 |

每臂取五条保留请求的中位数，以两侧关闭控制的几何均值为参考；漂移不是置信区间。
这次小样本对照不建立跨时段稳定收益；生产源码与部署EXE保持原状。
原有PLE双缓冲、专家Stager固定页环与缓存换入/commit/MTP重叠仍在；
本次只检查MTP已知元数据提前准备，不能据此解释全部后端差距。
模型轨迹检查不覆盖所有shape、耦合采样或所有中间张量；没有改动数学内核。
Linux GPU、长上下文、PPL与原有Direct 8K间歇异常不由本试验关闭。

输入、配置和C++源码在编译前备齐，编译、GPU、依赖哈希及归档均持共享实验锁。
同条件复现需要相同驱动、冻结源码/二进制/数学pack、输入、参数与请求历史；
保存的argv/env绕过已更新的生产启动JSON，历史依赖审计仅接受这一已知JSON差异。

```powershell
python -X utf8 build-orca-slm-async-20261104/mtp_prestage.py verify r1
python -X utf8 build-orca-slm-async-20261104/publish_mtp_prestage.py --check
```

重测先prepare新目录，再run；不覆盖原始记录。
[精确JSON](../../../sycl/orca/mtp-prestage-a770-windows-2026-11-05.json) ·
[原始证据](../../../sycl/orca/mtp-prestage-a770-windows-2026-11-05.json.gz)
