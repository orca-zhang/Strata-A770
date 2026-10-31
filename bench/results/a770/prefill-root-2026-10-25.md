# A770 批量预填充异常定位（2026-10-25）

本文保留修复前的异常定位过程。后续基本 XMX、尾块优化及专家链批量提交已完成生产验证，本机启动配置已切换，下次启动生效；当前状态见[生产切换确认](xmx-batch-production-2026-10-25.md)。下文的“尚未替换”描述当时状态。

已确认生产引擎在批量预填充中使用了昂贵的 oneMKL mixed-input GEMM 路径：FP16/BF16 输入、FP32 输出的调用内部转为 FP32 GEMM。独立运行时跟踪进一步显示，这条实现反复分配临时内存、把设备输入复制回主机并等待。逐专家执行会放大这些成本。这是已复现的主要慢点，尚未完成原工具请求的全程耗时归因，也尚未替换生产引擎修复。

## 原生产 EXE 的直接证据

设备为 Arc A770 16 GB、Xeon E5-2696 v4、256 GiB DDR3、PCIe 3.0 ×16，Windows，oneAPI DPC++ 2025.3.2 / oneMKL 2025.3，驱动 32.0.101.9034。

复用此前超时的原始 Anthropic 工具请求，生产 EXE 的 SHA256 仍为 `8f0d866ded297cf5a409e84cf5dd61f437c56c8a89aaeb2a85569265b6a3e122`。独立本地服务使用 IQ2_XS、FP16 KV、MTP4、prompt cache 0、自动专家缓存；本次实际为 6399 slots。只在私有配置中恢复 `--short-read 256`、开启 `MKL_VERBOSE=1` 并使用私有日志和端口；本机配置的 1024 阈值没有回退。

取得两个 GPU 实验锁后，先完成一次 255-token 工具请求初始化，再对原始请求限时跟踪 45 秒。初始化请求自然结束为 `tool_use`，cache_n=0。原请求的前 257 个位置进入批量预填充，跟踪期间没有 `T`、`TB`、`DONE` 或 `ERR`；HTTP 在 45.017 秒超时，随后私有服务退出码为 0。模型仍持续产生 GEMM 完成日志。

| 原请求诊断窗口内的调用 | 次数 |
| --- | ---: |
| `gemm[half,half,float]` | 1414 |
| `gemm[bfloat16,bfloat16,float]` | 22 |
| 上述 mixed 调用内部的同形状 `gemm[float]` | 1436 |
| 专家 gate/up 尺寸 N=1280、K=2560 的 mixed 调用 | 701 |
| 专家 down 尺寸 N=2560、K=640 的 mixed 调用 | 703 |

日志还有一次内部 FP32 调用，外层 mixed 调用在诊断截止后才返回，因此不计入 1436 次完整配对。尺寸分类只表示调用形状，不冒充路由专家数量或 GPU 耗时占比。日志接收时间也不等于 GPU 事件时间。本次没有完成该请求，不报告 prefill/decode 吞吐或修复倍率。

输入证据来自与历史记录完全一致的 HTTP 请求、相同的服务源码哈希，以及生产 GEMM 的 T=257。诊断的 stdin 探针挂在 `_send()` 上，而单请求 `generate()` 直接写入管道，因此 stdin 记录为空；不把空文件当作已捕获的 GEN 协议。原诊断脚本及其哈希保留，分析脚本明确检查这一限制，没有为补录协议重跑模型。

## oneMKL 内部在做什么

复核已有同工具链的独立 UR 跟踪，形状为 T=4、N=1280、K=2560，输入和输出都是设备分配。连续五次调用均具有以下操作：

| 每次 GEMM 的额外操作 | 实测 |
| --- | ---: |
| USM 分配 | 3 shared + 3 host + 1 device，共 7 次 |
| 累计申请字节数 | 26,054,656 bytes，约 24.85 MiB |
| 显式 device → host 拷贝 | 权重 6,553,600 bytes + 激活 20,480 bytes |
| 显式 host → device 拷贝 | 输出 20,480 bytes |
| `urEventWait` | 5 次 |
| USM 释放 | 7 次 |

拷贝方向通过 `urUSMHostAlloc` 返回值和 `UR_USM_ALLOC_INFO_TYPE=DEVICE` 检查确定，没有仅凭指针外观判断。以上是独立探针的 API 操作计数，不是本次生产请求的 UR 计数，也不包括 shared 内存可能发生的隐式迁移。原 UR 诊断曾被限时终止；这里只使用完整保留下来的调用段，不将它标为性能或正确性通过。

关闭跟踪的独立探针中，DPCT 调用在无额外 pinned 分配时约 **24.8～27.0 ms/次**，有 24576 个 1 MiB pinned 分配时约 **30 ms/次**。主机提交返回时间与等待完成时间几乎相同。栈标量指针查询只有 **0.064 / 0.095 微秒**；直接调用 typed oneMKL 没有稳定优势，移除 `SYCL_PROGRAM_COMPILE_OPTIONS` 也没有消除同量级开销。因此优先修复点是 mixed GEMM 实现，而非 DPCT 标量查询。

已有辅助对照在同一个独立探针中，把扩宽搬到 GPU 后，FP16 调用由 25.19 / 26.64 ms 降至 6.883 ms，BF16 由 22.96 / 23.18 ms 降至 6.828 ms，72 个布局检查通过。这支持绕开 mixed 路径的修复方向；该探针使用 oneMKL BLAS DLL import，不能把它与旧静态链接生产 EXE 混作端到端 A/B。

## 触发位置与处理方向

- [`windows_ok()`](../../../sycl/src/program/generate.cpp) 用待预填位置数和 `short_read` 比较。原请求共 258 tokens，前 257 个位置超过旧阈值 256，最后一个位置留给 decode。
- [`Prefill` 专家循环](../../../sycl/src/prefill/prefill.cpp) 对选中的专家解量化后，分别调用 gate/up 和 down GEMM。即使专家已驻留显存，这两个调用仍进入相同的 mixed 路径。
- [`Gemm::f16()` / `Gemm::bf16()`](../../../sycl/src/prefill/gemm.dp.cpp) 的原路径通过 [`DPCT gemm_impl`](../../../sycl/include/dpct/detail/blas_utils_detail.hpp) 调用 typed oneMKL。生产跟踪没有触发日志中所说的 plain SYCL fallback。

旧的无额外 GPU 等待的层级跟踪曾持续推进至第 27 层，相邻层启动间隔中位数 5.028 秒；另一个冻结实验中第二次批量请求仍需约 313 秒。因此不能只用死锁或首次初始化解释此前现象。这些历史构建和输入与本次生产请求不同，数字不相加。

当前 `--short-read 1024` 让原请求复用验证窗口，继续保留。源码已有 GPU widening 和原生 XMX 实验，生产 EXE 不含 widening 开关；仅设置环境变量不会使它采用新实现。下一步应在私有构建中绕开 mixed GEMM，并用原始 258-token 工具请求及超过窗口阈值的长输入验证完整响应、数值和 prefill/decode，再决定是否替换生产引擎。

[提取数据、生产日志片段、UR 操作片段与证据哈希](prefill-root-2026-10-25.json)。原有[阈值规避复测](short-read-fix-2026-10-24.md)、[独立 XMX 实验](native-xmx-2026-10-24.md)保留其各自范围。

本机不运行 GPU 的复核命令：

```powershell
python -X utf8 build-sycl-a770-prefill-root-20261025/analyze-v2.py --check
```
