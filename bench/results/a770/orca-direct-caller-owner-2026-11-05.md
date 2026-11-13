# Orca Direct：由调用线程执行已就绪的原生请求

2026-11-05（UTC+8），Windows 10 22H2，Intel Arc A770 16 GB，驱动 `32.0.101.9034`，oneAPI 2025.3。
本轮只替换冻结副本中 `sycl/orca_dg2_backend.hpp` 的 OwnerExecutor，原生 DG2 库、数学 pack、
模型、输入与采样门槛保持相同。原方案把同步调用交给专用 worker，再等待其完成；
候选让等待的调用线程逐个执行已发布的请求，省去专用线程交接。
MPSC 发布顺序转换为 FIFO，旧批次优先，仍在一个原子消费门内串行修改 runtime。
这不是完全无锁执行，也不提前执行尚未满足数据依赖的 GPU 工作。
栈上请求的完成字独占缓存行，TLS frame 链保留嵌套所有权；不新增依赖。

Windows CPU 检查通过 98,304 次并发工作，覆盖八个生产者的顺序、互斥、异常、void／引用／移动返回、
嵌套调用与 A→B→A 所有权恢复。以下是同日热态的每组 10,000 次顺序调用，
仅为 CPU 微测试；两侧 worker 有明显漂移，不能外推模型收益。

worker→caller→worker：**1368.44→59.31→1089.63 ns／调用**。

为避免头文件布局混用，重编译 69 个受影响 Host 单元、XMX 包装、三个 GPU 检查器与路由 probe，
共 74 项编译；11 个未包含该头的旧对象复用。编译、归档与链接全部成功。
kernel／engine／queue 三个 GPU 检查器及 120 次路由完整输出／guard／tick／Host 加法检查通过。
Linux／WSL 复用相同 CPU fixture，普通构建通过 98,304 次并发工作。
额外的 ASan／UBSan 编译在链接时失败：Alpine 环境缺少 libasan_preinit.o、libasan 与 libubsan；
保留初始失败记录，独立审计普通构建的原始输出，没有重跑、安装依赖或宣称 sanitizer 通过。
这不包含 Linux GPU，也不把 WSL 的 CPU 时间当作原生 Linux GPU 性能。

完整模型顺序为 worker→caller→worker、L0→caller→L0。每个新进程 15 次 Short28 请求，
丢弃前十次、保留五次，共 90 次。逐项比较完整 token、生成数、草稿采纳／提出数、复用／恢复数和停止原因，
另解析原始协议核对每次输出。无 GPU 标记或内层 profiling。
实际专家槽 5771，预留 1536 MiB，MTP4，step／last min-p=0.5，累计门槛 0，
short-read=256，adapt-decay=0.95，不复用 prompt；图在 READY 前预热。
净 decode 排除加载、初始化、准备与预填充，TTFT 单列。每进程取保留请求中位数，
两侧控制取几何均值；漂移为末次控制除以首次控制减一，不是置信区间。

完整协议质量检查：**通过**。

| 对照 | 指标 | 两侧控制 | caller | 变化 | 控制漂移 |
| --- | --- | ---: | ---: | ---: | ---: |
| caller_vs_worker | Decode tok/s | 58.583 | 58.669 | +0.146% | -0.599% |
| caller_vs_worker | 预填充 ms | 891.299 | 887.600 | -0.415% | -0.247% |
| caller_vs_worker | Decode ms | 1297.294 | 1295.400 | -0.146% | +0.603% |
| caller_vs_worker | TTFT ms | 928.714 | 924.969 | -0.403% | -0.177% |
| caller_vs_l0 | Decode tok/s | 59.871 | 58.597 | -2.128% | -0.016% |
| caller_vs_l0 | 预填充 ms | 888.699 | 887.900 | -0.090% | -0.247% |
| caller_vs_l0 | Decode ms | 1269.400 | 1297.000 | +2.174% | +0.016% |
| caller_vs_l0 | TTFT ms | 923.852 | 925.416 | +0.169% | -0.261% |

相对 worker 的净 decode 变化为 **+0.146%**，控制漂移 **-0.599%**；另一个窗口相对 L0 为 **-2.128%**。CPU 微测试的明显收益未转成已建立的整模型提速。

候选保持隔离，生产源码、部署 EXE 与默认策略未改。单次模型窗口与五个保留样本不足以建立跨日稳定性，
CPU 顺序调用的收益也不能直接折算为整模型速度。输入准备已有 PLE 双缓冲、Stager 和缓存自适应重叠，
本候选只检验同步原生调用交接，不增加推测 token 或专家预取。
本候选尚无长上下文／PPL／Linux GPU 结果，也没有关闭 Direct 8K 的既有间歇故障。

冻结执行依赖仍逐个核对原哈希。本轮根目录启动 JSON 已被并行 U4 发布更新；
试验直接使用保存的 C++ argv／env，不读取该启动 JSON，只允许并记录这一已审计的历史差异，
不回退新配置，也不把旧冻结 pack 的成绩当作最新生产 U4 成绩。
首个 build-r1 在预检因旧启动配置 pin 不同而停止，尚未复制／编译源码；事件记录随新 r2 归档。

```powershell
python -X utf8 build-orca-slm-async-20261104/caller_owner_cpu.py verify r1
python -X utf8 build-orca-slm-async-20261104/caller_owner_build.py verify r2
python -X utf8 build-orca-slm-async-20261104/caller_owner_model.py verify r1
python -X utf8 build-orca-slm-async-20261104/caller_owner_linux_cpu.py verify-audit
python -X utf8 build-orca-slm-async-20261104/publish_caller_owner.py --check
```

输入、命令与参考在等待 GPU 锁前准备；编译、设备工作、重型哈希与归档持共享实验锁。
新测试使用新 prepare 目录并保留相同硬件、驱动、冻结依赖与门槛，不覆盖旧记录。
归档包含源码、diff、编译／链接命令、逐次 CPU／GPU 检查和完整模型协议；二进制／模型只记录哈希。
精确 JSON (`direct-caller-owner-a770-windows-2026-11-05.json`, raw capture retained locally) ·
原始证据 (`direct-caller-owner-a770-windows-2026-11-05.json.gz`, raw capture retained locally)
