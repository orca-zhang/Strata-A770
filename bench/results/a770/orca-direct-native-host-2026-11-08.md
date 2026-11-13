# A770：原生 Host 分配路径校准与隔离验证（2026-11-08）

原生Host分配已移入Windows Direct源码，当前源码的所有权、失败清理及完整注册CTests通过。部署EXE与默认参数未替换；Direct仍为WIP，L0仍为推荐后端；Linux GPU尚未验证。

同一Direct后端的84次诊断已完成：首次2K的TTFT耗时下降15.155%／17.419%，整请求耗时下降13.197%／15.070%；热态Short decode耗时为+0.044%／−0.969%，没有建立稳定提速。该改动主要减少首次准备，不能解释此前Direct比L0慢约2%的Short差距。

Windows、Arc A770 16GB、驱动32.0.101.9034。25次原生分配校准覆盖64KiB至2GiB，全部CPU内容检查通过，校准没有提交计算内核。校准程序加载Intel UMD/GMM；独立执行候选仍只通过D3DKMT与驱动通信。

当前标准路径先VirtualAlloc，再使用WDDM Standard ExistingHeap的完整共享标志。原生Host路径使用2376字节私有资源描述、pSystemMem=nullptr、CreateAllocation flags=0，通过Lock2获得CPU映射。这里的Lock2是建立CPU映射的系统接口，不是每次派发的应用互斥锁。

达到2MiB后，原生资源对齐及分配大小向上按2MiB取整；2MiB+64KiB对应4MiB资源。该元数据不能证明GPU实际使用2MiB PTE。Host缓存策略字段和LocalOnly/NonLocalOnly等资源标志不同；不能从字段差异直接推断性能。

第一版复现原生CPU/GPU同址映射后，普通FreeGpuVirtualAddress返回STATUS_INVALID_PARAMETER。原样保留build-r3失败日志，没有提交计算内核。校准观察到厂商未对这些Host资源调用该释放接口。当前候选使用Orca已有的CPU/GPU独立地址映射，CPU锁定、解锁、同步销毁分开管理；导入调用者页面仍走标准路径。

补充逐字节检查发现，2GiB资源还有Gpu.NoRestriction标志；初版序列化遗漏此位，检查失败原样保留。驱动离线structToTokens转换证明offset0x1e/mask0x04只影响该语义字段，不能当作填充字节忽略。build-r5已补齐该位，实际C++函数生成的2376字节描述与全部25份实测描述一致；仅归零已校准的进程指针、分配标识和头部填充，共60字节。此为CPU检查，不代表GPU或整模型通过。

另加5次2GiB边界校准：请求2GiB−2MiB时资源保持该大小且NoRestriction=0；请求2GiB−64KiB、2GiB−1或2GiB时，资源取整到2GiB且NoRestriction=1。实际序列化比较全部通过，累计30份描述匹配。

隔离候选通过64KiB、2MiB+64KiB、16MiB、2GiB的Host全字节内容、CPU WB属性、调用者导入所有权、6处失败注入与清理、CPU编码检查，以及真实GPU传输检查。9次原生创建/9次同步销毁/8次解锁平衡；GPU检查覆盖460组录制、920次重放、H2D/D2D/D2H/H2H、非对齐地址、捕获填充值、完整缓冲区和边界保护。运行依赖检查确认未加载SYCL、UR、Level Zero、Intel UMD/GMM或IGC。

模型诊断固定HC128、MTP4、min-p0.5、5771个实际专家槽、1536MiB显存预留。使用相同模型、U4资产、专家历史和计算代码，标准／原生／标准与原生／标准／原生两个顺序，各执行首次2K、重复2K、10次Short、两次8K。准备计入TTFT与整请求，排除在decode_ms之外；两端仅保留首次U4加载分段计时，未加入逐token或kernel计时。

| 指标 | 顺序 | 标准路径 ms | 原生Host ms | 耗时变化 | 控制漂移 |
| --- | --- | ---: | ---: | ---: | ---: |
| first2K/ttft_ms | forward | 27699.541 | 23501.620 | -15.155% | -2.812% |
| first2K/decode_ms | forward | 4433.314 | 4397.200 | -0.815% | -2.431% |
| first2K/request_ms | forward | 32082.377 | 27848.439 | -13.197% | -2.762% |
| repeat2K/ttft_ms | forward | 12675.035 | 12652.262 | -0.180% | +0.389% |
| repeat2K/decode_ms | forward | 3538.718 | 3519.900 | -0.532% | -0.853% |
| repeat2K/request_ms | forward | 16174.646 | 16132.836 | -0.258% | +0.116% |
| warmShort/ttft_ms | forward | 639.991 | 635.034 | -0.775% | -0.147% |
| warmShort/decode_ms | forward | 1363.096 | 1363.700 | +0.044% | -0.454% |
| warmShort/request_ms | forward | 1971.197 | 1968.002 | -0.162% | -0.282% |
| first8K/ttft_ms | forward | 31042.777 | 30970.171 | -0.234% | +0.711% |
| first8K/decode_ms | forward | 4834.967 | 4812.900 | -0.456% | -1.166% |
| first8K/request_ms | forward | 35839.852 | 35745.196 | -0.264% | +0.457% |
| repeat8K/ttft_ms | forward | 30972.144 | 30969.263 | -0.009% | -0.094% |
| repeat8K/decode_ms | forward | 3836.696 | 3844.000 | +0.190% | -0.271% |
| repeat8K/request_ms | forward | 34770.828 | 34775.603 | +0.014% | -0.113% |
| u4_allocation_ms | forward | 6520.032 | 2071.238 | -68.233% | -4.441% |
| u4_read_validate_ms | forward | 8445.419 | 8680.878 | +2.788% | -4.908% |
| u4_total_ms | forward | 14980.920 | 10768.204 | -28.121% | -4.697% |
| first2K/ttft_ms | reverse | 28251.362 | 23330.199 | -17.419% | +1.472% |
| first2K/decode_ms | reverse | 4383.200 | 4393.889 | +0.244% | -0.450% |
| first2K/request_ms | reverse | 32584.026 | 27673.756 | -15.070% | +1.165% |
| repeat2K/ttft_ms | reverse | 12616.492 | 12555.366 | -0.484% | -0.011% |
| repeat2K/decode_ms | reverse | 3526.400 | 3499.740 | -0.756% | +0.473% |
| repeat2K/request_ms | reverse | 16103.839 | 16016.006 | -0.545% | +0.099% |
| warmShort/ttft_ms | reverse | 640.120 | 632.982 | -1.115% | +0.135% |
| warmShort/decode_ms | reverse | 1362.600 | 1349.400 | -0.969% | +0.119% |
| warmShort/request_ms | reverse | 1972.113 | 1954.137 | -0.912% | -0.023% |
| first8K/ttft_ms | reverse | 30785.106 | 30989.979 | +0.665% | -0.660% |
| first8K/decode_ms | reverse | 4819.200 | 4801.845 | -0.360% | +0.290% |
| first8K/request_ms | reverse | 35566.263 | 35754.033 | +0.528% | -0.533% |
| repeat8K/ttft_ms | reverse | 32053.278 | 31051.054 | -3.127% | +0.424% |
| repeat8K/decode_ms | reverse | 3827.900 | 3823.898 | -0.105% | +0.178% |
| repeat8K/request_ms | reverse | 35843.172 | 34837.320 | -2.806% | +0.396% |
| u4_allocation_ms | reverse | 7063.567 | 2196.348 | -68.906% | +6.072% |
| u4_read_validate_ms | reverse | 8507.124 | 8461.136 | -0.541% | +1.484% |
| u4_total_ms | reverse | 15585.823 | 10675.168 | -31.507% | +2.459% |

84次协议的输出token及草稿计数逐项核对。门结果：`{"quality": true, "drift": true, "no_regression": true}`。该诊断不直接用于默认采用，也不等于当前spec6部署配置的收益。

漂移门只覆盖TTFT、decode和整请求。U4 allocation分段的控制漂移为−4.441%／+6.072%，超过3%；分段数字仅用于定位，不作为通过同等漂移门的独立收益结论。未做PPL测试；精度检查限于本轮完整贪心输出与草稿计数一致。

当前工作树的运行时源码快照加同一Host补丁，也已通过上述生命周期、失败清理、CPU编码和真实GPU传输检查。该快照检查独立于冻结版本的模型计时。

当前源码的首轮无额外计时对照被容量保护中止：完成14次L0请求后，原生端READY报告5763槽，L0为5771槽。没有原生模型请求，不能算同条件性能；失败、启动日志和全部L0样本保留。当前源码新增的全局显存上限参与缓存裁剪，容量保护未放宽。后续r3固定原运行时，仅改变Host分配，保持5771槽；结果不代表当前完整工作树性能。更早的启动脚本换行哈希错误保留；r2在取得GPU锁前取消，原因是应隔离新链接输出，避免覆盖已完成诊断的EXE和日志。旧诊断文件哈希复核未变，这两次启动均无模型样本。

补充无额外诊断计时的L0／冻结运行时原生Host对照，同样覆盖84次请求、正反顺序、HC128、MTP4和实际5771槽。请求顺序为2K→2K→Short×10→8K→8K，Short取后5次中位数，不能和旧的8K之后Short历史混算。先列净decode吞吐，再列各段耗时；吞吐正数表示更快，耗时正数表示更慢。

| Decode位置 | 正序L0／原生 tok/s | 原生吞吐变化 | 反序L0／原生 tok/s | 原生吞吐变化 |
| --- | ---: | ---: | ---: | ---: |
| first2K | 29.293／29.180 | -0.385% | 29.323／29.078 | -0.837% |
| repeat2K | 36.612／36.366 | -0.673% | 36.665／36.336 | -0.896% |
| warmShort | 52.759／51.569 | -2.254% | 52.751／51.655 | -2.077% |
| first8K | 26.661／26.646 | -0.060% | 26.718／26.616 | -0.382% |
| repeat8K | 33.324／32.871 | -1.359% | 33.634／33.530 | -0.308% |

forward：Short吞吐变化-2.254%，重复8K为-1.359%；落后幅度缩小+0.896个百分点，绝对差距缩小+0.896个百分点。这是固定请求历史的观察；上下文、草稿轨迹和访存工作集同时变化，不单独证明API或等待为根因。

reverse：Short吞吐变化-2.077%，重复8K为-0.308%；落后幅度缩小+1.769个百分点，绝对差距缩小+1.769个百分点。这是固定请求历史的观察；上下文、草稿轨迹和访存工作集同时变化，不单独证明API或等待为根因。

| 指标 | 顺序 | L0 ms | 原生Host Direct ms | 耗时变化 | 控制漂移 |
| --- | --- | ---: | ---: | ---: | ---: |
| first2K/ttft_ms | forward | 25903.366 | 23877.349 | -7.821% | +1.662% |
| first2K/decode_ms | forward | 4369.700 | 4386.600 | +0.387% | +0.023% |
| first2K/request_ms | forward | 30216.282 | 28213.598 | -6.628% | +1.430% |
| repeat2K/ttft_ms | forward | 15217.821 | 12716.460 | -16.437% | -2.232% |
| repeat2K/decode_ms | forward | 3496.097 | 3519.800 | +0.678% | +0.252% |
| repeat2K/request_ms | forward | 18675.445 | 16197.114 | -13.271% | -1.777% |
| warmShort/ttft_ms | forward | 710.308 | 635.098 | -10.588% | -0.251% |
| warmShort/decode_ms | forward | 1326.799 | 1357.400 | +2.306% | +0.196% |
| warmShort/request_ms | forward | 2006.414 | 1961.537 | -2.237% | -0.029% |
| first8K/ttft_ms | forward | 33674.391 | 31091.273 | -7.671% | +0.644% |
| first8K/decode_ms | forward | 4800.938 | 4803.800 | +0.060% | +0.453% |
| first8K/request_ms | forward | 38434.206 | 35857.056 | -6.705% | +0.620% |
| repeat8K/ttft_ms | forward | 33775.163 | 31010.866 | -8.184% | +0.581% |
| repeat8K/decode_ms | forward | 3841.100 | 3894.000 | +1.377% | +0.099% |
| repeat8K/request_ms | forward | 37577.044 | 34866.763 | -7.213% | +0.533% |
| first2K/ttft_ms | reverse | 25755.690 | 23781.567 | -7.665% | -1.741% |
| first2K/decode_ms | reverse | 4365.100 | 4401.943 | +0.844% | -0.365% |
| first2K/request_ms | reverse | 30064.844 | 28130.615 | -6.434% | -1.545% |
| repeat2K/ttft_ms | reverse | 15036.610 | 12632.570 | -15.988% | +0.778% |
| repeat2K/decode_ms | reverse | 3491.100 | 3522.648 | +0.904% | +0.236% |
| repeat2K/request_ms | reverse | 18488.688 | 16116.051 | -12.833% | +0.661% |
| warmShort/ttft_ms | reverse | 711.598 | 637.377 | -10.430% | +0.211% |
| warmShort/decode_ms | reverse | 1327.000 | 1355.148 | +2.121% | -0.317% |
| warmShort/request_ms | reverse | 2005.405 | 1962.242 | -2.152% | -0.056% |
| first8K/ttft_ms | reverse | 33627.998 | 30914.070 | -8.070% | +0.008% |
| first8K/decode_ms | reverse | 4790.700 | 4809.049 | +0.383% | -0.131% |
| first8K/request_ms | reverse | 38377.764 | 35685.132 | -7.016% | -0.009% |
| repeat8K/ttft_ms | reverse | 33768.265 | 30926.299 | -8.416% | +0.047% |
| repeat8K/decode_ms | reverse | 3805.700 | 3817.448 | +0.309% | -0.212% |
| repeat8K/request_ms | reverse | 37535.113 | 34705.456 | -7.539% | +0.018% |

输出与计数全部一致；门结果：`{"quality": true, "drift": true, "no_regression": false}`。不因Host正确性通过而忽略相对L0的回退或漂移。离线复核：`python -X utf8 build-orca-native-host-20261107/contexts-source-r3.py check`。

源码采用门使用同一Direct的独立证明，以及无额外计时L0对照的完整输出、控制漂移和首次TTFT优势；未要求或声称Direct decode已胜过L0。当前源码全局显存保护仍保留；原先5763／5771槽的整模型对照失败不被本次源码检查覆盖。新CTest直接编译实际运行时，检查导入内存所有权、驱动CPU映射的解锁／同步销毁和六处失败清理。

本候选同时改变资源分配方式、缓存策略、对齐和CPU映射方式。即使模型有差异，也不能单独归因为某一位缓存标志或大页。此前U4读取流水线的漂移门失败保持不变，本轮不覆盖它。

结构化证据 (`direct-native-host-a770-windows-2026-11-08.json`, retained locally for privacy) · 源码、差异及原始协议归档 (`direct-native-host-a770-windows-2026-11-08.json.gz`, raw capture retained locally)。归档排除带进程地址的原始私有描述。

同条件复核：解压归档并按path还原文件；保留冻结父构建和其SHA256，使用相同SDK/驱动与资产缓存。运行`python -X utf8 build-orca-native-host-20261107/model.py check`核对完整协议、文件SHA256和资产身份；新采样必须使用新输出目录，保留所有失败和漂移，不能覆盖原样本。
