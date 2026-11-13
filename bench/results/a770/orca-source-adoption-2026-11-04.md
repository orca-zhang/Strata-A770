# Orca 源码接入：动态参数、BCS 复制与 FP16 双专家分块

2026-11-04，Windows 10 22H2，Intel Arc A770 16 GB，驱动 `32.0.101.9034`，oneAPI 2025.3。

三项经过隔离验证的改动已接入仓库源码，并从源码重新构建、验证 **Orca Direct 和 Orca L0**。
生产配置和部署 EXE 未替换。**Orca L0 仍为推荐路径；Direct 仍是显式选择的实验后端**。
本轮验证正确性，没有给新构建程序重新计量性能。

## 接入内容

| 改动 | 当前行为与边界 |
| --- | --- |
| 动态参数分流 | Direct 的 eager 派发使用 Host 参数；录制图保留 device mirror。参数放置改变时重新捕获，避免复用尚未上传的镜像。 |
| BCS 复制角色 | 共享 Windows 校准过的复制、填充与 flush 编码；Direct 仅在 `ORCA_DG2_BCS_COPY=1` 时使用 copy-only 队列。覆盖 prefill、验证和专家补槽。 |
| Linux/i915 | 初始化发现并缓存首个 COMPUTE/COPY engine instance，逻辑队列复用，不重复查询；缺少 COPY 时在改变队列状态前拒绝。未增加 xe 驱动支持。 |
| FP16 双专家 | rows≥128 的完整 16 行前缀使用 M16×N64 寄存器复用；小专家和尾部保留 M4。SG8、8 个 subgroups，保持 K16 累加顺序，无新增 workspace。两个 Orca 后端共用数学源码。 |
| 可运行检查 | `sycl/orca/check.cpp` 的双专家正确性循环扩展到 20 组 shape；新增共享 BCS 包体的 CPU 输出/源/guard 检查并注册 CTest。 |

`STRATA_ORCA_DG2` 的 CMake 默认仍为 OFF；BCS 不因这次接入而默认启用。
FP16 pair 继续沿用原有启用策略，只有进入 pair 路径且行数达到阈值时才选择大分块。

## 新源码构建与校验

| 路径 | 构建与检查 |
| --- | --- |
| Windows Direct | 全部 80 个 Host 编译单元重编，其中 69 个使用新适配器；48 组生成数学源码逐一匹配已校准 pack；runtime ring、blitter-packets、dg2-cpu 三项 CTest 通过。 |
| Windows L0 | 所有生成代码逐一比较，仅 GEMM 和主程序改变；重编这两处，核对复用对象的实际源码依赖，重新归档、链接。 |
| 两套新 GPU 检查程序 | 每套通过 840 项双专家完整输出检查和 36 组 FP16/BF16 GEMM shape；另通过常量捕获、图重放、默认矩阵批处理等检查。双专家合计 1,680 项。 |
| Linux/i915 | 在 WSL 中直接从仓库源码构建；ring、blitter-packets、drm-packets、drm-compute-cpu 四项 CTest 通过。Windows/Linux 各通过 2,268 个 BCS CPU 包体输出检查。 |

L0 的新主程序也包含专家补槽 `adapt_stream` 的 copy-role 生成变化，因此它**不等同于旧 seven-arm 实验中的 wide L0 EXE**。
Direct 的数学 pack 仅在全部 48 组生成数学源码匹配后复用；没有混用旧 Direct 适配器对象。

构建检查中保留了两次脚本失败：`source-build-r1` 完成 87 条构建/CPU 命令后，检查器引用旧路径而停止；
`source-build-checks-r2` 编译、链接检查器成功，但未设置原生内存模块环境变量，在计算前退出。
`source-build-checks-r3` 补齐环境后复用产物，完成真实 GPU 检查。两次失败均没有算作通过。

## 冷历史完整模型检查

每组均为新进程，不跑 Short 预热，依次 **2K → 2K → 8K**，无前缀复用。
固定 5,771 专家槽、1,536 MiB 预留、read256、decay0.95、margin1.5、MTP4、spec-min-p0.5、
cumulative0、suffix0、cost-window0。逐请求比较全部 token、生成数、草稿采纳/提供数、终止原因与复用状态。

| 构建/诊断 | 请求结果 |
| --- | --- |
| 原 Direct 冷历史复查 | 3/3 匹配 |
| 隔离组合 Direct | 3/3 匹配 |
| 隔离组合 Direct，scratch 填毒 | 3/3 匹配，非有限值诊断均为 0 bad halves |
| 本轮源码重建 Direct，BCS=1 | 3/3 匹配 |
| 本轮源码重建 L0 | 3/3 匹配 |

源码重建的两条路径合计 **6/6 请求匹配**；加上独立冷历史诊断共 15/15。
填毒诊断每半层覆盖复用的 attention/MoE scratch，随后检查非有限值；它会扰动执行时序。
**这些成功重试没有关闭此前 Direct 8K 间歇性连续 token0、草稿0/0 的问题**。
原 Direct 与诊断版曾经失败，后续同历史也可能成功；当前仍缺少确定根因与针对性修复。

## 性能范围

此前冻结的[七组组合对照](orca-direct-combined-2026-11-04.md)观察到：相对原 L0，2K/8K 预填充少用时
8.92%/6.97%；相同 wide 数学 pack 下少用时 7.84%/1.55%；Short 净 decode 仍慢 1.90%。
这些是**旧隔离 EXE** 的有限同历史观察，长请求每进程仅一次，不是这轮源码构建的新成绩。
本轮不叠加独立消融百分比，也没有宣称 Direct 的 Short 已超过 L0。
后续性能对照仍须同模型、参数、数学 pack、输入历史，decode 排除初始化/准备/prefill，TTFT 单列。

**Linux GPU 未测**。MOCS2 仍是 Windows 校准值；编译与 CPU 包体检查不能证明 Linux DMA、缓存可见性或跨引擎同步正确。
本轮没有 PPL、跨日性能重复性或多物理复制引擎利用率结论。

## 复查

```powershell
python -X utf8 build-orca-slm-async-20261104/adopt_sources.py --verify
python -X utf8 build-orca-slm-async-20261104/finish_adopted_gpu.py --verify
python -X utf8 build-orca-slm-async-20261104/build_adopted_l0.py --verify
python -X utf8 build-orca-slm-async-20261104/check_adopted_linux.py --verify
python -X utf8 build-orca-slm-async-20261104/adopted_model_check.py direct r1 --verify
python -X utf8 build-orca-slm-async-20261104/adopted_model_check.py l0 r1 --verify
python -X utf8 build-orca-slm-async-20261104/publish_adopted_sources.py --check
```

新完整模型检查使用 `adopted_model_check.py direct r2` / `l0 r2` 等新编号，不能覆盖 r1。
档案含源码、构建命令、依赖、原始协议/日志、两次脚本失败与 SHA-256；不分发模型或构建二进制。

精确 JSON (`source-adoption-a770-windows-2026-11-04.json`, raw capture retained locally) ·
原始证据 (`source-adoption-a770-windows-2026-11-04.json.gz`, raw capture retained locally) ·
[并行流水线](../../../docs/ORCA_PARALLEL_PIPELINE.md)
