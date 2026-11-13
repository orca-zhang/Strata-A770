# Direct 日常参数对照与显存分配回退修复

2026-11-04，Windows 10 22H2、Intel Arc A770 16 GB、驱动 32.0.101.9034。
本轮将[旧锚点缓存策略实验](orca-direct-copy-margin-2026-11-04.md)放到日常调参条件下验证。
**结论反转：专家替换热度差 0.5→1.5 使 Direct decode 下降 8.62%，不能直接设为日常默认。**
旧锚点的 +10.29% 保留原有测量身份；两组使用不同参数，不能拼接速率。
另修复了一个真实的原生运行时问题：已完成驻留检查的非本地显存分配被拒绝后，
现在同步释放全部分块，允许调用方缩小分配重试。

## 日常参数，固定相同缓存容量

沿用生产配置的 `short-read=1024`、`adapt-decay=0.5`、
`STRATA_MTP_CUM_MIN_P=0.5`，MTP4、step/last min-p 均为 0.5。
专家替换热度差与这些草稿概率门槛是两个独立设置。
两条后端使用同一份 CLI 参数、初始 expert profile 和原来 1341 文件的数值内核包。
没有学习 profile 存在，也没有在测量中保存新 profile。

首次按 `--expert-cache auto` 启动 Direct 时，在首个请求之前发生驻留拒绝；
该失败阶段完整保留，没有当作吞吐样本。
随后仅将两条后端都改成显式 `--expert-cache 5500`，
**实际 INFO 均为 5771 slots／7922 MiB**，预留仍为 1536 MiB。
5500 是 CLI 的初始槽位参数；按 profile 中不同专家的大小装入后，实际槽数会变化，
不是 5500 MiB。历史包装脚本的 scope 字符串误写了 MiB，公开清单已明确纠正，原始文件保留。
因此下面是日常调参在固定容量下的实验，**不是生产 auto 配置的完整性能验收**。

保留原 EU 复制运行时和所有数值内核，只替换主程序中的热度差常数。
顺序为 Direct 原值／Direct 1.5／Direct 原值／L0；每个独立进程十次预热、五次保留。

| 构建 | decode 中位数 tok/s | 保留范围 tok/s | TTFT 中位数 ms |
| --- | ---: | ---: | ---: |
| Direct，热度差 0.5，前侧 | 44.8589 | 44.6297–45.2650 | 890.89 |
| Direct，热度差 1.5 | **41.0168** | 40.9020–41.1456 | 827.80 |
| Direct，热度差 0.5，后侧 | 44.9093 | 44.7480–44.9784 | 888.45 |
| L0，热度差 1.5 | **42.0261** | 41.7514–42.1988 | 822.09 |

Direct 两侧几何参考值 **44.8841 tok/s**，前后速率漂移 **+0.1123%**。
热度差 1.5 的 decode 变化 **−8.6162%**，但 TTFT 点估计更低，说明两项指标需要分开评估。
同为 1.5 时，Direct 比本轮 L0 低 **2.4016%**；L0 只有一个进程，未做自身夹测。
Direct 默认 0.5 的速率比 L0 默认 1.5 高 6.80%，
**这里混入了缓存策略差异，不能称为剥离 API 层带来的 6.80% 收益**。
本轮未采集日常参数下的专家补入计数，不能把旧锚点的“零补入”结论套用过来。

四个进程合计 **60 次完整请求、20 个保留样本**。
每次 28-token 输入、76-token 自然输出、**58/59 草稿采纳**、`stop`、`reused=resume=0`；
所有输出 ID 和每次草稿计数均一致。旧锚点的草稿计数是 57/57，属于另一组条件。
吞吐使用引擎 DONE 的 `decode_ms`，排除加载、初始化、请求准备、图准备、预填充、HTTP 和排队。
TTFT 单列，没有内部逐阶段计时、GPU 时间戳或监控采样混入 clean 速率。
GPU 时钟未锁定，夹测漂移不是置信区间；单个重复输入也不等同于广泛质量或 PPL 验证。

## 已修复的显存回退故障

首次 auto 启动估算 10.82 GiB 空闲、6132 个初始槽位，8.62 GiB 分配随后被检查为非本地驻留。
旧代码将运行时标为失败并保留这笔分配，调用方随后尝试更小缓存时，
只能得到 `memory query after runtime failure` 或 `closed/failed recording`，无法恢复。
这些日志与零请求的失败结果都在公开证据中。

修复仅作用于 WDDM 的 `State::create_allocation`：

- `MakeResident` 的分页完成、驻留查询成功、但至少一个分块不在本地显存时，拒绝该分配。
- 此时尚未保留或映射 GPU VA；用 `SynchronousDestroy=1` 释放资源或完整分块列表，移除所有权记录后抛回原错误。
- 只有上述回滚成功才保持运行时可用。查询失败、分页／映射超时、销毁失败等不确定状态继续终止运行时。

没有降低本地驻留要求，没有接受系统内存回退，也没有修改热路径的命令编码、数值内核或草稿门槛。
只合入 `orca-l0/src/wddm/runtime.cpp`、CMake 中的检查目标和新增检查源码。
日常配置、已部署 EXE、初始 profile、内核包、根 README 和既有 Linux 交接包保持原样；
**需要重建 Windows 原生运行时／引擎才会使用此修复**。本轮没有 Linux GPU 结果。

检查编译实际的运行时源码，只在测试翻译单元中拦截 Windows 调用；生产代码没有故障注入开关。
旧源码按预期失败于“拒绝分配后仍持有 backing”，修复后通过：

- 17 次真实分配后的受控驻留拒绝，包括共享内存／未驻留状态，以及一次 1 GiB＋64 KiB 的双分块分配。
- 17 次小分配重试、已有 host 数据保持不变、完整分块同步释放。
- 查询失败和回滚销毁失败仍使后续操作被拒绝；销毁失败时所有权保留到 teardown。

这项检查不靠耗尽显存来制造故障。对当前源码重建后再次通过同一检查、CPU ABI／编码检查，
以及 **460 次 recording／920 次 GPU 内存回放**的完整缓冲区、边界和拷贝验证。

另外，隔离构建的完整引擎以原日常 `auto` 配置启动成功，实际 6106 slots／8379 MiB，
READY 时报告 1867 MiB 空闲；三次 Short 均为相同的 76 tokens、58/59 草稿。
**这次正常启动没有自然触发驻留拒绝或缩容，不能把它描述为完整引擎已实测缩容成功**；
失败恢复能力由前述确定性注入检查验证。三个请求仅为正确性 smoke，不与固定容量的性能样本合并。
初版验证脚本还保留了一次相对链接路径导致的失败；续跑复用成功的编译和 GPU 检查，只修正链接工作目录。

## 复现与独立核对

[公开数据](../../../sycl/orca/direct-daily-margin-a770-windows-2026-11-04.json)及
161 个原始证据文件 (`direct-daily-margin-a770-windows-2026-11-04.json.gz`, raw capture retained locally)
包括完整输出、配置、模块与内核哈希、冻结源码、构建命令、失败阶段和续跑记录。
归档 SHA256 为 `5b09439b612a7373d37654f99499715f68ff8d88884d16168a893c5f905e85f6`。
模型、内核二进制和引擎 EXE 以哈希识别，需准备相同构建和本机路径。
同等硬件、驱动、输入、容量、profile、预热历史和空闲条件是复测前提；实际速率仍有波动。

现有锚点工具支持独立 fixture。以下仅导出本轮已测参数，不启动 GPU；在仓库根目录的 PowerShell 运行：

```powershell
@'
import json
from pathlib import Path
r = json.loads(Path('sycl/orca/direct-daily-margin-a770-windows-2026-11-04.json').read_text(encoding='utf-8'))
d = r['results']['cache-margin-daily-fixed']
out = Path('build-orca-daily-repro-inputs'); out.mkdir()
f = json.loads(Path('sycl/orca/short-anchor-v1.json').read_text(encoding='utf-8'))
f.update(name='Fibonacci28-daily-fixed-capacity-v1', expected_accepted=58, expected_offered=59)
f['parameters'].update({'--short-read': '1024', '--adapt-decay': '0.5'})
f.pop('orca_exe_sha256', None)
(out/'fixture.json').write_text(json.dumps(f, indent=2), encoding='utf-8')
for name, cfg in d['configs'].items():
    (out/(name+'.json')).write_text(json.dumps(cfg, indent=2), encoding='utf-8')
candidate = dict(d['configs']['direct'], exe=d['runs'][1]['exe'])
(out/'margin1.5.json').write_text(json.dumps(candidate, indent=2), encoding='utf-8')
'@ | python -X utf8 -
```

这三份配置均携带累计门槛 0.5，不使用工具默认的旧锚点参数。
输出目录必须尚不存在，以下使用已有工具做新的 A/B/A；两条命令分别测试策略变化和匹配策略的后端差距：

```powershell
python -X utf8 tools/bench_orca_anchor.py --fixture build-orca-daily-repro-inputs/fixture.json --config build-orca-daily-repro-inputs/margin1.5.json --runtime direct --baseline-config build-orca-daily-repro-inputs/direct.json --baseline-runtime direct --out build-orca-daily-margin-repeat
python -X utf8 tools/bench_orca_anchor.py --fixture build-orca-daily-repro-inputs/fixture.json --config build-orca-daily-repro-inputs/margin1.5.json --runtime direct --baseline-config build-orca-daily-repro-inputs/l0.json --baseline-runtime l0 --out build-orca-daily-matched-repeat
python -X utf8 build-orca-direct-gap-20261104/publish_daily_recovery.py --check
```

本机源码修复检查目标为 `orca-l0-wddm-allocation-check`，CTest 名称
`orca-l0-dg2-allocation-recovery`，标为 GPU 检查。构建和执行需要串行占用共享实验锁。
源码修复不承诺额外 tok/s 收益；当前仍需定位匹配缓存策略后约 2.4% 的原生执行残差。
