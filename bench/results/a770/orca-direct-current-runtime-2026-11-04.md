# 完整当前运行时：Short 56.53 tok/s，仍低于同期 L0

2026-11-04，Windows 10 22H2，Intel Arc A770 16 GB，驱动 32.0.101.9034。
将当前 `orca-l0/` 的完整源码冻结、重新编译，只替换运行时库，复用相同的主机编译对象和数值内核包。
本轮当前 Direct 为 **56.5308 tok/s**，比同轮旧库 **55.8782 tok/s** 高 **1.1678%**；
L0 两侧参考为 **57.7552 tok/s**，当前 Direct 仍低 **2.1199%**。
该结果没有证明当前原生路径快于 L0。

## 先纠正版本范围

此前 2026-11-04 的缓存／复制／GRF 隔离实验沿用了旧冻结运行时。
逐文件核对发现，其未包含已经入源码的三项更新：Windows ISA 尾部缩减到 512 字节、
提前分配 bootstrap／数学代码堆，以及队列 scratch 复用。
旧实验的输出、哈希和相对其控制的变化有效，但不能据此描述最新源码的性能。
[GRF 实验报告](orca-direct-grf-combined-2026-11-04.md)已补清这一点，原始测量数据没有改写。

本轮构建逐个校验当前运行时的源文件、头文件、测试和 CMake 文件，确认冻结版本与工作区一致。
同时确认其公开 C++ 头文件与原主机对象的构建版本一致；复用同一份 main、GEMM 和三份主机／引擎／Kernel 库。
源码还包含驻留拒绝后的显存回退修复，以及现有参数缓存；未加入上一轮未采用的 GRF 命令省略候选。
这验证的是**完整当前运行时库**，不是把整个当前 Strata 工作区和全部数值内核重新构建后的版本。

## 同一轮五进程对照

顺序为 L0 → 当前 Direct → 旧 Direct → 当前 Direct → L0。
每个进程丢弃十次预热，保留五次请求；完整输出、草稿数和 stop 逐次核对。

| 独立进程 | decode 中位数 tok/s | 五次保留范围 tok/s | TTFT 中位数 ms |
| --- | ---: | ---: | ---: |
| L0，前侧 | 57.7683 | 57.5540–57.9666 | 866.11 |
| 当前 Direct，前侧 | 56.4804 | 56.3422–56.6192 | 872.21 |
| 旧 Direct，中央 | 55.8782 | 55.5840–56.0555 | 871.65 |
| 当前 Direct，后侧 | 56.5813 | 56.5434–56.8012 | 872.09 |
| L0，后侧 | 57.7420 | 57.3671–58.0818 | 865.24 |

两侧当前 Direct／L0 各取进程中位数的几何均值。
当前 Direct 速率漂移 **+0.1787%**，L0 **−0.0456%**；漂移不是置信区间。
旧库比 L0 低 **3.2498%**，当前库低 **2.1199%**，均来自本轮内部对照。
当前库的两侧结果都高于旧库，支持这组更新在本次 Short 上的小幅改善；尚未做第二次完整反向轮次。
TTFT 的对应参考为旧库 **871.65 ms**、当前 **872.15 ms**、L0 **865.67 ms**，没有建立预填充／TTFT 改善。

decode 使用引擎 DONE 的时间，**排除加载、准备、初始化和预填充**；无内部阶段计时、GPU 时间戳或遥测。
TTFT 为协议首 token 的主机观察值，单列统计。GPU 时钟未锁定。

## 实际 GPU 命令变化

独立诊断进程预热十次，抓取第十一次请求。以下旧库来自此前同参数抓取，当前库为本轮新抓取；
插桩进程的耗时不用于吞吐结论。两者输出相同，自适应专家补入均为零。

| 每个热态请求 | 旧冻结库 | 完整当前库 |
| --- | ---: | ---: |
| 提交次数 | 97 | 97 |
| COMPUTE_WALKER | 40,119 | 40,119 |
| 完整状态编程 | 463 | 100 |
| 指令堆窗口切换 | 268 | **0** |
| surface／bindless 基址切换 | 98 | **3** |
| STATE_BASE_ADDRESS 命令 | 926 | **200** |
| CFE_STATE／STATE_COMPUTE_MODE，各自 | 2,855 | 2,492 |
| PIPE_CONTROL | 38,848 | 38,485 |
| 抓取命令字节 | 7,409,832 | 7,319,808 |

逐字段解析确认减少 **363 组完整状态编程**，与已采用的代码堆／scratch 复用改动一致。
数值派发数和提交数不变。这说明已采用更新确实进入新构建，但不能把每条命令直接折算为固定的耗时收益。
L0 的独立抓取有 194 条 SBA、97 次提交、40,273 个 walker；其内部复制 Kernel 与 Direct 不同，
单凭命令数不能确定剩余约 2.1% 差距的原因。

本轮没有重测 D2D 流量或应用层锁。长上下文仍保留早期对照的独立范围，不能将其差距套到本轮新构建上。

## 验证、外部资产和复现

全部 **75 次无插桩请求（25 次保留）+ 11 次独立诊断请求** 均为相同的 76 个输出 tokens、
57/57 草稿、自然 stop，`reused=resume=0`。Direct 启动和负载结束时的模块审计均通过，
未加载 Level Zero、Intel GPU 用户态驱动、SYCL 或 UR。

当前源码通过 **14 项 CTest（13 项 GPU）**，以及 GEMV、scratch、私有内存、原子操作、多队列和 XMX 六组检查。
另用真实 device-local 内存通过 **12 次完整 H2D／D2D／D2H 回放、48 次 GEMV、144 次 FP16／BF16 XMX**，
覆盖变输入、尾部、边界、128/256 GRF 和混合 EU-fusion 状态；两项数值检查最大误差为零。
Linux GPU、PPL 和一般输入质量等价不由本轮证明。生产默认和已部署 EXE 不变。

固定参数仍是旧 Short 锚点：28-token 输入、5,771 实际专家槽、预留 1,536 MiB、
`short-read=256`、`adapt-decay=0.95`、MTP4、step/last min-p 0.5、累计门槛 0。
各路径的专家替换热度差统一为 1.5。它不是日常 `1024/0.5/累计0.5` 或自动缓存配置的成绩。

[完整数据与哈希清单](../../../sycl/orca/direct-current-runtime-a770-windows-2026-11-04.json)和
原始证据归档 (`direct-current-runtime-a770-windows-2026-11-04.json.gz`, raw capture retained locally)
包含冻结源码、编译／链接命令、配置、逐请求记录和 GPU 命令缓冲。
还保留离线解析器的两次失败及修正，以及第一次包装脚本在启动引擎前遇到的路径错误。
内核包、EXE 和专家 profile 有哈希；模型 GGUF／权重 pack 记录路径，本轮未重算全量哈希。
复现需要相同硬件、驱动、编译器、外部资产、配置、容量、初始 profile、预热历史和空闲条件，速率仍可能波动。

在仓库根目录的 PowerShell 导出本轮配置，需使用尚不存在的目录：

```powershell
@'
import json
from pathlib import Path
r = json.loads(Path('sycl/orca/direct-current-runtime-a770-windows-2026-11-04.json').read_text(encoding='utf-8'))
out = Path('build-orca-current-repro'); out.mkdir()
runs = r['results']['current-runtime-repeat-v2']['runs']
for name, index in [('l0', 0), ('current', 1), ('old', 2)]:
    (out/(name+'.json')).write_text(json.dumps(runs[index]['config'], indent=2), encoding='utf-8')
'@ | python -X utf8 -
```

以下用原工具做新的旧库／当前／旧库，以及 L0／当前／L0 对照，输出目录须尚不存在：

```powershell
python -X utf8 tools/bench_orca_anchor.py --config build-orca-current-repro/current.json --runtime direct --baseline-config build-orca-current-repro/old.json --baseline-runtime direct --out build-orca-current-vs-old-repeat
python -X utf8 tools/bench_orca_anchor.py --config build-orca-current-repro/current.json --runtime direct --baseline-config build-orca-current-repro/l0.json --baseline-runtime l0 --out build-orca-current-vs-l0-repeat
```

本机核对冻结证据和当前源码（不运行 GPU）：

```powershell
python -X utf8 build-orca-direct-gap-20261104/publish_current_runtime.py --check
```
