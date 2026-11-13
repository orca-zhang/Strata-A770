# Orca：MTP Q8 投影的独立操作分组

2026-11-05（UTC+8），Windows 10 22H2，Intel Arc A770 16 GB，驱动 `32.0.101.9034`，oneAPI 2025.3。
MTP 草稿的 Q、K、V 投影读取同一份已量化输入，输出缓冲互不重叠。
候选使用现有队列的 independent 接口，在归一化、KV 写入和量化 scratch 复用前汇合；
K/V-only 的追赶与预填充分支仍不计算 Q。不增加队列或线程，不改变数学内核、运行库或头文件布局。
这是减少不必要顺序依赖的试验，不宣称三个算子持续同时占用硬件。

每个后端只重编译一个 MTP Host TU，同一 EXE 切换 `STRATA_MTP_READY_PROJECTIONS=0/1`。
开关在进程内缓存，默认关闭；只有 native 队列允许 independent 时启用，非 native／eager 路径保留旧顺序。
首窗为 Direct 关→开→关，再 L0 关→开→关；反向窗为 L0 开→关→开，再 Direct 开→关→开。
每个新进程 15 次相同 Short28 请求，丢弃前十次、保留五次，两窗共 180 次。
反向窗在新目录重编相同补丁，两个窗口的候选 C++ 源码字节必须相同。无额外 GPU 时间戳或逐层 profiling。
逐条比较 tokens、generated、accepted/offered、reuse/resume、finish，并独立重放原始 T/TB/DONE 协议；
启用日志也必须与开关一致。它校验了本次模型轨迹，不等于所有中间张量的逐元素校验。

固定实际 5771 槽、reserve1536 MiB、MTP4、step/last min-p0.5、累计门槛0、read256、decay0.95、
suffix0、cost-window0、prompt-cache0，无前缀复用。现有 chain-submit 的置信度限制原样保留，
没有为了批量执行额外的低置信度草稿。输入、参考、配置与命令在 CPU 准备阶段生成；
编译、设备实验、重型哈希与归档持共享 GPU 实验锁。

完整协议质量检查：**通过**。

| 窗口／后端 | 净 decode tok/s：按执行顺序 | 开相对关 | 两侧漂移 | Prefill：关／开 ms | TTFT：关／开 ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| forward／direct | 58.344849／58.556129／58.448050 | +0.2735% | +0.1769% | 888.300／888.700 | 926.193／926.447 |
| forward／l0 | 59.767222／59.903839／59.706183 | +0.2798% | -0.1021% | 887.900／888.400 | 923.379／923.640 |
| opposed／direct | 58.403135／58.619360／58.574181 | -0.2231% | +0.2929% | 893.300／891.550 | 931.184／929.201 |
| opposed／l0 | 59.866089／59.757824／59.970015 | +0.2681% | +0.1736% | 886.400／887.500 | 921.565／922.799 |

Direct 的收益方向在反向窗口改变，尚未建立稳定改善；L0 两窗同向的小幅变化也不等于跨日重复性。这项共享调度优化没有证明缩小 Direct 与 L0 的差距，不能归因于移除 API 层。

每个进程取五条保留请求的中位数，forward 的两侧关闭控制取几何均值；
opposed 的两侧开启候选取几何均值，百分比始终为开／关−1。“两侧漂移”对应各窗两端的相同策略。
净 decode 排除初始化、请求准备与预填充，完整原始请求和所有分项都在 JSON 中。
两侧漂移不是置信区间；这是同日有限样本，不能将微小差异当作跨日稳定收益。
候选保持隔离且默认关闭，没有修改生产 MTP 源码或部署 EXE，也没有据此发布新的默认吞吐成绩。

“等待时准备”已有的路径包括 PLE 双缓冲、Stager 固定页环形缓冲，以及缓存规划与 commit／draft 重叠。
这轮固定 `STRATA_WARM_GRAPHS=1`，常用窗口、commit 和 MTP 图已在 READY 前准备。
MTP 后续步骤的位置可以预先算出，但首个 round 图会读取 2*T 行映射元数据；
不能在该图提交后直接覆盖这些槽位。图与固定布局提前缓存，动态数据只在最后读者释放后更新。
本次候选只调整已就绪数据的 GPU 依赖，没有增加等待线程或在 fence 轮询中执行大块准备工作。

试验复用冻结的通用 M16 数学 pack，核对历史执行依赖，仅允许已审计的根启动 JSON 更新。
保存的 C++ argv/env 不读取该 JSON；结果不是新生产 U4 pack 的性能比较。
首个 r1 仅做 CPU 准备，后因移除一项重复的源码镜像检查而保留，没有编译或运行 GPU；
成功 r2 使用新目录，失败准备也在归档中。Linux GPU、长上下文和原有 Direct 8K 间歇问题均未由本轮关闭。

```powershell
python -X utf8 build-orca-slm-async-20261104/mtp_ready.py self-check
python -X utf8 build-orca-slm-async-20261104/mtp_ready.py verify r2
python -X utf8 build-orca-slm-async-20261104/mtp_ready_opposed.py verify r1
python -X utf8 build-orca-slm-async-20261104/publish_mtp_ready.py --check
```

重新测试先 prepare 新目录，再 run；同条件复现需保留相同驱动、冻结源码／二进制／数学 pack、输入、参数和请求历史。
归档保留源码、可逆 diff、构建命令、配置、完整协议和依赖哈希；不分发模型／EXE。
精确 JSON (`mtp-ready-projections-a770-windows-2026-11-05.json`, raw capture retained locally) ·
原始证据 (`mtp-ready-projections-a770-windows-2026-11-05.json.gz`, raw capture retained locally)
