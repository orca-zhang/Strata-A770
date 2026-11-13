# A770：安全回收旧线程池

2026-11-08，Windows 10／Arc A770 16GB／驱动32.0.101.9034／oneAPI2025.3.2。

已采用通过检查的显式 `Runtime::trim_thread_memory()` 及单项检查源码，仅允许所有队列空闲、运行时健康、活动录制为空且打开时调用。逐项保留任意队列当前绑定，以及活动／保留列表引用的线程池和描述面；其余旧池通过已有平台释放路径回收。保留已退役池的索引空位，保护区检查跳过空位。回收失败使运行时停止后续操作；没有修改GPU线程槽数。重录及模型自动回收仍只用于隔离验证，未改默认。

隔离GPU生命周期检查覆盖Host／本地GPU两种存放方式，分别准确回收14.0625／10.3125MiB旧scratch／private池及描述面。旧图保留时不释放；另一队列和当前绑定受保护；非空／关闭／未完成状态拒绝；重复调用幂等。共24次重放和重新录制的精确输出、完整输入／输出及池保护区通过，原19项CTest全部通过。Linux编译、CPU编码检查与依赖检查通过；没有运行Linux GPU。

首轮检查在核对释放字节数时失败：测试预期没有计入Windows原生大Host分配的2MiB对齐，失败及原源码保留。r2只改为通过公开分配接口读取实际保留容量，回收实现逐字节不变；再核对准确释放字节差，并非放宽或删除断言。另一次执行前的辅助模块名称冲突也保留记录，修正为独立模块名导入，没有执行GPU。

整模型首次链接因工作目录错误而找不到相对路径下的引擎静态库，尚未发送GEN；该失败也保留。新阶段修正链接工作目录，复用同一份已编译运行时对象，未重复编译或修改数学、参数及检查条件。

整模型沿用冻结HC128／MTP4／min-p0.5／reserve1536及相同数值内核，复用此前通过的重录Verifier对象，只在隔离版启动显存查询时调用回收。八条2K／8K／Short／Prose2K完整输出及8项协议字段匹配，退出0；没有加载SYCL／UR／Level Zero／IGC／GMM DLL。

| 初始化内部存储 | 仅重录观察 | 重录并回收 |
| --- | ---: | ---: |
| scratch／private线程池 | 344MiB | 270.625MiB |
| scratch描述面 | 0.25MiB | 0.125MiB |
| 回收合计 | 0 | **73.5MiB** |

其中线程池回收73.375MiB，描述面0.125MiB。旧池5的49.125MiB仍有录图引用，因此继续保留；当前绑定的221.5MiB也保持。参数镜像仍为重录后的4MiB。分区守恒与释放字节差核对通过。

本次原始INFO：`INFO context=8192 kv=fp16 kv_resident=0 expert_slots=5707 expert_cache_mib=7835 expert_slots_primary=5707 expert_cache_primary_mib=7835 spec=4 mtp_max=0 lookup=0 vram_free_mib=1127 cvec=0 arena_mib=33812 pool_workers=21 pcie_frac=0.16 spec_min_p=0.50 conversation_cache_mib=0 conversation_cache_slots=4 conversation_cache_min_free_mib=2560 tail_role_token=-1 vram_elastic=0 engine=0.1.39-orca`。实际5707槽仅作功能／容量验证，不通过或放宽原5771槽性能条件。没有新tok/s、TTFT或PPL结论，也不能据此解释Short decode差距。此前只重录没有建立稳定性能收益，重录／模型自动回收未默认开启，部署EXE与参数保留；L0仍为推荐后端，Direct仍为WIP。

[此前未释放的引用观察](orca-direct-pool-recapture-2026-11-08.md)

[JSON](../../../sycl/orca/direct-thread-trim-a770-windows-2026-11-08.json) · 精确源码、测试及原始协议归档 (`direct-thread-trim-a770-windows-2026-11-08.json.gz`, raw capture retained locally)

复核：`python -X utf8 build-orca-thread-trim-20261108/publish.py check`。单项GPU检查：`orca-l0-dg2-check --strata-thread-trim <native-images> <memory.zebin>`；使用隔离结果中的原编译命令和输入摘要。
