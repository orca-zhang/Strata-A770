# 融合HC Q8：getter与128 GRF组合的编译／完整输出检查

2026-11-06，Windows10，Arc A77016GB，驱动32.0.101.9034，oneAPI2025.3.2。
这是新的getter128组合；此前getter256的组件及完整模型漂移失败均保留，不由本轮覆盖。
数学来源是已独立编译并逐入口核对的Q8-only六处行选择改写；普通HC分支及另外24个原生映像保持。
每次行选择返回含指针的行描述符，通过常量分支避免数组动态索引；八个token槽及数学归约顺序保持。
本轮复用这份精确SPIR-V，分别烘焙256／128 GRF；256完整ZEBIN字节等于已测getter256映像。
组合包仅变化融合image_3及两个跟踪清单；未替换生产源码、默认包或部署程序。

| 映像 | SIMD | GRF | 声明private／硬件线程（字节） | 声明spill（字节） | ELF代码段（字节） |
| --- | ---: | ---: | ---: | ---: | ---: |
| generic128 | 16 | 128 | 9728 | 0 | 27392 |
| getter128 | 16 | 128 | 0 | 0 | 27456 |

private是编译器声明，不能直接换算总显存或实际内存流量；代码大小也不等于指令数。
getter128／256的payload与per-thread-payload完全一致；相对generic的显式Host参数保持，
编译器删除的隐式private-base字段沿用既有独立ABI审计，不声称全部payload逐字节不变。

冻结的两份生产入口HC程序分别运行generic128与getter128：共256项完整输出、CPU参考、源与边界检查，
覆盖T1..8、改变输入、eager／replay、普通／融合Q8路径；四组合的64份完整输出哈希逐项相同。
本组覆盖的生产入口为images2／3；不将旧的独立image9 Host检查冒充本组合GPU检查。
这些是算子检查，不是完整模型性能、PPL、Linux GPU或8K稳定性验收；历史Direct8K故障仍开放。
声明private降至0不能单独证明更快；下一步需以同一EXE、同样128 GRF、固定参数和历史做模型对照。

编译、GPU、重型哈希及归档持共享锁；归档含脚本／SPIR-V来源摘要、资源与ABI文本、命令及全部检查输出，
不分发权重或二进制。复核已有记录，不运行GPU：

```powershell
python -X utf8 build-orca-hc-route-split-20261106/publish_getter128.py --check
```

[HC1与路由定位](orca-direct-hc-route-split-2026-11-06.md) · [Direct128默认采用](orca-hc-sg16-grf-adoption-2026-11-06.md)

JSON (`hc-q8-getter128-a770-windows-2026-11-06.json`, raw capture retained locally) · 精确文本归档 (`hc-q8-getter128-a770-windows-2026-11-06.json.gz`, raw capture retained locally)
