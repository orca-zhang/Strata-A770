# Flash-Next 原始 BF16 专家来源与 U4 样本（2026-10-27）

已找到当前 Qwen3.8-Flash-Next 模型对应的原始权重，并准备了直接 BF16→U4 的小样本。没有经过 Q2，也没有运行新的 GPU 性能或整模型质量测试。

官方源为 [Qwen/Qwen3.8-Flash-Next](https://huggingface.co/Qwen/Qwen3.8-Flash-Next)，固定提交 `de4b8e4d43b917e7706784d8bb445c9af86a3540`，与 `tools/mtp_fetch.py` 的固定源一致。[配置](https://huggingface.co/Qwen/Qwen3.8-Flash-Next/blob/de4b8e4d43b917e7706784d8bb445c9af86a3540/config.json) 标明 BF16、48 层、每层 512 专家、hidden=2560、expert intermediate=640；[索引](https://huggingface.co/Qwen/Qwen3.8-Flash-Next/blob/de4b8e4d43b917e7706784d8bb445c9af86a3540/model.safetensors.index.json) 给出 131 分片、359,999,963,128 字节张量载荷（360.0 GB / 335.28 GiB）。GSQ-RCO 模型卡也将该仓库列为 base_model；本轮未比较全部权重或运行混合替换验证。

本地 `G:/GitHub/Strata-data/mtp/tensors/` 已有原始 BF16 专家，之前将“没有主模型各层专家源”说成“没有专家源”的范围过宽。两个完整 MTP 张量本轮重新计算 SHA256，均与 `tools/mtp_fetch.py` 固定的摘要及本地 manifest 一致：

| 文件 | BF16 形状 | 字节 | SHA256 |
| --- | --- | ---: | --- |
| `mtp.layers.0.mlp.experts.gate_up_proj.bin` | 512×1280×2560 | 3,355,443,200 | `839e98f9642ebc884b178abd2090b6dc8b0bcd81ed92d7f7a08314c6a68f96d5` |
| `mtp.layers.0.mlp.experts.down_proj.bin` | 512×2560×640 | 1,677,721,600 | `539c46234f73bac70450f731f40e715f77ac32faf77def0f3f800e58aaabbbc0` |

这 4.6875 GiB 包含 512 个 **MTP** 专家，不能代替主模型 48 层各自的权重。可立即用于草稿层或同形状算子实验。

另外发现 `G:/models/Qwen3.5-35B-A3B-ms/`：14 个 safetensors 分片均存在，逐个头部检查为 1,751 个 BF16、60 个 F32 张量，总张量载荷 71,903,655,008 字节。它属于另一模型（40 层、256 专家、hidden=2048、intermediate=512），只适合作为另一组测试数据；未计算该整套权重的内容摘要。

主模型专家可通过现有下载器的 HTTP Range 读取，不必先下载 360 GB。读取 safetensors 的头部后，一个专家的 fused gate/up 为 6.25 MiB，down 为 3.125 MiB，合计 9.375 MiB。本轮取到：

| 层／专家 | gate/up 分片 | down 分片 | 来源 |
| --- | --- | --- | --- |
| 0／0 | `model-00002-of-00131.safetensors` | `model-00003-of-00131.safetensors` | 固定提交的 HTTP Range |
| 24／255 | `model-00069-of-00131.safetensors` | `model-00070-of-00131.safetensors` | 固定提交的 HTTP Range |
| 47／511 | `model-00119-of-00131.safetensors` | `model-00120-of-00131.safetensors` | 固定提交的 HTTP Range |
| MTP／511 | 本地原始张量 | 本地原始张量 | 通过完整张量 SHA256 检查后切片 |

新增权重下载 29,491,200 字节（28.125 MiB），不含很小的配置、索引和头部请求；8 个原始 BF16 切片共 37.5 MiB。每次远程切片要求 HTTP 206、精确的 Content-Range 和长度。记录了固定源 URL、字节范围及本地 SHA256；远程切片没有发布方提供的独立逐切片摘要，不能将本地摘要说成发布方完整性认证。

四个专家各拆成 gate、up、down，共 12 个矩阵。复用 `tools/u4s4_pack.py` 分别生成单 U4 v001 和正 scale 双 U4 radix16 v002，共 24 个文件。独立按行／列计算 native B 地址，解码了全部 39,321,600 个打包后权重值，验证头部、尺寸、有限值及逐元素量化误差界。

| 格式 | 12 个矩阵的相对 L2 误差范围 | 含 FP32 group64 scale 的载荷 |
| --- | ---: | ---: |
| 单 U4 | 10.8011%～11.8250% | 4.5 bits/weight |
| 双 U4 radix16 | 0.6362%～0.6960% | 8.5 bits/weight |

这些只是少量权重的重建误差，不能推断模型质量、激活量化误差或 GPU 速度。现有 HC 消费者也不能直接作为专家层加载器使用。下一步可以从本地 MTP 专家做同形状 gate/up/down 的 repack 与融合对照，再验证主模型接入。

数据与源码在本地 `build-sycl-a770-bf16-experts-20261027/`；原模型文件未修改。离线复核：

```powershell
python -X utf8 build-sycl-a770-bf16-experts-20261027/prepare.py --check
python -X utf8 -m unittest tools.test_mtp_fetch
```

两项均通过：8 个 BF16 切片、24 个 native repack，以及下载器现有的 8 个测试。[源、范围、摘要和逐矩阵误差记录](bf16-expert-sources-2026-10-27.json)。
