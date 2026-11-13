# 当前 U4/min16 的 decode 专家链诊断与三条复用对照

2026-11-05，Windows，Intel Arc A770 16 GB、Xeon E5-2696 v4，驱动 `32.0.101.9034`，oneAPI 2025.3.2。
固定生产 U4N16/min16、M16 投影、9402 个 U4 专家、5771 实际缓存槽、1536 MiB 预留、MTP4/min-p0.5、read256、decay0.95、prompt-cache0。

先使用主 EXE 的旧阶段 profiler，五次完整输出与草稿计数通过。源码核对发现该 profiler 会关闭独立投影及共享/路由交错；旧 `VRAM hits` 区间实际执行显存与固定页主存混合权重。Host PCIe 计数为零不能解释为没有 PCIe 读取，旧插桩速度不用于性能比较。

随后仅重编冻结 Verifier Host TU，保持 `prof_on_=false` 和正常独立调度；使用已有 grouped phase1/2/3 拆分 GU、激活量化、Down。每层读取 12 个边界，加 head 两个，共 578 tick/窗口。36 位 19.2 MHz 时钟；离线检查每个时间戳、回绕、阶段之和、249 个窗口及五请求与参考一致。GU/激活/Down 边界包含同阶段共享专家工作及标记开销，不是纯 routed kernel 时间。

| 暖态 cycle1，ms/窗口 | Code2K | Prose2K |
| --- | ---: | ---: |
| attention | 19.526 | 18.877 |
| hc1 | 6.588 | 6.402 |
| GU+shared | 23.961 | 16.388 |
| act+shared | 0.399 | 0.390 |
| Down+shared | 11.095 | 7.557 |
| 全部标记跨度 | 67.363 | 55.234 |

暖态 IQ2_S 的 GU 区间分别为 17.858/11.974 ms/窗口。该格式是本轮主要关注项；插桩区间不能外推为等幅度整模型收益。

候选复用已有 `ORCA_IQ_PAIR_COMPACT=1`，将同专家三条输入由 2+1 两次权重解码改为共享 E3/E4 主体，原路径为 4/2/1。该路径此前在 Short 上没有明确的原版对照收益；本次补测当前 U4 的 2K Code/Prose，不修改数学源、EXE、images 或权重。

正式测速关闭全部插桩。每个新进程先 Short23/16，再交错三轮 Code/Prose2K/full128，丢 cycle0，后两轮中位数。正序关→开→关，控制取两侧几何均值；每场景至少 +1%、相同策略控制漂移不超过 3% 才进入反序。所有完整 token、生成数、accepted/offered、reuse/resume、finish 与原始协议均检查；全部样本含预热保留。

| 顺序/场景 | 关 decode ms | 开 decode ms | 吞吐变化 | 两侧相同策略耗时漂移 |
| --- | ---: | ---: | ---: | ---: |
| 正序/Code2K | 4370.434 | 4498.600 | -2.849% | +2.334% |
| 正序/Prose2K（漂移超限，排除） | 4571.133 | 4626.300 | -1.192% | +11.871% |

超限配对仍保留全部原始数值，但不用于判断候选的提速或回退，也不替换样本。

本次各进程启动 INFO 的空闲显存 MiB：[2174, 2129, 1776]。该值只反映 READY 时刻，没有运行期显存或时钟监测。

未达到预设采用门槛，未采用。正序未过，不运行反序。 主配置、U4N16 数据、RAM 保护与原生回退保持。没有新的稳定 decode 提速结论。

阶段采集和正式测速属于不同进程，不能互相代替。控制漂移不是置信区间；没有新 PPL、跨日重复或 Linux GPU 结论。编译、GPU 工作及资源哈希均持共享实验锁；保留原始失败/成功目录。归档不含权重、EXE、OBJ、SPV 或二进制张量；旧诊断的继承 OS 环境不发布。

```powershell
python -X utf8 build-orca-decode-stages-20261105/fine.py --check-source
python -X utf8 build-orca-decode-stages-20261105/analyze.py fine-r1
python -X utf8 build-orca-decode-stages-20261105/compact_model.py --check compact-forward-r1
python -X utf8 build-orca-decode-stages-20261105/publish.py --check
```

新测使用不存在的编号，例如 `fine.py fine-r2` 或 `compact_model.py compact-forward-r2`，保留冻结依赖、输入顺序和配置。
[精确 JSON](../../../sycl/orca/decode-expert-stages-a770-windows-2026-11-05.json) · 原始文本证据 (`decode-expert-stages-a770-windows-2026-11-05.json.gz`, raw capture retained locally)
