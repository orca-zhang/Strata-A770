# Orca 通用 FP16 投影：M16 寄存器复用

2026-11-04，Windows 10 22H2，Intel Arc A770 16 GB，驱动 `32.0.101.9034`，oneAPI 2025.3。
本轮把已检查的双专家 M16 寄存器复用扩展到普通 FP16 投影。
Direct 和 L0 各自使用同一个新 EXE／数学 pack，只切换 `STRATA_ORCA_XMX_WIDE=0/1`。
这既检查内核优化本身，也检查在相同 M16 数学下 Direct／L0 的差距。

## 实现与正确性

每个工作组计算 M16×N64；八个 SIMD8 子组各用四个 4×8 FP32 累加器，
在寄存器中复用 A／B，不新增 SLM 或屏障。K 仍按 16 元素顺序累加，外层按 K64 展开。
新内核的实际 zebin 元数据为 SIMD8、256 GRF。仅 FP16、M≥128、N 与 K 均为 64 整倍数时启用；
完整 M16 前缀之后由原 M4 处理不足 16 行的尾部，支持输入 stride。BF16 与小形状保留原路径。
选择开关每进程缓存一次，无效值直接报错。

两个后端、开关两种状态共通过 **336 项尾块／输出／源／边界检查**和 **3360 项双专家检查**。
独立边界检查覆盖 1/4/15/127/128/129/143/159/160/257 行、非对齐 N/K 的 M4 回退、
带 padding 的输入、两个输出地址、NaN 填毒、图重放及重放前改变输入。
正式生成器的数学内核源码与该 GPU 候选逐字一致；默认选择分支另行编译检查。CPU 检查为
`python tools/test_orca_xmx_prepare.py`。原 M4/BF16 数学源码保持不变，但重新编译的三个旧内核
各有 8 个 text-section 字节不同，因此不宣称旧 ISA 全字节一致。

## 独立投影：正向与反向

固定 M=2047/7999、N=6144、K=2560、stride=2568。每形状每进程九个保留样本，
先取中位数，再对两侧控制取几何均值。输入生成、分配、上传、填毒、预热和检查在计时外。
测量 launch+wait 墙钟，无 GPU 时间戳、内部 profiler 或命令观察器。
正向 L0关→Direct关→Direct开→L0开→Direct开→Direct关→L0关；反向交换所有开关。
共 **252 个时间样本、168 项完整数据检查**。单位 ms；负的耗时变化表示更快。

| 轮次／M | Direct关→开 | Direct耗时变化 | L0关→开 | L0耗时变化 | 相同M4下Direct/L0 | 相同M16下Direct/L0 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| r1／2047 | 62.3130→19.6860 | -68.408% | 59.7353→19.0196 | -68.160% | +4.315% | +3.504% |
| r1／7999 | 243.8051→74.1085 | -69.603% | 231.2896→71.8697 | -68.927% | +5.411% | +3.115% |
| r2／2047 | 62.4639→19.5305 | -68.733% | 59.7489→19.1583 | -67.935% | +4.544% | +1.943% |
| r2／7999 | 244.2797→74.7453 | -69.402% | 231.3079→71.9024 | -68.915% | +5.608% | +3.954% |

## 整模型：相同 EXE 的关／开／关

六个独立进程：Direct关／开／关，再 L0关／开／关。固定 5771 专家槽、1536 MiB 预留、
read256、decay0.95、MTP4/min-p0.5、cumulative0、suffix0、prompt-cache0；
Direct 保留 dependency-only1 与 BCS1。所有输入 ID、参考输出、六组配置和重放命令均在等待 GPU 锁前准备。
每进程 Short28 丢弃十次预热后保留五次，随后 Code2K 与 Long8K 各一次。
**102 次请求**逐项核对完整 token、generated、accepted、offered、reused、resume 和 finish。
下表的两侧控制取几何均值，TTFT／预填充用秒；净 decode 不含初始化、准备或预填充。
READY 耗时单独保留于 JSON；TTFT 沿用原客户端，包含请求处理及首 token 计算／交付。

| Case／后端 | 预填充关→开（变化） | TTFT关→开（变化） | 净decode关→开 tok/s（变化） |
| --- | ---: | ---: | ---: |
| Short28／direct | 0.903→0.891 (-1.31%) | 0.941→0.929 (-1.29%) | 57.75→58.30 (+0.95%) |
| Short28／l0 | 0.895→0.897 (+0.28%) | 0.931→0.933 (+0.25%) | 59.33→59.37 (+0.07%) |
| Code2K／direct | 23.623→15.701 (-33.53%) | 23.676→15.754 (-33.46%) | 19.02→19.08 (+0.34%) |
| Code2K／l0 | 27.021→19.867 (-26.48%) | 27.071→19.917 (-26.43%) | 19.19→19.06 (-0.71%) |
| Long8K／direct | 67.908→38.306 (-43.59%) | 67.963→38.362 (-43.55%) | 19.03→19.08 (+0.27%) |
| Long8K／l0 | 71.308→43.524 (-38.96%) | 71.359→43.575 (-38.94%) | 19.15→19.21 (+0.31%) |

| Case | M4：Direct相对L0的TTFT | M16：Direct相对L0的TTFT | Direct关控制漂移：TTFT／decode | L0关控制漂移：TTFT／decode |
| --- | ---: | ---: | ---: | ---: |
| Short28 | +1.15% | -0.40% | +2.64%／-2.54% | +0.75%／-0.24% |
| Code2K | -12.54% | -20.90% | +2.19%／-0.99% | +2.25%／-0.77% |
| Long8K | -4.76% | -11.96% | -0.21%／-0.39% | +0.10%／+0.66% |

这是同日单个六进程窗口，长请求每进程仅一次，控制漂移不是置信区间。
完整输出匹配不等于完成广泛 PPL／质量评测。内核优化同时惠及 L0 与 Direct，
不能把优化后 Direct 与旧数学 L0 的差距全部归给 API 层，也不与其他独立优化百分比相加。
既有 Direct 8K 间歇问题仍未关闭；Orca L0 仍为推荐路径。Linux GPU 由用户稍后验证。

## 源码与复现

`sycl/orca/prepare.py` 已接入相同数学内核；通过整模型对照后，
`STRATA_ORCA_XMX_WIDE` 默认 1，显式 0 关闭。每次切换需重启进程。
默认选择分支重新编译并链接两个后端，复用逐字节相同的设备 pack；
通过 **168 项边界检查、48 项 probe 数据检查和 72 个采样**，并拒绝非法开关。
正式 `strata-orca-kernel-check` 还扩展到每后端 120 个 FP16/BF16 形状，
覆盖 M16 门限和余数、N/K 回退及改变输入的两次重放；两个后端另通过 1680 项双专家检查。
Linux 重新构建后可复用同一 GPU 检查入口；这不是 Linux GPU 已验证的声明。
WSL/Linux 的 Python 源码生成回归检查通过，未编译完整前端，也未执行 GPU。
这些采样仅检查默认选择，不作为新的整模型成绩。默认／显式开启的中位数（ms）为：

| 后端／M | 新默认 | 原候选显式1 |
| --- | ---: | ---: |
| direct／2047 | 20.4957 | 20.5646 |
| direct／7999 | 75.4502 | 75.6537 |
| l0／2047 | 19.3298 | 19.2302 |
| l0／7999 | 71.7336 | 71.7484 |

另用新默认 EXE 跑 Direct→L0→Direct 三个新进程，每个进程直接执行 2K→2K→8K，
没有 Short 预热或前缀复用。输入 ID、参考输出、配置和请求命令在等待 GPU 锁前已经准备；
默认开关不设置，只启用已有的全零结果后诊断，不在故障之前增加扫描、填毒或 logits 回读。
本轮 9/9 次请求完整匹配；诊断日志数为
0。这仅增加冷历史检查证据，
不据此关闭原有间歇故障，也不将这九次请求作为新的性能成绩。

部署 EXE、生产配置及专家权重格式未改；需要重新构建原生 Orca 才会采用源码新默认。
正常 Orca 构建自动生成四个 XMX 导出，编译器适配器转发新内核的 256-GRF 属性。
保持相同 A770、驱动、oneAPI、模型／权重、输入 ID、固定槽位及全部配置；主机空闲。
GPU 测试、编译与重型校验使用同一共享实验锁，不并发压测或编译。

```powershell
python tools/test_orca_xmx_prepare.py
python -X utf8 build-orca-slm-async-20261104/generic_projection_check.py r1 --verify
python -X utf8 build-orca-slm-async-20261104/generic_projection_check.py r2 --verify
python -X utf8 build-orca-slm-async-20261104/generic_projection_contexts.py r1 --verify
python -X utf8 build-orca-slm-async-20261104/adopt_generic_projection.py r1 --verify
python -X utf8 build-orca-slm-async-20261104/generic_cold_check.py r1 --verify
python -X utf8 build-orca-slm-async-20261104/publish_generic_projection.py --check
```

新采样用不存在的编号，原目录不覆盖：`generic_projection_check.py r3`、
`generic_projection_check.py r4 --opposed`，以及 `generic_projection_contexts.py r2 --prepare`
后运行相同命令去掉 `--prepare`。冻结原始 EXE／库的哈希用于审核；归档不分发模型或 EXE。
归档包含实际构建命令、候选／基底源码、新旧 XMX 二进制、逐样本数据、完整协议与准备好的输入／配置。
需要从仓库源码重新构建时，按 [Orca 构建说明](../../../sycl/orca/README.md)生成两个后端，
使用归档的输入及固定配置，分别设置开关 0/1；新二进制应另立结果，不能混作冻结 EXE 的复测。

[精确 JSON](../../../sycl/orca/generic-projection-m16-a770-windows-2026-11-04.json) ·
[原始证据](../../../sycl/orca/generic-projection-m16-a770-windows-2026-11-04.json.gz)
