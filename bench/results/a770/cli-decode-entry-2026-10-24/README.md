# CLI 单 token 解码入口回归（2026-10-24）

原生 IQ pack 的单 token 提示从位置 0 进入验证器；原有 `spec_pos=0` 同时表示“未启用”，
随后 `spec_pos>0` 跳过整个解码，正常退出却输出 0 token。
SYCL 与 CUDA/HIP 共用入口各改两行：未启用值改为 -1，合法位置接受 0。
没有改变采样精度、设备数学、实验开关、生产 EXE 或配置。

- [x] 冻结原有修改、源码与构建哈希；复用上一轮干净候选作为基线，仅编译一个新 SYCL 入口，所有其他对象、头文件及库相同。
- [x] 直接抽取两种入口的初始化、native/普通交接及真实 guard，编译执行：修改前/后共 432 项 CPU 检查。覆盖禁用推测解码、位置 0/7、输出上限 1/4/6、达到及超过上限、EOS。
- [x] 11 次真实 CLI 调用：基线复现 exit 0 / 0 输出；候选在阈值 0/0.50 下各覆盖 greedy 上限 1/4/6、固定种子采样及首 token EOS。每次候选输出与同输入服务端参考逐 token 一致。
- [x] 16 次服务端请求：基线/候选、阈值 0/0.50、单 token 与原有 Short 提示、greedy/固定种子采样，Short 包含重复/频次/存在惩罚；对应输出一致。
- [x] CLI 实际缓存 5771 slots / 7.74 GiB，服务端精确容量见 `runtime_info`，Q8 KV；关闭自适应换入与 PCIe 专家分流，专家 CPU 计算固定为可复现配置。
- [x] 原始 stdout/stderr/protocol、提取的 CPU 源码、编译日志、参数和哈希已归档；生产文件哈希与冻结时一致。
- [ ] CUDA/HIP 完整引擎及实机运行：本轮只有抽取真实控制条件的 CPU 检查。
- [ ] CLI 多 token 原生批量 prefill、取消及会话恢复：本轮不覆盖，前述未完成的 prefill 不计为通过。
- [ ] 首轮即结束时，已有 MTP 摘要在 `rounds=0` 下显示 NaN 的 `ms/round`；生成 token 正常，这个诊断字段留待修正，不用于性能计算。

这次修复恢复缺失的输出，没有测量或宣称整模型提速。日志中的耗时只用于回归溯源，
开启了窗口 trace，不应用于性能比较。整模型采用门槛仍为 1%，算子正收益可保留。
数据见 [results.json](results.json)，实际差异见 [cli-entry.patch](cli-entry.patch)。

离线审计（不使用 GPU）：

```powershell
python -X utf8 bench/results/a770/cli-decode-entry-2026-10-24/check_results.py
```

当前入口 CPU 回归（在已配置的 C++ 编译器环境中，`--cxx` 指向编译器）：

```powershell
python tools/test_spec_decode_entry.py --cxx c++ --out build-entry-check-repeat
```

在保留的私有构建目录重新执行全部回归，每次指定新结果目录；使用共享锁串行执行：

```powershell
python -X utf8 build-sycl-deepinfer-20261023/check_bf16_arena.py build-sycl-cli-entry-20261024/check.py build-sycl-cli-entry-20261024/checks-repeat
```

归档的 `prepare.py`、`build.py`、`check.py` 为实际执行脚本副本，其路径按私有构建目录解析。
独立候选为 `build-sycl-cli-entry-20261024/candidate/strata.exe`，基线为
`build-sycl-mtp-terminal-20261024/candidate/strata.exe`，构建命令及依赖校验见 [build.json](build.json) 和 [frozen.json](frozen.json)。
未替换正在使用的 EXE 或配置。
