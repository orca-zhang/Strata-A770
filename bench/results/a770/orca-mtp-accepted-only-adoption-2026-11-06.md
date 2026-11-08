# MTP 只补算已采纳行：接受小幅收益后采用

2026-11-06，Windows 10／Intel Arc A770 16GB／Xeon E5-2696 v4／驱动32.0.101.9034／oneAPI2025.3.2。
本机主配置已启用 `STRATA_MTP_ACCEPTED_ONLY=1`，下次启动生效。
沿用现有EXE和images，将MTP补算范围从整个验证窗口收缩到目标模型已提交的 `a+1` 行；
后续轮次会覆盖未采纳行的草稿KV槽。置信度、量化和采样参数保持原值。

| 2048输入／128输出 | 正序净decode吞吐变化 | 独立反序净decode吞吐变化 | 正序控制耗时漂移 | 反序控制耗时漂移 |
| --- | ---: | ---: | ---: | ---: |
| Code2K | +0.963714% | +0.498766% | +1.166724% | −0.092188% |
| Prose2K | +0.766691% | +0.101509% | +1.344526% | +0.608136% |

顺序为关→开→关、开→关→开，共六个独立引擎、42次完整请求、4704个输出token。
每进程先运行Short23/16，再交错三轮Code／Prose；首轮不计，保留后两轮中位数，外侧控制取几何均值。
两侧逐项核对全部token、输入／生成数、草稿采纳／提供数、结束原因、reuse、resume及原始协议。
不启用逐层计时或额外GPU回读；净decode使用引擎DONE计时，排除READY、初始化及prefill。

收益很小，尤其Prose反序的+0.10%接近测量噪声，不能据此认定稳定加速。
原Code≥0.2%／Prose≥0.5%规则的反序判定仍为 `false`，没有改写旧记录或追加挽救重复。
本次采用依据是用户随后明确允许较小改善，以及两个顺序均观察到正decode变化、漂移在3%内和功能检查通过；
这是本候选的采用决定，不改变其他实验的筛选规则。

TTFT和整请求没有全部改善：Code的TTFT耗时正／反序约−0.0004%／−0.4282%，
Prose约+1.1437%／+0.6921%；Prose整请求耗时约+0.7823%／+0.5401%。
因此不能把这次decode观察写成完整请求提速，也不与此前HC、缓存或运行时收益相加。

固定U4 N16/load4/M16、HC SG16／NC1、`--expert-cache 6200`（实际6509槽）、adapt-swaps16、
context8192、reserve1536MiB、MTP4及step／last／cumulative门槛0.5。
采用前两设置各完成首次8K→Short→Code2K→Prose2K→重复8K，
再执行两次Chat Completions、一次Messages和一次Responses，共18/18次完整请求匹配彼此及此前参考。
health／models检查通过，进程均正常退出。原／新分别168／167个1Hz显存样本，
最低空闲322.359／303.090MiB，均未通过原1024MiB余量线；保留既有8K上下文的余量限制。

主配置只新增一个环境开关，原配置逐字节备份到 `strata-iq2_xs-before-mtp-accepted-only-20261106.json`；
可设 `STRATA_MTP_ACCEPTED_ONLY=0` 回退该项，或恢复备份。RAM保护、原生回退、权重、专家列表和EXE／images均保留。
本轮未新增PPL、Linux GPU或累计20%目标结论。

本机复核，不重新运行GPU：

```powershell
python -X utf8 build-orca-decode20-20261105/mtp_accepted_only_small_default.py check
```

同条件复测须保持同一设备、驱动、编译器、EXE／images、权重／初始profile、参数、输入和进程顺序，使用新输出目录。
归档保留采用决定、原失败判定、原始协议、配置、检查脚本及依赖摘要；不包含权重或二进制，仍需本机已保留资源。

[JSON](../../../sycl/orca/mtp-accepted-only-adoption-a770-windows-2026-11-06.json) · [文本归档](../../../sycl/orca/mtp-accepted-only-adoption-a770-windows-2026-11-06.json.gz)
