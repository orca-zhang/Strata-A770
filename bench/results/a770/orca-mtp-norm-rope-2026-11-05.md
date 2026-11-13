# Orca：MTP RMSNorm＋RoPE 融合与 Q 拆分消除

2026-11-05（UTC+8），Windows 10 22H2，Intel Arc A770 16 GB，驱动 `32.0.101.9034`，oneAPI 2025.3。
主模型已有融合的 RMSNorm＋RoPE；本次候选把同一个已有内核用于 MTP 草稿。
K 在原缓冲内完成融合，Q 直接从交错的 Q/gate 以 `2*HD` 步长读取，写入独立 Q 缓冲，gate 保留。
源码层每个 full token 的 K/Q 归一化与旋转由四次 kernel 调用减为两次，并移除一次 Q 拆分复制调用；
K/V-only 分支只融合 K，后续 FWHT、KV 写入和注意力保持原顺序。
这里没有新增设备 kernel、线程或队列，没有改写数学 image pack、运行库或头文件布局。

每个后端仅重编一个 MTP Host TU，同一 EXE 切换进程缓存的 `STRATA_MTP_FUSED_NORM_ROPE=0/1`，默认关闭。
仅在 native QSA／RoPE 和已有几何支持检查通过时融合；旧路径保留。
首窗 Direct 关→开→关、L0 关→开→关，反向窗 L0 开→关→开、Direct 开→关→开。
每个新进程 15 次 Short28 请求，丢弃前十次、保留五次，两窗共 180 次；两个窗口的候选 C++ 源码字节相同。
逐条校验 tokens、generated、accepted/offered、reuse/resume、finish，并独立重放原始 T/TB/DONE 协议和核对启用日志。
这验证了本次模型轨迹；下面的独立内核检查另外验证完整中间结果。

固定实际 5771 专家槽、reserve1536 MiB、MTP4、min-p0.5、累计门槛0、read256、decay0.95、
suffix0、cost-window0、prompt-cache0，无前缀复用。置信度截断和 chain-submit 保护不变，
没有为凑批多跑低置信度草稿。净 decode 排除初始化、请求准备和 prefill，TTFT 单独报告。
没有额外 GPU 时间戳、逐层 profiling 或频率锁定。

完整协议质量检查：**180 次全部通过**。

| 窗口／后端 | 净 decode tok/s：按执行顺序 | 开相对关 | 两侧漂移 | Prefill：关／开 ms | TTFT：关／开 ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| forward／direct | 58.439062／58.574181／58.192956 | +0.4429% | -0.4211% | 890.200／891.000 | 927.877／929.003 |
| forward／l0 | 59.818969／59.847232／59.575135 | +0.2518% | -0.4076% | 889.848／886.600 | 925.213／921.959 |
| opposed／direct | 58.340370／58.412113／58.632927 | +0.1273% | +0.5015% | 888.400／890.549 | 925.838／928.385 |
| opposed／l0 | 59.970015／59.922731／59.776624 | -0.0826% | -0.3225% | 886.100／888.400 | 921.140／923.653 |

Direct 两窗的净 decode 变化同为正，但幅度接近相同策略的两侧漂移；L0 在反向窗口改变方向。
目前没有建立稳定端到端改善或缩小后端差距，保留为可复测候选，不据此发布新的默认吞吐成绩。

每个进程取五条保留请求的中位数。forward 用两侧关闭控制的几何均值，opposed 用两侧开启候选的几何均值；
百分比始终是开／关−1，两侧漂移比较各窗首尾相同策略。漂移不是置信区间，同日小样本不能建立跨日重复性。
减少源码调用不直接等于端到端收益，也不能将这项共享融合归因于移除 Level Zero API 层。
本次候选保持隔离且默认关闭，没有修改生产 MTP 源码或部署 EXE。

独立检查复用 `rope_parity` check 6 的逐位契约，使用同一冻结 native 内核和数学 pack，分别编译 Direct／L0。
每个后端覆盖 HD128／256、rows1／4／32／128、无缩放／Linear／YaRN、交错读取／连续原地，
每个图用两套改变的输入与位置重放：共 **192 项完整输出逐位一致、输入与 guard 检查**，
每后端检查 760,320 个输出元素。独立参考执行旧的归一化与 RoPE 两段，不用融合输出生成参考；
检查所有结果有限、输出边界未覆盖，输入、gamma 和位置含 guard 完整不变。
独立探针覆盖解析 RoPE，未覆盖表查找、多模态 MRoPE 或所有模型中间张量，也没有测吞吐。

等待 GPU 时，已知 prompt 的 PLE 双缓冲、专家 Stager 固定页环形缓冲、缓存规划与 commit／draft 重叠均已有实现。
这轮常用命令图仍在 READY 前缓存。下一草稿的精确 token 需等结果；round 图会读取 `2*T` 行映射元数据，
提交后不能直接覆盖未来草稿槽。已知数据可提前准备，资源重用仍需等最后 GPU 读者或 DMA 释放。
本次从已就绪的 Q/K 数据减少后续操作，没有另建准备线程。

输入、参考、配置、构建命令与两组检查源码均在获取 GPU 实验锁前准备；编译、GPU、重型哈希和归档持共享锁。
试验使用历史冻结的通用 M16 pack，核对全部历史执行依赖，仅接受已经审计的根启动 JSON 差异。
保存的 C++ argv/env 绕过该 JSON；结果不是同时更新的生产 U4 pack 对照。
Linux GPU、长上下文、PPL 和 Direct 原有 8K 间歇故障均未由本次检查关闭。

```powershell
python -X utf8 build-orca-slm-async-20261104/mtp_norm.py verify r1
python -X utf8 build-orca-slm-async-20261104/mtp_norm_opposed.py verify r1
python -X utf8 build-orca-slm-async-20261104/mtp_norm_check.py verify r1
python -X utf8 build-orca-slm-async-20261104/publish_mtp_norm.py --check
```

重新测试先用 prepare 新目录，再 run；同条件复现需保留驱动、冻结源码／二进制／数学 pack、输入、参数与请求历史。
归档含全部原始协议、逐项检查日志、源码、diff、构建命令和依赖哈希，不分发模型或 EXE。
精确 JSON (`mtp-norm-rope-a770-windows-2026-11-05.json`, raw capture retained locally) ·
原始证据 (`mtp-norm-rope-a770-windows-2026-11-05.json.gz`, raw capture retained locally)
