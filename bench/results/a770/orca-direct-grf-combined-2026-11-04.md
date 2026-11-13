# Direct 的 GRF 状态更新：命令减少，未确立整模型收益

2026-11-04，Windows 10 22H2，Intel Arc A770 16 GB，驱动 32.0.101.9034。
在此前堆基址复用、缓存失效修正的基础上，仅对已校准 C0 路径中“只有 GRF 模式变化”的情况，
省去内容不变的 CFE、流水线选择和抢占寄存器写入。保留 GRF 更新、依赖屏障和其他状态变化时的完整编程。
候选保持隔离，生产源码、默认参数和已部署 EXE 均未采用此次改动。

**版本范围补充：**后续逐文件核对发现，本轮沿用的冻结运行时没有包含当前源码中的 Windows ISA 尾部缩减、
提前分配代码堆和队列 scratch 复用。这里的结果仅用于该冻结版本上的 GRF 改动对照，
不能代表最新源码构建的性能。原始数据和哈希保持不变，完整当前源码另行重建测量。

本轮使用旧版固定 Short 锚点：28 个输入、自然结束的 76 个输出，实际 5,771 个专家槽位，预留 1,536 MiB，
`short-read=256`、`adapt-decay=0.95`、MTP4、step/last min-p 都是 0.5、累计门槛 0。
三条路径的专家替换热度差统一为 1.5；它与 MTP 的概率门槛是不同参数。
这组速率不能替代日常 `1024/0.5/累计0.5` 或自动显存容量配置的测量。

## 完整请求对照

每个独立进程先丢弃十次预热，再保留五次请求；完整输出、草稿数量和结束条件逐次核对。
无计时插桩、GPU 时间戳或遥测。decode 使用引擎 DONE 的时间，排除加载、准备、初始化和预填充。
TTFT 单独统计。两侧对照取各进程中位数的几何均值，不将不同轮次的速率混成一个结果。

首轮顺序为旧 Direct → 候选 → 旧 Direct → L0：

| 路径 | decode 中位数 tok/s | TTFT 中位数 ms |
| --- | ---: | ---: |
| 旧 Direct，两侧参考 | 55.8536 | 872.48 |
| 候选 Direct | 56.0307 | 874.77 |
| L0，单进程参考 | 57.2074 | 875.98 |

候选比旧 Direct 高 **0.3170%**；两侧旧 Direct 的时间漂移 **+0.2060%**。
候选比此轮未夹测的 L0 低 **2.0569%**。漂移不是置信区间，这一轮不能证明稳定提速。

反向顺序为 L0 → 候选 → 旧 Direct → 候选 → L0，复用同一批 EXE，没有重新编译：

| 路径 | decode 中位数 tok/s | TTFT 中位数 ms |
| --- | ---: | ---: |
| 旧 Direct，中央进程 | 55.7634 | 875.85 |
| 候选 Direct，两侧参考 | 55.5861 | 876.29 |
| L0，两侧参考 | 57.6109 | 868.56 |

候选比旧 Direct 低 **0.3181%**，候选两侧速率漂移 **−0.2848%**，L0 两侧速率漂移 **−0.4991%**。
候选比 L0 低 **3.5146%**。两轮对旧 Direct 的变化方向相反，不能宣称候选有稳定的整模型收益，
也不能把约 0.3% 当作已确立的回退幅度。该候选不进入生产默认。
这个冻结版本的 Direct 在同参数、同输出的 Short 对照中仍未追平 L0。

## 实际提交了哪些命令

以下来自独立的命令抓取进程，只分析第十一次请求，不使用插桩进程的时间宣称性能收益。
三个抓取均保持相同输出、57/57 草稿和零自适应专家补入。

| 第十一次请求的命令 | 旧 Direct | 候选 Direct | L0 |
| --- | ---: | ---: | ---: |
| 提交次数 | 97 | 97 | 97 |
| CFE_STATE | 2,855 | 463 | 97 |
| MI_LOAD_REGISTER_IMM | 2,855 | 463 | 0 |
| PIPELINE_SELECT | 2,855 | 463 | 0 |
| STATE_BASE_ADDRESS | 926 | 926 | 194 |
| STATE_COMPUTE_MODE | 2,855 | 2,855 | 2,392 |
| COMPUTE_WALKER | 40,119 | 40,119 | 40,273 |
| PIPE_CONTROL | 38,848 | 38,848 | 38,579 |
| 抓取的命令字节数 | 7,409,832 | 7,314,152 | 7,253,228 |

三类冗余命令各减少 **83.78%**，共减少 **7,176 条命令 / 95,680 字节**。
候选与旧 Direct 的 **40,119 个完整派发几何/控制元组逐项、按序一致**；
这验证了派发形状保持一致，不能单独用来证明 Kernel、ISA 或参数相同。
这个结果说明少发状态命令确实可行，但不能据此推导同比例的 GPU 或整请求加速。
Direct 与 L0 仍有堆状态、同步和派发几何差别；单凭命令数量不能确定剩余性能差距的原因。
本轮没有重新统计 D2D 数据量或应用层锁，沿用此前数据时须保留其原始测量身份。

## 正确性和复现

初版 CPU 检查按默认完整头字寻找命令，忽略了已写入的 mask/MMIO 位，因而失败；原始失败记录保留。
修正测试中的命令区间匹配后，CPU 检查通过。测量使用的运行时库未因此重编或改变。
初版十二项 GPU CTest 全通过；另通过 GEMV、scratch、私有内存、原子操作和多队列数值检查。
共 **135 次无插桩完整请求（45 次保留）+ 33 次独立诊断请求**，输出均为相同的 76 个 tokens、57/57 草稿、
自然 stop，前缀复用和 resume 都为零。所有预热和保留请求都留下原始记录，未挑选最快结果。
Linux GPU、PPL 和其他输入的质量等价不在本轮验证范围内。

[完整结果与哈希清单](../../../sycl/orca/direct-grf-combined-a770-windows-2026-11-04.json)及
可还原的原始证据 (`direct-grf-combined-a770-windows-2026-11-04.json.gz`, raw capture retained locally)
保留冻结源码、失败记录、构建命令、逐请求协议和实际 GPU 命令缓冲。
数值内核包、EXE 和专家 profile 按哈希标识；模型 GGUF／权重 pack 记录了路径，本轮未重算其全量哈希。
复测需准备相同外部资产、对应构建，并映射本机路径。
同等硬件、驱动、编译器、模型和内核包、容量、初始专家 profile、预热历史与空闲条件是复现前提；实际速率会波动。

以下仅导出已测配置，不启动 GPU。在仓库根目录的 PowerShell 执行，输出目录需尚不存在：

```powershell
@'
import json
from pathlib import Path
r = json.loads(Path('sycl/orca/direct-grf-combined-a770-windows-2026-11-04.json').read_text(encoding='utf-8'))
out = Path('build-orca-grf-repro'); out.mkdir()
runs = r['results']['grf-combined-repeat']['runs']
for name, index in [('l0', 0), ('candidate', 1), ('control', 2)]:
    (out/(name+'.json')).write_text(json.dumps(runs[index]['config'], indent=2), encoding='utf-8')
'@ | python -X utf8 -
```

用现有工具分别重测旧 Direct / 候选 / 旧 Direct，以及 L0 / 候选 / L0：

```powershell
python -X utf8 tools/bench_orca_anchor.py --config build-orca-grf-repro/candidate.json --runtime direct --baseline-config build-orca-grf-repro/control.json --baseline-runtime direct --out build-orca-grf-new-repeat
python -X utf8 tools/bench_orca_anchor.py --config build-orca-grf-repro/candidate.json --runtime direct --baseline-config build-orca-grf-repro/l0.json --baseline-runtime l0 --out build-orca-grf-l0-repeat
```

本机完整证据核对（不运行 GPU）：

```powershell
python -X utf8 build-orca-direct-gap-20261104/publish_grf_combined.py --check
```
