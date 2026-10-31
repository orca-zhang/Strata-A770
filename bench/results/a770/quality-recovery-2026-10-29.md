# A770：质量入口修复与实际验收状态

2026-10-29，A770 16GB / PCIe 3.0 / Windows / oneAPI 2025.3。GR 冷态九组共 758 个固定目标已通过，A1/B1/B2/A2 的评分全部有限且相同，汇总 PPL 变化为 0%。最新热态 B2 在 CurrentProse2K 评分前填充时出现 NaN，质量失败，GR 未采用，35 tok/s 目标未达到。

原 `build-sycl-a770-gr-a4-quality-recovery-20261029` worker PID 6172 / 创建时间 1793271973174 ms 已退出，监督进程记录实际 exit 1。它完成冷态评分后，在热态基线 A1 的评分解析处断言。原始热态引擎完整完成 Short16、八条交错 Code/Prose 2K/full128 历史及九个评分请求，实际 exit 0；编译启动监视完整通过，events=[]。原失败、协议、评分和 DLL 记录均保留。

独立检查原始 `hot/A1.logpos.tsv` 得到 271 行，全部有限，严格匹配 Short 初始化及 memory/short/gcd 的记录，但只覆盖 112/758 个固定目标，缺少其余六组的 646 个目标。不是非有限概率导致这次断言。根因是 cache0 不启用聊天轮次拆分，长评分输入全部走批量预填；该路径不输出逐位置 STRATA_LOGPOS。原解析器还错误地期待八条 2K 热历史输入的评分记录。不得跳过缺失行、删掉长输入，或把 112 个目标当成 758 个通过。

新入口 `build-sycl-a770-gr-hot-score-from-20261029/run.py` 已完成独立诊断main及双方EXE的编译/链接，原生边界检查exit0，正在基线A1的完整热历史。它在独立诊断 EXE 的 GEN 中增加显式 `score_from`，复用现有 teacher forcing 路径。八条热历史请求不带此键，继续保持 cache0/short-read1024；九个固定目标请求才指定边界。保留原全部输入和目标、A1/B1/B2/A2、基线与候选完整热历史及 accepted/offered、有限值、精确重复、总 PPL ≤+1% 和单组 ≤+3%。正常请求不带新键时保留原路径。诊断 EXE 只用于质量，不供应任何测速样本。

新入口复用原 GR 对象及其基线对象，双方仅替换共同的诊断 main。源码变换可逆，编译和链接写独立目录；先执行整数解析、溢出、范围和文本边界的原生检查，再进行完整热态验收。逐项验证每组目标位置和 758 个目标总数。只有原冷态与新热态均通过才发布独立可选配置，默认配置不改。CPU 自检已覆盖实际失败文件、缺失目标核算、原请求保留、评分请求键、未知目标拒绝和生成命名空间的全部全局引用。

19:57:16 +08:00 原生查询确认 supervisor PID 23648 / 1793275036478 ms，worker PID 4312 / 1793275036699 ms，精确等待专家异常诊断 PID 22780 / 1793273668472 ms，然后获取共享 GPU/编译锁。没有为这项工作终止桌面程序或其他测试。

20:49 +08:00，前置诊断已结束，worker取得锁后完成编译、原生检查及双方链接；实际A1在READY报告6297槽、1817MiB空闲，并记录实际DLL路径/摘要。当前仍在八条2K/full128热历史，尚未完成任何一组新的热态758评分；不得将旧112个已覆盖目标、日志有限或诊断main成功构建写成热态质量通过。MTP新复测等待本worker实际退出。

20:56 +08:00，A1已完整执行八条热历史及九个评分请求，实际exit0，worker严格解析成功并进入候选B1（READY6297槽、1738MiB空闲）。另外直接调用原q.parse，从已关闭的A1原始logpos重建九组评分，核对目标位置范围及总数758：followup6、long/code/prose/CurrentCode2K/CurrentProse2K各128、memory6、short75、gcd31。所有必需目标概率有限。首次证实显式边界实际补齐了原缺失的646目标；候选两次评分和末基线仍未完成，尚不能发布配置或宣称完整热态质量通过。

21:04 +08:00，A1/B1均完整完成八条热历史和九个评分请求、实际exit0。独立从两份已关闭的原始logpos重新解析全部758目标并核对位置，**评分字典逐项完全相同，汇总及九个单组PPL变化均为0%**。这只是第一组热配对的结果；B2已实际启动（PID1056 / 创建于21:03:51），候选精确重复和末基线A2尚未完成，`state.json complete=false`，未发布配置。

此前 metadata 修复仍保留：速度清单缺少 `dll_sha256`，生成 namespace 缺少 `PARENT`。当前实际速度清单的 `files_sha256` 也没有三项 DLL 摘要，不能声称已有。新的 owned DLL 捕获只证明相应质量引擎；已有冷态四个及失败热态 A1 的真实记录，不倒推原测速引擎的模块状态。新热态会继续捕获实际子进程的 PID、创建时间、命令、EXE 路径及三项 DLL 的路径和摘要。

`build-sycl-a770-gr-runtime-confirm-20261029` 已因 GR 热态未通过而完整跳过，没有组合测速。后续组合仍需新的独立确认，不能将 GR 和 runtime 的历史收益相乘。现有 tail/MTP 质量接续保留；本次新评分入口只处理 GR，不把其他候选的排队状态写成通过。

```powershell
python -X utf8 build-sycl-a770-gr-hot-score-from-20261029/run.py --self-check
python -X utf8 build-sycl-a770-gr-hot-score-from-20261029/supervise.py --self-check
```

另见[专家阶段异常定位](prefill-first-bad-2026-10-29.md)。FP16 的真实 Down 非有限值失败与本次评分入口缺失是不同问题。

21:11:34 +08:00，显式边界 worker4312 实际 exit1，B2 模型实际 exit3221225477（0xc0000005）。A1/B1 各完成九组758目标且逐项相同；B2完成八条完整热历史及前五个评分请求，严格重建518目标也与B1逐项相同。第六个评分请求 CurrentProse2K 的 bulk prefill 在第20层失败：Down 中10009600个非有限值，H中2502400个非有限值；样本位模式分别为FP32 NaN `ffc00000`、FP16 NaN `fe00`。当前请求128目标没有完成，其后112目标及末基线A2未执行；不能把已完成518个目标当作758通过。编译启动监视通过、events=[]，READY为6297槽、1827MiB空闲。

21:16:46 +08:00，随后MTP复测的原始基线A1也在第八条长请求Prose2K失败，候选未启动，第40层Down/H分别24960000/6240000个非有限值。这证明原基线路径本身也会失败，尚不能把GR B2的异常单独归因于GR；也不能将reason=overflow等同于已经确定FP16溢出。两次真实失败、慢样本、协议和原状态均保留。

新增离线检查从原协议重新核对每次完整输出、实际退出及评分边界，不读或重新哈希大型二进制：

```powershell
python -X utf8 bench/results/a770/check_gr_hot_failure_20261029.py
python -X utf8 bench/results/a770/check_mtp_baseline_failure_20261029.py
```

两项均exit0；检查通过表示真实失败已被准确核算，不是模型质量通过。`build-sycl-a770-gr-runtime-35-20261029` 的组合测速准备及源变换/35门槛自检已完成，但因GR质量失败未启动。新 `build-sycl-a770-gr-hot-first-bad-20261029` 将在既有QSA诊断之后，复用其55阶段/专家计数对象，仅替换原GR评分两臂的prefill和expert诊断对象，保留原score_from main、V4、全部八条历史和758目标。它是定位版本，没有速度或采用结论。

21:22 +08:00原生核对：新GR诊断supervisor17668 / 创建毫秒1793280119992，worker5264 / 1793280120125，精确等待QSA worker12232 / 1793277827236。QSA本身正在等待另一会话的真实编译worker22920 / 1793279566584；没有绕过共享锁或终止它。新诊断CPU自检通过两臂可逆对象替换、原main保留、八条历史、九组758目标和第六评分请求定位。该版本改变诊断调度，若暂未复现也不能抵消已有失败。

21:32 +08:00续查：原生查询确认22920仍有新的cmake/icx/clang编译子进程，12232及5264均存活，尚无新模型启动；两项诊断脚本摘要与各自冻结state一致。实际失败基线的runtime_flags明确为 `STRATA_PROMPT_ATTN_XMX=0`、`STRATA_PREFILL_NATIVE_XMX=1`、`STRATA_VALUE_PACKED_BITS=4`：prompt attention未启用XMX，GEMM仍启用XMX。下一步应以55阶段观测选择attention回退、gate或输出投影的实际起点，不能根据单独查看XMX attention源码归因。静态查看未确定根因，本轮没有修改计算路径或产生新的测速结论。

21:36 +08:00：静态检查发现新GR诊断worker在计算queue.stdout摘要后，还会追加唯一的完成消息。这会使整文件摘要在退出后变化，但不影响原计算输入；冻结脚本未改动。新增 `check_gr_hot_trace_20261029.py` 在诊断结束后持共享锁重核全部来源/对象/二进制及原协议、评分位置和阶段记录；对这一个日志严格核对原摘要的完整前缀，以及与实际reproduced状态一致的唯一末行，再计算关闭日志的新摘要，不跳过文件或忽略任意尾部。CPU自检已通过LF/CRLF、前缀被改、额外尾部、重复完成行及伪造quality_passed拒绝。完整诊断尚未运行，本检查不代表模型或性能通过。

```powershell
python -X utf8 bench/results/a770/check_gr_hot_trace_20261029.py --self-check
# 仅在GR诊断worker实际终态后运行完整持锁审计：
python -X utf8 bench/results/a770/check_gr_hot_trace_20261029.py
```
