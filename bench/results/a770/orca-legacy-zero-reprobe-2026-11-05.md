# 原 Direct 8K 全零异常：原二进制有界复查

2026-11-05，Windows 10 22H2，Intel Arc A770 16 GB，驱动 32.0.101.9034，oneAPI 2025.3。

**两次新进程共 6/6 请求完整匹配，原间歇故障仍未定位。**
这不是修复、故障率估计、环容量归因或新的性能成绩。

此前同一 Direct 二进制先通过两次 Code2K，随后 Long8K 生成了 64 个 token ID 0，
草稿采纳／提供为 0/0；当时 L0 与诊断 Direct 通过，合计 8/9。
原失败的完整协议、stderr、配置及参考输出随本报告保留。

本次复用原 EXE、argv／env 和原 M4 数学 pack，只增加已有
`STRATA_VERIFY_ZERO_DIAG=1`。原 EXE 本身包含此诊断，无须重编译；
仅在一轮 verifier 全部输出 token 0 后，才额外回读 logits 与 final residual。
本轮没有触发该回读，也未启用逐层扫描或 NaN 填充。
启用 Host 条件本身仍可能扰动时序，不能视作原诊断关闭执行的完全重现。

| 新进程 | Code2K 第一次 | Code2K 第二次 | Long8K | 8K 首四 token | 8K 草稿 |
| --- | --- | --- | --- | --- | ---: |
| 0 | 匹配 | 匹配 | 匹配 | 760, 3766, 1414, 17855 | 32/44 |
| 1 | 匹配 | 匹配 | 匹配 | 760, 3766, 1414, 17855 | 32/44 |

每进程直接执行 2K→2K→8K，无 Short 预热或前缀复用；每次生成 64 token。
逐项核对完整 tokens、数量、草稿、结束条件及复用状态，两个进程均正常退出。
2K 参考首四 token 为 760, 3766, 1970, 41643，草稿为 38/44。
预先限定最多两个进程、首次不匹配即停止，每进程有 600 秒 watchdog；本次未触发。
准备目录 r1 在 GPU 执行前被替代；实际运行及原始结果为 r2，没有覆盖尝试。

固定实际 5,771 专家槽、reserve 1,536 MiB、read256、decay0.95、MTP4，
step／last min-p0.5、cumulative0、suffix0、cost-window0、prompt-cache0。
这是历史 M4 Direct，不是当前生产 U4／通用 M16，也没有测量 L0 对照或 PPL。

独立核对确认初始 expert-profile 与原失败记录一致；原 EXE 的 SHA-256 为
`ae2c8fad4de801ad34c1823ce503759aff94ad7121a7e3c7f55868f569fd895c`。
原数学目录 1,516 个文件的哈希全部匹配；本次 memory.zebin 哈希
`39e8a1a406575069a7324e7468ca1d9b55e2cc3e3b741ff8d49ecd3e5c36709c`
也与更早的 contexts-bounded-forward 历史记录一致。

运行与归档持共享实验锁，生产源码、EXE 和配置未由本次复查改变。
校验允许其它工作后续仅更新主启动 JSON；实验使用冻结 argv／env，不读取该 JSON。
准备在锁前完成，编译未发生。同条件复测需要相同设备、驱动、模型资产、初始 profile、
EXE、数学 pack、参数和请求历史；成功重试不保证重现间歇故障。
归档包含原失败及本轮协议、日志、输入、脚本和依赖哈希，不分发模型或 EXE。

```powershell
python -X utf8 build-orca-slm-async-20261104/legacy_zero_reprobe.py verify r2
python -X utf8 build-orca-slm-async-20261104/publish_legacy_zero_reprobe.py --check
```

重跑应使用新的准备目录，保留原有尝试；先 prepare 新名字，再 run 同一名字。
若再次出现全零，优先保留第一次 logits／residual 诊断，再定位前一正常阶段。
本轮没有新的失败诊断，不能进一步区分非法数值、陈旧数据或其它故障来源。
Linux GPU、Short 后端差距及原 8K 间歇异常均保持未解决。

[JSON](../../../sycl/orca/legacy-zero-reprobe-a770-windows-2026-11-05.json) ·
原始证据 (`legacy-zero-reprobe-a770-windows-2026-11-05.json.gz`, raw capture retained locally)
