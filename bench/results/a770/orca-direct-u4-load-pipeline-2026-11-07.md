# U4 分层分配与读取重叠：固定正反序对照

Windows 10／Arc A770 16GB／驱动32.0.101.9034／oneAPI2025.3.2。
冻结HC128／MTP4／step及last min-p0.5／实际5771专家槽／1536MiB预留；不是当前spec6校准配置的主成绩。
同一Direct后端，仅重编prefill Host单元；GPU数学包、运行时、其他Host库、输入及参数固定。
原版→流水线→原版；流水线→原版→流水线。每臂新进程执行Code2K两次，再Short15次；暖态Short取后5次中位数。
两侧控制取几何均值，每个请求位置分别比较。全部102条完整输出、草稿计数、结束与复用状态逐项一致。
所有初始化和加载仍留在实际READY／prompt／TTFT／整请求中；decode采用引擎decode_ms，排除准备和prefill。无内部计时、GPU时间戳或profiler。
最多4个读取任务；分配仍在原owner执行；future在任何失败清理前结束，全部读取与校验完成后才消费。

| 顺序／请求 | 原版TTFT s | 流水线TTFT s | TTFT变化 | decode耗时变化 | 整请求变化 | 最大控制漂移 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| forward/Code2K_first | 27.195 | 22.217 | -18.304% | +0.856% | -15.674% | 0.464% |
| forward/Code2K_repeat | 12.610 | 12.625 | +0.118% | +0.258% | +0.147% | 0.450% |
| forward/Short_warm | 0.634 | 0.634 | -0.125% | -0.481% | -0.470% | 0.382% |
| reverse/Code2K_first | 27.574 | 22.305 | -19.110% | +0.063% | -16.507% | 0.405% |
| reverse/Code2K_repeat | 12.731 | 12.644 | -0.686% | +7.858% | +1.216% | 13.806% |
| reverse/Short_warm | 0.634 | 0.634 | +0.108% | -0.110% | +0.095% | 0.249% |

预先固定的门：全部协议相同、控制漂移≤3%、两个方向首次2K TTFT均改善、各请求decode／TTFT／整请求耗时回退≤1%。
耗时变化为负表示更快；控制漂移是观察值，不是置信区间。保留全部样本和失败门，不追加复测取通过。
门结果：`{"drift": false, "first_ttft": true, "no_regression": false, "quality": true}`；可采用：`False`。

独立CPU检查直接提取改后的C++加载代码，使用合成文件覆盖1／2／4读取线程、乱序列表、空层、全部payload字节、15处分配失败及18处文件／scale失败；无泄漏。
此轮只判断加载排程。没有新的PPL、Linux GPU或广泛长上下文稳定性结论，也不把首次TTFT收益当成decode收益。

本机复核（不运行GPU）：

```powershell
python -X utf8 build-orca-u4-load-pipeline-20261107/report.py check
```

重新测量使用新目录、原冻结源／EXE／库／数学包、硬件和驱动、初始profile、输入、参数及完整请求顺序。
模型和U4资产沿用SHA256＋文件身份／大小／时间戳缓存。原始文本归档不含权重、EXE、OBJ、LIB或ZEBIN。

[分段诊断](orca-direct-u4-load-profile-2026-11-07.md)
[JSON](../../../sycl/orca/direct-u4-load-pipeline-a770-windows-2026-11-07.json) · 文本证据 (`direct-u4-load-pipeline-a770-windows-2026-11-07.json.gz`, raw capture retained locally)
