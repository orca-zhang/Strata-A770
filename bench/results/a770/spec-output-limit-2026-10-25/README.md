# SYCL 输出尾部验证窗口（2026-10-24～25）

服务端和 CLI 在选择 MTP / suffix 窗口后，再将验证长度限制为剩余输出数。
例如只剩两个输出名额时，原来的四行窗口改为两行；不再验证、提交无法返回的额外行。
运行时仅两处长度限制和注释，无新依赖、内核、开关或采样精度变化；已合入源码默认路径。

## 验证

- [x] 冻结原有修改和 210 项文件/依赖；候选只重编 SYCL 入口，复用相同 MTP、FP32 sampler、verifier、头文件和库。
- [x] 1,302 项 CPU 接受边界检查通过，97 个超长窗口被截短；覆盖不同剩余输出数、全部拒绝/部分接受/全部接受及最后一个上下文位置。
- [x] 72 次服务端请求通过：阈值 0/0.50、1/2/3/4/6 token 输出、16-token 参考、固定种子采样与惩罚、取消与重试、parked/live 续写。
- [x] 两次服务端指定 EOS 检查通过；六次 CLI 的 2/4/6 token 检查通过。
- [x] 两次 CLI 恰好容纳提示和生成的七 token 上下文检查通过，生成六 token，与大上下文参考相同。此用例的原基线也通过，不记作已复现的上下文错误修复。
- [x] 所有非取消输出均与参考逐 token 相同；候选每个验证窗口都不超过当时剩余输出数。
- [x] 独立进程两轮 A/B/A/B/A，预定主测点六 token 在筛选与独立确认均通过 1% 门槛。
- [ ] FP16 KV、长输出和完整 Short / Code2K / Prose2K / Long8K 性能矩阵；CUDA/HIP 对应路径与非 eager 图模式未在本轮验收。

功能数据见 [checks/results.json](checks/results.json)，原始协议和 stderr 同目录。
例如阈值 0 的 16-token 基线末两个窗口只剩 2/1 个输出名额，仍各验证四行；候选分别改为两行和一行。

## 性能

A770 16GB、Xeon E5-2696 v4、Windows、oneAPI DPC++ 2025.3.2。
Short 为 23-token 提示，Q8 KV，`--spec 4 --spec-min-p 0.5`，默认 suffix lookup 为 3；内部验证容量为 6、MTP 上限为 4。
实际专家缓存固定为 **4191 slots / 5760 MiB**，请求预算为 4000；与此前 5771-slot 测量分开。
使用 `STRATA_IQ_MT_MIN=1`、coupled sampling 配置，`--adapt-swaps 0 --pcie-frac 0`。
GPU 串行运行，关闭 trace、状态哈希及性能分析器；四条 DeepInfer 实验路径均关闭。

每阶段五个独立进程 A/B/A/B/A，每进程先预热两次 16-token 请求，再对 4/6/16 token 各保留三次请求。
共十个计时进程、20 次预热、90 次保留请求；每测点使用三个 decode 毫秒值的中位数。
每个候选对照相邻两个基线的几何均值。下表范围为每阶段两个有效配对，吞吐按相同生成 token 数计算。

| Short 输出上限 | 筛选 decode 吞吐提升 | 独立确认 decode 吞吐提升 |
|---|---:|---:|
| 4 token | +7.59%～+8.08% | +7.24%～+7.81% |
| 6 token（预定主测点） | +11.34%～+11.67% | +11.77%～+12.01% |
| 16 token | +6.97%～+8.01% | +7.16%～+8.03% |

十二组 decode 配对全部满足基线漂移≤3%，最大绝对漂移为 **0.8973%**，全部为正收益。
六 token 的进程中位 decode 耗时：基线 **441.1～445.8 ms**，候选 **396.2～397.0 ms**。
全部保留请求输出相同、未复用提示。原始批次、预热、prompt/TTFT、接受/提议数及所有比较见 [measure/results.json](measure/results.json)。

结论限于本轮 **Short/Q8 的短输出 decode**；不声称 prefill、TTFT 或四场景整模型加速，不与其他优化收益相加或连乘。
源码与已验证候选逐字节相同；独立 EXE 位于 `build-sycl-window-limit-20261024/candidate/strata.exe`，SHA256 为
`232d47960540bdf77898a6bb002484b5dbc13796ccde99605271f47e90e1f094`。

## 失败记录和生产文件

首轮模型脚本未处理 Windows 的 CRLF，已生成 16 个正确基线 token 后因解析不到窗口日志而停止。
[checks-attempt1](checks-attempt1/) 保留原始日志和未完成状态；修复换行解析后完整运行上述矩阵，不将首轮当作通过。
最初 CPU 调用的 `-std=c++20/-O0` 被 clang-cl 警告忽略；[cpu-final.txt](cpu-final.txt) 使用正确的 `-Qstd:c++20 /Od` 再次通过同一检查。

排队期间另一会话把生产 `--short-read` 从 256 改成 1024，保护检查在运行模型前拦截了该变化。
[external-config-change.json](external-config-change.json) 与 [config-external.json](config-external.json) 保存变化及哈希。
本轮保留该外部修改，对照始终使用冻结的 [config.json](config.json)（256），没有写生产配置或替换生产 EXE。
生产 EXE 的 SHA256 仍为 `8f0d866ded297cf5a409e84cf5dd61f437c56c8a89aaeb2a85569265b6a3e122`。

[源码差异](window-limit.patch)、[冻结依赖](frozen.json)、[构建命令和哈希](build.json)、[交付核对](delivery.json) 均保留。
`check.py`、`measure.py` 是实跑脚本；`check-replay.py`、`measure-replay.py` 仅增加新输出目录入口，后者额外将 CPU 检查日志放入新目录。
复测入口改动只做语法检查，不更换已记录的样本；完整私有构建仍复用原冻结对象。

## 复核和复测

离线复核，无需模型、GPU或私有构建：

```powershell
python -X utf8 bench/results/a770/spec-output-limit-2026-10-25/check_results.py
```

最小 CPU 回归（先配置本机 C++ 编译环境，每次使用新目录）：

```powershell
python tools/test_spec_window_limit.py --cxx c++ --out build-window-limit-repeat
```

本机冻结引擎的 GPU 复测，使用共享锁，每次换新输出目录：

```powershell
python -X utf8 build-sycl-deepinfer-20261023/check_bf16_arena.py build-sycl-window-limit-20261024/check.py build-sycl-window-limit-20261024/checks-repeat
python -X utf8 build-sycl-deepinfer-20261023/check_bf16_arena.py build-sycl-window-limit-20261024/measure.py build-sycl-window-limit-20261024/measure-repeat
```
