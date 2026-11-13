# Orca：复用已完成的 HC1 Q8 输入

2026-11-05（UTC+8），Windows 10 22H2，Intel Arc A770 16 GB，驱动 `32.0.101.9034`，oneAPI 2025.3。
本轮保留原运行库、数学 pack、模型与采样策略，每个后端仅重建一个 Verifier Host 编译单元。
候选在 HC1 已融合生成 FFN Q8 时，仅调用已有 FP32→BF16 转换，避免再次生成 Q8＋BF16 双输出。
两个原调用点使用同一守卫：`q8_ffn && G == 1 && tb == 0`；其余情况保留原量化。
这不减少 kernel 数，不修改共享分支的独立 Q8、QDEDUP、ABI 或数学内核。
拆组情况下 HC1 的局部行偏移还需单独核查，故不启用复用。

开关为 `STRATA_VERIFY_REUSE_FFN_Q8=0/1`，候选默认关闭。初始化时解析一次；非 0/1 值报错。
HC1 独立回放先通过 CPU 参考、融合 Q8 对原生量化以及完整回放／guard 校验，
见 [HC1 回放](orca-direct-hc1-replay-2026-11-05.md)。
输入、参考、源码、配置与命令在等待设备锁前准备；编译、GPU 工作、重型哈希和归档持共享实验锁。

Direct 关→开→关，再 L0 关→开→关。每个新进程执行 15 次相同 Short28 请求，丢弃前十次，保留五次。
共 90 次请求逐个比较完整 token、生成数、草稿采纳／提出数、复用数、恢复数与停止原因，原始协议也独立解析。
固定 5771 个实际专家槽、1536 MiB 预留、MTP4、step/last min-p=0.5、累计门槛 0、
short-read=256、adapt-decay=0.95、无 prompt 复用。图在 READY 前预热。
不插 GPU 时间戳或内层计时探针；decode 使用引擎内部 decode_ms，排除加载、初始化、准备和预填充，TTFT 单列。
两侧关状态的进程中位数取几何均值作为对照；漂移为末次关除以首次关减一。
控制漂移不是置信区间，五个保留样本和一次后端顺序尚不能证明跨日稳定性。

完整协议质量检查：**通过**。

| 后端 | 指标 | 关状态对照 | 开状态 | 变化 | 关状态漂移 |
| --- | --- | ---: | ---: | ---: | ---: |
| direct | Decode tok/s | 58.457 | 57.971 | -0.831% | +0.000% |
| direct | 预填充 ms | 890.947 | 891.900 | +0.107% | +0.484% |
| direct | Decode ms | 1300.100 | 1311.000 | +0.838% | +0.000% |
| direct | TTFT ms | 928.810 | 930.265 | +0.157% | +0.516% |
| l0 | Decode tok/s | 59.566 | 59.838 | +0.457% | +0.157% |
| l0 | 预填充 ms | 892.400 | 888.200 | -0.471% | +0.022% |
| l0 | Decode ms | 1275.900 | 1270.100 | -0.455% | -0.157% |
| l0 | TTFT ms | 927.532 | 923.254 | -0.461% | -0.017% |

仅发布该冻结窗口，不将本轮结果称为正式采用或新的长期成绩；源码默认与部署 EXE 未修改。
没有本候选的长上下文、PPL 或 Linux GPU 结果，也没有关闭已有 Direct 8K 间歇故障。
初版 r1 的准备阶段曾错误假设汇总含 request_ms，在编译／GPU 工作前发现；
失败快照与修正后的全新 r2 一并归档，未覆盖失败目录。

```powershell
python -X utf8 build-orca-slm-async-20261104/ffn_q8_reuse_prepare.py --check
python -X utf8 build-orca-slm-async-20261104/ffn_q8_reuse_model.py self-check
python -X utf8 build-orca-slm-async-20261104/ffn_q8_reuse_model.py verify r2
python -X utf8 build-orca-slm-async-20261104/publish_ffn_q8_reuse.py --check
```

新运行先 prepare r3 再 run r3，使用不存在的目录、同硬件／驱动／依赖／环境，不覆盖旧证据。
归档保留完整源码、可逆 diff、编译／链接命令、配置、原始协议、全部请求和结果哈希，不分发模型或 EXE。
精确 JSON (`ffn-q8-reuse-a770-windows-2026-11-05.json`, raw capture retained locally) ·
原始证据 (`ffn-q8-reuse-a770-windows-2026-11-05.json.gz`, raw capture retained locally)
