# Orca Direct：实际大投影命令与 L1 缓存隔离

2026-11-04，Windows 10 22H2，Intel Arc A770 16 GB，驱动 `32.0.101.9034`，oneAPI 2025.3。
沿用[大矩阵参数位置试验](orca-direct-projection-parameters-2026-11-04.md)的同一通用 FP16 M4 内核、冻结 Host 对象和数学 pack。
本轮先抓取实际命令，再只切换观察到的 L1 策略差异。没有新整模型性能、PPL 或 Linux GPU 结果。

## 实际命令

进程内部的 KMT 观察器从初始化开始，记录 CPU 可访问的 GPU 命令映射；有边界、深度和未知命令检查。
两后端各执行 28 次 M4 投影，按精确 group/local/SIMD 几何从其它初始化／复制 kernel 中识别；所有 56 个投影都检查到了。
两进程的 24 项完整输出、源数据和保护区检查通过，命令抓取失败数均为 0。
观察器会扰动执行，**这部分日志中的耗时不作为性能数据**。

| 已捕获量 | Direct | L0 |
| --- | ---: | ---: |
| KMT submissions（含初始化／复制） | 47 | 98 |
| 原始命令文件／字节 | 47／23,616 | 111／17,252 |
| 所有 compute walkers（含初始化／复制） | 53 | 78 |
| 识别出的大投影 walkers | 28 | 28 |
| SBA L1 策略 | WBP=0，94 条 | WB=2，1 条 |
| 投影 preferred SLM | MAX=0，28 条 | 16 KiB=9，28 条 |

投影的底层 group 为 `(1,96,512)`／`(1,96,2000)`，local 为 `(64,1,1)`，SIMD8、每组 8 个硬件线程。
执行掩码、64 字节 indirect payload、硬件 local-ID、walk/dispatch order、L3 prefetch 字段、SLM 实际大小、barrier、denorm 和 thread-group dispatch 字段一致。
在本轮逐投影解码的字段中，只有 preferred SLM 不同；它的单独试验见[SLM 反向复测](orca-direct-projection-slm-2026-11-04.md)，未消除差距。
全局 CFE 的最大线程数为 4,096，overdispatch 等已解码设置相同；计数不同的状态分布包含内存 kernel，不能视为投影独占资源测量。
另一次离线解码复用完全相同的原始命令，扩展 pipeline-select 和 LRI 字段；所有原字段和计数重新匹配，没有额外运行 GPU。
两边均选 GPGPU pipeline，mask 为 `0x83`，寄存器 `0x2580` 均写入 `0x60002`；systolic 模式的已观察取值集合也相同，切换次数包含内存 kernel。
L0 全程只发出一条 SBA，Direct 在各 list 重复编程；共同 MOCS=6，但 L1 策略、dynamic-state MOCS、bindless surface size 仍有差异。
本轮只隔离 L1，不能仅凭命令数量判断带宽、锁开销、物理 PCIe 传输量或剩余瓶颈。

首次只抓有限调用的尝试混入首次初始化 kernel，末尾“恰好四个 walker”断言失败；失败证据保留，后续全程抓取独立完成并分类。

## 相同内核的 L1 单变量对照

候选只改运行时内部 `encoding.hpp` 的 SBA L1 字段，`ORCA_DG2_L1_WB=1` 选 WB，0／未设置保留 WBP。
开关按进程缓存，不在每次派发重新读取环境变量；其它载荷、数学、同步、MOCS 和 SLM 策略不变。
测试范围是全局 SBA 策略，包括准备阶段的内存 kernel；不能称为仅投影 walker 的策略改动。
14 项 CTest 通过；两种设置均通过 CPU 编码检查，非法开关按预期拒绝。
第一版因枚举名错误编译停止，第二版被原有固定 WBP 的 CPU 断言停止；失败目录均保留。
第三版保留默认值检查，并按显式开关验证两种实际策略，复用原 probe 对象和数学库重新链接运行时。

固定 M=2,047／7,999、N=6,144、K=2,560、stride=2,568，交替输出指针；完整精确参考和全部源／边界检查保持不变。
分配、上传、NaN 毒化与预热均在计时外；干净进程不链接观察器、不写 GPU timestamp。
先 **L0→关→开→关→L0**，再 **L0→开→关→开→L0**，共 180 个 launch+wait 墙钟样本、120 项数据检查通过。
下表先取每形状每进程 9 个采样的中位数，再对同类两侧控制取几何均值；单位 ms，负的变化代表更短。

| 轮次／M | L0 两侧几何均值 | Direct关 | Direct开 | 开／关耗时变化 | Direct两侧模式／漂移 | 开／L0耗时变化 | L0漂移 |
| --- | ---: | ---: | ---: | ---: | --- | ---: | ---: |
| r1／2047 | 59.8371 | 63.4549 | 65.0093 | +2.450% | off／+0.861% | +8.644% | -0.246% |
| r1／7999 | 231.5437 | 248.3698 | 249.2355 | +0.349% | off／+0.511% | +7.641% | -0.111% |
| r2／2047 | 59.8070 | 63.2283 | 64.0453 | +1.292% | on／+2.640% | +7.087% | -0.007% |
| r2／7999 | 231.5765 | 248.2238 | 249.8238 | +0.645% | on／+1.559% | +7.880% | +0.021% |

WB 在两轮中均未缩短这两个形状的耗时，因此本轮不采用它。反向 on 控制漂移为 +2.640%／+1.559%，小幅 8K 变化不能据此判定稳定退化。
这些结果不支持“只需匹配 L1 字段就能消除大投影差距”，但没有排除其它缓存、内存分配、context 或派发因素。
这两个合成形状不覆盖全部实际投影，launch+wait 也不是独占 kernel 时间；两轮同日括号对照不能证明跨日稳定性。
本轮没有整模型吞吐／TTFT／PPL 结论，未部署候选或更改默认运行时策略；Orca L0 仍是推荐路径。
Linux GPU 待验证，既有 Direct 8K 间歇异常未关闭。独立实验百分比不相加。

## 复现和证据

```powershell
python -X utf8 build-orca-slm-async-20261104/projection_packet_full.py r1 --verify
python -X utf8 build-orca-slm-async-20261104/projection_state_fields.py r1 --verify
python -X utf8 build-orca-slm-async-20261104/projection_cache_check.py build --verify
python -X utf8 build-orca-slm-async-20261104/projection_cache_check.py run r1 --verify
python -X utf8 build-orca-slm-async-20261104/projection_cache_check.py run r2 --verify
python -X utf8 build-orca-slm-async-20261104/publish_projection_packets_cache.py --check
```

新采样使用不存在的编号，例如 `run r3`／`run r4 --opposed`，不能覆盖成功或失败目录。
保持相同硬件、驱动、冻结 Host／运行时／数学 pack、环境与空闲主机；构建、运行、哈希和归档均持有共享 GPU 锁，性能进程间不编译或查询 GPU。
归档包含原始命令字节、观察器和解码器、补丁、自检、构建／链接命令、日志与所有采样；可执行文件和数学二进制以 SHA-256 标识。

精确 JSON (`direct-projection-packets-cache-a770-windows-2026-11-04.json`, raw capture retained locally) ·
原始证据 (`direct-projection-packets-cache-a770-windows-2026-11-04.json.gz`, raw capture retained locally)
