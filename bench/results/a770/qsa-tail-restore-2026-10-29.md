# A770：当前冻结 verifier 的 QSA gate/Q8 尾部融合

A770 16GB / PCIe 3.0 / Windows / oneAPI 2025.3。288 组 eager 和 288 次变输入 graph 对照通过；新的完整六进程复测没有两轮至少 1% 的同场景收益，不采用该融合。默认配置未改，35 tok/s 未达到。

当前实际模型 verifier 在 QSA gate 后单独调用 `native_quantize_q8_1`；链接 map 的六参数 `native_qsa_gate_apply` 属于冻结 `strata_kernels:native_qsa.dp.cpp.obj`，对应实现没有 Q8 输出。工作区已有 gate/Q8 融合，但配置开关存在并不证明当前测速 EXE 已接入。此前算子 +8.8% 不当作本候选整模型收益。

既有 eager 原始 CSV 中，QSA gate 为0.012460秒/1644次；全部独立 native Q8 量化为0.108315秒/24013次，分别约占9.572915秒累计计算 kernel 时间的0.13%和1.13%。量化数字包括其他调用点，不能全部归入 QSA/GDN；多队列累计时间也不是正常 graph 的串行关键路径。这些数字用于限制预期，不作为性能验收或可兑现的端到端上限。

隔离入口 `build-sycl-a770-qsa-tail-restore-20261029/run.py` 提取现有融合 gate，实现独立导出 `qsa_tail_current`。CPU 检查逐字确认 gate 的乘法、sigmoid 和 native exp 表达式与冻结原实现相同。保留 FP32 输出，只省去后续 Q8 派发及其激活重读；没有移除所有 FP32 中间写回。verifier 只改 gate 调用点及紧邻量化，其他 norm/RoPE、GDN、HC、GU/Down、预填、MTP、runtime 和实际6297槽均保留。输出 EXE/MAP 写独立目录，链接集合要求仅替换 verifier、增加一个 tail 对象，并检查唯一符号所有者。实际调用融合后才输出 `QSA_TAIL_CURRENT active=1 source=frozen-verifier`。

先验证 HD128/256、NH1/3/48、T1–8、整批和逐 token 两种调用方式，覆盖零、随机和大幅值输入。288组 eager 对照及288次输入变化的 graph 重放，逐字比较完整 FP32 与 Q8 字节；每次运行检查只读输入和所有缓冲区前后守卫。另检查非法维度拒绝。任一失败均停止模型计时。CPU 自检不替代这些 GPU 检查。

复用既有完整六进程模型采集器及 GDN 恢复的构建/验收程序，仅更换候选、运行见证及探针计数，全部输入来源摘要继续记录。原始 Code/Prose2048/full128、相同预热与交错保留历史、tail64、V4、HC BF16、默认 runtime、实验 min-p0.5，READY和采样空闲显存均至少1024MiB。生产 min-p0.8不改。同一场景独立两轮至少1%收益、相邻基线耗时漂移不超过3%；绝对相对吞吐变化低于0.3%视为噪音。35要求同一确认场景的每条保留候选请求均至少35 tok/s。758固定目标及匹配热历史质量仍需速度通过后单独完成。

本候选与 GDN 恢复分别对照原冻结基线，没有叠加两项融合或其他候选，也不将历史增幅相乘。生产文件和默认配置未由本实验修改。

2026-10-29 16:45 +08:00：CPU 自检通过后冻结脚本及两个输入，隐藏 worker PID12192，原生创建时间1793263536833ms。精确等待 U2恢复 PID15256/1793263097487（后者等待 GDN恢复），然后使用既有 GPU/编译共享锁。当前排队，不与独占模型计时并行编译；没有把排队写成 GPU 已通过。

```powershell
python -X utf8 build-sycl-a770-qsa-tail-restore-20261029/run.py --self-check
# 仅在该 cohort 完整成功后复核：
python -X utf8 build-sycl-a770-qsa-tail-restore-20261029/run.py --check
```

2026-10-29 17:41 +08:00：已补上条件冷/热质量入口`build-sycl-a770-tail-quality-20261029/run.py --tail qsa`。隐藏worker PID13136，原生创建毫秒1793266880381，精确等待原QSA模型任务PID12192/1793263536833。原队列、已冻结输入及生产文件不改。只有完整独立同场景测速和请求内采样显存≥1024MiB均通过才启动质量；否则记录跳过，不将质量记为通过。

质量复用现有评分器，选择本QSA候选EXE和`STRATA_QSA_Q8_TAIL`的0/1，不叠加GDN候选。冷、热各A1/B1/B2/A2，均为原9组758固定目标；所有必须概率有限，候选评分两次精确重复，汇总PPL增幅≤1%、各组≤3%。冷基线等原冻结评分；热阶段先保留Short16和测速全部8条2K/full128历史，再评分相同758目标。完整输出、候选accepted/offered、8×48扫描、tail64和唯一实际QSA tail启用见证均核对；热基线A1/A2精确重复。冷失败则不运行热评分。

```powershell
python -X utf8 build-sycl-a770-tail-quality-20261029/run.py --self-check
# 仅在本质量或跳过记录完整结束后检查：
python -X utf8 build-sycl-a770-tail-quality-20261029/run.py --tail qsa --check
```

CPU自检已通过两种tail的隔离路径、双EXE/开关、评分正反例和缺失/重复启用见证拒绝；尚无本QSA候选的GPU parity、整模型或冷/热质量结论。质量runner输入已冻结，采集及大型摘要复核使用共享锁。

2026-10-29 18:24 +08:00：实际编译、链接和GPU检查已结束，`build.json complete=true`。`parity.log`报告288组eager对照和288次变输入graph重放全部通过，比较完整FP32/Q8、只读输入和守卫；parity阶段编译监视器通过且无事件。worker12192已进入`model_screen`，持共享锁执行原六进程采集器。此处仅重读小型终态与日志，未在模型计时期间重新哈希EXE或对象；尚无完整模型配对、独立确认或758冷/热质量结果。

2026-10-29 18:38 +08:00??worker12192??????A1??8????B1??4???????????????????cohort `complete=false`??????? `complete=true,passed=false`????2026-10-29T18:31:20.647+08:00 icx.exe PID24168??PID17516?2026-10-29T18:31:20.648+08:00 clang-cl.exe PID18032??PID24168?2026-10-29T18:31:33.797+08:00 icx.exe PID16524??PID21872?2026-10-29T18:31:34.799+08:00 clang-cl.exe PID17420??PID16524????????????????B1????/??????worker13136??????????assert??????????????????????????

? `build-sycl-a770-qsa-tail-confirm-20261029/run.py` ?????288/288?????EXE??????????Supervisor PID8036/1793270250838?worker PID14712/1793270250971?????GDN??PID21468/1793269058177??????????????????/EXE??????????????????????????1024MiB?????????/??????758????????????1%/3%/0.3%/6297?/35???????CPU???exit0????GPU????????

```powershell
python -X utf8 build-sycl-a770-qsa-tail-confirm-20261029/run.py --check
```

## 完整确认结果：未通过收益门槛

2026-10-29 20:05:46 +08:00，`build-sycl-a770-qsa-tail-confirm-20261029` 的 worker PID 14712 / 创建时间 1793270250971 ms 实际 exit 0。A1/B1/A2/A3/B2/A4 均完整完成八条请求且退出码为 0，完整输出、原协议和各进程 8×48 层扫描通过。编译启动监视 complete/passed，events=[]。全请求采样空闲显存最低 1392.66796875 MiB，通过 1024 MiB 门槛。

| 场景 | 首轮吞吐变化 | 独立第二轮变化 | 结论 |
| --- | ---: | ---: | --- |
| Code2K/full128 | +1.1386% | +0.1080% | 第二轮为噪音，未确认收益 |
| Prose2K/full128 | −0.2053% | +0.1375% | 两轮均为噪音 |

首轮 Code 的 A1/B1/A2 保留请求耗时中位数为 4448.7/4348.9/4348.7 ms，基线漂移 −2.2478%；第二轮 A3/B2/A4 为 4358.2/4351.8/4354.8 ms，漂移 −0.0780%。Prose 两轮分别为 4739.2/4749.0/4739.3 ms 和 4703.6/4711.6/4732.6 ms，漂移 +0.0021%/+0.6165%。独立从原始逐请求 decode_ms 重算几何配对结果，与采集器一致。绝对相对吞吐变化低于 0.3% 按用户规则分类为噪音，原值保留。

B1 的首条保留 Prose 为 8069.8 ms / 15.8616 tok/s，完整保留。对应 A1/A2/B2 请求的 accepted/offered 均为 72/95，输出相同，耗时分别为 4811.8/4679.1/4711.6 ms。以 host_end_ns−decode_ms 近似定位的解码区间内，B1 有三个 600 MHz 采样点，其他采样点及对照均为 2400 MHz；节流标志为 0，B1 该区间采样空闲显存约 1599–1605 MiB。这只是频率与延迟的相关记录，尚不能判定是 JIT、调度、访存或其他原因，也不能替代硬件剖析。不得丢掉该慢请求。

`passes_confirmation=false`、`confirmed_scenes=[]`、`sampled_memory_passed=true`、`speed_target35=false`。本次确认没有启动质量阶段；这表示速度门槛未通过，不表示质量通过。先前受外部编译干扰的失败组和其质量入口失败仍保留。本次与 GDN、GR、runtime 和 MTP 均未叠加。
