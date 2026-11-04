# A770 原生 CLI 短提示读取（2026-10-24）

原生 SYCL CLI 现在与服务端一样，用现有 `--short-read` 选择短提示的验证窗口读取，避免为很短的提示启动完整批量 prefill。单 token 提示沿用已有入口；`--short-read 0`、超过阈值或部分 `--prefill-until` 继续走原路径。没有新增内核、依赖、实验开关或 API。

- [x] 冻结原工作区、基线、工具链和依赖；只重编 SYCL generator。MTP/采样器/验证器对象与全部库不变；头文件仅叠加 MTP 未加载队列初始化的一行修复，其余头文件共用冻结版本。
- [x] 每个提示窗口关闭 head sampling、提交全部提示 token，按顺序填入 MTP KV，等待最后提交后处理最后一个提示 token。prefill 计时与 decode 阶段计数分开。
- [x] 456 项 CPU 检查：原有两种后端入口 216 项，新路由谓词 240 项，包含关闭窗口、阈值边界、非原生 pack 与部分 prefill。
- [x] 11 次真实 CLI、11 次服务端请求通过。1/2/7/8/23-token 提示、6-token 窗口及跨窗口、阈值 0/0.50、固定种子采样、自然 EOS、指定首 token EOS、关闭 MTP 和批量回退；对应输出逐 token 相同。
- [x] 修复未加载 MTP 时的队列所有权：初始队列设为空，`load()` 才创建自有队列。旧基线在正确输出单 token 后异常退出（Windows `0xC0000409`），初版候选无 MTP 两 token 场景在正确输出后返回 `0xC0000005`；最终候选正常退出。
- [x] 独立进程 A/B/A/B/A，两个 token 输入 `271,91`、最多 6 个输出 token；相同参数和 Q8 KV，实际缓存固定为 5771 slots / 7.74 GiB（服务 INFO 7922 MiB）。性能分析和诊断日志关闭；两条路径此前均已通过独立进程预热，正式记录各 CLI 进程的首次提示处理。
- [x] 原始输出、构建命令、源码/构建哈希和可离线审计脚本归档。生产 EXE 和配置哈希不变。
- [ ] 原生批量长提示和 Short/Code2K/Prose2K/Long8K 的完整整模型矩阵仍待完成。本轮没有修改 CUDA/HIP 入口，也没有为它声明验证或收益。

| 阶段 | 基线范围 | 候选范围 |
|---|---:|---:|
| prefill (ms) | 57706.9–58236.4 | 175.8–179.5 |
| TTFT (ms) | 57869.8–58394.6 | 281.2–281.9 |
| 6 token decode (ms) | 614.6–618.8 | 668.3–674.2 |
| 进程总耗时 (s) | 88.0–88.8 | 30.6–30.6 |

两组相邻基线对照的 TTFT 均通过 1% 收益及 3% 漂移门槛；延迟下降 99.52%, 99.52%，相邻基线漂移 +0.91%, -0.81%。

6-token decode 耗时变化为 +8.01%, +9.32%（正数表示变慢）；保留全部数据，未声明 decode 加速。这里比较的是提示读取路径造成的后续短输出阶段变化，并非新 decode 内核的算子测量。

这些数字只代表上述 **两 token 原生 CLI 提示的 prefill/TTFT**。模型、缓存和输入相同，但选择的提示读取路径不同；不能当作服务端 decode 或四场景整模型吞吐提升。decode 单独保留，不凭这组短输出时延宣布 decode 加速。进程总耗时还包含模型与约 33 GiB 主机镜像的加载；TTFT 是引擎原有会话计时口径，不包含整个进程的加载。

诊断中的旧基线以 58938.6 ms 完成一个 conditioning token，输出 `42`，并非永久死锁。失败记录也保留：阶段同步 runner 错误要求首次诊断超时，因此没有启动引擎；首轮功能脚本把边界提示的正常 EOS 当成“未达到 6 token”；无 MTP 的退出崩溃；退出复现脚本首次只接受 access violation，后来按保留的 fast-fail 实际退出码校正。另一个已启动的旧候选检查在队列修复前被中止，进程和锁所有权检查也归档。首轮计时还出现缓存 tuple/list 比较误判，基线本身正确完成；修正后在新目录完整重跑五轮，未把该首轮拼入最终序列。失败及中止尝试不计入通过数量。

基线 EXE SHA256：`db7c79f96d351cb6ade0d57e6051309d8e41ca75caefab9c4f85cc2652cfe452`。
候选 EXE SHA256：`6baa74cb88b73e2b82d0b8d4ad2a2ef46d6941ba23a6b13c195e9e5f0063ef8c`。

## 复测

离线检查无需 GPU：

```powershell
python -X utf8 bench/results/a770/cli-prompt-windows-2026-10-24/check_results.py
```

真实模型检查及计时使用保留的独立构建；每次指定新目录，GPU 通过已有共享锁串行运行：

```powershell
python -X utf8 build-sycl-deepinfer-20261023/check_bf16_arena.py build-sycl-cli-prefill-final-20261024/check.py build-sycl-cli-prefill-final-20261024/checks-repeat
python -X utf8 build-sycl-deepinfer-20261023/check_bf16_arena.py build-sycl-cli-prefill-final-20261024/measure.py build-sycl-cli-prefill-final-20261024/measure-repeat
```

`runners/` 保存本轮脚本来源，运行入口仍位于上面的独立构建目录。`build.json` 给出同一工具链的精确编译/链接命令，`frozen.json` 给出所有复用依赖的哈希。[功能结果](results.json)、[计时和漂移](measurements.json)、[源码差异](cli-prefill.patch)、[初始诊断](diagnosis/results.json)。
