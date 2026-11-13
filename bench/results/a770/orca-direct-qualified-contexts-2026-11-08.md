# A770：当前合格 Direct／L0 运行时对照

2026-11-08，Windows10，Arc A770 16GB，驱动32.0.101.9034，oneAPI2025.3.2。

补齐此前版本边界：Direct直接链接通过19项检查的纯原生运行时CMake对象，源码与准备时已采用版本逐字节一致；包含原生Host分配／页面导入和显式安全trim API，但没有调用trim、自动重录、倒序录图或测试内存审计包装。L0控制为已通过六类查询故障和八请求功能检查的全局驻留统计修正版。

继续冻结HC128／MTP4／min-p0.5／reserve1536MiB、数学包、其他Host对象、profile和资产身份缓存；两端参数完全一致，环境仅差四个既有DG2设置。不是当前spec6／0.6生产版本，也不是整个最新工作树的性能。部署EXE、默认参数、推荐L0及Linux GPU待测状态保持。

预先固定原始84请求：L0／Direct／L0与Direct／L0／Direct；每进程依次2K×2、Short×10、8K×2，Short取后5次中位数。每个进程要求实际5771槽／7922MiB缓存及完整token／8字段一致、退出0；所有指标控制漂移≤3%，无回退门为所有TTFT／decode／整请求耗时增加≤1%。任何容量／质量／驱动故障停止，不重试、不降低reserve、不覆盖此前失败。两个顺序按固定计划执行，速度差本身不触发追加采样。

复用合格对象，不重复编译或运行已成功的19项检查。新增DLL检查放在每个引擎全部计时请求之后；Direct要求未加载SYCL／UR／Level Zero／IGC／GMM，L0要求实际加载Level Zero。没有增加逐kernel、逐token或U4诊断计时；准备保留在TTFT／整请求，排除在decode_ms之外。

原生对象SHA256：`f78ecceb3920c07aba62185221daa63a9cba37e3cc3f20c5c629c19752222213`。L0 EXE SHA256：`1e729da29b42687fbc30ca968c91ca01cd628668c3d7d7e49302b7df92b15759`。

完整请求0条／输出0个，独立原始协议及计算复核通过。

原条件处停止：`AssertionError(('link', 1104))`。没有一份新的完整合格Direct／L0性能结果，不能用部分请求更新吞吐标题或旧表。已完成及失败记录全部保留；没有放宽5771槽要求或追加救援重复。

原始INFO：


[此前冻结运行时对照](orca-direct-native-host-2026-11-08.md) · [原生运行时资格](orca-direct-thread-trim-2026-11-08.md) · [L0统计资格](orca-direct-memory-budget-2026-11-08.md)

[JSON](../../../sycl/orca/direct-qualified-contexts-a770-windows-2026-11-08.json) · 精确源码、计划、构建命令与原始协议 (`direct-qualified-contexts-a770-windows-2026-11-08.json.gz`, raw capture retained locally)

离线复核：`python -X utf8 build-orca-qualified-contexts-20261108/publish.py check`。新采样用新目录，沿用固定配置和资产身份缓存，不覆盖本次数据。
