# Orca Direct / L0：预填充物理提交与等待普查

2026-11-04，Windows 10 22H2，Intel Arc A770 16 GB，驱动 `32.0.101.9034`，oneAPI 2025.3。

使用[已接入源码](orca-source-adoption-2026-11-04.md)的两条路径：复用原 Host 对象和 runtime，
只重编带阶段边界的主程序并链接进程内 IAT 观察器。两条路径共用逐字节相同的数学 pack。
本轮 **34 个请求全部匹配**完整 token、生成数、草稿采纳/提供数、复用状态和终止原因。
每进程按顺序运行 Short 15 次、2K 1 次、8K 1 次；与干净实验的同序号请求核对。

这是一轮独立诊断：已移除每次 API 调用的时钟读取及命令缓冲抓取，
但原子计数和观察器内部 SRW 锁仍会扰动执行，**诊断耗时不用作性能结论**。
下面 Short 取第 15 次，2K/8K 各取唯一一次；不把它们当作长请求稳态。

## 提交及等待次数

prefill 为 `r0..prompt_ms`，包含槽位归还和解码前专家缓存准备。
decode 为 `d0..decode_ms`，排除准备、prefill 和循环后最终 `wait_commit`。
阶段为 Host 调用区间，不是 GPU 的原子快照；区间之外的初始化及清理不在表内。

| 请求/阶段 | 路径 | D3DKMT 提交 | Compute node5 | Copy node2 | CPU fence wait | GPU queue wait | `_Mtx_lock` |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Short28/prefill | direct | 29 | 29 | 0 | 22 | 0 | 69 |
| Short28/prefill | l0 | 120 | 120 | 0 | 0 | 50 | 63 |
| Short28/decode | direct | 97 | 97 | 0 | 77 | 0 | 112 |
| Short28/decode | l0 | 97 | 97 | 0 | 0 | 21 | 112 |
| Code2K/prefill | direct | 67669 | 39667 | 28002 | 4529 | 27998 | 20766 |
| Code2K/prefill | l0 | 184527 | 128387 | 56140 | 0 | 3665 | 18766 |
| Code2K/decode | direct | 160 | 134 | 26 | 109 | 0 | 4519 |
| Code2K/decode | l0 | 1017 | 159 | 858 | 0 | 145 | 4438 |
| Long8K/prefill | direct | 81510 | 47634 | 33876 | 5388 | 33872 | 16901 |
| Long8K/prefill | l0 | 225908 | 158018 | 67890 | 0 | 4058 | 18436 |
| Long8K/decode | direct | 194 | 162 | 32 | 130 | 0 | 4357 |
| Long8K/decode | l0 | 1250 | 193 | 1057 | 0 | 178 | 4234 |

表内提交均成功且映射到已知 node；`D3DKMTSubmitCommandToHwQueue` 在全部区间为零。
相同 node 上的多个逻辑队列不表示多个独立物理引擎。
CPU fence 等待与 GPU queue wait 承担不同工作，不能将两列相减视为可删除等待。
`_Mtx_lock` 只覆盖可见导入；未观察到的私有/内联/驱动锁不等于零。

## 定位结果

- Code2K prefill：Direct 67,669 次，L0 184,527 次，提交次数变化 **-63.3284%**。
- Long8K prefill：Direct 81,510 次，L0 225,908 次，提交次数变化 **-63.9189%**。

两种长度的提交次数降幅接近，8K 的 Direct 批量提交优势没有消失。
干净实验中 8K 时间差更小，因此不能仅用提交次数解释端到端变化；
计算、访存与专家流水线等待的耗时占比仍需独立测量。
热 Short decode 则两条路径都为 97 次提交、112 次可见 `_Mtx_lock`，
剩余 Short 性能差不能解释为 Direct 有更多底层提交。

以下只列实际观察到的驻留及分配调用；不能由调用次数推算显存容量或物理搬运量。

| 请求/阶段 | 路径 | CreateAllocation2 | MakeResident | Evict | MapGpuVirtualAddress |
| --- | --- | ---: | ---: | ---: | ---: |
| Short28/prefill | direct | 0 | 0 | 0 | 0 |
| Short28/prefill | l0 | 7 | 31 | 10 | 7 |
| Short28/decode | direct | 0 | 0 | 0 | 0 |
| Short28/decode | l0 | 0 | 79 | 21 | 0 |
| Code2K/prefill | direct | 684 | 684 | 0 | 684 |
| Code2K/prefill | l0 | 682 | 4240 | 1651 | 682 |
| Code2K/decode | direct | 0 | 0 | 0 | 0 |
| Code2K/decode | l0 | 25 | 448 | 78 | 25 |
| Long8K/prefill | direct | 140 | 140 | 0 | 140 |
| Long8K/prefill | l0 | 148 | 5705 | 2366 | 148 |
| Long8K/decode | direct | 0 | 0 | 0 | 0 |
| Long8K/decode | l0 | 31 | 589 | 100 | 31 |


## L0 可见调用与主命令缓冲

主命令字节仅为 D3DKMT 提交的 `CommandLength` 之和，
不包含所有间接命令内容，**不是 D2D 字节或 PCIe 数据量**。

| 请求/阶段 | 路径 | 主命令字节 | L0 launch append | L0 memory copy append | L0 memory fill append | L0 immediate append lists | L0 host list sync |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Short28/prefill | direct | 2304640 | 0 | 0 | 0 | 0 | 0 |
| Short28/prefill | l0 | 24704 | 0 | 35 | 63 | 21 | 29 |
| Short28/decode | direct | 7198104 | 0 | 0 | 0 | 0 | 0 |
| Short28/decode | l0 | 12416 | 0 | 0 | 0 | 97 | 117 |
| Code2K/prefill | direct | 41629440 | 0 | 0 | 0 | 0 | 0 |
| Code2K/prefill | l0 | 29600000 | 82363 | 14858 | 64 | 0 | 2375 |
| Code2K/decode | direct | 9109696 | 0 | 0 | 0 | 0 | 0 |
| Code2K/decode | l0 | 346432 | 0 | 857 | 0 | 109 | 135 |
| Long8K/prefill | direct | 49049480 | 0 | 0 | 0 | 0 | 0 |
| Long8K/prefill | l0 | 36192384 | 99268 | 19656 | 64 | 0 | 2539 |
| Long8K/decode | direct | 11093160 | 0 | 0 | 0 | 0 | 0 |
| Long8K/decode | l0 | 426112 | 0 | 1055 | 0 | 131 | 163 |

原始 JSON 保留全部 68 个阶段及所有观察到的 API 名称，不只保留表中选定行。
观察器覆盖 EXE 导入和经已挂钩 `GetProcAddress` 动态解析的入口；厂商库仅挂钩 D3DKMT。
先前缓存的入口、未挂钩模块及内联实现可能在覆盖范围之外。
node 元数据在创建 context 时查询并缓存，不在每次提交中重新查询。

## 与干净性能数据的关系

[成组分配实验的独立干净对照](orca-direct-point-batch-2026-11-04.md)中，
未开启候选的 Direct 相对同数学 pack 的 L0，2K/8K prefill 分别短 **8.03%/0.69%**，
净 decode 吞吐分别低 **0.44%/1.42%**；Short 低 **2.28%**。
那些数值来自无本观察器的 EXE，本轮计数不能替代或修正其性能结果，
调用次数差异本身也不证明时间因果。

固定 5,771 专家槽、reserve1,536MiB、read256、decay0.95、margin1.5、
MTP4、spec-min-p0.5、cumulative0、suffix0、cost-window0、BCS1，没有前缀复用。
这是锚定配置，不是日常平衡默认；配置、EXE 和数学 pack 均记录哈希。
本轮没有 GPU 时间线、物理传输量、PPL 或 Linux GPU 结论，也不关闭此前 Direct 8K 间歇异常。
没有因诊断改动生产源码或部署 EXE，Orca L0 仍为推荐路径。

## 复查

```powershell
python -X utf8 build-orca-slm-async-20261104/prefill_submit_census.py r1 --verify
python -X utf8 build-orca-slm-async-20261104/publish_prefill_submit_census.py --check
```

新采集使用 `prefill_submit_census.py r2` 等新编号，不覆盖成功或失败证据。
同条件复现需相同硬件、驱动、编译器、数学 pack、模型资产、初始专家 profile、参数和请求历史。
归档含观察器及主程序源码、编译/链接命令、配置、协议、日志和哈希；不分发模型或二进制。

[精确 JSON](../../../sycl/orca/direct-prefill-submit-census-a770-windows-2026-11-04.json) ·
原始证据 (`direct-prefill-submit-census-a770-windows-2026-11-04.json.gz`, raw capture retained locally)
