# Direct：复用计算队列的隔离验证

2026-11-06，Windows 10／Intel Arc A770 16GB／驱动32.0.101.9034／oneAPI2025.3.2。
使用11月6日冻结完整引擎与已校准全局显存记账Runtime；没有重测原生产SYCL或切换部署。
同一EXE以STRATA_ORCA_DG2_SHARE_COMPUTE=0/1切换，默认关闭。计算Stream复用同一native Queue所有者；BCS、内部存储与初始上传队列保持独立。
没有别名OS context／完成fence；图保留其录制Queue及scratch／private所有权。共享计算队列拒绝HostGate等待；外部inline信号须先提交，同列表中较早信号可直接等待。
这是GPU-only计算依赖的隔离候选，不能据此全局替换带Host回调的多队列执行。

81个Host单元重编，70个依赖文件实际包含隔离头，三个库及engine／功能检查／HC检查链接通过；数值pack与原生Runtime对象保持。
开／关各通过128波复制→计算→计算交接、32次销毁录制者后的重放、完整readback／guard与inline依赖检查；开启时验证HostGate及未来生产者拒绝。
两设置各64项HC结果匹配冻结参考；另14条2K→2K→8K→8K→Prose2K→Short→Short完整协议匹配。
带已校准IAT观察器的独立程序再运行四次Short；实际净decode从3计算／1复制context变为1计算／1复制context，计数不是物理引擎数或切换延迟。
观察器日志及锁仅用于计数；后面的性能EXE不含观察器、逐算子标记或新增GPU回读。

固定实际5771专家槽／7922MiB缓存、1536MiB预留、MTP4、step／last min-p0.5、U4/min16/M16、read256、adapt32/decay0.95、BCS1、ISA／线程内存device；cumulative／suffix／cost／prompt-cache0。
顺序预先固定为关→开→关、开→关→开；各新进程2K→2K→8K→8K→Prose2K→Short×15。Short丢弃前10次、保留后5次中位数；外侧控制取几何均值；长请求每位置每臂一次。
净decode排除准备／初始化／prefill；TTFT、READY、整请求与首token后交付速度分别保存。TTFT与引擎decode存在重叠，不能直接相加。

| 场景 | 正序开／关净decode变化 | 反序开／关净decode变化 | 正序TTFT变化 | 反序TTFT变化 |
| --- | ---: | ---: | ---: | ---: |
| Code2K/first | +0.511% | +0.092% | -0.538% | +1.510% |
| Code2K/repeat | +0.580% | +0.018% | -0.480% | +0.090% |
| Long8K/first | -0.242% | +1.181% | -0.239% | +0.475% |
| Long8K/repeat | +0.533% | +1.090% | +0.343% | -0.051% |
| Prose2K | +0.009% | +1.140% | -0.162% | -0.361% |
| Short | -2.658% | +0.105% | +2.848% | -0.999% |

原始门：`{"both_order_positive_short_decode": false, "control_drift": true, "original_short_decode_gain": false, "quality": true, "ttft_request_nonregression": true}`。
原收益门仍为两个顺序Short净decode各至少+1%；用户接受较小正收益，但不改写失败门、删除样本或追加重复取通过。控制漂移与TTFT／整请求非回退门仍为3%，漂移不是置信区间。
本报告不自行切换生产EXE／配置；实际采用决定须结合完整门与HostGate支持范围。组件数量减少本身不能折算成模型吞吐。
首次验证把pack根目录传给只接受单pack的Program，关侧报cannot read DG2 kernel index，尚未进入交接测试；保留原脚本、日志、结果。
首个修正runner又因Python同名validate导入冲突在GPU入口前失败；唯一模块名runner只修正入口路径。关侧GPU检查和64项HC通过后，Python tuple／JSON list直接比较误报失败；规范化后逐项相同。保留原失败并复用关侧原始结果，只执行尚未运行的开侧，没有重复性能采样。
没有新增PPL、跨日稳定性或Linux GPU结论；既有Direct8K间歇与首次大缓存驻留问题不由本轮关闭。

本机归档复核，不重新运行GPU：

```powershell
python -X utf8 build-orca-shared-compute-20261106/publish.py check
```

同条件复测须保留设备／驱动／编译器、冻结EXE／pack／资产／初始profile、完整参数、输入及进程顺序，使用新的输出目录；资产身份沿用明确标注的stat-cached SHA256，不重新哈希89.8GiB权重。
归档保留脚本、变更前后适配头、编译／链接命令、依赖文件、全部输出与失败；二进制／权重仅记录摘要。命令中的无关环境变量仅保留本机原文摘要，公开副本只含复现所需环境，移除清单明确列出。
[JSON](../../../sycl/orca/direct-shared-compute-a770-windows-2026-11-06.json) · 文本归档 (`direct-shared-compute-a770-windows-2026-11-06.json.gz`, retained locally for privacy)
