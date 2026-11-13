# Orca Direct：inline Point 成组分配

2026-11-04，Windows 10 22H2，Intel Arc A770 16 GB，驱动 `32.0.101.9034`，oneAPI 2025.3。

这是从[已接入源码](orca-source-adoption-2026-11-04.md)派生的隔离候选。
原适配器每次提交都为每个 inline 事件执行一次 `make_shared<Point>`。
候选将最多 16 个 `optional<Point>` 放进一个标准库 vector，以别名 shared_ptr 保留各自的对象地址与生命周期。
200 个 inline 事件对应 13 组，尾组只分配所需元素；不使用全局对象池，不增加依赖。
普通队列尾 Point 保持原来的独立分配。

每个新提交仍创建全新的对象，绝不重置旧 Point 的完成状态或序号。
保留一个 Point 最多会连带保留同组另外 15 个小对象；弱引用也可能因同组强引用而继续有效，
但不能指向新 generation。这里没有等任务凑批，也不改变 GPU 的依赖、提交顺序或数学内核。
旧事件完成查询、新 tag 的 store/clflush/mfence、默认事件等待均保持原逻辑；
本候选没有叠加前轮完成证明复用或主动轮询。

## 正确性与构建

- 关/开两种策略共通过 12,800 个未预先观察的旧 Point 检查，含 staged readback、被 gate 挡住的新一轮、在途重放拒绝和 guards。
- 每种策略再保留 256 轮强/弱引用，核对旧对象的逻辑及 GPU 序号不可变，新 tag 在 gate 关闭时未完成，销毁 graph/event 后旧点仍可读，最终强引用释放后弱引用失效。
- 每种策略检查 1/15/16/17/31/32/33/200 个信号，直接核对 shared_ptr 所有权分组、尾组和跨轮不可变状态。
- 扩展回读检查再通过 12,800 个旧点，其中 800 个明确依赖更早、尚未发布的 staged readback，覆盖旧回读发布和新 tag 状态。
- 重编全部 69 个依赖适配器的 Host TU，其余 11 个根据实际依赖复用；runtime、数学 pack 和模型策略不变。
- kernel 检查含 840 项双专家完整输出与 36 组 FP16/BF16 GEMM；engine/queue 及 BCS=0/1 队列检查通过。
- 五组完整模型 **85 次请求全部匹配**完整 token、生成数、草稿采纳/提供数、终止原因和复用状态。

## 单项重放实验

200 个 inline 信号，无模型计算。每进程 8 次丢弃、32 次保留。
Host 时间在 owner 内包围 replay；whole 还包括线程交接与原生尾部等待。
这些是独立微型数据，不能当作模型加速或实际 GPU 内存分配计数。

| 进程 | 成组分配 | Host 中位数 µs | whole 中位数 µs |
| --- | --- | ---: | ---: |
| bench-0-0 | 关 | 124.95 | 348.35 |
| bench-1-1 | 开 | 112.20 | 355.60 |
| bench-2-0 | 关 | 131.10 | 496.00 |
| bench-3-1 | 开 | 122.35 | 378.10 |
| bench-4-0 | 关 | 126.95 | 490.50 |

保留全部五个进程；微型控制存在明显波动，不从其中挑最快值作为收益。

## 完整模型对照

顺序为 **当前源码 L0 → 同一 Direct EXE 关 → 开 → 关 → 当前源码 L0**。
唯一 Direct 策略变量是启动时解析的 `STRATA_ORCA_DG2_POINT_BATCH=0/1`，BCS 固定为 1。
两条 transport 逐字节共用数学 pack，L0 已含 M16 和 copy-role 源码改动。
固定 5,771 专家槽、1,536 MiB 预留、read256、decay0.95、margin1.5、MTP4、spec-min-p0.5、
cumulative0、suffix0、cost-window0；这是锚定配置，不是日常平衡默认。
每进程 Short 15 次（10 次丢弃、5 次保留），再运行 2K、8K 各一次；没有前缀复用。
Short 取进程内中位数，两侧同策略进程取几何均值。
DONE 净 decode 排除准备、初始化和 prefill；TTFT 单列。
计时无内部探针、GPU 时间戳、遥测或并行编译；GPU 时钟未锁。

| 输入 | 路径 | prefill ms | TTFT ms | 净 decode tok/s |
| --- | --- | ---: | ---: | ---: |
| Short28 | 当前源码 L0 | 914.18 | 950.87 | 58.1574 |
| Short28 | Direct 关 | 918.45 | 957.00 | 56.8318 |
| Short28 | Direct 开 | 906.90 | 945.06 | 56.5098 |
| Code2K | 当前源码 L0 | 27688.55 | 27739.48 | 18.7241 |
| Code2K | Direct 关 | 25463.90 | 25518.21 | 18.6413 |
| Code2K | Direct 开 | 25382.70 | 25435.84 | 18.6252 |
| Long8K | 当前源码 L0 | 72247.81 | 72299.30 | 18.8480 |
| Long8K | Direct 关 | 71750.64 | 71805.68 | 18.5804 |
| Long8K | Direct 开 | 71888.50 | 71944.42 | 18.5132 |

时间减少为负，吞吐提高为正。

| 输入 | 比较 | prefill 时间变化 | TTFT 变化 | 净 decode 吞吐变化 |
| --- | --- | ---: | ---: | ---: |
| Short28 | Direct 开/关 | -1.2574% | -1.2470% | -0.5667% |
| Short28 | Direct 开/L0 | -0.7966% | -0.6110% | -2.8330% |
| Short28 | Direct 关/L0 | +0.4667% | +0.6441% | -2.2792% |
| Code2K | Direct 开/关 | -0.3189% | -0.3228% | -0.0860% |
| Code2K | Direct 开/L0 | -8.3278% | -8.3046% | -0.5282% |
| Code2K | Direct 关/L0 | -8.0345% | -8.0076% | -0.4426% |
| Long8K | Direct 开/关 | +0.1921% | +0.1932% | -0.3617% |
| Long8K | Direct 开/L0 | -0.4973% | -0.4908% | -1.7765% |
| Long8K | Direct 关/L0 | -0.6881% | -0.6827% | -1.4200% |

| 输入 | 两侧控制 | prefill 漂移 | 净 decode 吞吐漂移 |
| --- | --- | ---: | ---: |
| Short28 | Direct 关 | +0.3599% | +1.1431% |
| Short28 | L0 | +1.2548% | +0.1685% |
| Code2K | Direct 关 | +0.0691% | -0.3170% |
| Code2K | L0 | +0.3988% | -0.1783% |
| Long8K | Direct 关 | -0.4184% | +0.2907% |
| Long8K | L0 | +0.2160% | -0.5814% |

## 结论

成组分配的 Short 净 decode 为 56.5098 tok/s，相对两侧关闭控制 -0.5667%；关闭控制自身漂移 +1.1431%。本轮未建立完整模型收益，因此不接入生产源码。


## 边界

长请求每进程只运行一次，属于有限序号匹配观察，没有建立长请求稳态或跨日重复性。
控制漂移是观测值，不是置信区间；候选进程数有限，不能仅凭少分配就认定完整模型更快。
共享 GPU 实验锁保证与其他验证串行，计时期间没有并行构建或诊断。
本轮未新增 D2D/应用锁普查、PPL 或 Linux GPU 数据，也不关闭已有 Direct 8K 间歇异常。
候选仍隔离；生产源码、配置、部署 EXE 未因此改变，当前推荐路径仍为 Orca L0。

## 复查

```powershell
python -X utf8 build-orca-slm-async-20261104/point_batch_experiment.py --verify
python -X utf8 build-orca-slm-async-20261104/build_point_batch.py --verify
python -X utf8 build-orca-slm-async-20261104/check_point_batch_readbacks.py --verify
python -X utf8 build-orca-slm-async-20261104/point_batch_contexts.py r1 --verify
python -X utf8 build-orca-slm-async-20261104/publish_point_batch.py --check
```

重新计量使用 `point_batch_contexts.py r2` 等新编号，成功证据不覆盖。
档案保留候选 header、安全检查源码、构建命令/依赖、配置、原始协议/日志和 SHA-256。
未变的完整基础源码见源码接入档案；数学 pack 和二进制记录哈希，不分发模型或二进制。

精确 JSON (`direct-point-batch-a770-windows-2026-11-04.json`, raw capture retained locally) ·
原始证据 (`direct-point-batch-a770-windows-2026-11-04.json.gz`, raw capture retained locally)
