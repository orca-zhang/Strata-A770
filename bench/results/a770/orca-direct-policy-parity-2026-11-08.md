# 相同Q2准入源码：较小固定缓存预算下的嵌套对照

2026-11-08，Windows10／Arc A77016GB／驱动32.0.101.9034／oneAPI2025.3.2。

此前合格性能表的Direct已允许Q2_0 shared-down并发，而L0保留旧准入条件。本次只为隔离L0构建扩展同一个Verifier白名单，复用已通过1152项L0检查的shared专家对象。新L0与Direct的整个Verifier及shared-expert源码文本逐字节相同；数学ZEBIN、运行时和其它Host对象复用。其它前端／适配层仍有差异，没有据此声称完整动态工作或纯API等价。

这是预先声明的新诊断配置：三种模式统一cache预算5000（此前5500），reserve仍为1536MiB，短提示热度准备均关闭。固定old-L0→matched-L0→Direct→matched-L0→old-L0，每进程Code2K×2后Short×10，Short保留最后5次中位数。要求五进程实际槽数／缓存MiB完全一致、至少5000槽；遇容量、质量或驱动失败停止，不追补重跑。新条件不替代或救援此前失败的5771槽测试，也不能把百分比转移到原5771槽或当前spec6生产成绩。

matched-L0／Direct／matched-L0形成内层后端对照；matched-L0两侧几何均值与old-L0两侧几何均值形成对称策略对照，两个L0模式各有首尾漂移。只执行这个预设顺序，没有独立反序；所有漂移绝对值门槛3%，不是置信区间。准备保留在TTFT／整请求，排除在decode；没有新增GPU时间戳、逐kernel／token计时或回读探针。

完整请求60条／输出4780个。

实际容量：[5243, 7201]（槽数／MiB）；全部原始输出协议和独立指标复算通过；控制漂移门：True。

| 场景 | old-L0 tok/s | matched-L0 tok/s | Direct tok/s | L0新准入吞吐变化 | Direct相对matched-L0吞吐变化 |
| --- | ---: | ---: | ---: | ---: | ---: |
| first2K | 27.782 | 27.614 | 27.740 | -0.606% | +0.457% |
| repeat2K | 34.574 | 34.758 | 34.297 | +0.532% | -1.325% |
| warmShort | 50.505 | 50.822 | 49.918 | +0.628% | -1.779% |

| 场景／指标 | 新L0相对old-L0耗时变化 | Direct相对新L0耗时变化 | old-L0控制漂移 | 新L0控制漂移 |
| --- | ---: | ---: | ---: | ---: |
| first2K／ttft_ms | +0.460% | -11.086% | +0.220% | -1.585% |
| first2K／decode_ms | +0.609% | -0.455% | +0.309% | +1.500% |
| first2K／request_ms | +0.482% | -9.485% | +0.233% | -1.143% |
| repeat2K／ttft_ms | +0.764% | -17.749% | +1.728% | +0.157% |
| repeat2K／decode_ms | -0.529% | +1.343% | -0.429% | -0.176% |
| repeat2K／request_ms | +0.515% | -14.101% | +1.306% | +0.094% |
| warmShort／ttft_ms | -0.240% | +0.105% | +0.107% | -0.116% |
| warmShort／decode_ms | -0.624% | +1.811% | -0.374% | -0.167% |
| warmShort／request_ms | -0.250% | +1.017% | -0.136% | -0.379% |

完整源码准入相同不证明全部地址、动态路由、内存访问或GPU调度完全相同；本轮未抓取动态计划。固定较小缓存和关闭短提示热度都是明确的新条件，不能从跨轮数字反推其独立影响，或把剩余差值归因于API／锁。

生产源码、部署和默认推荐L0保留；Linux GPU仍待用户测试，没有新增8K／PPL数据。

[原5771槽合格对照](orca-direct-qualified-contexts-r2-2026-11-08.md) · [先前Q2调度隔离](orca-direct-q2-fork-2026-11-07.md)

[JSON](../../../sycl/orca/direct-policy-parity-a770-windows-2026-11-08.json) · 精确源码／编译头／原始协议 (`direct-policy-parity-a770-windows-2026-11-08.json.gz`, raw capture retained locally)

离线复核：`python -X utf8 build-orca-policy-parity-20261108/publish.py check`。新采样使用新目录、保留全部条件和原始失败，资产复用身份缓存。
