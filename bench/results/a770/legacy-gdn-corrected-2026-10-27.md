# A770：保留旧优化引擎、仅替换 GDN 修正对象

A770 16 GB / PCIe 3.0 / Windows / DPC++ 2025.3.2 / driver 32.0.101.9034。

隔离链接及来源检查通过，但固定输出检查**没有通过当前参考的一致性门槛**：两个新进程的六次完整长请求均为非零、均与修正参考不同。各场景的完整输出在两个进程及重复请求之间一致。不能据此宣称数学错误或通用质量退化，也不能作为恢复约31 tok/s、速度收益或达到40的证据。

保留旧 HC-arena 引擎的 main、verifier、prefill、native_dense、fused_gr、layer 及原 archives，仅将旧 native-XMX GEMM 对象替换为 `gdn-build-v2/candidate.obj`。没有重新编译；输出目录独立，生产文件未改。两份 GEMM 头文件仅相差一个额外非虚方法声明，数据布局相同；链接配方检查保证只有这一处对象替换及输出目录变化。复用已有 widen/XMX 各158项 GDN 检查，并非本轮重新执行这些原语。

每进程固定 Short16、Code2K128、Prose2K128、Code2K128，使用与当前融合对照相同的原始输入、修正参考、显式 min-p0.5、cache5500/实际5771槽；每次长输入2048、完整输出128，prompt-cache0。HC Q8替换、GU/Q8融合、BF16 widening、有限值/AB/HC探针、decode日志和剖析器关闭。共享GPU锁覆盖模型运行及来源哈希，未终止其他实验。

| 进程 | 长请求 | 匹配当前参考 | 全零 | 非零但不同 | Short匹配 | 引擎退出码 | READY空闲MiB |
| --- | ---: | ---: | ---: | ---: | --- | ---: | ---: |
| L1 | 3 | 0 | 0 | 3 | 否 | 0 | 2497 |
| L2 | 3 | 0 | 0 | 3 | 否 | 0 | 2497 |

两个Short、四个Code、两个Prose各只有一种完整输出。所有请求及不匹配均保留，runner正常退出0。运行中及终端审计均通过，重建完整T/TB/DONE并核对标志、计数和全部来源哈希；`quality_passed=false`专指本组固定输出一致性门槛，并非困惑度或通用质量评估。没有进一步启动正式性能验收。

源码核对确认旧prefill仍按N48/K2560/ldy96调用alpha/beta投影，替换对象包含该布局的修正入口；本组没有开启运行时分派追踪，不能仅凭链接与源码认定每次实际命中。旧decoder也与当前参考构建不同；输出差异的具体来源、是否属于可接受的数值差异仍待隔离或质量评估，不能全部归因于GDN。

原始证据：`build-sycl-a770-legacy-gdn-corrected-20261027/` 的 `protocol.json`、`build.json`、`link.log` 和 `quality/` 中的固定协议、完整输出、日志与结果。可运行复核：

```powershell
python -X utf8 build-sycl-a770-legacy-gdn-corrected-20261027/build.py --check
python -X utf8 build-sycl-a770-legacy-gdn-corrected-20261027/run.py --check
```

这次检查没有把历史31直接替换为28，也没有建立某项改动的耗时归因。[历史与当前基线](gu-q8-hc-context-model-2026-10-27.md)仍分开记录：旧BF16配对基线Code/Prose为30.984/30.621 tok/s、6297槽；当前为28.162/25.846 tok/s、5771槽，构建与输出参考不同。两场景稳定正确40 tok/s仍未达到。

随后同一EXE只移除`STRATA_GDN_Q8_TAIL`和`STRATA_QSA_Q8_TAIL`，保留其他解码/主机优化、原输入/参数/参考和5771槽。源码和有效设置复核确认，这两条旧版支持的路径在上一组开启，而当前参考版没有对应开关。新组仍固定两个进程、每个Short/Code/Prose/Code：六次长请求均非零且与当前参考不同，但Short、Code、Prose的全部输出均逐项等于上一组尾部融合开启时的输出。因此这两项不是本次输出差异的来源；没有据此测算速度或通用质量变化。

尾部关闭组两个进程READY空闲2494/2485MiB，均以0退出；运行中及终端完整协议/来源审计均通过。所有证据位于同目录`tailoff/`，复核为：

```powershell
python -X utf8 build-sycl-a770-legacy-gdn-corrected-20261027/tailoff.py --check
```

下一步单独进行固定目标概率评分，避免把贪心输出不同直接当作质量退化。预先固定368目标的短文本/原始2K评分冒烟，不替代后续完整九场景758目标质量检查或正式性能验收。
