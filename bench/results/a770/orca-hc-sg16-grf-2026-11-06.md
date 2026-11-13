# 当前 SG16 HC Q8：128／256 GRF 整模型对照

没有后端通过完整Short采用门，候选保持隔离。

2026-11-06，Windows10，Intel Arc A77016GB，驱动32.0.101.9034，oneAPI2025.3.2。
L0指Orca L0；本组没有原生产SYCL新测量。Direct沿用已验收全量栈数组Host EXE，L0沿用匹配完整源码构建。

此前256GRF用于SG32融合HC Q8的spill消除；当前内核已是SG16，本组只移除该单映像的large-register-file编译选项。
两版来自同一份SPIR-V，256重新编译的ZEBIN与原映像逐字节相同；128仅改变image_3和两个跟踪清单。
payload及per-thread-payload元数据完全一致；其它数学内核、运行库、Host、模型资产和调度保持冻结。

| 配置 | SIMD | GRF | private字节／硬件线程 | spill字节 | barrier |
| --- | ---: | ---: | ---: | ---: | ---: |
| 256 | 16 | 256 | 9728 | 0 | 1 |
| 128 | 16 | 128 | 9728 | 0 | 1 |

寄存器减少没有引入编译器报告的spill，但private空间仍然存在；这里没有测量实际占用率或物理显存流量。
完整原生元数据仅有grf_count256→128及eu_thread_count4→8两处变化；后者是编译器声明，不是运行时占用率。
机器码也发生变化：代码段256／128配置分别为27840／27392字节，重定位表和兼容标记完全相同。
代码字节数不等于指令数；本组无法单独分离寄存器资源、编译器生成代码与GRF切换的影响。
两后端×两种GRF配置通过256项完整HC检查：T1..8、改变输入、eager／replay、全部输出／源／guard、CPU FP64及独立Q8参考。
64个完整输出哈希在四种组合中一致。CPU元数据自检也覆盖真实零／非零spill记录、ABI变化和缺少参数元数据的拒绝。

模型计划在测量前固定：Direct256→128→256，L0256→128→256，再L0128→256→128、Direct128→256→128。
每个新进程15次相同Short，丢前十次、保留五次中位数，外侧同配置取几何均值；共180次请求，不追加重复来通过门。
输入23token、最多128输出、自然EOS70；全部180/180完整token／草稿采纳／提供／结束及reuse/resume匹配。
实际5771专家槽、reserve1536MiB、U4/min16/M16、read256、adapt32/decay0.95、MTP4，
step／last min-p0.5、cumulative/suffix/cost-window/prompt-cache0；Direct BCS1、ISA及线程内存device。
不从当前启动JSON取参数；同一后端使用同一EXE，只有那一个数学映像变化。
无内层profiler、GPU时间戳或额外回读；净decode排除准备、初始化和prefill，TTFT和整请求分别计时。
TTFT与引擎decode的首窗口有重叠，不能相加。实际请求序列由groups和selection指定；
清单继承的旧order／sequence／candidate_exe仅保留来源上下文，不参与本组调度。

| 臂 | 净decode tok/s | TTFT ms | 整请求 ms | READY ms |
| --- | ---: | ---: | ---: | ---: |
| 0-direct-256 | 49.4350 | 757.098 | 2131.671 | 30059.928 |
| 1-direct-128 | 50.1145 | 755.290 | 2111.547 | 30259.113 |
| 2-direct-256 | 49.6630 | 757.063 | 2125.620 | 29848.179 |
| 3-l0-256 | 51.3761 | 752.864 | 2074.865 | 26478.209 |
| 4-l0-128 | 51.3498 | 750.498 | 2072.988 | 26367.984 |
| 5-l0-256 | 51.2520 | 754.188 | 2078.845 | 26130.037 |
| 6-l0-128 | 51.4819 | 754.955 | 2075.313 | 26098.494 |
| 7-l0-256 | 51.4290 | 753.321 | 2074.519 | 25979.403 |
| 8-l0-128 | 51.4365 | 750.240 | 2071.167 | 26109.379 |
| 9-direct-128 | 49.9500 | 754.967 | 2116.142 | 29897.899 |
| 10-direct-256 | 49.6630 | 758.878 | 2129.161 | 29704.543 |
| 11-direct-128 | 50.1145 | 756.952 | 2112.985 | 29830.641 |

| 后端／顺序 | 128/256净decode变化 | decode控制漂移 | TTFT变化 | 整请求变化 |
| --- | ---: | ---: | ---: | ---: |
| direct／256→128→256 | +1.142% | +0.461% | -0.237% | -0.803% |
| l0／256→128→256 | +0.070% | -0.242% | -0.402% | -0.186% |
| l0／128→256→128 | +0.059% | -0.088% | -0.097% | -0.062% |
| direct／128→256→128 | +0.743% | +0.329% | -0.385% | -0.686% |

| 顺序窗口 | GRF256 Direct／L0净decode | GRF128 Direct／L0净decode |
| --- | ---: | ---: |
| 0 | -3.440% | -2.405% |
| 1 | -3.434% | -2.773% |

后端表使用同GRF、同数学输入的各自包围对照代表值；两个后端组依次执行，控制漂移不是跨后端置信区间。
每后端均要求两个顺序decode至少+1%、所有报告指标控制漂移绝对值不超过3%、TTFT和整请求不恶化超过3%，且完整质量通过。
控制漂移不是置信区间；不把不同历史的Short绝对成绩混为同条件回退。
结果：`{"by_backend": {"direct": {"control_drift": true, "request_nonregression": true, "short_decode_gain": false, "ttft_nonregression": true}, "l0": {"control_drift": true, "request_nonregression": true, "short_decode_gain": false, "ttft_nonregression": true}}, "eligible_for_long_context": [], "quality": true}`。

这是编译资源选择，两后端可能共同受益，不能把共享数学收益计为去除API层的收益。
没有新长上下文、广泛PPL、Linux GPU或长期稳定性结论；原Direct8K间歇故障未关闭。
本报告不替换部署EXE、启动配置或源码默认。Orca L0仍是推荐路径，Direct仍为WIP。
资产用明确标注的stat-cached SHA256检查；编译、GPU、重型哈希及归档持共享锁。
归档保留脚本、计划、编译命令、元数据、完整原始日志／协议和精确文本，不分发权重或二进制。

独立复核已有记录，不重新运行GPU：

```powershell
python -X utf8 build-orca-hc-sg16-grf-20261106/publish.py --check
```

同条件复测需要相同硬件／驱动／编译器、冻结EXE／pack／资产、参数和请求历史，并使用新输出目录。
父构建、数值pack及源转换的精确来源见[全Host对照](orca-direct-argument-array-2026-11-06.md)和[当前源码对照](orca-current-tree-2026-11-05.md)。
[此前SG32寄存器实验](orca-hc1-production-contexts-2026-11-05.md) · [当前窗口诊断](orca-direct-current-window-2026-11-06.md)

JSON (`hc-sg16-grf-a770-windows-2026-11-06.json`, raw capture retained locally) · 本轮精确原始文本 (`hc-sg16-grf-a770-windows-2026-11-06.json.gz`, raw capture retained locally)
