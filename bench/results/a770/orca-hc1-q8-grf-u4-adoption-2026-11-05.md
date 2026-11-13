# 当前 U4 HC1 256 GRF：服务验收与采用

2026-11-05，Windows，Intel Arc A770 16 GB，单插槽 Xeon E5-2696 v4；驱动 `32.0.101.9034`，oneAPI 2025.3.2。

已将 HC1 融合 Q8 的单内核 256-GRF 映像用于默认 U4/min16 L0 配置。仍使用同一生产 EXE、其余数学 images、9402 个 U4 专家、M16 投影、5771 实际缓存槽、reserve1536MiB、MTP4/min-p0.5、read256、adapt32/decay0.95、prompt-cache0。
仅改变 `STRATA_ORCA_IMAGES` 指向的目录。原128-GRF images 保持原样，切换前配置保存为 `strata-iq2_xs-u4n16-before-hc256-20261105.json`，候选也保存为 `strata-iq2_xs-u4n16-orca-l0-m16-r16-hc256.json`。

[冻结测速报告](orca-hc1-q8-grf-u4-2026-11-05.md)保留全部42次协议：Code／Prose decode正序观察 +1.349%／+1.730%，反序 +1.932%／+4.415%。
每场景达到预设1%提升且外侧同策略耗时漂移在3%内。反序保留样本间存在明显波动，不把外侧漂移当作置信区间，不外推跨日稳定性、8K性能、PPL或Linux GPU。

服务验收分别使用128与256-GRF，在两个新进程中执行首个8K→Short→Code2K→Prose2K→重复8K，随后真实localhost HTTP health/models、两次相同chat、Anthropic messages、OpenAI responses。
全部18次生成请求的完整token、生成数、prompt数、MTP接受/提供数、结束原因和复用数一致；两次8K跨混合历史的完整输出与参考也一致。两侧HTTP规范化响应、实际引擎输入和输出一致。

| 服务进程 | 1 Hz显存样本数 | 采样最低空闲MiB |
| --- | ---: | ---: |
| 128 GRF | 166 | 1519.145 |
| 256 GRF | 166 | 1390.508 |

两侧采样均超过1024MiB余量线；1Hz采样不能排除两次采样间更低的瞬时空闲值。加载后及工作负载后的DLL审计未发现SYCL/UR/oneMKL运行库。

永久目录再次运行Short23/16和Code2K/full128，完整输出与草稿计数匹配，正常退出，之后才原子切换默认配置。
源码构建器为 `gr_up_multi_q8_kernel` 单独设置large-GRF选项；使用真实CMake/Ninja分别生成L0/Direct命令并核对标志。没有据此宣称Direct完整模型采用或关闭其既有8K故障。
启动器、RAM预算保护、原生回退、原生learned profile、固定expert profile和此前U4/min16可选配置的SHA均保持；U4N16权重没有重写或复制。

```powershell
python -X utf8 build-orca-hc1-grf-u4-20261105/service.py check service-r1
python -X utf8 build-orca-hc1-grf-u4-20261105/adopt.py check adopt-r1
python -X utf8 build-orca-hc1-grf-u4-20261105/finish.py --check
```

以上核对保留的历史记录；日后合法更新当前源码或配置可能使当前文件哈希核对失败。归档含文本、源码、配置、日志与哈希，不包含模型、EXE、对象、SPIR-V或二进制张量。

[精确JSON](../../../sycl/orca/hc1-q8-grf-u4-adoption-a770-windows-2026-11-05.json) · 文本证据 (`hc1-q8-grf-u4-adoption-a770-windows-2026-11-05.json.gz`, raw capture retained locally)
