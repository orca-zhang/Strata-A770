# Orca Direct：大矩阵投影的 preferred SLM 隔离

2026-11-04，Windows 10 22H2，Intel Arc A770 16 GB，驱动 `32.0.101.9034`，oneAPI 2025.3。
延续[相同内核的大矩阵定位](orca-direct-projection-parameters-2026-11-04.md)，本轮只改变 preferred SLM；没有整模型测试或默认策略接入。

候选复用此前经过校验的占用率分桶规则：每组实际 SLM 向上取整，乘以 `ceil(128 / 每组硬件线程数)`，上限 128 KiB。
128 是先前与 L0 命令匹配的校准参数，不是本轮查询或独立测得的硬件拓扑。
本次只对已校准 C0、名字含 `native_xmx_launch_m4`、至少 4,096 个 workgroup 的派发启用；参数位置、GRF、ISA、payload、依赖、拷贝和所有权不变。
两个被测形状每组 8 个硬件线程、实际动态 SLM 1,024 字节，因而 preferred SLM 从 MAX 改为 16 KiB。
preferred 字段不等于实际 SLM 使用量，也不代表已经测出占用率。

14 项 CTest 通过；新增 14 项命令逐位检查覆盖零／非零 SLM、取整边界和连续派发，归一化 preferred 字段后所有命令字节相同。
构建重试的两个失败目录保留：r1 在源码定位断言停止，r2 编译成功但自检命令缺少 `--self-check` 参数而停止；两次均未进入 GPU 测试。
修正后的构建使用新目录 r3，复用原有 Host probe 对象与数学库，只重新链接候选运行时。

## 单算子对照

固定 M=2,047／7,999，N=6,144，K=2,560，输入 stride=2,568；相同 FP16 输入／权重与 FP32 输出，交替输出指针。
分配、上传、NaN 毒化检查、CPU 精确二进制分数参考和预热在计时外。
先按 **L0→Direct关→Direct开→Direct关→L0**，再以 **L0→Direct开→Direct关→Direct开→L0** 反向重复；每形状每进程 9 个采样，共 180 个干净墙钟样本。
计时范围为派发加等待，不含 GPU timestamp 或 trace 日志。
每轮另有两个独立 trace 进程确认开／关各 28 次配置实际执行；合计 112 条 trace、168 项完整输出、源数据和保护区检查全部通过。
首轮末尾的校验器误把底层 XYZ 当成 SYCL 维度，断言停止；失败结果保留，独立校验器重新核验全部原始样本和 trace 后才引用数据。
实际底层 group 为 `(1,96,512)`／`(1,96,2000)`、local 为 `(64,1,1)`，与 SYCL 维度顺序相反；两个形状的 workgroup 总数和工作量不变。

以下单位为 ms；先取进程内中位数，再对两侧控制取几何均值。负的耗时变化代表更短。

| 轮次／M | L0 首／末 | Direct关 | Direct开 | 开／关耗时变化 | Direct两侧模式／漂移 | 开／L0耗时变化 | L0漂移 |
| --- | ---: | ---: | ---: | ---: | --- | ---: | ---: |
| forward／2047 | 59.7456／59.7828 | 64.1718 | 62.9096 | -1.967% | off／-3.489% | +5.263% | +0.062% |
| forward／7999 | 231.5266／231.4702 | 248.5565 | 243.5352 | -2.020% | off／-0.600% | +5.200% | -0.024% |
| opposed／2047 | 59.8481／59.8349 | 62.9338 | 62.9605 | +0.042% | on／+0.453% | +5.212% | -0.022% |
| opposed／7999 | 231.6319／231.6851 | 246.9879 | 244.6570 | -0.944% | on／+0.275% | +5.611% | +0.023% |

这是代表性合成投影形状，未覆盖全部实际模型投影，也没有测量独占 kernel 时间、SLM／缓存命中率或物理引擎利用率。
两轮反向进程括号对照仍不足以证明跨日稳定性；不能由此推导整模型 tok/s、TTFT 或 PPL，也不能将其与其它实验的百分比相加。
候选保持隔离且默认关闭，Orca L0 仍是推荐后端；没有部署 EXE、生产配置或仓库运行时策略变化。
Linux GPU 未测试，已有 Direct 8K 间歇性异常仍未关闭。

## 复现

```powershell
python -X utf8 build-orca-slm-async-20261104/projection_slm_check.py build --verify
python -X utf8 build-orca-slm-async-20261104/projection_slm_repeat.py r1 --verify
python -X utf8 build-orca-slm-async-20261104/publish_projection_slm.py --check
```

新采样使用 `projection_slm_repeat.py r2` 等不存在的目录；保留相同硬件、驱动、冻结源／Host 对象／数学 pack 和空闲桌面条件。
编译、运行、哈希和归档均持有共享 GPU 锁，性能进程之间不插入编译或 GPU 查询。
原始证据保存补丁、校验源码、命令、样本和日志，二进制以 SHA-256 标识。

精确 JSON (`direct-projection-slm-a770-windows-2026-11-04.json`, raw capture retained locally) ·
原始证据 (`direct-projection-slm-a770-windows-2026-11-04.json.gz`, raw capture retained locally)
