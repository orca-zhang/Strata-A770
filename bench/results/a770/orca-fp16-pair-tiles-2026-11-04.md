# Orca Direct FP16 双专家：按行数选择寄存器复用分块

2026-11-04，Windows 10 22H2，Intel Arc A770 16 GB，驱动 `32.0.101.9034`，oneAPI 2025.3。

将独立 XMX demo 的寄存器复用方式接入真实 FP16 双专家链后，这组 Direct 对照观察到
**2K／8K 预填充时间减少 3.77%／10.09%**，TTFT 同步缩短；Short 基本持平。
51 次请求的全部输出与草稿工作量匹配。长请求每个进程各一次，尚未建立稳态或重复性结论。
候选仍隔离，未采用到生产源码、默认配置或部署 EXE，也没有证明优于 Orca L0。

## 最小改动

两组都保留[动态参数分流候选](orca-direct-dynamic-parameters-2026-11-04.md)：eager 参数留在 host，
录制图保留 device mirror。唯一数学变化在 `native_gemm.cpp`，每条传输路径只重编一个 Host 单元。
native_gemm 数学 pack 由 9 个模块变成 10 个，其余数学文件保留原文件和哈希。

行数至少 128 的专家，其完整 16 行前缀使用 M16×N64 工作组；每 subgroup 的四个 M4×N8
accumulator 复用两份 A／B 片段。沿用 SG8、8 subgroups、256 GRF 编译标志及 K16 顺序累加。
小行数及不足 16 行尾部保留原 M4 路径；不扩大工作区或 padding，也不读取越界的尾行。
128 仅是本轮候选阈值，未声称最优；没有测量最终 spill、occupancy 或指令重叠。

单独的行数诊断发现：Short 不调用 FP16 pair；2K／8K 中 rows≥128 的专家分别覆盖
**55.982%／89.643% 的专家行工作量**。它说明覆盖范围，不是性能测量。

## 干净整模型对照

原 pair／新 pair／原 pair，三个独立进程。每进程先跑 Short 15 次（丢弃 10 次、保留 5 次），
再跑一次 2K、一次 8K。相同输入序号、无前缀复用，两侧控制取几何均值。
固定 5,771 专家槽、1,536 MiB 预留、read256、decay0.95、margin1.5、MTP4、step／last 门槛 0.5、
cumulative0，关闭 suffix draft 和 MTP cost window。这是锚定配置，不是日常平衡配置。

净 decode 用 DONE 时间，排除初始化、准备和预填充；TTFT 为协议端墙钟时间。
无内层计时、探针、GPU 时间戳、遥测或并行编译；GPU 时钟未锁定。

| 输入 | 原 pair 预填充 ms | 新 pair ms | 预填充时间变化 | TTFT 变化 | 净 decode 吞吐变化 | 两侧预填充漂移 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Short28 | 889.39 | 888.70 | -0.0781% | -0.1135% | +0.0031% | -0.6947% |
| Code2K | 27033.69 | 26014.70 | -3.7693% | -3.7650% | +0.5428% | -0.9571% |
| Long8K | 79519.38 | 71499.30 | -10.0857% | -10.0805% | +2.1183% | -1.2878% |

这组没有 L0 整模型性能 arm，不能将此前其他历史下的 L0 绝对值拼接来宣称追平。
8K decode 的 +2.12% 小于两侧 decode 吞吐漂移 +3.65%，且只观察一次，不能据此认定有 decode 收益。
Short 没有使用这条 FP16 pair 路径，也没有建立吞吐收益。

## 正确性与保留的问题

Direct 与 L0 各通过 840 次完整 GU／H／Y 输出和 guards 检查，共 **1,680 次**。
20 组专家行数组合覆盖 127／128／129、159／160／161、256／257 及一大一小的组合，
包括同步、异步、有序／无序图录制重放、拒绝输入及 FP16 饱和值；普通 GEMM 检查也通过。
这不是 PPL 或任意分布的模型质量等价证明；Linux GPU 未测。

此前 Direct 在缩短请求历史后偶发 8K 连续 token0、草稿 0/0 的异常仍未解决。
本轮全部匹配不能关闭该问题；未据此采用候选。

保留首次构建头文件与三参数 runtime 不匹配的链接失败，以及旧验收脚本误期待检查次数的失败。
旧检查实际各通过 504 次；修正后的独立扩展检查实际各通过 840 次。行数统计原脚本因 JSON 整数键变字符串
在末尾自检失败，原结果未改写，单独审计规范化键并核对全部日志、请求和哈希后通过。

## 多引擎的当前状态

EU／XMX 计算与 BCS 复制是可流水并用的资源，但当前完整前端的 Direct `create_queue`
仍固定在 Windows compute node5，适配器此前忽略 copy_engine role，没有实际接入旧隔离版本的 BCS。
旧 BCS 收益及执行时间包络交集不能直接代表当前前端。当前 BCS 三方合并另建版本验证，尚未采用。
多建逻辑队列不会自动增加物理引擎数；同专家 GU→SwiGLU→Down 仍需保持依赖。

## 复现与原始证据

档案包含候选源码、补丁、命令、依赖、原始协议／日志、配置、全部结果与 SHA-256。
二进制及模型不分发，基础完整前端和对象文件要求见[前端快照报告](orca-direct-current-frontend-2026-11-04.md)。
同机器、驱动、冻结构建、输入历史及参数下可复现流程；未锁频，具体百分比会受系统负载影响。
本轮未重新哈希完整模型数据，也未做 Linux GPU 验证。

```powershell
python -X utf8 build-orca-slm-async-20261104/build_pair_tiles.py --verify
python -X utf8 build-orca-slm-async-20261104/expanded_pair_checks.py --verify
python -X utf8 build-orca-slm-async-20261104/audit_pair_rows.py --verify
python -X utf8 build-orca-slm-async-20261104/pair_context_bracket.py --verify r1
python -X utf8 build-orca-slm-async-20261104/publish_pair_tiles.py --check
```

新性能测量使用 `pair_context_bracket.py r2` 等新编号，不能覆盖冻结的 r1。
L0 的新内核已完成数值检查，L0 整模型性能仍待测。

精确 JSON (`fp16-pair-tiles-a770-windows-2026-11-04.json`, raw capture retained locally) ·
原始证据档案 (`fp16-pair-tiles-a770-windows-2026-11-04.json.gz`, raw capture retained locally)
