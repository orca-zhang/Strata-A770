# DeepInfer 四场景前缀复用筛选（2026-10-24）

结论：未通过本轮筛选。
四场景输出和前缀复用量一致；不调整运行时默认值，不替换生产 EXE 或配置。

A770 16GB、Xeon E5-2696 v4、Windows、oneAPI 2025.3.2。
三次独立进程 A/B/A，全部使用同一个冻结候选 EXE（41824ddcfa4f…）及其依赖，只切换四项 DeepInfer 开关。
阈值0.50，故固定长度草稿链连续提交不生效；Q8 KV，输入23/2048/2048/8000 tokens，输出上限128。
缓存请求5500，实际各臂5771 slots / 7922 MiB；prompt-cache 128，每512 token保存检查点。
保留自适应专家缓存和PCIe比例0.16；共享GPU锁、每请求前后检查其他GPU/编译进程，分析器和阶段打印关闭。
每进程每场景先发一次初始请求、再预热两次、保留三次；共72次请求、36次保留请求。
所有初始、预热、保留输出逐 token 相同，统计与原始协议独立核对。

以下为**缓存前缀后的完整模型生成**，每臂取三次每token解码耗时中位数，单位tok/s。
对照吞吐为两侧基线耗时几何平均除以候选耗时；漂移按后一基线耗时/前一基线耗时计算，绝对值超过3%排除。

| 场景 | A0 | B1组合 | A2 | 吞吐变化 | 基线耗时漂移 | 配对 |
|---|---:|---:|---:|---:|---:|---|
| Short | 31.99 | 33.16 | 32.84 | +2.29% | -2.59% | 合格 |
| Code2K | 27.48 | 28.30 | 28.39 | +1.32% | -3.19% | 排除 |
| Prose2K | 28.46 | 28.40 | 27.68 | +1.18% | +2.80% | 合格 |
| Long8K | 26.05 | 25.77 | 25.41 | +0.14% | +2.55% | 合格 |

四场景吞吐变化的描述性几何平均为+1.23%；该描述值包含漂移排除项，不能替代验收。
事先规则：全部四场景配对合格、几何平均至少+1%、任一场景不低于−2%、输出与复用量一致，才通过筛选。
本轮仅A/B/A一次筛选，没有独立确认，不声明最新全部优化已获得稳定整模型加速。
被测构建是冻结的DeepInfer候选，不包含后续CPU tokenizer或其他会话的XMX实验；结果不与其他算子收益相加或连乘。

| 场景 | A0 TTFT ms | B1 TTFT ms | A2 TTFT ms | 每次复用token |
|---|---:|---:|---:|---|
| Short | 249.64 | 257.84 | 242.88 | [16] |
| Code2K | 242.75 | 239.75 | 241.61 | [2041] |
| Prose2K | 243.05 | 238.84 | 243.56 | [2041] |
| Long8K | 242.12 | 241.55 | 246.05 | [7993] |

TTFT为命令发出到收到首token的主机计时，包含检查点恢复；此表各指标独立取中位数。
缓存请求仍保留完整上下文，实际新读取token较少，不能代表首次长输入TTFT。

初始请求使用已有窗口读取路径，没有该场景预热。以下单次读数保留供排障，既不用于正式prefill性能判定，也不代表原生批量prefill：

| 进程 | 场景 | 复用token | 初始prompt ms | 初始TTFT ms |
|---|---|---:|---:|---:|
| 0 baseline | Short | 0 | 1028.0 | 1099.9 |
| 1 combined | Short | 0 | 1028.9 | 1100.8 |
| 2 baseline | Short | 0 | 1019.1 | 1090.4 |
| 0 baseline | Code2K | 0 | 113424.0 | 113491.2 |
| 1 combined | Code2K | 0 | 115244.2 | 115313.0 |
| 2 baseline | Code2K | 0 | 114248.3 | 114314.5 |
| 0 baseline | Prose2K | 0 | 105492.5 | 105544.8 |
| 1 combined | Prose2K | 0 | 103835.1 | 103887.6 |
| 2 baseline | Prose2K | 0 | 105473.4 | 105527.6 |
| 0 baseline | Long8K | 0 | 403972.0 | 404027.4 |
| 1 combined | Long8K | 0 | 395993.5 | 396049.0 |
| 2 baseline | Long8K | 0 | 399292.0 | 399349.3 |

| 进程 | 场景 | 三次保留请求接受草稿数合计 | 提议数合计 |
|---|---|---:|---:|
| 0 baseline | Short | 153 | 210 |
| 1 combined | Short | 140 | 164 |
| 2 baseline | Short | 153 | 210 |
| 0 baseline | Code2K | 225 | 297 |
| 1 combined | Code2K | 209 | 250 |
| 2 baseline | Code2K | 225 | 297 |
| 0 baseline | Prose2K | 201 | 276 |
| 1 combined | Prose2K | 201 | 276 |
| 2 baseline | Prose2K | 201 | 276 |
| 0 baseline | Long8K | 215 | 297 |
| 1 combined | Long8K | 209 | 282 |
| 2 baseline | Long8K | 215 | 297 |

全部原始DONE统计、TTFT、输出token和协议保留；preparation.json含事先规则、固定参数、输入、源码/构建/生产文件哈希。
source-manifest.json与candidate-source-manifest.json描述冻结引擎源码；不包含EXE、权重或新依赖。

```powershell
python bench/results/a770/deepinfer-cached-matrix-2026-10-24/check_results.py
```

复测：将 run.py、frozen_bench.py、prepare.py、preparation.json 放入仓库根目录下新的构建目录，不复制旧results.json；
准备记录中的冻结EXE和依赖必须仍存在且哈希相同，再运行（会等待共享GPU锁）：

```powershell
python -X utf8 build-sycl-deepinfer-20261023/check_bf16_arena.py <新目录>/run.py
```
