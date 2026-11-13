# Orca Direct：大投影的分配描述符与 2 MiB 地址对齐

2026-11-04，Windows 10 22H2，Intel Arc A770 16 GB，驱动 `32.0.101.9034`，oneAPI 2025.3。
继[命令与 L1 隔离](orca-direct-projection-packets-cache-2026-11-04.md)之后，检查此前在同卡驱动中校准的
device allocation 描述符与 GPU 虚拟地址对齐策略。保持相同通用 FP16 M4 内核、Host 对象、数学 pack、
同步与参数位置；仅改动两个 WDDM runtime 源文件并重建运行时。没有部署或采用这项候选。

## 策略和正确性

同一份冻结源码分别用 `ORCA_DG2_CALIBRATED_ALIGN_2M` 关闭／开启构建。
开启后，2 MiB 整倍数的本地显存物理分片使用先前校准的描述符，较大缓冲预留足够 GPU VA 并对齐到 2 MiB；
非整倍数尾部单独保留原大小。每片仍不超过 1 GiB，LocalOnly 和驻留要求不变，物理分配总字节和 runtime 记账不增加。
描述符和 VA 布局同时改变，因此此试验不能区分它们的独立贡献，也没有测量 GPU 页表实际页大小。
描述符来源见[此前分配校准](../../../orca-l0/demo/dg2-allocation-alignment-windows-2026-11-02.json)，
该旧试验不是本轮大投影性能结果。

两个构建共通过 **28 项 CTest**。独立分配检查覆盖从 64 KiB 到 5 GiB 加 128 KiB 的十种大小、
全部 1 GiB 分界与尾部；两种模式共通过 **80 次改变输入的重放**，核对完整复制区域、源／边界、
分配类型、物理字节记账与释放。开启时所有符合条件的 GPU VA 都通过实际低 21 位为零的检查。
这些检查验证内存与所有权，不代表整模型或 Linux GPU 已验证。

首个构建 r1 的脚本覆盖了 CMake 默认 C++ flags，意外去掉 `/EHsc`，因此在编译时停止，没有执行 GPU 检查或采样。
r2 保留冻结基底的 `/DWIN32 /D_WINDOWS /EHsc`，仅追加策略宏；失败源码、命令和日志均保留，没有算作通过。

## 正向和反向对照

固定 M=2,047／7,999，N=6,144，K=2,560，输入 stride=2,568，交替输出地址。
每个形状丢弃准备和预热后保留九次 launch+wait 墙钟样本；输入生成、分配、上传、NaN 填毒、输出检查均在计时外。
不链接命令观察器、不写 GPU 时间戳。依次执行 **L0→关→开→关→L0** 和 **L0→开→关→开→L0**，
共 **180 个样本、120 项完整输出／源／边界检查**。所有比较均使用本轮实际运行的独立进程。

先取每个形状每进程的中位数，再对同类两侧控制取几何均值；单位 ms。负的耗时变化代表更短。
控制漂移是观察值，不是置信区间。两种形状是代表性合成投影，没有覆盖全部模型投影。

| 轮次／M | L0 两侧均值 | Direct关 | Direct开 | 开／关耗时变化 | 中间两侧控制／漂移 | 开／L0耗时变化 | L0漂移 |
| --- | ---: | ---: | ---: | ---: | --- | ---: | ---: |
| r1／2047 | 59.7993 | 62.0431 | 62.3356 | +0.472% | off／-0.210% | +4.241% | -0.126% |
| r1／7999 | 231.7170 | 243.6373 | 243.6800 | +0.018% | off／-0.135% | +5.163% | +0.089% |
| r2／2047 | 59.8796 | 62.3122 | 62.0706 | -0.388% | on／-0.318% | +3.659% | -0.045% |
| r2／7999 | 231.7678 | 243.1941 | 244.0724 | +0.361% | on／-0.100% | +5.309% | -0.023% |

这是分配／地址布局的单组策略筛查，没有新整模型吞吐、TTFT、PPL 或 Linux GPU 结果。
同一候选的正反顺序也不能证明跨日重复性；结果不与独立优化的百分比相加。
生产配置、EXE 与默认分配策略未更改，Orca L0 仍为推荐路径；既有 Direct 8K 间歇异常尚未关闭。

## Linux 检查入口

`tools/check_orca_native_linux.py` 补上共享 BCS 的 `orca-l0-blitter-check`，现在显式执行四项 CPU 检查，
并记录检查脚本自身的源码哈希。在 WSL 中从当前仓库源码重新构建，四项检查和四个可执行文件的 `ldd`
依赖审计全部通过；共享 BCS 覆盖 2,268 项输出／源／边界检查。没有执行 GPU，也没有构建完整 Strata 前端。
这是当前 Linux/i915 运行时的编译与 CPU 检查，**不是上述 Windows 2 MiB 候选的 Linux 验证**。
稍后在 Linux 可运行 `python3 tools/check_orca_native_linux.py --out /tmp/orca-native-cpu-new`，输出目录需不存在。

## 复查

```powershell
python -X utf8 build-orca-slm-async-20261104/projection_alignment_check.py build --verify
python -X utf8 build-orca-slm-async-20261104/projection_alignment_check.py run r1 --verify
python -X utf8 build-orca-slm-async-20261104/projection_alignment_check.py run r2 --verify
python -X utf8 build-orca-slm-async-20261104/publish_projection_alignment.py --check
```

新采样使用不存在的编号，如 `run r3`、`run r4 --opposed`；既有成功与失败目录均不覆盖。
保持相同硬件、驱动、冻结二进制／Host 对象／数学 pack 和环境，主机空闲。
测试、编译、哈希与归档持有共享 GPU 锁；GPU 测试期间不并发构建或查询遥测。
补丁已在等待锁时预备；实际 GPU 输入生成、分配与预热仍由冻结 probe 执行，且不计入执行时间。
归档含补丁及基底、生成 probe、构建／链接命令、原始日志、逐样本数据和二进制 SHA-256；不分发模型和 EXE。

精确 JSON (`direct-projection-alignment-a770-windows-2026-11-04.json`, raw capture retained locally) ·
原始证据 (`direct-projection-alignment-a770-windows-2026-11-04.json.gz`, raw capture retained locally)
