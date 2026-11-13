# 完整冻结源码：Orca L0 与 Direct 的首次、重复和 Short 对照

2026-11-05，Windows 10，Arc A770 16 GB，驱动 32.0.101.9034，oneAPI 2025.3.2。
这是同一构建时源码完整重编的两个后端，不是原 SYCL 对照，也不是旧 Host／pack 的隔离结果。
此后工作区的GGUF缓存源码已另行更新；归档使用匹配原构建摘要的冻结副本，不把新源码冒充本轮测量。

两个完整构建均通过 PLE CPU CTest，以及每后端 64 项 HC eager／replay 的完整输出、源和边界检查。
该构建时 HC 检查使用保留的匹配 pack；下文新烘焙 pack 用于完整模型对照，二者不混称。
HC 检查覆盖 T1～8、CPU FP64 参考、融合／分开 Q8、pending write 开关和改变输入。
两构建的 pack 索引及参数 ABI 逐字节一致；从新 L0 SPIR-V 烘焙同一集合供两者使用。
共 1254 个 ZEBIN，24 个硬件不支持的变体明确保留 SPIR-V；Direct 不支持执行 SPIR-V 回退。
静态 PE 检查中，L0 按预期导入 ze_loader.dll，Direct 无 Level Zero／SYCL／UR／oneMKL 直接导入。
这不是动态或递归 DLL 审计，也不代表 Linux GPU 已验证。

固定实际 5771 专家槽、1536 MiB 预留、U4/min16/M16、read256、adapt32/decay0.95、
MTP4、step／last min-p0.5、累计门限0、suffix0、cost-window0、prompt-cache0。
顺序 L0→Direct→L0，每臂新进程：Code2K→Code2K→Long8K→Long8K→Prose2K→Short×15。
每请求最多128个输出，Short按EOS结束、实际70个；丢弃前十次Short，保留后五次中位数。
两侧L0控制取几何均值；长请求每位置每臂仅一次，首次与重复分列，不外推稳态。
所有请求核对完整token、生成／输入数量、草稿采纳／提供、结束原因及reuse／resume。
净decode排除准备、初始化和prefill；TTFT独立计时，没有GPU时间戳或内层profiling。

| 请求位置 | L0／Direct 净decode tok/s | Direct吞吐变化 | decode控制漂移 | L0／Direct TTFT ms | Direct TTFT变化 | TTFT控制漂移 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Code2K/first | 28.233／28.063 | -0.605% | +1.149% | 26763.228／28093.737 | +4.971% | -0.139% |
| Code2K/repeat | 35.626／35.164 | -1.298% | -0.075% | 15630.052／13051.661 | -16.496% | -1.307% |
| Long8K/first | 28.427／28.758 | +1.163% | -1.824% | 34880.896／31938.595 | -8.435% | -1.156% |
| Long8K/repeat | 33.198／32.806 | -1.181% | +0.564% | 34586.785／31898.713 | -7.772% | -0.410% |
| Prose2K | 32.687／32.010 | -2.069% | +0.980% | 14925.371／12635.609 | -15.341% | +0.051% |
| Short | 51.228／49.480 | -3.411% | +0.668% | 737.201／739.927 | +0.370% | -0.947% |

完整协议 **60／60匹配**；预设门：`{"quality": true, "control_drift": true}`。

Direct的decode差距从Short的−3.411%缩小到重复2K的−1.298%、重复8K的−1.181%；
首次8K观察到+1.163%，但L0控制漂移为−1.824%，不能据此建立稳定的Direct优势。
重复2K／8K的TTFT分别少用16.496%／7.772%，首次冷2K却多用4.971%。
因此长请求总体更有利于Direct，但不能将首次和重复混用，或仅凭上下文趋势断定剩余差距完全来自API。
各场景草稿采纳／提供不同，原始计数保留，也不能从跨场景百分比直接推算固定派发成本。

首次2K与重复2K的L0 TTFT分别约26.763／15.630秒；Direct为28.094／13.052秒。
日志确认每进程首请求加载9402个U4专家，Host载荷25994649600字节（约24.21 GiB）、
设备暂存44236800字节。加载在prefill内、净decode之前；READY前的图准备不包含它。
模块／kernel句柄也按需缓存；本次没有分别计时，不能把首次／重复差值全部归于权重加载。
初始化前移仍是单独候选，必须同时报告READY、READY＋首次TTFT及READY＋首次完整请求。

本对照没有替换部署EXE或主配置；独立采用须使用自己的证据。历史Direct8K间歇全零故障
在这组六次8K请求中没有出现，仍未定位；通过本组不等于修复。广泛PPL、HTTP TTFT、
Linux GPU及长时间稳定性不由这组有限请求证明。Orca L0仍是推荐路径，Direct为替代／WIP。

归档包含精确输入、argv／相关环境、三份原始protocol、源码、构建／烘焙／检查命令和日志、
ABI及内核元数据；EXE、SPIR-V和ZEBIN记录哈希，不分发内容。
原始对照未采集模型／MTP／U4权重及外置Direct memory.zebin的完整资产摘要；
路径和相关参数已保留，但不能把它称为这些资产的逐字节冻结或完整复现验收。
后续资产快照不能追溯证明原始运行时的文件内容；新对照须在运行前后核对独立资产快照。
同条件复测须相同设备、驱动、编译器、冻结EXE／内核、模型资产、初始profile、参数与请求顺序。
独立归档核对及本机冻结资产重跑：

```powershell
python -X utf8 build-orca-hc-up-capture-20261105/current_report.py --check
python -X utf8 build-orca-hc-up-capture-20261105/current_report.py --rerun current-tree-repro-r1
```

重跑创建新目录并拒绝覆盖旧尝试；它先核对原记录中的源码／EXE／数学pack，沿用同一输入和L0／Direct／L0顺序，
不选择最好的单次成绩。归档核对不要求后续源码保持原版本；本机重跑要求已记录的哈希一致，
其检查不覆盖上文缺失的权重和外置memory.zebin摘要，不能独立保证完整资产一致。
GPU、编译、重型哈希和归档持同一实验锁。反向后端顺序尚未补测，运行波动仍须结合原始控制判断。

JSON (`current-tree-a770-windows-2026-11-05.json`, raw capture retained locally) ·
原始文本 (`current-tree-a770-windows-2026-11-05.json.gz`, retained locally for privacy)
