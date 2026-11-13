# Direct：复用标准库执行器运行适应任务

2026-11-08，Windows10，Arc A77016GB，驱动32.0.101.9034，oneAPI2025.3.2。

上一轮Host阶段追踪将3.460ms空队列／DMA时间定位在路由计数、接受判断和适应线程准备的合并区间。该区间不能直接等同于线程创建成本；本轮隔离更换每窗口的适应任务启动方式。只重编generate.cpp一个Host单元，将std::thread改成std::async(std::launch::async, adapt)，两处原join改为future.get并保留bool结果检查。没有自建线程池或新增依赖。

安装的MSVC标准库实测：256个新建thread任务的TLS计数最大1，256个async任务的最大计数256，证明此次线程复用；false结果和异常传播检查通过。这不保证Linux实现复用，也不是模型提速测量。

两臂复用相同的原生运行时、其余88个Host对象、数学ZEBIN与参数／环境／lib_dirs。原Owner／wait诊断代码两侧都保留但不开ETW；没有阶段标记、GPU时间戳、逐算子计时或额外回读。不是纯生产无诊断二进制的发布成绩。实际5243槽／7201MiB、cache预算5000、reserve1536MiB、HC128、MTP4、min-p0.5、短提示热度关闭、adapt-every1／swaps32／decay0.95。不能替代5771槽锚定、当前spec6生产成绩或L0对照。

预先固定thread→future→thread，通过才执行future→thread→future。各新进程Code2K×2→Short×15，Short丢弃前10次、取后5次中位数，外侧控制取几何均值；原decode_ms排除准备／初始化／prefill，TTFT和整请求单独检查。每顺序Short吞吐至少+0.2%，三项控制漂移绝对值≤3%，TTFT／整请求耗时回退≤3%；不通过则停止，不调门槛、不追加取好样本。两次Code2K仍完整保留，只作历史／协议检查，不发布其孤立速度收益。

完成102请求／7836输出，全部八个协议字段匹配、引擎退出0；配置容量、资产身份和冻结源码／头文件／对象／EXE前后核对通过。

| 顺序 | Short decode thread ms | future ms | 吞吐变化 | decode控制漂移 | TTFT耗时变化 | 整请求耗时变化 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 0 | 1409.3976 | 1404.1000 | +0.377% | -0.368% | +0.207% | -0.181% |
| 1 | 1408.6000 | 1424.2231 | -1.097% | +2.100% | +1.159% | +1.101% |

完整门：`[{"quality": true, "control_drift": true, "short_gain": true, "ttft_request": true}, {"quality": true, "control_drift": true, "short_gain": false, "ttft_request": true}]`。

双向Short门未通过，不采用；若正序未通过，按预案不执行反序。

线程复用可能改变TLS／亲和性及缓存状态，不能将净差值全部当作线程创建成本，也不能据此认定原生已超过L0。Orca L0仍推荐，部署EXE、生产源码、默认策略与Linux待测状态保持；全部窗口和原始协议均归档。

[阶段归属证据](orca-direct-decode-stages-2026-11-08.md)

[JSON](../../../sycl/orca/direct-adapt-future-a770-windows-2026-11-08.json) · 完整证据 (`direct-adapt-future-a770-windows-2026-11-08.json.gz`, raw capture retained locally)

离线复核：`python -X utf8 build-orca-adapt-future-20261108/publish.py check`；判定负控：`python -X utf8 build-orca-adapt-future-20261108/check.py`。新采样使用新目录与原资产身份缓存，不重哈希89.8GiB模型。
