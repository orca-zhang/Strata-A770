# A770：五项组合中的 GU/Q8 只在 T1 融合

此前 chunk2 合成检查中，混合主存/显存的 T1/T2 有约1%–2%信号，但纯显存路径出现约2.4%–8.3%回退，T2 更明显。本次检查缩小适用范围能否改善已采用的五项组合。历史算子信号不作为本轮整模型收益。

两臂均开启五项原开关，以 `build-sycl-a770-small-stack-v3-20261030/bin/strata-unified-all-operators.exe` 为精确基线。候选只替换原冻结 `build-sycl-a770-gu-q8-export-20261027/fusion.obj`：原 IQ2_S/Q2_0 融合条件 `cap_groups<=20` 改为 `cap_groups<=10`，即保留 T1，T2 回到原未融合路径。新开关为 `STRATA_STACK_GU_Q8_T1_ONLY=1`；关闭时仍是原20上限。其余形状/格式回退、kernel 计算、SG/WG、量化、Down 及其他对象均保持原状。

CPU 自检通过：源码反向替换完全恢复原文、链接输入仅差一个对象、两臂原五开关全开、基线 EXE 与完整采集器正确。沿用未改 kernel 的既有数值检查，不声称完成了新的算子实验。编译沿用原 `compile-fusion` 命令，只替换输出目录；等共享专家三种模式结束后，再取得同一 GPU/编译锁。

测量保持6297槽、tail64、V4、min-p0.5、graph、batch关闭，Short16 后执行原 Code/Prose2048输入及完整128输出的交错历史。初筛 A1/B1/A2；同场景至少1%、相邻基线耗时漂移至多3%，才执行独立 A3/B2/A4。绝对变化小于0.3%按噪声解释。检查完整输出、原始协议、8×48层扫描、实际退出、编译监视、来源摘要和请求内显存/频率采样；最低空闲门槛1024MiB。额外核对候选 `cap=20 active=0`、基线 `cap=20 active=1`，两臂原 `cap=10 active=1` 见证保留。

对象编译和模型链接已完成，`build.json complete=true`。候选日志确认 `cap=10 active=1`、`cap=20 active=0`，前置与后置基线均保留原融合路径；初筛三进程全部正常exit0，完整输出检查通过。

| 场景 | 初筛配对吞吐变化 | 初筛基线耗时漂移 | 独立配对吞吐变化 | 独立基线耗时漂移 |
| --- | ---: | ---: | ---: | ---: |
| Code2K | −1.599670% | −0.277237% | −1.247208% | +2.357097% |
| Prose2K | +1.827912% | −1.694845% | −1.254289% | +2.283485% |

Prose 首轮达到初筛门槛，但 A3/B2/A4 未重复收益；Code 两轮均回退。六个模型进程全部正常exit0，48条长请求与6条Short16共6240个输出token匹配参考，原始协议、层扫描、固定槽位、全部原开关与新分派见证、请求内遥测、来源摘要均通过采集器核对。最低采样空闲1358.742188MiB，显存检查通过；编译监视器完整结束、零事件。

`state.json complete=true`，`passes_screen=true`、`passes_confirmation=false`、`confirmed_scenes=[]`、`sampled_memory_passed=true`。最终文件复核直接执行与 `run.py --check` 相同的检查体，通过完整采集器、编译监视与全部摘要核对，结果保存于 `file-audit.json`。文件复核没有编译或提交GPU任务，略去外层GPU锁；自己尚未持锁的重复CLI等待进程已取消，没有中断其他实验。原CLI入口仍可复核完整结果。

性能未确认，因此没有追加固定目标评分或服务器验收，没有新增采用；原已采用配置和默认配置保持原状。

入口与最终复核：

```powershell
python -X utf8 build-sycl-a770-small-stack-gu-t1-20261030/run.py --self-check
# 六进程已完整结束；最终复核：
python -X utf8 build-sycl-a770-small-stack-gu-t1-20261030/run.py --check
```
