# Direct SG16 HC Q8：128／256 GRF长上下文对照

2026-11-06，Windows 10，Arc A770 16 GB，驱动32.0.101.9034，oneAPI2025.3.2。
L0为Orca L0；本轮没有原生产SYCL测量。Direct源码默认已按用户接受小幅改善的要求采用128 GRF，
此前双向Short的+1.142%／+0.743%及未过原双向1%门的记录保留。本轮为独立长历史验证，不用于改写旧结果。

固定顺序L0256→Direct256→Direct128→Direct256→L0256，各为新进程。
每进程2K→2K→8K→8K→Prose2K→Short×15；丢弃前十次Short，保留后五次中位数。
外侧同配置取几何均值；长请求各位置每臂仅一次，控制漂移不是置信区间。
实际5771专家槽、1536 MiB预留、U4/min16/M16、read256、adapt32/decay0.95，
MTP4、step／last min-p0.5、cumulative/suffix/cost-window/prompt-cache0；Direct BCS1，ISA／线程内存device。
同一Direct EXE，仅切换融合HC Q8的128／256 GRF包；其他数学映像、模型资产及策略冻结。
100次完整token／草稿采纳提供／结束／reuse协议均匹配；模型资产前后通过stat-cached SHA256核对，
编译产物另按原烘焙摘要复核。未重新哈希89.8 GiB资产，未增加内层profiler或GPU时间戳。
准备阶段发现候选包选择缩进错误，未运行GPU即保留初版并修正；新增实际五臂配置分支检查通过。

| 请求位置 | L0256 tok/s | Direct256 | Direct128 | 128／256 | Direct128／L0 |
| --- | ---: | ---: | ---: | ---: | ---: |
| Code2K/first | 28.093 | 28.294 | 29.107 | +2.874% | +3.610% |
| Code2K/repeat | 35.657 | 35.166 | 36.225 | +3.010% | +1.591% |
| Long8K/first | 28.990 | 28.496 | 29.673 | +4.128% | +2.357% |
| Long8K/repeat | 33.105 | 32.549 | 33.343 | +2.440% | +0.719% |
| Prose2K | 33.020 | 31.688 | 32.977 | +4.067% | -0.130% |
| Short | 51.573 | 50.209 | 50.872 | +1.320% | -1.359% |

| 请求位置 | Direct256／128 TTFT秒 | 128／256 TTFT变化 | 整请求变化 | L0／Direct控制漂移（decode） |
| --- | ---: | ---: | ---: | ---: |
| Code2K/first | 26.861／26.651 | -0.782% | -1.070% | +1.032%／+2.747% |
| Code2K/repeat | 13.040／12.731 | -2.368% | -2.485% | -0.691%／+3.108% |
| Long8K/first | 32.195／31.383 | -2.520% | -2.698% | -2.273%／+0.581% |
| Long8K/repeat | 32.196／31.542 | -2.032% | -2.066% | +5.145%／+1.287% |
| Prose2K | 12.762／12.492 | -2.111% | -2.542% | +1.754%／-0.087% |
| Short | 0.733／0.725 | -1.126% | -1.192% | +0.266%／-3.211% |

净decode排除准备、初始化及prefill；TTFT与decode首窗口有重叠，整请求直接计时。
完整原始结果另含排除首次交付后的速度，以及READY、READY＋首次TTFT／整请求，避免只看等待成本前移。
原完整门结果：`{"all_cases_decode_ahead_of_l0": false, "control_drift": false, "direct_nonregression": true, "quality": true, "short_decode_gain": true}`。不增加重复来取得通过。
旧核心仍有后端相关的长度截断尾部草稿cap区别；长请求不能证明两后端丢弃工作完全相同。
自然结束Short不触发该区别，同后端Direct128／256共用同一核心。
这些有限历史不能单独定位全部API开销或锁，亦不关闭既有Direct8K间歇故障。
Orca L0仍为推荐路径、Direct为WIP；本轮未改部署EXE／启动配置，无新PPL／Linux GPU／长期稳定性结论。

复核已有记录，不运行GPU：

```powershell
python -X utf8 build-orca-hc-grf-adopt-20261106/publish_contexts.py --check
```

同条件复现需相同设备／驱动／编译器、冻结EXE／pack／资产、完整env／参数／输入与请求历史；
使用新输出目录保留每次尝试。本轮不为取通过而执行额外复测。
归档保留正确及失败准备脚本、冻结计划、完整原始协议／日志、资源构建摘要与本次独立检查脚本；不分发权重或二进制。

[源码默认采用](orca-hc-sg16-grf-adoption-2026-11-06.md) · [原双向Short对照](orca-hc-sg16-grf-2026-11-06.md)

JSON (`hc-sg16-grf-contexts-a770-windows-2026-11-06.json`, raw capture retained locally) · 精确文本归档 (`hc-sg16-grf-contexts-a770-windows-2026-11-06.json.gz`, retained locally for privacy)
