# 空闲 Short 复测：剩余差距与完成等待实验

2026-11-04，Windows 10 22H2，Intel Arc A770 16 GB，驱动 `32.0.101.9034`。用户确认机器空闲后，复用完整前端重建的匹配配置与同一数学内核集合。
正向 Direct **58.3180**、L0 **59.3843 tok/s**，Direct 相对 **-1.7956%**；
反向 Direct **58.3002**、L0 **59.5705 tok/s**，Direct 相对 **-2.1324%**。
两种顺序保留各自结果；这轮差距不能全归因于用户同时运行其他软件，仍未建立 Direct 快于 L0 的结论。

## 无诊断钩子的计时

每个独立进程丢弃十次预热、保留五次。表中是进程内中位数；两侧参考取两个中位数的几何均值。decode 使用 DONE 净解码时间，排除加载、初始化、准备和预填充；TTFT 单独列出。没有内部阶段计时、GPU 时间戳或遥测；GPU 时钟未锁定，后台系统活动仍可能影响结果。

| 对照顺序 | 进程 | decode tok/s | 五次范围 tok/s | TTFT 中位数 ms |
| --- | --- | ---: | ---: | ---: |
| idle-l0-direct-l0 | L0 / Orca L0-before | 59.4261 | 59.3750–59.4865 | 864.81 |
| idle-l0-direct-l0 | 事件 / Orca Direct | 58.3180 | 57.9843–58.5561 | 865.32 |
| idle-l0-direct-l0 | L0 / Orca L0-after | 59.3425 | 59.1394–59.6406 | 864.53 |
| idle-direct-l0-direct | 事件 / Orca Direct-before | 58.4076 | 57.5932–58.5832 | 866.10 |
| idle-direct-l0-direct | L0 / Orca L0 | 59.5705 | 59.5191–60.0079 | 858.62 |
| idle-direct-l0-direct | 事件 / Orca Direct-after | 58.1930 | 57.9799–58.2599 | 867.85 |
| poll-event-poll | 轮询 / Orca Direct-before | 58.6601 | 58.5697–58.7009 | 861.40 |
| poll-event-poll | 事件 / Orca Direct | 58.1885 | 58.0197–58.2242 | 863.28 |
| poll-event-poll | 轮询 / Orca Direct-after | 58.8144 | 58.6873–58.9833 | 865.13 |
| event-poll-event | 事件 / Orca Direct-before | 57.7508 | 57.2117–58.1395 | 886.10 |
| event-poll-event | 轮询 / Orca Direct | 58.9010 | 58.7099–59.0704 | 858.58 |
| event-poll-event | 事件 / Orca Direct-after | 57.9312 | 57.8783–58.4930 | 862.77 |
| idle-l0-poll-l0 | L0 / Orca L0-before | 59.8425 | 59.6546–60.0363 | 858.86 |
| idle-l0-poll-l0 | 轮询 / Orca Direct | 58.5787 | 58.5110–58.7689 | 859.47 |
| idle-l0-poll-l0 | L0 / Orca L0-after | 59.8472 | 59.6874–60.0126 | 861.52 |

L0 前后速率漂移 **-0.1405%**；反向 Direct 前后漂移 **-0.3675%**。这些范围和漂移不是置信区间。

## 当前前端的独立计数

API/命令抓取与复制量普查各用独立诊断程序，各运行两种后端的十一轮请求，取第十一轮热解码。诊断会扰动执行，其耗时不用于性能结论。

| 第十一轮热解码 | Direct | L0 |
| --- | ---: | ---: |
| 提交次数 | 97 | 97 |
| 显式 D2D 次数 | 134 | 134 |
| 显式 D2D 字节 | 4,554,752 | 4,554,752 |
| 显式 H2D / D2H 字节 | 0 / 0 | 0 / 0 |
| 自适应专家换入次数 / 字节 | 0 / 0 | 0 / 0 |
| 热路径分配 / 释放 / 映射 | 0 / 0 / 0 | 0 / 0 / 0 |
| 导入 `_Mtx_lock` / `_Mtx_unlock` 调用 | 112 / 112 | 1,048 / 1,048 |
| COMPUTE_WALKER 命令 | 40,119 | 40,273 |
| CFE_STATE 命令 | 2,492 | 97 |
| CPU fence wait 注册 | 77 | 0 |
| GPU queue wait 调用 | 0 | 77 |

十一对请求的逻辑复制和填充计数均相同。复制量按每次图重放累计，包含记录后的实际重放次数，不能当成物理 PCIe／显存流量；内核直接读取映射权重、参数内部上传和 CPU memcpy 不由这组逻辑复制计数完整覆盖。
锁观察覆盖 EXE 导入及动态解析入口，厂商库 IAT 仅观察 D3DKMT；不覆盖全部私有、内联或驱动锁，也不测量锁竞争。上述次数已低于 L0，但不证明锁开销是剩余瓶颈。

Direct 已对跨队列前驱提交 GPU fence 依赖，同队列依赖由顺序保证。CPU fence 等待用于完成观察及后续主机结果发布；不能把两列 wait 次数当作同一工作，直接删掉 CPU 等待。
命令字段解码保留全部原始缓冲和分布。当前仍有 GRF 相关 CFE 状态、非零 SLM 分区和 SBA 缓存字段差异；已有独立负向控制，数量不同不等于性能因果。输入数值模块相同也不等于本轮验证了最终加载 ISA 的逐字节身份。

## 完成观察改为主动轮询

隔离候选只修改 Windows `NativeCompletion::wait`，保留超时、关闭检查、精确目标 fence、队列依赖和结果发布。owner 等待路径不变；等待期间占用 CPU，实验代码未合入。
事件／轮询／事件中，轮询相对事件 **+1.8328%**，事件两侧漂移 **+0.3125%**；
轮询／事件／轮询中，轮询相对事件 **+0.9430%**，轮询两侧漂移 **+0.2631%**。
两种顺序独立保留，不能跨轮取最好值组合成提升。更换等待方式也可能影响 CPU 频率和调度，其吞吐变化不是纯 OS 通知延迟的测量。候选先通过 14 项 Windows CTest、六组底层 GPU 数值检查及当前前端 kernel／engine／queue 三项 GPU 检查。
随后独立 L0／轮询 Direct／L0 对照：轮询 Direct **58.5787**、L0 **59.8449 tok/s**，Direct 相对 **-2.1158%**；L0 两侧漂移 **+0.0079%**。

## 条件与复现

固定 Short 28 输入／自然 76 输出；实际 5,771 专家槽／7,922 MiB，预留 1,536 MiB；read256、decay0.95、MTP4、step／last 门槛 0.5、累计门槛 0，缓存热度差 1.5。
**这是固定锚点，不能当成日常平衡参数或自动缓存容量的成绩。** 共 225 次干净请求、75 次保留测量及另外 44 次诊断请求，均逐 token 匹配，57/57 草稿、自然 stop、无前缀复用。生产配置、已部署 EXE 和候选默认等待策略均未修改；未新增 PPL、长上下文或 Linux GPU 结论。

[完整结果与哈希](../../../sycl/orca/direct-idle-audit-a770-windows-2026-11-04.json)及证据归档 (`direct-idle-audit-a770-windows-2026-11-04.json.gz`, raw capture retained locally)包含精确配置、源代码、构建命令、日志、协议和原始命令缓冲；完整前端构建依赖[前一份源快照](orca-direct-current-frontend-2026-11-04.md)。EXE／数值包有哈希，GGUF 与权重 pack 内容未在本轮完整重算哈希。
同等条件复现需要相同硬件、驱动、编译器、外部资产、初始专家 profile、预热历史和参数；空闲条件下仍允许时钟与系统波动。

本机复测使用尚不存在的输出目录：

```powershell
python -X utf8 tools/bench_orca_anchor.py --config build-orca-current-frontend-20261104/matched-policy/direct.json --runtime direct --baseline-config build-orca-current-frontend-20261104/matched-policy/l0.json --baseline-runtime l0 --out build-orca-idle-repeat-forward
python -X utf8 tools/bench_orca_anchor.py --config build-orca-current-frontend-20261104/matched-policy/l0.json --runtime l0 --baseline-config build-orca-current-frontend-20261104/matched-policy/direct.json --baseline-runtime direct --out build-orca-idle-repeat-reverse
```

另一台机器需按归档命令重建并映射资产路径；不可用不同构建的 EXE 代替。仅离线核对本轮证据：

```powershell
python -X utf8 build-orca-direct-gap-20261104/publish_idle_audit.py --check
```
