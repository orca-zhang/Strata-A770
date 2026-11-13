# 真实模型 48 层 HC1：独立 L0 / Direct 回放

2026-11-07，Windows 10，Arc A770 16GB，驱动 32.0.101.9034，Xeon E5-2696 v4，oneAPI 2025.3.2。
Orca L0 为推荐后端，Direct 为 WIP。

从冻结 full-current Direct 的一个真实 T=4 窗口记录 48 层 HC1 的残差输入、bo、injection、
四组权重和全部 HC 输出。位置为 0；固定 Short 的完整 70 tokens、
草稿采纳/提供及结束/复用协议与参考完全一致。记录节点会扰动运行，记录过程不报告性能。
48 份原始文件合计 663,379,968 字节，含权重的 payload 留在本机，公开证据只保留哈希。

独立回放加载这些真实输入与权重，原生数学包、Host kernel 库和运行库冻结；共同使用 HC128 包，
Direct 沿用驻留修复对象。未重编译数值内核。每层固定读取 R0 并写入独立 R1，
避免重复回放改变数据；原模型使用原地残差。回放不包含周围算子或跨层递归。
两种列表均依次遍历全部 48 层权重，分别包含 192 / 768 个实际 kernel。

每后端新进程，顺序 L0 → Direct → L0；每臂两次预热、八次固定交错测量，
每种列表保留四次样本，不删样本或补跑。每个测量样本均为 6,144 次 HC read / 24,576 次 kernel launch。
Host 计时包含 replay 与每列表 wait，初始化、清空、回读、完整校验和打印在计时外；无 GPU 时间戳或 profiler。
两端 L0 中位数的几何均值作为对照。

| kernel/list | L0 Host µs/read | Direct Host µs/read | Direct 耗时变化 | L0 首尾耗时漂移 |
| --- | ---: | ---: | ---: | ---: |
| 192 | 75.724288 | 76.975643 | +1.653% | -0.065% |
| 768 | 72.976021 | 72.742578 | -0.320% | +0.660% |

30/30 work 协议、663,552 次原生派发全部通过。
三个进程共 144 个真实层 case 检查完整输入/输出/guards，输出逐位匹配模型记录，
融合 Q8 与独立量化逐位一致，并以原 checker 的固定 FP64 容差核验所有浮点输出。
三臂最大 CPU 绝对误差分别为 4.59847678e-05 / 4.59847678e-05 / 4.59847678e-05；完整输出 hash 为 `b71f656ec4f65bce`。

这是一个后端顺序，控制漂移不是置信区间。真实数据与此前合成 fixture 的分配布局也不同，
跨轮百分比变化不能只归因于数据。表中不是纯 GPU 时间或模型 tok/s，不能直接加到整模型吞吐。
该独立图仍省略原地任意递归和周围算子，不据此关闭真实模型差距或历史 Direct 8K/nonfinite/驻留问题。
本轮不改生产默认、部署 EXE 或 README 主成绩；没有新增 PPL、TTFT、长上下文或 Linux GPU 数据。

第一次构建收尾误用原字节相等检查，拒绝 Windows CRLF 与 LF 的差异；两份程序已经编译、链接成功。
保留原脚本及失败记录，finish.py 核对规范化文本和实际编译文件、库与头文件的哈希；没有重编译或救援补跑。
文本归档不分发模型权重、捕获 payload、EXE、OBJ 或 LIB。复测需本地重新记录同模型、参数及窗口，
保持硬件、驱动、工具链、冻结 Host/数学包、完整模型资产及输入历史一致，使用新输出目录。

```powershell
python -X utf8 build-orca-hc-real-20261107/run.py selfcheck
python -X utf8 build-orca-hc-real-20261107/report.py check
```

[合成 48 组权重对照](orca-direct-hc-chain-banks-2026-11-07.md) ·
[JSON](../../../sycl/orca/direct-hc-real-a770-windows-2026-11-07.json) · 文本证据 (`direct-hc-real-a770-windows-2026-11-07.json.gz`, raw capture retained locally)
