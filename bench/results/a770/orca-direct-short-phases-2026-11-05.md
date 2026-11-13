# Orca Direct：Short 解码的粗粒度 GPU 区间

2026-11-05（UTC+8），Windows 10 22H2，Intel Arc A770 16 GB，驱动 `32.0.101.9034`，oneAPI 2025.3。
使用当前默认 Host、运行库与通用 FP16 M16 pack，仅为两个后端各重编一个 Verifier Host TU。
顺序 Direct → L0 → Direct；每进程十次预热后保留五次 Short28，每请求生成 76 token。
固定实际 5771 专家槽、1536 MiB 预留、read256、decay0.95、MTP4/min-p0.5、
cumulative0、suffix0、cost-window0、prompt-cache0；Direct dependency-only1／BCS1。

原有 `STRATA_VERIFY_PROFILE=1` 会关闭共享／路由专家交错和部分独立 GDN／QSA 调度，
不能直接用它定位正常路径。本隔离补丁保持 `prof_on_` 为 false，仅复用请求开始／结束的报告入口，
在每层既有 0／16／17／20／24 边界及 head 0／1 写时间戳：48 层共 242 个原始标记／窗口。
两种后端均核对共享／路由交错日志；不改 Verifier ABI、数学内核或厂商驱动。

初版构建脚本误把三次出现的对象路径按一次替换，编译前检查失败，未启动编译／GPU。
保留 `short-coarse-build-r1` 失败记录。第二版编译通过，但首个请求的首窗口完成后，
校验把 Direct 浮点时钟周期 52.083333… 与 L0 的整数 52 作相等比较，诊断器退出，尚无输出 token。
保留 `short-coarse-build-r2`／`short-coarse-r2` 的失败日志；不把它作为硬件故障或性能采样。
本报告仅使用新目录 `short-coarse-build-r3`／`short-coarse-r3`。
修正版先在 CPU 准备阶段核对输出路径，并拒绝两种已知时钟表示以外的值，再等待共享实验锁。

按 A770 已校准的 19.2 MHz／36 位时钟计算差值，保留两个 API 报告的原始周期表示，
不使用取整后的 52 ns 缩放。检查计数、完整覆盖、回绕与单调性；
零值、缺失和倒退标记均拒绝。完整输出和每请求的实际 T 序列跨后端一致。
**45/45 请求完整匹配**，逐项复查原始 token／DONE 协议、草稿采纳／提供、reuse／resume／finish。

下表是保留五请求的阶段合计算术均值，单位 ms／请求；Direct 再取两侧进程的算术均值，
差值为 Direct 减 L0，正值表示 Direct 区间更长。算术均值使各阶段差值可相加。
G 为 36 个 GDN 层，Q 为 12 个 QSA 层；不是每层平均值。

| 区间 | Direct ms | L0 ms | 差 ms | 差异 | Direct 控制漂移 |
| --- | ---: | ---: | ---: | ---: | ---: |
| G:attention-half | 315.2271 | 317.3131 | -2.0860 | -0.657% | -1.641% |
| G:hc1-router-plan | 167.0353 | 160.4675 | +6.5677 | +4.093% | -1.775% |
| G:shared-routed-chain | 354.5616 | 352.8607 | +1.7009 | +0.482% | -0.885% |
| G:misses-combine | 6.9521 | 4.9697 | +1.9824 | +39.890% | -1.489% |
| G:between-layer | 1.3343 | 0.7581 | +0.5762 | +75.999% | -1.127% |
| Q:attention-half | 117.0602 | 115.2100 | +1.8502 | +1.606% | -1.815% |
| Q:hc1-router-plan | 57.2407 | 54.8111 | +2.4297 | +4.433% | -1.947% |
| Q:shared-routed-chain | 115.2636 | 114.9807 | +0.2829 | +0.246% | -0.788% |
| Q:misses-combine | 2.1950 | 1.6433 | +0.5517 | +33.576% | +1.939% |
| Q:between-layer | 0.6868 | 0.4596 | +0.2272 | +49.427% | +2.766% |
| head | 32.4211 | 33.4465 | -1.0254 | -3.066% | -1.085% |
| 全部标记跨度 | 1169.9777 | 1156.9203 | +13.0574 | +1.129% | -1.354% |

原有 Host 计时的 `stage` 仅覆盖 Verifier 每窗口输入准备：token／QSA 步骤、PLE 读取和 flag 重置。
保留日志报告的两位小数，再按实际窗口数加权；下列是近似墙钟时间，包含 I/O 等待，精度受日志取整限制。

| 进程 | 输入准备 ms／窗口 | 输入准备 ms／请求 |
| --- | ---: | ---: |
| 0／direct | 0.06 | 1.20 |
| 1／l0 | 0.04 | 0.80 |
| 2／direct | 0.06 | 1.20 |

这里只覆盖本配置的窗口输入准备，不是整个 CPU 成本；不包含 draft、commit、缓存适应或预填充。

在本窗口，GDN／QSA 的 hc1-router-plan 分别多用时 +4.09%／+4.43%，合计差 +9.00 ms／请求；两侧 Direct 进程的这两个区间均高于中间 L0。全部标记跨度差 +1.13%，Direct 控制漂移 -1.35%。
这把下一步细分重点指向 hc1、router 和设备计划，不能仅凭总体差值小于漂移的本轮诊断宣称稳定性能差距。窗口输入准备约 1.20／0.80 ms／请求，不能解释这个 GPU 区间的全部差异；也不意味着其他输入、缓存或预填充场景无需提前准备。

`attention-half` 包含 hc0、投影、GDN／QSA；`hc1-router-plan` 包含 hc1、路由、量化与设备计划；
`shared-routed-chain` 包含共享准备及共享／路由专家链；`misses-combine` 是余下分支与合并；
`between-layer` 包含跨层收尾（最后一层通向 head）。本配置为 no-host／device-plan／collect-routing，
不能把标为 misses 的阶段视为实际发生了 CPU 专家回填。

这是定位诊断：时间戳增加 barrier，请求还开启原有 Host decode 计时，每窗口额外回传并打印时间戳。
区间包含 GPU 工作和可能的依赖空隙，不是独占 kernel 时间。总跨度仅累计窗口内部，
不含窗口间的 draft／commit／CPU 准备，也不等于净 decode 时间。
不从插桩运行发布 tok/s、收益或 GPU 利用率，也不能用它排除 CPU 准备优化。
控制漂移不是置信区间；L0 仅一个进程，结论需用无插桩整模型对照验证。

等待时的准备应遵循“当前计算提交 → 准备已知数据 → 消费点等对应事件”：
PLE 已双缓冲预读下一块，Stager 已在工作线程准备固定页权重环形槽；固定模块／布局／命令模板提前建立。
路由 ID 可用后才能生成精确的专家计划，未来 token 未确定时不能准备对应的精确 PLE 数据。
复用槽位仍须等待上次 DMA 或最后读取者完成，不能为了提前准备而覆盖在用数据。

生产源码、默认策略和部署 EXE 未改变。本轮没有新的长上下文、PPL 或 Linux GPU 数据，
也不关闭既有 Direct 8K 间歇问题。

```powershell
python -X utf8 build-orca-slm-async-20261104/short_coarse_v3.py self-check
python -X utf8 build-orca-slm-async-20261104/short_coarse_v3.py verify-build
python -X utf8 build-orca-slm-async-20261104/short_coarse_v3.py verify r3
python -X utf8 build-orca-slm-async-20261104/publish_short_coarse.py --check
```

新采样先使用不存在的编号执行 `short_coarse_v3.py prepare r4`，再 `short_coarse_v3.py run r4`。
保持相同硬件、驱动、模型、初始 profile、参数、pack 和请求历史；不覆盖冻结运行。
归档含补丁脚本、生成源、编译／链接命令、配置、原始时间戳、完整协议及逐请求 JSON，不分发模型／EXE。

精确 JSON (`direct-short-phases-a770-windows-2026-11-05.json`, raw capture retained locally) ·
原始证据 (`direct-short-phases-a770-windows-2026-11-05.json.gz`, raw capture retained locally)
