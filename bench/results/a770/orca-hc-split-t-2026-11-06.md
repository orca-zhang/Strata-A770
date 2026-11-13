# HC 融合 Q8：独立 T1／T4 入口与小参数捕获

两后端均未通过完整Short采用门，候选保持隔离。

2026-11-06，Windows10，Intel Arc A77016GB，驱动32.0.101.9034，oneAPI2025.3.2。
本组L0指Orca L0，Direct绕过Level Zero；没有原生产SYCL或Linux GPU新测量。

上一轮单入口内同时包含通用及T1／T4分支，没有通过收益门。本组新增两个独立入口，
只捕获1／4行参数；原通用入口与T2／T3／T5..8回退保留。
`STRATA_ORCA_HC_FIXED_T=1`仅在隔离Host中开启，缺省关闭。
两个后端各只重编同一个fused_gr Host TU，链接冻结的其余数学／核心依赖；没有部署到生产。
新EXE的静态PE导入检查确认两后端无SYCL／UR／oneMKL直接导入；L0导入ze_loader.dll，Direct无此导入。
这是静态直接导入检查，不是运行时或递归DLL审计；工具、完整输出和摘要随记录保留。
同一后端的基线／候选使用同一EXE和扩展pack，只切换该标志。
不改变派发数、队列、拷贝、凑批等待、归约／量化顺序或SLM布局；partials仍按原8行物理步长寻址。

| 入口 | 捕获对象字节 | SIMD／GRF | private字节／硬件线程 | spill字节 | 代码段字节 |
| --- | ---: | --- | ---: | ---: | ---: |
| 原通用Q8 | 616 | 16／256 | 9728 | 0 | 27840 |
| T1 | 112 | 16／256 | 1792 | 0 | 24768 |
| T4 | 328 | 16／256 | 5248 | 0 | 42240 |

104／320字节是std-layout参数块，捕获对象还包含local accessor。新入口使用新的编译器ABI；
原25入口的参数描述、参数数及捕获尺寸逐项相同，部署的25份原生映像逐字节保持。
前端有21份逐字节相同，另外4份仅8个OpName的生成类型编号后缀改变；
对应目标ID、所有其他指令及构建标志完全相同。检查只接受明确类型名的数字后缀变化，
并验证拒绝指令修改、非数字改名及截断文件，不能泛称25份SPIR-V字节相同。
private是编译声明，不等同实际显存流量、占用率或耗时；T4代码仍大于通用入口，不能由资源量推算提速。

首版前端字节相等检查因上述类型编号变化失败；独立接续脚本首次又把GRF匹配计数当列表，
在烘焙前以TypeError退出。两次失败结果及第二次源码快照保留；修正版只改计数读取并另用新目录。
未覆盖失败或重编译取通过。两个新增映像离线烘焙后，与匹配Host在两后端×基线／候选上
通过256项完整HC检查；T1..8、两次改变输入、eager／replay、源／输出／guard、CPU FP64及独立Q8参考，
全部64个输出哈希与冻结原程序一致。

整模型计划在采样前固定：Direct基线→候选→基线，L0同序，再L0及Direct反序。
每新进程15次相同Short，丢前十、留后五中位数；外侧同模式取几何均值，共180次请求。
每请求输入23token，最多128输出，自然EOS70；完整tokens、草稿采纳／提供、结束及reuse/resume全部比较。
固定实际5771专家槽、reserve1536MiB、U4/min16/M16、read256、adapt32/decay0.95、MTP4，
step／last min-p0.5、cumulative/suffix/cost-window/prompt-cache0，Direct BCS1、ISA／线程内存device。
不读取当前生产JSON选择参数，没有内层profiler、GPU时间戳或额外GPU回读。
净decode排除准备／初始化／prefill；TTFT与直接测得的整请求分别报告，首窗口重叠意味着两者不能相加。

| 臂 | 净decode tok/s | TTFT ms | 整请求 ms | READY ms |
| --- | ---: | ---: | ---: | ---: |
| 0-direct-baseline | 49.5155 | 759.346 | 2131.862 | 30874.422 |
| 1-direct-candidate | 49.4211 | 758.970 | 2134.032 | 29903.922 |
| 2-direct-baseline | 49.3514 | 759.783 | 2136.207 | 29766.450 |
| 3-l0-baseline | 51.2483 | 751.199 | 2076.018 | 26515.958 |
| 4-l0-candidate | 49.4315 | 773.832 | 2150.991 | 27255.955 |
| 5-l0-baseline | 49.9251 | 769.919 | 2134.066 | 26417.758 |
| 6-l0-candidate | 49.5330 | 773.959 | 2146.401 | 26286.674 |
| 7-l0-baseline | 49.7194 | 770.074 | 2136.869 | 26713.460 |
| 8-l0-candidate | 49.9679 | 772.525 | 2131.975 | 26276.854 |
| 9-direct-candidate | 48.4697 | 778.764 | 2181.165 | 31132.183 |
| 10-direct-baseline | 49.2923 | 758.969 | 2136.478 | 30453.405 |
| 11-direct-candidate | 49.6595 | 762.942 | 2131.644 | 29987.850 |

| 后端／顺序 | 净decode变化 | decode控制漂移 | TTFT变化 | 整请求变化 |
| --- | ---: | ---: | ---: | ---: |
| direct／基线→候选→基线 | -0.025% | -0.331% | -0.078% | -0.000% |
| l0／基线→候选→基线 | -2.275% | -2.582% | +1.753% | +2.193% |
| l0／候选→基线→候选 | +0.061% | +0.878% | +0.411% | +0.108% |
| direct／候选→基线→候选 | -0.469% | +2.455% | +1.560% | +0.926% |

| 顺序窗口 | 基线Direct／L0净decode | 候选Direct／L0净decode |
| --- | ---: | ---: |
| 0 | -2.272% | -0.021% |
| 1 | -0.859% | -1.385% |

跨后端组依次执行；表中使用各自包围对照的代表值，控制漂移不是跨后端置信区间。

每后端要求两个顺序decode均至少+1%，所有报告指标控制漂移绝对值≤3%，TTFT／整请求退步≤3%，完整质量通过。
结果：`{"by_backend": {"direct": {"control_drift": true, "request_nonregression": true, "short_decode_gain": false, "ttft_nonregression": true}, "l0": {"control_drift": true, "request_nonregression": true, "short_decode_gain": false, "ttft_nonregression": true}}, "eligible_for_long_context": [], "quality": true}`。
不追加重复取通过；只有完整通过的后端才可进入后续长上下文验证。
编译与数学优化可由两后端共享，不能将收益全计为移除API层。
本组未关闭历史Direct8K间歇故障，没有新广泛PPL或长期稳定性结论。
默认源码、生产配置和部署EXE未改变；Orca L0仍是推荐路径，Direct为WIP。
构建、GPU、重型哈希及归档持共享锁；资产采用明确标注的stat-cached SHA256身份核对。
归档保留源码、原计划、失败、资源、完整原始HC和模型协议，不分发权重或二进制。

独立复核已有记录，不重跑GPU：

```powershell
python -X utf8 build-orca-hc-split-t-20261106/publish.py --check
```

同条件复测须相同硬件、驱动、编译器、冻结资产／EXE／pack、argv/env及请求历史，使用新输出目录。
[上一轮单入口试验](orca-hc-fixed-t-2026-11-06.md) · [同数学全Host与上下文对照](orca-direct-argument-array-2026-11-06.md)

JSON (`hc-split-t-a770-windows-2026-11-06.json`, raw capture retained locally) · 精确文本归档 (`hc-split-t-a770-windows-2026-11-06.json.gz`, retained locally for privacy)
