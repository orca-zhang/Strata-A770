# Q2_0 shared-down：扩展已有专家并发链

2026-11-07，Windows 10 / Arc A770 16GB，驱动 32.0.101.9034，Xeon E5-2696 v4，oneAPI 2025.3.2。
本轮是 frozen full-current / HC128 / MTP4 / spec-min-p=0.5 / 5771 实际专家槽的同条件诊断。
Orca L0 仍为推荐后端，Direct 为 WIP；不与当前另一个 spec6／校准 MTP 会话混用数据。

上一轮实际 48 层分支记录中，6、7、11、22、30 层 shared-down 为 Q2_0（type 42），被现有 full-fork 白名单排除。
这次仅放开 Verifier 和 shared-expert phase5 的两处类型检查，保留其他 shape、权重类型、scratch 和阶段屏障条件。
审计发现 Q2_0 在 T1～4 均只有一次 kernel launch，使用传入的权重／Q8／输出指针，无附加全局 scratch 或分配。
shared Q8 仍在 projections join 后才被 SwiGLU 改写，shared/routed down 写不同输出。
基线与扩展版都重编译同样两个 Host 文件，复用冻结数学包、Host 库和 Direct 驻留修复；无数值内核重编译。

先用真实第 6 层 IQ4_XS gate/up、Q2_0 down、BF16 scalar 权重校验。
L0 与 Direct 各通过 1,152 次完整输入／输出／scratch／guard 逐位比较，共 2,304 次；检查 2752 个 GPU 尾部 epoch。
覆盖两种 routed 格式、T1～8、量化数据已就绪／未就绪、六组变化路由含空组，以及三种阶段方案和重复回放。
T>4 保留原准入限制；这不是把完整并发链扩展到 T8。
这些比较验证调度等价，不能当作独立精度或 PPL 测试。

完整模型固定 8 臂：L0 → base → q2fork → base → q2fork → base → q2fork → L0。
每臂新进程 15 次 Short，去前 10、留后 5；共 120 次完整协议匹配，含每次 70 tokens、草稿采纳／提供、stop、reuse/resume。
中间两组对照分别为 base/q2fork/base、q2fork/base/q2fork，两端中位数取几何均值。
decode 使用引擎 decode_ms，排除准备、初始化和 prefill；TTFT 与整请求分别保留。
无 GPU timestamps/profiler，不删除漂移或负收益样本，不补跑筛样本。

| 对照 | base decode tok/s | q2fork decode tok/s | decode 变化 | TTFT 耗时变化 | 整请求耗时变化 | 控制 decode 漂移 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| base → q2fork → base | 48.660409 | 49.508452 | +1.743% | -0.282% | -1.283% | -0.700% |
| q2fork → base → q2fork | 48.817909 | 49.233401 | +0.851% | -0.436% | -0.155% | -0.239% |

外层 L0 decode 为 50.190030 tok/s，首尾漂移 -0.201%。
所有保留样本和每臂数据见 JSON 与原始协议。
预先要求两顺序 decode 为正、控制漂移不超过 3%、TTFT/整请求回退不超过 3%；
发布审计额外要求两顺序 decode 增益均超过对应控制漂移。当前门槛：`{"workload_exact": true, "decode_positive_both_orders": true, "controls_within_3pct": true, "ttft_request_regression_within_3pct": true, "outer_l0_within_3pct": true, "decode_gain_exceeds_control_drift_both_orders": true}`。
本实验不自动修改生产默认、部署 EXE 或 README 主成绩；实际采用需单独验证适用后端和当前源文件。
没有新的长上下文、PPL 或 Linux GPU 数据；历史 Direct nonfinite/驻留问题没有因此关闭。

初版构建在等待锁、尚未生成构建产物时主动取消：冻结 manifest 没有 generated cpp 哈希项。
修正版从固定原始源和固定 prepare 脚本重新生成文本核对，再记录实际编译输入；初版脚本保留。
没有丢弃或重跑性能样本。私有权重 fixture 不进入公开归档，公开只含哈希、脚本、命令、源／头文件 pins 与原始文本日志。

```powershell
python -X utf8 build-orca-q2-fork-20261107/experiment.py selfcheck
python -X utf8 build-orca-q2-fork-20261107/report.py check
```

复测需使用相同硬件／驱动、工具链、完整模型资产、输入历史、配置与 HC128 数学包，在新实验输出目录依次运行 `build`、`checks`、`run`。
原输出目录不可覆盖；公开归档不包含模型权重、fixture、EXE、OBJ、LIB。

[前一轮分支捕获](orca-direct-hc-router-2026-11-07.md) · [JSON](../../../sycl/orca/direct-q2-fork-a770-windows-2026-11-07.json) · 文本证据 (`direct-q2-fork-a770-windows-2026-11-07.json.gz`, raw capture retained locally)
