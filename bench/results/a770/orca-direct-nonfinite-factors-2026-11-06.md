# Direct：提前准备与镜像回填的四组隔离检查

2026-11-06，Windows／Intel Arc A770 16GB／驱动32.0.101.9034／oneAPI2025.3.2。
用户接受较小收益后，L0 已默认采用 MTP 位置元数据提前准备与持久镜像批量回填；
本轮继续检查 Direct 组合验证曾出现的 8K 非有限值，没有调整置信度、量化或数学内核。

| Direct 设置 | 首次2K | 重复2K | 8K | Short |
| --- | --- | --- | --- | --- |
| 两项关闭 | 一致 | 一致 | 一致 | 一致 |
| 仅 MTP 提前准备 | 一致 | 一致 | 一致 | 一致 |
| 仅镜像批量回填 | 一致 | 一致 | 一致 | 一致 |
| 两项开启 | 一致 | 一致 | 一致 | 一致 |

四个独立进程按上表顺序，各直接运行2K→2K→8K→Short，每请求最多128个输出token。
16/16条完整协议逐项匹配冻结参考：全部token、生成数、输入数、草稿采纳／提供数、结束原因、reuse及resume；
另核对原始T／DONE记录和退出码。没有先跑Short预热8K，也没有失败后追加重复取通过。

固定实际5771专家槽、1536MiB预留、MTP4、step／last min-p0.5、cumulative0、U4/min16/M16、read256、
adapt32/decay0.95、suffix／cost-window／prompt-cache0、Direct BCS1。
复用11月6日冻结引擎、已校准全局显存记账Runtime及原数学包，仅重编隔离prefill Host单元和检查程序。
MTP／generate链接此前两项开关已接入的对象；本轮不是当前生产SG16包的完整重建。

非有限值错误现在分别报告 `input` 与 `hidden` 标志，复用原来chunk末尾的检查；
没有增加逐层计时、逐层回读或有限值探针。Direct和L0分别注入真实输入NaN、中间激活NaN及两者同时NaN，
共6种后端／异常组合均正确拒绝，并验证下一次begin清零恢复。

本轮没有复现异常，不能确定原8K故障的根因或宣布修复。
[原组合失败与L0默认采用记录](orca-preparation-defaults-2026-11-06.md)完整保留；
L0保留已验证的默认值，Direct两项仍显式开启。本轮不宣称新的TTFT／decode收益、PPL或Linux GPU结论。
原始READY、prefill、TTFT、净decode和整请求耗时均保留，准备／初始化／prefill不计入净decode。

本机复核，不重新运行GPU：

```powershell
python -X utf8 build-orca-direct-nonfinite-20261106/publish.py check
```

同条件复测须使用新目录，保持设备、驱动、编译器、冻结引擎／数学包、模型／初始profile、参数、输入与请求顺序。
文本归档保留脚本、源代码、差异、编译／链接命令、完整协议及原失败；权重和二进制只记录身份摘要，不随归档分发。

[JSON](../../../sycl/orca/direct-nonfinite-factors-a770-windows-2026-11-06.json) · 精确文本归档 (`direct-nonfinite-factors-a770-windows-2026-11-06.json.gz`, retained locally for privacy)
