# SYCL 恢复入口（2026-10-31）

本机 SYCL 恢复入口已固定到冻结的 M4／256-GRF 构建。运行 `run-iq2_xs-sycl.bat`，使用 `strata-iq2_xs-sycl.json`，监听 127.0.0.1:8080。默认 Orca 配置未由本次操作修改，现有服务未重启。旧 `.sycl-backup` 保留为已拒绝的历史档案，不作为恢复入口。

Windows 10 22H2、Intel Arc A770 16GB、驱动 32.0.101.9034。采用完整 EXE 和配置替换：请求 3000 专家槽，实际 3134 槽／4320 MiB；prefill 2048、context 8192、MTP4、min-p 0.5、预留 3072 MiB，PCIe fraction 0、adapt-swaps 0，不借用 prefill 缓存，suffix draft 关闭。GDN AB 为 xmx，完整环境见 JSON。没有新增内核代码，也未单独定位 NaN 根因。

EXE：`G:\GitHub\Strata\build-orca-native-20261029\current-backends-1793392341055522500\sycl-build\strata.exe`，SHA256 `369c21ff662d78babb4d053f7a6f1bed0930cd32c46fc03ecb855cdca3dcf83e`。冻结源码的 557 个文件哈希已核对；配置、EXE、脚本在评分与服务回归期间均未变化。原构建的 configure/build 均成功；该目录早期测速被取消导致顶层 complete=false，不把它写成完整验收。此前 72 请求仅覆盖 Short／2K，本次补充 8K。

质量以[冻结 Orca 参考及原始 PPL 门槛](orca-quality-policy-2026-10-31.md)为准：加权合计不超过 +1%，单场景不超过 +3%；条件 PPL 作诊断。每个场景独立冷进程，两轮各 7 场景、502 个固定 assistant 目标，共 1004 个有效目标。每轮全部评分 TSV 均与冻结参考逐字节一致；两轮合计原始与条件 PPL 变化均为 **0.000000%**。下表变化方向为本次 SYCL 相对冻结 Orca。

| 场景 | 每轮目标 | 原始 PPL 变化 | 条件 PPL 变化 |
|---|---:|---:|---:|
| short | 75 | +0.000000% | +0.000000% |
| code | 128 | +0.000000% | +0.000000% |
| gcd | 31 | +0.000000% | +0.000000% |
| memory | 6 | +0.000000% | +0.000000% |
| followup | 6 | +0.000000% | +0.000000% |
| prose | 128 | +0.000000% | +0.000000% |
| long | 128 | +0.000000% | +0.000000% |

实际服务器的 Chat、SSE、Anthropic Messages、Responses 四项 HTTP 检查通过。随后同一引擎连续执行 code、long、prose、followup、code、long；全部有效，重复 2K／8K 完整 token 列表一致。共生成 583 个 token，followup 正常结束于 7 个；进程退出码 0。服务回归全程 1 Hz 物理显存采样 580 次，最低空闲 **3762.17969 MiB**，高于 1024 MiB 门槛；该监控不覆盖两轮冷评分的所有进程。

没有采用只通过冷评分的候选：较新 FP16-pair／prefill2048 方案虽两轮 502 目标有效、合计原始 PPL +0.16624%，仍在相同混合历史中的第二次 8K 请求触发非零输出检查失败。较新默认 prefill、仅改 GDN AB 及 FP16-pair 全长 prefill 均未通过 8K；10-21 生产参数回退在首次 2K 请求超时，物理空闲最低 728.90625 MiB，也被拒绝。主动取消及参数无效的探针分别登记，不推断数值通过或失败。13 项早期探针、失败记录、配置和结果哈希均保留于 JSON。

这是本次固定语料和会话回归范围内的恢复验收；`quality_equivalence_established=false`。未测量本次恢复方案的速度，不回填旧测速数据，也不宣称纯运行时提速。旧 1022 目标上的 +6.94%／−7.41% 分歧及失败结论保持不变。

复跑时使用新输出目录，共享 GPU 锁会串行执行：

```powershell
python -X utf8 tools/check_sycl_recovery.py --config strata-iq2_xs-sycl.json --out build-sycl-quality-new --repeats 2
python -X utf8 tools/check_sycl_recovery_service.py --config strata-iq2_xs-sycl.json --out build-sycl-service-new
```

质量门槛与整请求统计的两项 CPU 回归检查已通过。启动入口为本机生成文件；复核所需的完整配置、逐目标分数、原始日志、物理显存样本、检查脚本和来源哈希见[恢复 JSON](sycl-recovery-2026-10-31.json)。
