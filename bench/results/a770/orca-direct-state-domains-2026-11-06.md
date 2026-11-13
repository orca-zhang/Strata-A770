# 当前 Short：按真实重放加权的 Direct 状态命令普查

2026-11-06，Windows10／Intel Arc A77016GB／驱动32.0.101.9034／oneAPI2025.3.2。
结果排除了本次候选假设：这两次 Short 请求的列表内部没有 systolic 切换，也没有基址不变却重复编程 SBA 的转换。
没有据此修改运行库默认；本轮不是性能试验，也没有新的 L0 或原生产 SYCL 吞吐样本。

沿用完整源码 Direct 引擎的核心、Host、数学包及已验证全局显存记账，只重编独立诊断 Runtime。
每次追加保留前后十项状态及真实编码 DWORD；列表关闭校验长度，每次成功 D3DKMTSubmitCommand 后记录列表身份与队列序号。
离线按实际提交次数加权，包含已录图的每次重放；不把初始化追加次数当成每 token 的派发量。
统计范围按 READY 后 GEN 前后日志字节边界选取，覆盖完整请求，包括准备、prefill、decode、commit／MTP及相应内部传输。
它不是净 decode 区间或每 token 计数；内核追加块之外的显式屏障、列表尾和 BCS 包未纳入逐 kernel 包计数。

固定实际5771槽／7922MiB缓存、reserve1536MiB、MTP4、step／last min-p0.5、cumulative／suffix／cost-window／prompt-cache0、U4/min16/M16、BCS1。
同一新进程连续两次 Short，完整 tokens、草稿采纳／提供、结束与复用状态均匹配冻结参考；没有预先插入长请求。
诊断写日志会改变 CPU 调度及 GPU 提交间隔；原始耗时仅保留，不用来计算或宣称吞吐优势。

| 请求 | 成功提交／其中BCS | 实际walker | 列表首次状态 | 仅GRF改变 | 仅参数基址改变 | 仅scratch基址改变 | SBA包 | 不变SBA重复转换 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 0 | 212／28 | 67271 | 184 | 3934 | 50 | 28 | 524 | 0 |
| 1 | 211／29 | 67271 | 182 | 3934 | 50 | 28 | 520 | 0 |

逐项校验七类命令的头／长度、每次追加恰有一个walker、列表身份／关闭／队列顺序、前后状态与所有SBA副本的原始DWORD。
四项损坏负控拒绝部分DWORD、未知opcode、截断命令和外来列表；原生查询／模型配置和数学包未改变。
首次发布器未处理JSON把整数对象键转成字符串，在归档写入前被比较断言拒绝；保留原脚本，仅离线规范化掩码键，不改采集结果或重跑GPU。
SBA对应字段是ISA基址／长度、参数基址、MOCS和scratch描述符基址。已有仅GRF转换不重写SBA；本轮其余非首次SBA均对应实际参数或scratch基址变化。
systolic状态结论限于列表内部；独立列表首次仍完整编程，不能假设前一列表或被调度上下文留下了可复用状态。
因此不继续构建“systolic改变时跳过不变SBA”的候选：它在这两次真实请求中没有可作用的转换。
只GRF时进一步删去LRI／PIPELINE_SELECT／CFE已在此前试验中未建立模型收益，本轮不原样重跑以取通过。

当前无探针的完整源码对照仍显示Short Direct较L0低约2.5%；重复8K差距没有一致缩小。
本次只排除上述状态冗余假设，不排除真实基址转换成本、IDD／scratch／内存策略、GPU执行间隙或其它阶段差异。
没有新增长上下文、PPL或Linux GPU结论；原首次驻留及8K间歇故障仍开放，生产EXE／配置未替换。

源码、编译／链接命令、每次追加的精确DWORD、完整原始协议／日志、参考与配置摘要归档；不分发模型、EXE、obj或zebin。
编译、GPU、重型哈希和归档持共享锁；资产使用已建立的stat-cached SHA256身份核验。
同条件复现需保留硬件／驱动／编译器、冻结Runtime与完整数学／Host依赖、初始profile、参数、输入与请求顺序，并使用新输出目录。

本机复核，无新增GPU：

```powershell
python -X utf8 build-orca-state-domains-20261106/publish.py check
```

[此前仅GRF精简](orca-direct-current-grf-u4-2026-11-06.md) · [完整源码及长上下文对照](orca-direct-full-current-2026-11-06.md)
[JSON](../../../sycl/orca/direct-state-domains-a770-windows-2026-11-06.json) · 精确原始证据 (`direct-state-domains-a770-windows-2026-11-06.json.gz`, raw capture retained locally)
