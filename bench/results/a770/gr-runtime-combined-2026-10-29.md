# A770：GR 固定索引与 runtime append 的组合实测

目标仍是Windows完整decode吞吐至少提升20%，通过后继续Linux。单组件的约3%收益不能相乘作为完成证据。本批复用已链接EXE，只测GR固定索引和runtime append的组合，不改变原V4、权重、6297槽、tail64、min-p0.5、输入或草稿策略。

## 当前状态

17:56启动条件worker PID18320、原生创建时间1793267814166ms，绑定GR质量22392/1793266788763的实际Windows进程对象等待。启动脚本摘要和依赖在 `build-sycl-a770-gr-runtime-combined-20261029/launch.json`，当前没有本组合的计时或质量结果。

准备期间重新查询发现runtime服务器profile进程4768已结束，原 `build-sycl-a770-runtime-profile-20261029/results.json` 为 `complete=true, passed=true, exit_code=0`，独立配置已经存在。随后实际运行原 `run.py --check`，exit0，重读真实服务器输出、配置和来源通过；未重启服务器或重复冷/热质量。组合任务要求GR冷/热质量和runtime冷/热、服务器profile都通过；获锁后再次调用两项原验收入口，重新检查实际输出、配置、评分、guard及来源，而非只读通过布尔值。前置未通过则记录跳过，不运行组合模型。

## 同一负载上的直接对照

| 臂 | EXE | 运行时 | 共同负载 |
| --- | --- | --- | --- |
| A | 原device-counter基线 | 默认 | Short16；Code/Prose各2048输入、完整128输出；各一次预热、交错保留三次 |
| B | 已链接的GR固定索引EXE | 已测的legacy adapter、immediate/in-order/counter/append组合 | 与A相同 |

复用原完整六进程 `A1-B1-A2 / A3-B2-A4` 收集器，保留全部输出、实际退出、每进程8×48层扫描、tail64、源文件/EXE/DLL摘要及逐请求显存/频率覆盖。两个EXE和每臂runtime参数分别记入结果。计时使用共享GPU/编译lease和原编译启动监视器，1Hz监控只豁免自身精确进程。无需编译或链接。

原单场景≥1%、相邻基线漂移≤3%仍只控制是否做独立确认。额外的 `windows20_speed_scenes` 仅在同一场景两轮新配对都≥20%、漂移都≤3%、全部采样空闲≥1024MiB时写入；不会从旧组件数字计算。即使速度达到20%，组合质量、实际配置及Linux仍须分别完成，不把测速器的完成标记当作最终目标完成。

CPU自检已通过可逆的最小收集器修改、双EXE及精确runtime设置、V4保持，以及19.99%/缺确认/漂移/显存拒绝检查。自检和排队不构成组合性能结论。

```powershell
python -X utf8 build-sycl-a770-gr-runtime-combined-20261029/run.py --self-check
# 仅在该条件任务终态后离线核对：
python -X utf8 build-sycl-a770-gr-runtime-combined-20261029/run.py --check
```

## 等待期间排除的机械推广

重新读取既有 `build-sycl-a770-igc-spill-native-20261029/recovery/baseline/` 的六个精确metadata：GU16、GDN step16、GDN ab、IQ4XS NC3/NC4、MMVQ21 NC4都没有 `private_size`、`private_space` 或 `private_base_stateless` 项。原spill仍分别由既有资源结果记录；未报告private不是实际流量为零的证明，但这些模块不提供GR大参数副本同类问题的证据，因此不为它们盲目排固定索引候选。

另读已核对的GU索引对照helper，metadata同样没有上述private项；`gu_index_256`的107条静态UGM消息含93条 `ca.ca` 读与14条 `wb.wb` 写。该helper不是当前模型原生代码身份的替代证明，也没有动态消息权重；不据此宣称缓存命中率、PCIe带宽、可删除耗时或再排全局cache覆盖试验。原文件与此前负结果均保留。
