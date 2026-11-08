# 当前 U4 decode 缓存分块工作量评分对照

2026-11-05，Windows，Intel Arc A770 16 GB，单插槽 Xeon E5-2696 v4；驱动 `32.0.101.9034`，oneAPI 2025.3.2。

沿用 U4N16/min16、9402 个 U4 专家、5771 实际缓存槽、M16 投影、MTP4/min-p0.5、read256、adapt32/decay0.95、prompt-cache0。
只重编冻结 Verifier Host TU；数学 images、主程序和 prefill 对象、运行库均使用原链接组合。正式计时使用同一重编 EXE 切换 `STRATA_VERIFY_CACHE_WORK=0/1`，未修改生产 EXE/配置。

候选将逐路由条目加 1 改为每专家本窗口的分块工作量：IQ2_S GU 和 Q2_0 Down 按 4/2/1 分块，IQ2_XXS GU 逐条，IQ1_M GU 按 2/1 分块。
分数为 `(GU字节×GU遍数 + Down字节×Down遍数)/blob字节`；保留原换入阈值、衰减、成本权重和 prefill-tail。仅限审计过的原生格式/2560×640/单组；默认关闭。逻辑工作量不等于物理 PCIe 流量。

CPU 检查覆盖三格式、n=0..40、重复计数、无效 ID、专家 511、已有热度。每个启用进程的首次计分日志确认全部 48 层符合条件。
历史固定路由回放仅作参考：它缺少 prefill-tail、异步换入、float32 热度舍入、C++ 同分顺序和路由重算，不能证明当前 U4 的吞吐。

正式顺序关→开→关；每进程先 Short23/16，再三轮交错 Code/Prose2K/full128。丢 cycle0，取 cycle1/2 中位数，控制取外侧几何均值。
门槛：每场景 decode 吞吐至少 +1%，外侧相同策略耗时漂移不超过 3%，完整 token/协议及 MTP 计数均匹配。通过才运行独立反序。所有预热和计时样本均保留，无 profiler。

| 顺序/场景 | 关 decode ms | 开 decode ms | 吞吐变化 | 同策略耗时漂移 |
| --- | ---: | ---: | ---: | ---: |
| 正序/Code2K（漂移超限，排除） | 4270.829 | 4233.000 | +0.894% | -3.147% |
| 正序/Prose2K | 4322.049 | 4267.400 | +1.281% | -2.999% |

共 21 次完整请求通过 token、生成数、accepted/offered、finish、reuse/resume 和协议检查。READY 空闲显存 MiB：[2497, 2375, 2497]；未采运行期显存、时钟或功耗。

未通过采用门槛，未运行反序，未采用；本轮没有确认新的稳定 decode 提速。
生产配置、U4N16 数据入口、原生回退配置与 learned profile 保留。未新增 PPL 或 Linux GPU 结论。编译、GPU 和重型资源哈希均持共享实验锁。归档仅含文本，不包含模型、可执行文件、对象、SPIR-V 或二进制张量。

```powershell
python -X utf8 build-orca-cache-work-20261105/run.py check forward-r1
python -X utf8 build-orca-cache-work-20261105/publish.py --check
```

[精确 JSON](../../../sycl/orca/decode-cache-work-a770-windows-2026-11-05.json) · [文本证据归档](../../../sycl/orca/decode-cache-work-a770-windows-2026-11-05.json.gz)
