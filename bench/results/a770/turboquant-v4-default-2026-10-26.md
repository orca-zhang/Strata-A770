# 本机默认启用 K16/V4：2026-10-26

已按用户选择更新本机 `strata-iq2_xs.json`。`run-iq2_xs.bat` 下次启动使用已验证的 packed-V 程序；K 保持 FP16，V 使用 4 bit，不附加残差。启动参数保持原样，包括自动专家槽、自适应换槽、8192 上下文与 1536 MiB 显存预留。

沿用 [A770 实际打包测试](turboquant-v-packed-2026-10-26.md) 的同一可执行文件，SHA-256：`acb6d565ef66ee099531867dfb68ec4af8deb8e04e2d524da4c9cbc0fb6523a0`。新增配置为：

```text
STRATA_VALUE_PACKED_BITS=4
STRATA_VALUE_PACKED_RESIDUAL=0
STRATA_NO_BATCH_KV_STEP=1
STRATA_QSA_FUSED_KV_STORE=0
STRATA_PROMPT_ATTN_XMX=0
STRATA_KV_RING_OFF=1
```

兼容范围为常驻显存的 KV；现有 prompt cache 和 conversation parking 继续关闭。融合 KV store、prompt attention XMX 及 KV ring 沿用测试时的禁用设置，这些路径尚不能直接使用 packed V。

本次使用原生产启动参数，经 Intel 服务包装层的 HTTP 接口验证健康状态、记忆代码、第二轮追问、独立算术以及 1722-token 代码输入，四组请求全部通过。日志确认自适应换槽工作，13 个主模型/MTP 状态均为 K16/V4；引擎正常退出，返回码 0。验证服务仅监听 `127.0.0.1:8097`，已关闭；没有留下后台模型服务。

本次 auto-cache 实际为 6922 个驻留专家槽，启动空闲显存 1405 MiB。这是一次启动观测，没有同条件旧默认对照，不能把全部槽位变化归因于 V4。代码输入生成 64 tokens，单次为 20.8 tokens/s，仅作功能验证。

实测 K 分配 104 MiB，V 分配 26.8125 MiB。对前轮 K16/V8 对照，V 分配减半，K+V 从 157.625 MiB 降至 130.8125 MiB，减少 17.01%；前轮两组固定输入的生成吞吐分别提高 2.29% 和 1.76%。这些分配字节数不是显存带宽计数器测量，也不等于整机显存或端到端速度的同等降幅。

复核数据与配置备份：`build-sycl-a770-v4-default-20261026/results.json`、`smoke.stderr`、`config-before.json`。`enable.py` 在 HTTP 检查成功且原配置未被其它任务修改后，原子替换默认配置。恢复此前默认时，将 `config-before.json` 复制回根目录的 `strata-iq2_xs.json`，再启动原脚本。
