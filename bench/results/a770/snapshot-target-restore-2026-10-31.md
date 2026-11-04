# 直接恢复目标检查点：Windows A770，2026-10-31

后续[独立复测](snapshot-target-restore-reconfirm-2026-10-31.md)已确认8K缓存命中 TTFT 降低4.34%/4.64%；2K及生成吞吐仍未确认。下文保留首轮因漂移未通过的原始结论和数据。

继续参考 [a770-qwen-xe1 issue #1](https://github.com/ef777/a770-qwen-xe1/issues/1) 的主机快照与缓存验证思路。Strata 已有 host KV、动态 QSA 检索、循环状态检查点和增量快照；本次从现有恢复路径中去掉一次重复上传。

原路径先恢复 parked 会话末尾的 running state，再恢复请求实际需要的较早检查点。候选直接恢复该目标检查点，后续跳过第二次相同恢复。完整 KV、驻留范围、draft ring 恢复、检查点 LRU 和回写边界均沿用原逻辑。接口在写设备之前检查目标确实属于该快照，拒绝外部检查点；传输失败仍终止当前会话。

在本机 Arc A770 16 GB、Xeon E5-2696 v4、256 GiB RAM、PCIe 3.0、Windows 原生 Orca/Level Zero 上测量。隔离 A/B 使用同一冻结的数值对象、prefill 对象和 5370 个内核映像；A/B 均包含上一轮快照批量复制，本轮差异仅为目标状态恢复及对应回归检查。原版 4392 项、候选 4680 项检查通过。检查覆盖外部目标拒绝且原状态不变，以及直接恢复与旧两步恢复的 GDN、PLE、indexer、主模型/draft KV 和备用行逐字节一致。

完整模型按 A0/B0/A1/B1/A2 执行。保持当前 FP16 KV/packed-value4 设置、MTP4、min-p=0.5；请求专家缓存 4500，实际 4717 槽/6480 MiB，reserve3072；prompt-cache6、会话 RAM 2048 MiB/4 槽。仅为对照固定 adapt-swaps0、PCIe fraction0、suffix-draft0。每进程先 Short16 预热，再交替执行 Code2048/Long8000 各四次，每次完整输出128 token；首轮冷请求不进入热请求中位数。

40 条完整输出均匹配各自首个原版冷请求；每个 Code 请求 MTP 接受/提交75/93，Long 为67/100。热请求分别复用2041/7993 token；五进程均有6次实际 RAM 恢复，共30次，全部退出0。1 Hz 物理显存样本的全程最低空闲为 1805.363 MiB。每个完整请求都有样本覆盖；采样不证明亚秒级峰值。

两次相邻 A/B/A 以原版耗时几何均值配对；基线漂移须不超过3%，TTFT 耗时降低须两次均至少1%，decode/wall 按吞吐变化两次均至少1%。全部结果保留，不能通过跨输入平均抵消漂移。

| 输入 | 指标 | 原版配对耗时 ms，两次 | 候选耗时 ms，两次 | 耗时降低 %，两次 | 原版漂移 %，两次 | 确认 |
| --- | --- | ---: | ---: | ---: | ---: | --- |
| Code2K | ttft_ms | 584.468 / 540.708 | 545.224 / 485.426 | 6.715 / 10.224 | -7.503 / -7.472 | 未确认 |
| Code2K | decode_ms | 6715.037 / 6406.845 | 6761.800 / 5988.800 | -0.696 / 6.525 | 2.242 / -10.965 | 未确认 |
| Code2K | wall_ms | 7225.825 / 6874.880 | 7232.429 / 6409.292 | -0.091 / 6.772 | 1.308 / -10.646 | 未确认 |
| Long8K | ttft_ms | 608.920 / 574.789 | 573.982 / 526.678 | 5.738 / 8.370 | -0.117 / -10.792 | 未确认 |
| Long8K | decode_ms | 7927.578 / 7550.568 | 8054.000 / 7066.600 | -1.595 / 6.410 | 1.955 / -11.025 | 未确认 |
| Long8K | wall_ms | 8466.287 / 8065.142 | 8554.606 / 7521.558 | -1.043 / 6.740 | 1.752 / -10.814 | 未确认 |

本轮没有 TTFT 指标通过两轮门槛，不能认定已复现速度收益。上表是 RAM 缓存命中的匹配配置，不能替代生产默认、冷请求或更长上下文的速度/质量结论。原版日志中的 restored 耗时尚未包含后续 checkpoint restore；因此不能拿这条局部日志直接比较端到端收益。

另提供本机可选配置 `strata-iq2_xs-ram-cache.json`，指向本次 B 程序：专家缓存4500、reserve3072、prompt-cache6、会话 RAM2048 MiB/4槽，保留生产 PCIe0.16、自适应32/decay0.5、MTP4/min-p0.5。主配置 `strata-iq2_xs.json`、其中 reserve1536 和默认程序均保持不变。可选配置会减少专家驻留，适合来回切换会话；此 HTTP 检查没有测量它相对主配置的速度。

可选配置通过实际 Python HTTP 服务和临时 loopback 端口检查：约2K输入的两个会话分别包含 `ORBIT_CODE=731` 与 `946`，按 A/B/A/B 调用 OpenAI非流式、Anthropic、OpenAI流式、Responses。四次均准确返回对应三位数字，重复输入 IDs 和完整输出 tokens 一致，后两次复用至少90%输入并确有检查点恢复。实际 HTTP 进程退出0，采样最低空闲 2056.715 MiB，模块审计未载入 SYCL/UR/MKL 运行时。这提供有限的会话隔离与恢复覆盖，不证明8K以外的检索质量。

启动可选配置（默认仅本机）：

```powershell
python -m serve.server --config strata-iq2_xs-ram-cache.json
```

CUDA 对应接口和回归用例同步更新，本机无 nvcc/CUDA GPU，未编译或运行 CUDA 路径；本次也未运行 Linux GPU 或 SYCL 运行时，不外推平台验证。

原始数据、完整输出、原始协议/HTTP/显存/构建日志、全部配对、配置、源文件哈希、改动 diff 和控制器源码见 [JSON](snapshot-target-restore-2026-10-31.json)。最初 HTTP 控制器名为 http.py，遮蔽标准库，仅在导入时失败；更名后才创建实验目录和请求，没有失败的模型请求被丢弃。

本机复核与重跑入口；新测量输出目录须不存在：

```powershell
python -X utf8 bench/results/a770/check_snapshot_target_restore_20261031.py
python -X utf8 build-orca-snapshot-target-20261031/build.py --out build-orca-snapshot-target-20261031/build-v2
python -X utf8 build-orca-snapshot-target-20261031/model.py --build build-orca-snapshot-target-20261031/build-v2 --out build-orca-snapshot-target-20261031/model-v2
python -X utf8 build-orca-snapshot-target-20261031/http_cache.py --out build-orca-snapshot-target-20261031/http-v2
```

首条命令只用标准库读取已发布数据，不需要 GPU；重跑构建和模型均持共享 GPU 锁。HTTP 入口使用固定已测 build-v1/B，未暗中替换主配置。
