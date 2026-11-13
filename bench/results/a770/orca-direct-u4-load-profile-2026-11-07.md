# 首次 U4 准备：Host 分配、读取与校验的粗粒度定位

2026-11-07，Windows 10／Arc A770 16GB／驱动32.0.101.9034／oneAPI2025.3.2。
同一冻结HC128／MTP4／min-p0.5／实际5771槽，1536MiB预留；Direct含已采用Q2_0并发及默认短提示热度。
L0→Direct→L0三个新进程，各执行Code2K两次，6条完整输出、草稿、结束及复用协议全部匹配固定参考。
只重编两个prefill Host单元；数值包、运行时和其他Host库固定，未重烘数学内核。
仅在首次Cache构造加入Host steady_clock与两行日志，无新增GPU kernel、读回、事件或等待。

9402个专家，共25,994,649,600字节Host载荷；48个分层Host分配，4个读取线程，44,236,800字节device staging。
正文时段含列表元数据、Host分配、并行文件读取＋复制＋有限正scale校验、device staging分配；外层构造还覆盖Packed成员初始化。
各时段是Host墙钟，不是GPU时间、物理磁盘带宽、传输实测字节数或纯分配器成本。

| 时段（ms） | L0前 | Direct | L0后 | L0算术均值 | Direct−L0 | L0漂移 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| metadata_ms | 8.063 | 10.324 | 7.412 | 7.738 | +2.586 | -8.074% |
| allocation_ms | 2171.139 | 5621.046 | 1999.103 | 2085.121 | +3535.925 | -7.924% |
| read_validate_ms | 8749.335 | 8086.676 | 8565.109 | 8657.222 | -570.546 | -2.106% |
| device_ms | 3.159 | 4.836 | 3.083 | 3.121 | +1.715 | -2.406% |
| total_ms | 10931.696 | 13722.882 | 10574.708 | 10753.202 | +2969.680 | -3.266% |
| constructor_ms | 10939.696 | 13728.950 | 10581.444 | 10760.570 | +2968.380 | -3.275% |
| initializers_ms | 8.000 | 6.068 | 6.736 | 7.368 | -1.300 | -15.800% |

body total_ms等于前四段之和；constructor_ms等于body＋initializers_ms。按日志精度核对闭合关系。
两侧L0取算术均值以保持分段可加；漂移仅描述本轮观察，不是置信区间。
这个诊断不把首次／重复请求之差全部归给构造，也不解释构造之外的GEMM、cache或完成路径。
没有提前移动准备时间、默认策略变更或新的模型吞吐声明；原READY／TTFT／decode／整请求值仅保留在原始数据。
不重复GPU样本取通过，不关闭历史8K间歇故障，不声称PPL或Linux GPU已验证。

本机复核（不运行GPU）：

```powershell
python -X utf8 build-orca-u4-load-profile-20261107/experiment.py selfcheck
python -X utf8 build-orca-u4-load-profile-20261107/experiment.py check
```

同条件复现使用新输出目录、相同冻结源／库／数学包／资产及初始profile、参数和进程／请求顺序。
资产沿用SHA256＋文件身份／大小／时间戳缓存；所有编译、GPU、重型哈希及归档持共享锁。
归档含源码差异、命令、日志及协议，不含权重、EXE、OBJ、LIB或ZEBIN。

[无探针完整上下文对照](orca-direct-heat-contexts-2026-11-07.md)
[JSON](../../../sycl/orca/direct-u4-load-profile-a770-windows-2026-11-07.json) · 精确证据 (`direct-u4-load-profile-a770-windows-2026-11-07.json.gz`, raw capture retained locally)
