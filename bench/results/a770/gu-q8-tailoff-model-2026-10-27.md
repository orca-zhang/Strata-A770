# A770 GU/Q8：关闭追踪后的整模型验收（2026-10-27）

A77016GB / PCIe3.0 / E5-2696v4 / Windows / DPC++2025.3.2 / driver32.0.101.9034

本轮在末端关闭融合的基线A2中再次出现Prose2K完整128输出全零，按门槛停止，没有有效配对收益，40目标未达到。生产未改。

原 IQ/head/dequant 对象保留，仅增加已通过数值检查的融合入口及冻结 verifier 的一处调用。两臂使用相同的预填充对象，关闭有限值追踪、分配和探针核；C 为同预填充对象但不含融合入口的控制，A/B 为同一候选 EXE 关闭/开启融合。之前18次诊断通过不足以证明间歇缺陷修复，本轮关闭融合的失败也不能归因于开启融合。

固定 cache5500、实际5771槽、READY空闲至少1500MiB，原 Code2K/Prose2K 各2048输入、完整128输出，prompt-cache0。每进程Short16初始化，正式臂各场景一次预热、三次交错保留；每次请求均核对全部输出。GPU/编译独占，计时关闭分析器，统一1Hz只读Sysman采样。

融合仅用于IQ2_S GU22/down42/N2560/F640、cap10/20及原SG16/GU16/down4配置，其余调用保留原内核。本轮采用GPU混合镜像计划，显存与映射主存指针共同处理；分开处理是源码的另一条分支。合成mixed指针的固定比例不代表模型实际驻留分布，因此算子收益仍不能直接外推。

C/A/B各两次完整输出检查通过，B在cap10/20启用融合，其他形状保留原内核。正式A1与B1各8次请求也全部匹配；A2的Code预热匹配，随后Prose全零。共24次完整2K请求，23次匹配、1次全零；不得删除失败后只比较A1/B1。


收益门槛仍为两场景均至少1%、两端基线耗时漂移至多3%。A2未形成完整保留样本，故配对结果与确认均为空。

| 正式臂 | 场景 | 预热 | 输出匹配 | decode ms | accepted/offered |
| --- | --- | --- | --- | ---: | --- |
| A1 | Code2K | True | True | 5173.3 | 73/92 |
| A1 | Prose2K | True | True | 5058.2 | 72/96 |
| A1 | Code2K | False | True | 4568.2 | 73/92 |
| A1 | Prose2K | False | True | 5038.3 | 72/95 |
| A1 | Code2K | False | True | 4594.4 | 73/92 |
| A1 | Prose2K | False | True | 4941.8 | 71/96 |
| A1 | Code2K | False | True | 4535.0 | 73/92 |
| A1 | Prose2K | False | True | 4991.9 | 71/98 |
| B1 | Code2K | True | True | 5158.4 | 73/92 |
| B1 | Prose2K | True | True | 5050.2 | 72/96 |
| B1 | Code2K | False | True | 4543.4 | 73/92 |
| B1 | Prose2K | False | True | 4890.3 | 72/95 |
| B1 | Code2K | False | True | 4520.8 | 73/92 |
| B1 | Prose2K | False | True | 4926.2 | 71/96 |
| B1 | Code2K | False | True | 4519.8 | 73/92 |
| B1 | Prose2K | False | True | 4970.4 | 71/98 |
| A2 | Code2K | True | True | 5177.3 | 73/92 |
| A2 | Prose2K | True | False | 无效输出，不计性能 | 0/0 |

保留所有预热、低值、输出摘要、分派记录及逐请求频率范围；无通用质量/PPL结论。频率采样不能排除短暂变化，READY显存不是峰值余量。未通过初筛时不追加确认以挑选样本。

离线复核：`python -X utf8 bench/results/a770/check_gu_q8_tailoff_model_20261027.py --self-check`。`--local`等待共享GPU锁后核对原始输出、分派日志及文件摘要。
