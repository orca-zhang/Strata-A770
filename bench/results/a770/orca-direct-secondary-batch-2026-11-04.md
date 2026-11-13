# Orca Direct：小主命令缓冲与二级命令正文

2026-11-04，Windows 10 22H2，Intel Arc A770 16 GB，驱动 `32.0.101.9034`，oneAPI 2025.3。

此前[提交普查](orca-direct-prefill-submit-census-2026-11-04.md)发现热 Short 解码的两条路径
都有 97 次提交、112 次可见应用锁调用，但 Direct/L0 主命令缓冲总长度为 7,198,104/12,416 字节。
**主命令长度不包括所有间接命令，也不是 D2D、显存或 PCIe 数据量。**

这个隔离候选保留原封存的完整命令正文，增加一个 16 字节的主命令缓冲：
`MI_BATCH_BUFFER_START` 调用 PPGTT 中的二级正文，正文的 `MI_BATCH_BUFFER_END` 返回，
再由主缓冲的 `MI_BATCH_BUFFER_END` 结束。两个缓冲均保留登记与队列 fence 生命周期。
原实现本就只在封存时复制正文，重放不复制正文；新实现也不在重放时改写任一命令缓冲。
它测试 Windows/KMD 对主命令缓冲处理的差异，不减少实际计算或张量拷贝。

只改 WDDM runtime 与来自相同固定 NEO revision 的未修改跳转指令定义；
复用[依赖合并版本](orca-direct-dependency-only-2026-11-04.md)已验证的 Host 对象与数学 pack。
Direct 各组固定 `STRATA_ORCA_DG2_DEPENDENCY_ONLY=1`；
同一 EXE 唯一策略变量为启动时缓存的 `ORCA_DG2_SECONDARY_BATCH=0/1`。
没有叠加主动轮询、旧事件完成证明或 Point 成组分配。

## 构建和正确性

- 14 项底层 CPU/GPU CTests、840 项完整 FP16 双专家输出、36 组 FP16/BF16 GEMM，以及 engine 检查通过。
- 新策略的 BCS=0/1 队列检查通过，关闭策略也通过；覆盖多队列、门控前缀、重复录制/回收、八槽 staging 和跨线程完成发布。
- 无探针五组共 85 个请求与独立计数两组共 30 个 Short 请求，完整 tokens、草稿工作、复用和结束状态全部匹配。
- 独立计数的开启组共 30 个 prefill/decode 区间，每次实际提交的主命令长度均为 16 字节。

## 无探针模型对照

顺序为 **L0 → Direct 关 → 开 → 关 → L0**，共用数学 pack 和模型参数。
每进程 Short 15 次（10 次丢弃、5 次保留），然后 2K/8K 各一次，无前缀复用。
表中 Short 取进程内中位数，两侧同策略进程取几何均值。
净 decode 排除初始化、准备及 prefill；TTFT 单列。无内部调用时钟、GPU 时间戳、
并行编译、计数探针或遥测；GPU 时钟未锁。

| 输入 | 路径 | prefill ms | TTFT ms | 净 decode tok/s |
| --- | --- | ---: | ---: | ---: |
| Short28 | L0 | 912.40 | 948.49 | 58.3090 |
| Short28 | Direct 关 | 909.50 | 949.90 | 57.1089 |
| Short28 | Direct 开 | 917.80 | 956.97 | 56.9374 |
| Code2K | L0 | 27685.53 | 27738.35 | 18.8360 |
| Code2K | Direct 关 | 23846.69 | 23901.12 | 18.6649 |
| Code2K | Direct 开 | 24260.70 | 24313.87 | 18.6938 |
| Long8K | L0 | 72046.51 | 72100.08 | 18.9783 |
| Long8K | Direct 关 | 69566.64 | 69622.50 | 18.5739 |
| Long8K | Direct 开 | 69852.30 | 69908.29 | 18.3245 |

时间减少为负，吞吐提高为正。

| 输入 | 比较 | prefill 时间变化 | TTFT 变化 | 净 decode 吞吐变化 |
| --- | --- | ---: | ---: | ---: |
| Short28 | Direct 开/关 | +0.9130% | +0.7444% | -0.3003% |
| Short28 | Direct 开/L0 | +0.5921% | +0.8939% | -2.3524% |
| Short28 | 关闭控制后/前漂移 | -0.5701% | -0.2770% | +0.7088% |
| Short28 | L0 控制后/前漂移 | -0.4374% | -0.4559% | +0.0460% |
| Code2K | Direct 开/关 | +1.7361% | +1.7269% | +0.1548% |
| Code2K | Direct 开/L0 | -12.3705% | -12.3456% | -0.7551% |
| Code2K | 关闭控制后/前漂移 | -1.5862% | -1.5800% | -0.0408% |
| Code2K | L0 控制后/前漂移 | +0.2434% | +0.2283% | -0.0853% |
| Long8K | Direct 开/关 | +0.4106% | +0.4105% | -1.3432% |
| Long8K | Direct 开/L0 | -3.0455% | -3.0399% | -3.4450% |
| Long8K | 关闭控制后/前漂移 | -1.0161% | -1.0172% | +1.6208% |
| Long8K | L0 控制后/前漂移 | -0.6109% | -0.6077% | +0.7142% |

控制漂移不是置信区间；长请求每进程仅一次，未建立稳态或跨日重复性。
本轮为隔离候选观察，不能用主命令字节减少比例替代性能改善比例，也不累加独立实验收益。

本轮 Short 开/关分别为 **56.94/57.11 tok/s**（**−0.30%**），关闭控制漂移 **+0.71%**；
开启组仍比 L0 **58.31 tok/s** 低 **2.35%**。2K/8K 预填充相对关闭组耗时
**+1.74%/+0.41%**，对应关闭控制漂移 **−1.59%/−1.02%**。
这些结果没有建立整模型收益，候选不采用；也不把这些单轮小幅变化当作已确立的稳定回退。
缩小主命令缓冲未缩小这轮 Short 差距，不能据此进一步宣称 KMD 的全部处理开销已被排除。

## 独立 Short 提交计数

诊断 EXE 仅增加阶段标记及无逐 API 时钟/无 packet 抓取的导入观察器。
原子计数与观察器 SRW 锁仍会扰动执行，**诊断耗时不用于性能结论**。
同一诊断 EXE 两进程分别关闭/开启，每进程 15 次 Short，保留所有 60 个 prefill/decode Host 区间。
下表为各进程最后一次热 Short；并非 GPU 原子快照。

| 阶段 | 策略 | 提交 | 主命令字节 | 可见 `_Mtx_lock` | CPU fence wait | GPU queue wait |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| prefill | off | 29 | 2304640 | 63 | 22 | 0 |
| prefill | on | 29 | 464 | 63 | 22 | 0 |
| decode | off | 97 | 7198104 | 112 | 78 | 0 |
| decode | on | 97 | 1552 | 112 | 77 | 0 |

观察覆盖 EXE 导入及已挂钩动态入口，厂商库仅观察 D3DKMT；私有、内联和此前缓存入口不在覆盖范围内。
成功提交均映射已知节点，硬件队列 API 计数为零。CPU fence wait 与 GPU queue wait 承担不同工作。

## 条件与复查

固定 5,771 专家槽、reserve1,536MiB、read256、decay0.95、margin1.5、MTP4、
spec-min-p0.5、cumulative0、suffix0、cost-window0、BCS1。这是锚定参数，不是日常平衡默认。
候选保持隔离，仓库 runtime、生产配置及部署 EXE 不变，Orca L0 仍为推荐路径。
本轮没有 PPL 或 Linux GPU 数据，成功请求不关闭此前 Direct 8K 间歇异常。

```powershell
python -X utf8 build-orca-slm-async-20261104/build_secondary_batch.py --verify
python -X utf8 build-orca-slm-async-20261104/secondary_batch_contexts.py r1 --verify
python -X utf8 build-orca-slm-async-20261104/secondary_batch_census.py r1 --verify
python -X utf8 build-orca-slm-async-20261104/publish_secondary_batch.py --check
```

重新采集使用不存在的 r2 等编号；成功与失败证据均不覆盖。同条件复现需要相同硬件、驱动、编译器、
模型资产、数学 pack、初始专家 profile、参数及请求历史。归档保存改动源码、固定 Intel 指令定义、
构建/链接命令、原始协议/日志及 SHA-256；不分发模型或二进制。

精确 JSON (`direct-secondary-batch-a770-windows-2026-11-04.json`, raw capture retained locally) ·
原始证据 (`direct-secondary-batch-a770-windows-2026-11-04.json.gz`, raw capture retained locally)
