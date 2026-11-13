# Orca Direct 组合版：七组同历史对照与相同数学内核控制

2026-11-04，Windows 10 22H2，Intel Arc A770 16 GB，驱动 `32.0.101.9034`，oneAPI 2025.3。

将 eager Host 参数、BCS 复制角色和 M16 寄存器复用 FP16 双专家放到同一个隔离 Direct EXE，
并与原 L0、相同 wide 数学 pack 的 L0、原 Direct 一起对照。全部 **119 次请求**的 token、
生成数、草稿采纳／提供数、终止原因和复用状态匹配。组合版另通过 **840 项双专家检查**。
这次可以直接比较本轮各条链路，不需要相加独立实验的百分比。

Short28：组合版相对原 L0，预填充时间 -0.46%，净 decode 吞吐 -1.90%；相对相同数学的 wide L0，预填充时间 -0.45%，净 decode 吞吐 -1.78%。

Code2K：组合版相对原 L0，预填充时间 -8.92%，净 decode 吞吐 -0.54%；相对相同数学的 wide L0，预填充时间 -7.84%，净 decode 吞吐 +0.19%。

Long8K：组合版相对原 L0，预填充时间 -6.97%，净 decode 吞吐 -1.10%；相对相同数学的 wide L0，预填充时间 -1.55%，净 decode 吞吐 -0.63%。

## 对照方式

七个独立进程依次为：原 L0 → wide L0 → 原 Direct → 组合 Direct → 原 Direct → wide L0 → 原 L0。
每进程先跑 Short 15 次（丢弃前 10 次、保留 5 次），再各跑一次 2K、8K；无前缀复用。
Short 取保留请求的中位数，重复控制进程取几何均值。长请求每进程仅一次，
这是有限的输入序号匹配观察，没有建立长请求稳态、随机化或跨日重复性。

固定 5,771 专家槽、1,536 MiB 预留、read256、decay0.95、margin1.5、MTP4、step／last 门槛0.5、
cumulative0，关闭 suffix draft 和 MTP cost window。这是锚定配置，不是日常平衡配置。
净 decode 使用 DONE 时间，排除初始化、准备和预填充；TTFT 为协议端墙钟时间，单独比较。
计时期间无内部计时／探针、GPU 时间戳、遥测或并行编译；GPU 时钟未锁定。

原 Direct 使用冻结的当前前端，没有混入 hybrid 参数、BCS 或 wide 数学变化。
组合 Direct 从完整 BCS Host 构建出发，仅重新编译 native_gemm Host 单元，并使用 BCS 新适配器，
避免与旧适配器的内联定义混用。wide L0 和组合 Direct 使用**逐字节相同的数学 pack**；
原 L0 与 wide L0 只改变 native_gemm，不改变其余数学文件或参数。

## 同轮绝对值

| 输入 | 路径 | 预填充 ms | TTFT ms | 净 decode tok/s |
| --- | --- | ---: | ---: | ---: |
| Short28 | 原 L0 | 890.30 | 925.64 | 59.5565 |
| Short28 | wide L0 | 890.20 | 925.51 | 59.4866 |
| Short28 | 原 Direct | 891.35 | 929.08 | 58.2557 |
| Short28 | 组合 Direct | 886.20 | 923.45 | 58.4256 |
| Code2K | 原 L0 | 27157.01 | 27207.20 | 19.1764 |
| Code2K | wide L0 | 26838.18 | 26888.27 | 19.0375 |
| Code2K | 原 Direct | 28964.93 | 29017.39 | 18.9335 |
| Code2K | 组合 Direct | 24735.00 | 24787.67 | 19.0732 |
| Long8K | 原 L0 | 75257.62 | 75309.01 | 19.2149 |
| Long8K | wide L0 | 71118.49 | 71170.55 | 19.1253 |
| Long8K | 原 Direct | 81967.75 | 82022.54 | 18.9355 |
| Long8K | 组合 Direct | 70013.40 | 70066.42 | 19.0041 |

## 本轮相对变化

时间减少为负，吞吐提高为正。前三项比较组合实施效果；相同 wide 数学控制用于区分数学收益与运行时链路差异。

| 输入 | 候选／控制 | 预填充时间变化 | TTFT 变化 | 净 decode 吞吐变化 |
| --- | --- | ---: | ---: | ---: |
| Short28 | 原 Direct／原 L0 | +0.1177% | +0.3719% | -2.1841% |
| Short28 | 组合 Direct／原 Direct | -0.5775% | -0.6062% | +0.2916% |
| Short28 | 组合 Direct／原 L0 | -0.4605% | -0.2365% | -1.8989% |
| Short28 | 组合 Direct／wide L0 | -0.4493% | -0.2227% | -1.7835% |
| Short28 | wide L0／原 L0 | -0.0113% | -0.0139% | -0.1174% |
| Code2K | 原 Direct／原 L0 | +6.6573% | +6.6534% | -1.2662% |
| Code2K | 组合 Direct／原 Direct | -14.6036% | -14.5765% | +0.7374% |
| Code2K | 组合 Direct／原 L0 | -8.9185% | -8.8930% | -0.5382% |
| Code2K | 组合 Direct／wide L0 | -7.8365% | -7.8123% | +0.1874% |
| Code2K | wide L0／原 L0 | -1.1740% | -1.1722% | -0.7242% |
| Long8K | 原 Direct／原 L0 | +8.9162% | +8.9146% | -1.4542% |
| Long8K | 组合 Direct／原 Direct | -14.5842% | -14.5766% | +0.3623% |
| Long8K | 组合 Direct／原 L0 | -6.9684% | -6.9614% | -1.0972% |
| Long8K | 组合 Direct／wide L0 | -1.5539% | -1.5514% | -0.6340% |
| Long8K | wide L0／原 L0 | -5.4999% | -5.4953% | -0.4662% |

## 控制漂移

以下为相同路径后一个控制进程相对前一个的变化。组合版只有一个进程，不能用控制漂移代替其重复性。

| 输入 | 控制 | 预填充时间漂移 | 净 decode 吞吐漂移 |
| --- | --- | ---: | ---: |
| Short28 | 原 L0 | -0.0674% | +0.1883% |
| Short28 | wide L0 | +0.2024% | +0.1410% |
| Short28 | 原 Direct | -0.4589% | +0.6768% |
| Code2K | 原 L0 | -0.3584% | +0.4354% |
| Code2K | wide L0 | +0.2373% | +0.5249% |
| Code2K | 原 Direct | +0.4505% | +0.3824% |
| Long8K | 原 L0 | +0.1909% | +0.2134% |
| Long8K | wide L0 | -0.4214% | -0.1821% |
| Long8K | 原 Direct | -0.2257% | -0.0887% |

## 能回答的问题与边界

原 Direct／原 L0 检查本轮原生链路差距；组合 Direct／原 Direct 检查组合改动；
组合 Direct／wide L0 在相同数学 pack 下检查剩余链路差距。
本轮组合同时改变参数策略、复制路径和大行数数学分块，不能单凭组合结果分解每个因素的贡献。
各项独立消融见[动态参数](orca-direct-dynamic-parameters-2026-11-04.md)、
[BCS 接入](orca-direct-current-bcs-2026-11-04.md)和[FP16 pair](orca-fp16-pair-tiles-2026-11-04.md)。
不同测试历史下的绝对值不能拼接；小于控制漂移的差异不能据此认定收益。

BCS 三个复制角色来自 prefill、验证和专家补槽；计算使用 Windows compute node5，复制使用 Copy node2。
多个逻辑队列共享这些节点，不代表新增物理引擎，也未测量本轮物理引擎同时活跃比例。
Short 没有调用 FP16 pair；大行数数学优化不应被解释为 Short decode 优化。

候选仍隔离，未采用到生产源码、默认配置或部署 EXE。此前缩短请求历史后出现的 Direct 8K
连续 token0、草稿0/0 间歇问题仍未解决；本轮匹配不能关闭它。未测试 PPL、任意模型质量等价或 Linux GPU。
首次脚本误期待旧结果中的 quality_passed 字段，在启动 GPU 前预检失败，保留原脚本和失败证据；
实际执行的 v2 核对旧69请求全部输出／草稿、结构与哈希，未改写旧实验。

## 计时后的共享 BCS 源码检查

独立准备目录将已校准的 Windows BLT packet、填充／复制编码及 flush 包体原样提取为共享文件，
并为 Linux/i915 增加 copy-only 队列。启动时发现并缓存首个 COMPUTE／COPY engine instance，
后续逻辑队列复用，不重复查询；没有 COPY 时在改变队列状态前拒绝。BCS 只接受复制／填充和同步，
关闭列表前追加 MI_FLUSH_DW；没有扩展 xe 驱动支持。

这些源码**不参与上面的性能计时，也未替换生产 runtime**。重新编译 Windows runtime 后通过
1,632 项完整填充／源／guards 检查、12 组跨引擎链和16个双路输出；Windows CPU ring／DG2 两项检查通过。
WSL 中的 Linux/i915 runtime 编译通过，ring、DRM packet、compute CPU 三项检查通过。
同一份共享编码在 Windows 和 Linux 各通过 **2,268 项** CPU 完整输出／源／guards 检查，
覆盖对齐／未对齐、1～128字节 pattern、奇数长度、16K分块、尾部和有序／无序 flush，另核对大矩形分割及 post-sync。

**Linux GPU 未测**。MOCS2 仍取 Windows 的校准值，CPU 解码检查不证明 Linux 的缓存可见性、
DMA 或引擎同步正确。Linux 硬件验收仍需真实 i915/DG2，不能由 WSL 编译成功推导支持完成。

## 复现

同机器、驱动、冻结构建、输入历史及参数可复现流程；未锁频，具体百分比会受系统负载影响。
本轮未重新哈希完整模型；基础构建要求见[完整前端](orca-direct-current-frontend-2026-11-04.md)。
档案包括源码、命令、依赖、配置、原始协议／日志和 SHA-256，不分发模型或二进制。

```powershell
python -X utf8 build-orca-slm-async-20261104/build_combined.py --verify
python -X utf8 build-orca-slm-async-20261104/combined_context_bracket_v2.py --verify r1
python -X utf8 build-orca-slm-async-20261104/check_portable_bcs.py --verify r1
python -X utf8 build-orca-slm-async-20261104/publish_combined.py --check
```

新计时使用 `combined_context_bracket_v2.py r2` 等新编号；冻结 r1 不可覆盖。

精确 JSON (`direct-combined-a770-windows-2026-11-04.json`, raw capture retained locally) ·
原始证据 (`direct-combined-a770-windows-2026-11-04.json.gz`, raw capture retained locally) ·
[完整并行流水线](../../../docs/ORCA_PARALLEL_PIPELINE.md)
