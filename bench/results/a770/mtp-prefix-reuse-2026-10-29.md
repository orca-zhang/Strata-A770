# A770：复用 MTP 首个草稿的 catch-up 前缀

2026-10-29，A770 16GB / PCIe 3.0 / Windows / oneAPI 2025.3。候选已在共享锁内完成编译和链接，mode0/2/1 三个独立引擎的正确性检查全部通过。修复准入后的复测在原始基线A1第八条长请求失败，候选尚未运行，没有收益或回退结论。

当前链接的原对象是 `build-sycl-a770-mtp-spec4-last-draft/mtp-last-draft.obj`。其对应源码 `bench/results/2026-10-11-a770-mtp-spec4-last-draft-policy/mtp.cpp` 的 SHA256 为 `2fe26dc4699d0a2e637bc9583cf6e02ef79803bb5cda9397a9506f70adc42e7c`，已匹配原构建来源清单。没有拿早期不同 min-p 策略的源码替代实际对象。

`record_round` 和 `capture_round` 先为 T 行执行输入投影、GR、Q8 和 K/V，随后又为选中的 a 行执行同样前缀。候选将该行的 R/inject/Q8 移到 row0，首个草稿复用已有前缀并跳过重复 K/V 写入。保留后续 Q 投影、MLP、head、采样、后续草稿及 min-p 策略。mixed/lo/rs/xn 在后续 MLP GR 中重新生成，不复用。Q8 按字节复制，避免通过 float 赋值改变表示。

只给私有 `record_forward` 增加两个默认 bool 参数，class 布局不变。未计时 audit 使用此前缀阶段未用的 parts scratch，以及已有 mapped 输出额外 64 字节 padding，检查其容量，不增加 arena。生产文件不改。

新入口 `build-sycl-a770-mtp-prefix-reuse-20261029/run.py` 验证三个独立引擎：

- mode0：保留原重算路径。
- mode2：保存完整选中行 R/inject/Q8/K/V，与原 T1 重算逐位比较，记录真实 T/a、差异 mask 和最大绝对误差。
- mode1：复用前缀；首个真实 round 完成后记录 `MTP_PREFIX_USED source=completed-round`。

每个模式完整执行 Short16、Code/Prose 2K/full128，输出匹配固定参考。任何非有限值或逐位差异均停止进入测速，先保留误差证据；本轮没有直接放宽精度门槛。mode2 的比较和 host wait 不在性能样本中。

通过上述检查后才运行原完整六进程采集器：原八条交错历史、全部输出、实际6297槽、READY和完整请求采样空闲显存≥1024 MiB、同场景两轮收益≥1%、相邻基线耗时漂移≤3%、吞吐绝对相对变化<0.3%视为噪音。稳定35要求确认场景每条保留候选请求均≥35 tok/s。758固定目标和完整匹配热历史质量另验，不能拿算子或 audit 时间当收益。

原生查询确认 worker PID17292 / 创建时间1793270840446 ms。`audit/results.json` 为 `complete=true, passed=true`，三个模式均完整完成 Short16、Code2K128、Prose2K128 且实际 exit0。mode2 共117次完整 R/inject/Q8/K/V 对照，全部 mask=0、max_abs=0；实际 T/a 覆盖 (1,0)、(2,0/1)、(3,0/1/2)、(4,0/1/2/3)、(6,5)。mode1 的完整输出与 mode0 匹配。旧 `audit/partial.json` 是未置终态的过程快照，不能取代完整结果。audit 编译启动监视通过。

20:19:30 +08:00，`queued.json` 为 `model_screen`，但实际尚未进入性能采集：stdout 显示 `WAIT 22840 deepinfer`，共享锁内容与之匹配。原生查询 PID22840 / 创建时间1793275756887 ms 是另一个会话的 `build-orca-native-20261029/build-session.py`，仍有真实编译子进程。没有删锁、终止该任务或将排队状态算作测速完成。117次 audit 只证明本次逐位一致，不是吞吐结果，也不代替758目标的质量验收。

20:20:29 +08:00，原worker实际exit1。原 `performance/results.json` 只建立了A1，没有初始化输出或模型请求；错误为 `workspace executable busy: monitor.exe`。audit将检查闭包安装到共享Python模块后，下一次构建准入闭包又将旧闭包捕获为根检查；旧闭包没有新测速器的monitor句柄，因而将自己的遥测误判为外部任务。这是测速准入失败，不是性能回退或GPU正确性失败。原guard完整通过、events=[]，原全部失败文件保留。原通用质量任务已完整跳过三项，不代表MTP质量通过。

新 `build-sycl-a770-mtp-prefix-confirm-20261029/run.py` 每个新阶段从原始 `bench_deepinfer.busy` 函数建立一条检查链，并断言准入根就是该原始函数；不修改 `tools/bench_deepinfer.py`。保留现有精确、存活、调用者持有句柄的monitor豁免，外来monitor和编译器仍阻止运行。复用已通过audit的同一EXE和原完整采集器，获锁后重核全部构建/audit摘要、输出及失败确实发生在首请求前。不重编模型或拼接样本。

CPU自检通过根函数身份、原精确monitor正反例、完整收集器、两臂质量链接只替换共同main、原9组评分边界和普通GEN128请求保留，以及生成namespace全部全局引用。速度/显存达标后才检查冷态758目标，再用独立诊断main对完整热历史后的同样758目标评分；诊断main使用已准备的显式score_from，不供应性能样本。冷态仍用原EXE和原评分，热态保留cache0/shortread1024以及八条完整请求。

20:26:11 +08:00，supervisor PID22276 / 创建毫秒1793276771591，worker PID12936 / 1793276771763，精确等待GR热态worker4312 / 1793275036699，再获取共享锁。该入口未改动仍活跃的诊断或GR脚本，默认配置不改。

```powershell
python -X utf8 build-sycl-a770-mtp-prefix-confirm-20261029/run.py --self-check
python -X utf8 build-sycl-a770-mtp-prefix-confirm-20261029/supervise.py --self-check
```

通用质量接续仍在等待本候选。当前发现的 cache0 长输入评分缺失问题见[质量入口记录](quality-recovery-2026-10-29.md)；不能把缺失记录算作质量通过，也不能将新 GR 评分入口视为已经验证了本候选。

```powershell
python -X utf8 build-sycl-a770-mtp-prefix-reuse-20261029/run.py --self-check
# 完整运行结束后复核：
python -X utf8 build-sycl-a770-mtp-prefix-reuse-20261029/run.py --check
```

21:16:46 +08:00：修复准入的worker12936实际exit1。它成功使用自己的monitor并完成Short16及前七条完整2K/full128请求，所有完整输出与冻结参考相同；第八条Prose2K在第40层Down/H分别出现24960000/6240000个非有限值，模型实际exit0xc0000005。此时仅运行原始device-counter基线EXE，日志无MTP_PREFIX标记，MTP候选尚未启动。READY为6297槽、1625MiB空闲；编译监视完整通过、events=[]。这次失败是原基线的热态稳定性问题，不能用残缺基线与历史候选比较收益；758质量未启动，原117次逐位audit仍保留其独立含义。

```powershell
python -X utf8 bench/results/a770/check_mtp_baseline_failure_20261029.py
```

该离线检查已exit0，重建所有完成输出并核对真实失败、无候选运行及零编译事件；不重新读取或哈希权重/EXE。现有QSA55阶段诊断已进入共享锁等待，之后的GR热历史诊断也保留原V4路径，以定位共同的非有限值来源。

独立复用原纯遥测解析器，七条已完成请求的最小采样空闲显存为1119.515625MiB；最后一条已完成Code请求之后至monitor结束的26个采样点，最小1119.55078125MiB。后一区间包含失败请求及退出清理，不能充当失败请求精确边界或瞬时峰值测量。记录不支持直接把该NaN归因于桌面显存占用，仍需阶段诊断定位。PCIe计数器返回error2013265923，不能从这些记录计算实际PCIe吞吐。
