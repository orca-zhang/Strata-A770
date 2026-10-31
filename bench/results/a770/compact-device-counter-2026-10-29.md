# A770：compact GEMV 与 device counter 的新模型对照

本轮继续优化 decode/GEMV，按单场景独立确认。私有目录为 `build-sycl-a770-compact-device-counter-20261029/`。模型重链接、完整质量、六进程性能实验及全部离线核对已完成。初筛Code2K +1.251%没有在独立回合复现：确认Code2K +0.041%、Prose2K +0.158%，均低于本轮0.2%噪声线。没有同一场景连续两轮达到1%，候选未采用，维护源码、生产EXE及配置未改。

候选沿用[已独立确认的 compact IQ4_XS 原生对象](iq4xs-compact-sg16-2026-10-28.md)，仅覆盖输出头1–6列、QKV 5/6列、SSM 4列这九种组合。本轮不重编译 kernel，保留原验证器及其他模型对象，未启用专家裁剪。原版为 `build-sycl-a770-down-device-counter-20261028/baseline/`，两侧使用相同的新 device-USM 扫描计数器、原扫描 kernel、512上限和失败时CPU核对。

重链接脚本只改变私有输出路径、map路径，以及旧扫描对象到新扫描对象的引用；候选原生对象仍为原冻结文件。CPU 自检验证这些替换可逆、分派恰为九种已确认组合，并继承扫描器已完成的边界检查。本轮会校验输入哈希和链接公共符号，避免把未确认的对象带入模型。

流水线先运行 A1/B1/B2/A2 四个新进程的九场景、758固定目标质量检查。保留原协议、必需概率有限检查和6297实际缓存槽、至少1024MiB READY余量；新增的严格要求是全部四进程的评分和Short16输出均精确匹配冻结原版。候选质量不精确时，记录失败并停止测速，不套用裁剪候选的质量误差预算。

本轮实际四进程均exit0，所有必需概率有限，九场景合计3032个目标评分全部精确匹配冻结原版，所有场景及汇总PPL变化均为0。实际缓存均6297槽，READY空闲分别为A1 2080、B1 2037、B2 1964、A2 1958MiB。结果与来源哈希保存在 `quality/results.json`，本组质量耗时不用于速度比较。这证明当前组合通过该固定目标检查，未证明旧间歇NaN根因已修复，也不替代完整热历史和独立性能验收。

质量通过后运行原始 Code/Prose 2048输入、完整128输出：每正式进程Short16，然后每场景一次预热和三轮交错保留请求，所有完整输出包括预热均匹配冻结原版。A1/B1/A2中任一场景吞吐提升至少1%、相邻基线耗时漂移绝对值不超过3%，才启动独立A3/B2/A4。只有同一场景在两轮均通过才确认，另一场景的回退完整保留且不作联合否决。绝对吞吐变化小于0.2%按噪声解释。频率遥测、每条长请求的48层扫描、分派、输入来源及实际子进程退出均检查；完整热历史质量仍是采用前的后续工作。

实际A1/B1/A2初筛中，Code2K吞吐变化为+1.251%，相邻基线耗时漂移−2.011%；Prose2K为+0.253%，漂移−2.552%，未达到1%门槛。按预设规则完成独立A3/B2/A4后，Code2K仅+0.041%，漂移−0.165%；Prose2K +0.158%，漂移−0.391%。两项独立变化均按噪声解释，不挑选初筛数字作为收益，也不与其他候选组的时间混用。

六个性能进程全部exit0，实际均6297槽，READY余量为1957–2086MiB。全部48条长请求（包含预热）及六条Short16共6240个完整输出token匹配不可变参考，每条长请求的48层扫描、实际分派、遥测覆盖与全部来源哈希核对通过。`confirmed_scenes=[]`，`passes_confirmation=false`，`speed_target35=false`。根流水线exit0表示实验和检查完整结束，不表示性能通过。固定目标质量通过也不构成旧NaN根因已修复的证明。

本轮graph、FP16 KV/V packed4、实验min-p0.5、HC BF16、专家链ON/batchOFF、U4 OFF等条件沿用冻结协议。生产配置未改。旧compact模型在最后Prose prefill中失败、缺少末端基线的记录原样保留，不拿旧时间参与本轮验收；新扫描器也未被认定为旧NaN的根因修复。

```powershell
python -X utf8 build-sycl-a770-compact-device-counter-20261029/run.py --self-check
```

权威结果为本轮目录的 `build.json`、`quality/results.json`、`performance/results.json` 和 `validation.json`；完成后已运行 `run.py --check`，完整通过链接、固定目标评分、全部性能请求、容量、扫描、分派、遥测与来源检查。失败的性能采用门槛完整保留。

条件执行的 `hot-history.py` 已取得共享锁并正常exit0，实际记录 `skipped=true`、`passed=false`：独立速度确认未通过，因此没有发射模型请求。结束后的离线 `--check` 通过；这不是热历史质量通过。原定检查使用四个新进程、测速的 `prompt-cache0/short-read1024` 和Short16、Code/Prose预热及三轮完整请求顺序；前六条长请求不加诊断，最后两条使用引擎现有的逐请求 `logprobs=1`。概率读取会同步队列，因此原定检查不用于速度比较。

```powershell
python -X utf8 build-sycl-a770-compact-device-counter-20261029/hot-history.py --self-check
python -X utf8 build-sycl-a770-compact-device-counter-20261029/hot-history.py --check
python -X utf8 build-sycl-a770-compact-device-counter-20261029/run.py --check
```
