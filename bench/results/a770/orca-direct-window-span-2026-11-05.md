# Orca Direct：实际模型的低密度窗口时间戳

2026-11-05（UTC+8），Windows 10 22H2，Intel Arc A770 16 GB，驱动 `32.0.101.9034`，oneAPI 2025.3。
这轮在实际 Short28 模型上区分整个 Verifier 窗口的设备跨度与 Host 提交／返回。
每个窗口只有首尾两个有序 GPU 时间戳，围住输入复制、48 层和 head／采样；
末尾再记录 32 字节回读，与原有最终等待一起完成，没有在等待后另提交诊断工作。
首尾时间戳各自增加排序屏障。回读和 Host 日志仍会改变调度，带诊断的耗时不作为吞吐成绩。
缓冲前后 guard、原始 tick 非零／跨度／回绕／单调性以及逐次 Host 提交＋提交后至返回＝墙钟均检查。

仅各重编译一个 Verifier Host 单元，头文件布局、原生运行库和数学 pack 不变。
STRATA_VERIFY_PROFILE=1 只触发既有 GEN 的 decode 起止边界与请求统计；
Verifier 的 prof_on_ 强制为 false，不录制旧的逐层标记，也不关闭共享／路由交错和 GDN／QSA 独立投影。
每个 Verifier 初始化时分配一次 32 字节设备诊断缓冲，模块、arena 与各窗口图在 READY 前预热。
测量帧排除初始化、请求准备与预填充；窗口内部必需的输入准备仍计入 stage。
commit、MTP 草稿和窗口间 Host 工作位于 GPU 首尾标记范围之外。

顺序为 Direct→L0→Direct，每个新进程 15 次相同 Short28 请求，丢弃前十次、保留五次。
完整比较 token、生成数、草稿采纳／提出数、复用／恢复与结束条件，并独立重放原始协议；
每条请求的实际 T 序列也须一致。固定实际 5771 槽、reserve1536 MiB、MTP4、step／last min-p0.5、
累计门槛0、read256、decay0.95、suffix0、prompt-cache0，Direct 使用冻结的 BCS／依赖策略。
输入、参考、源码与命令先准备；编译、设备工作、重型哈希和归档持共享实验锁。

完整协议质量检查：**通过**。45 次请求共 **900 个窗口**，保留 **300 个窗口**。

| 每请求窗口时长合计的中位数 | Direct 两侧控制 ms | L0 ms | Direct 多用时 | Direct 控制漂移 |
| --- | ---: | ---: | ---: | ---: |
| GPU 首尾跨度 | 1132.5819 | 1120.2773 | +1.098% | +0.203% |
| Host 窗口输入准备 | 1.1814 | 0.7480 | +57.936% | +3.645% |
| Host 提交 | 1.8121 | 0.9155 | +97.932% | -4.524% |
| Host 提交后至返回 | 1138.2573 | 1122.0929 | +1.441% | +0.412% |
| Host 提交至返回墙钟 | 1140.0519 | 1123.0907 | +1.510% | +0.419% |

先逐条请求对其窗口求和，再取进程内五条保留请求的中位数；Direct 两侧进程取几何均值。
不同分项的中位数未必相加等于墙钟中位数，原始每窗口 Host 纳秒数据满足精确加法。
所有原始窗口、温热过程、三个进程的独立统计与实际 T 序列均在 JSON 和归档中。
控制漂移不是置信区间；一次顺序、同日有限样本不建立稳定吞吐收益。

GPU 跨度包含 kernel、内存访问、依赖空隙和标记排序成本，不是独占内核时间。
Host“提交后至返回”包含尚未完成的 GPU、32 字节回读、完成发布及唤醒，不能称为纯唤醒延迟。
没有绝对 CPU／GPU 时钟关联，不将两个时间域相减计算精确完成延迟。
本轮减少了标记密度，仍不能直接把与无探针对照的差异归因为运行库修复。

全部冻结执行依赖继续核对原哈希；只允许并记录已审计的根目录启动 JSON 更新。
试验直接传递保存的 C++ argv／env，不读取该 JSON，不使用新生产 U4 pack，也不回退其发布。
没有修改生产源码、默认等待策略或部署 EXE。Linux GPU、长上下文与原有 Direct 8K 间歇问题未由本轮验证。
首个 r1 在 CPU 审阅阶段发现 GEN 帧触发条件不足；r2 误用 SYCL 分配入口而编译失败，未执行 GPU。
上述失败准备与编译日志、成功 r3 的源码和完整原始记录一并保留。

```powershell
python -X utf8 build-orca-slm-async-20261104/window_span.py self-check
python -X utf8 build-orca-slm-async-20261104/window_span.py verify r3
python -X utf8 build-orca-slm-async-20261104/publish_window_span.py --check
```

重新运行先 prepare 新目录，再 run；同条件保留驱动、冻结源码／二进制／数学 pack、输入、参数和请求历史。
归档包含源码、可逆 diff、命令、配置、GPU 原始 tick、Host 时长、全部输出和依赖哈希；不分发模型／EXE。
精确 JSON (`direct-window-span-a770-windows-2026-11-05.json`, raw capture retained locally) ·
原始证据 (`direct-window-span-a770-windows-2026-11-05.json.gz`, raw capture retained locally)
