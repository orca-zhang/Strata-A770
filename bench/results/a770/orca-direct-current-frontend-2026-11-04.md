# 完整前端重建：补齐缓存策略修复，Direct 尚未快于 L0

2026-11-04，空闲 Windows 10 22H2 主机，Intel Arc A770 16 GB，驱动 `32.0.101.9034`，oneAPI 2025.3.2。
本轮重建完整 Strata 前端、运行时和数值包，再做双向 Short 对照。
修复缓存策略后，低漂移的反向轮次为 **58.7963 Direct / 59.7296 L0 tok/s，Direct 低 1.5625%**；
此前同策略正向轮次低 **4.1798%**，伴随更大的执行时间波动。两轮全部保留，**没有建立 Direct 快于 L0 的结论**。

## 先修复一个未合入源码的策略差异

完整重建发现，源码的专家缓存替换条件仍将两条 Orca 路径分开：
L0 热度差为 **1.5**，Direct 为 **0.5**。此前的对齐发生在冻结实验入口对象中，尚未进入这个源码分支；
命令行、MTP 门槛和实际专家槽相同，不能证明这个编译期策略也相同。

原源码的两轮结果为：

| 顺序 | Direct 参考 tok/s | L0 参考 tok/s | Direct 相对 L0 |
| --- | ---: | ---: | ---: |
| L0 → Direct → L0 | 52.8438 | 59.7437 | −11.5492% |
| Direct → L0 → Direct | 52.6833 | 59.3425 | −11.222% |

这些是不同缓存策略下的整版本结果，不能归因于提交层。
本次已修改 [generate.cpp](../../../sycl/src/program/generate.cpp)，让 `STRATA_ORCA_NATIVE` 的 L0 和 Direct 都采用现有的 **1.5**；
SYCL 分支保留原值。仅重编译 Direct 入口，复用本轮的全部运行时／引擎／算子库和数值包；L0 的已编译行为不变。
当前源码与冻结原源码的唯一差异由构建检查逐字核对，生产配置和已部署 EXE 未替换。

## 同策略双向复测

每个独立进程丢弃十次预热，保留五次请求。表中为进程内中位数，两侧参考取两个进程中位数的几何均值。

| 顺序／进程 | decode tok/s | 五次保留范围 tok/s | TTFT 中位数 ms |
| --- | ---: | ---: | ---: |
| 正向：L0 前侧 | 59.3425 | 59.1716–59.7296 | 861.32 |
| 正向：Direct | 56.4972 | 56.0389–57.3196 | 890.52 |
| 正向：L0 后侧 | 58.5832 | 57.7332–59.7343 | 875.30 |
| 反向：Direct 前侧 | 58.9147 | 58.7281–58.9833 | 858.96 |
| 反向：L0 | 59.7296 | 59.5658–59.8378 | 860.45 |
| 反向：Direct 后侧 | 58.6782 | 58.2063–58.7689 | 864.78 |

正向参考为 **56.4972 / 58.9617 tok/s**，Direct 低 **4.1798%**，L0 两侧速率漂移 **−1.2796%**。
反向参考为 **58.7963 / 59.7296 tok/s**，Direct 低 **1.5625%**，Direct 两侧漂移 **−0.4015%**。
正向 Direct 的预填充也较慢：保留请求的 prompt 中位数为 **851.3 ms**，反向 Direct 两侧为 **820.7 / 827.4 ms**；
不能把两轮变化全部解释成某一个 API 的开销。GPU 时钟未锁定，这些漂移和样本范围不是置信区间。
不能只挑反向较高的数字，也不能将两轮简单平均后宣称稳定差距。

decode 采用引擎 DONE 的净解码时间，**排除加载、准备、初始化和预填充**。
TTFT 为协议首 token 的主机观测值，单独列出；测试没有内部阶段计时、GPU 时间戳或遥测。
所有编译均在性能计时前结束。

## 本轮验证范围

- 从同一份 **585 文件源码快照**完整构建两种后端，固定同一个 ggml checkout；然后只对 Direct 入口应用上述一行策略修复。
- 两边 **48 组 ABI／内核符号／模块编译选项一致，1,269 个数值 SPIR-V 文件逐字节相同**。
  两个推理后端加载同一份新烘焙包：**1,245 个 zebin、24 个明确标记的不支持变体**。
  Direct 若实际选择保留的 SPIR-V 变体会拒绝执行；离线烘焙允许使用 Intel 编译器／驱动。
- L0、Direct 的 kernel／engine／queue **六项 GPU 检查均通过**，包括 XMX FP16／BF16 的 36 个形状、
  FP16 双专家的 504 项输出检查、GEMM API 的 96 个用例、矩阵自动批处理、图回放和跨队列依赖。
- 修复前后共 **180 次请求，60 次保留测量**，全部逐次匹配同样的 76 个输出 token、57/57 草稿、自然 stop 和 `reused=resume=0`。
  Direct 启动和负载结束的模块审计均通过，未加载 SYCL、UR、Level Zero 或 Intel GPU 用户态运行库。

第一次辅助 queue 检查误用了另一后端的测试包，因为条件编译改变了测试 lambda 编号而触发 ABI 拒绝。
修正为各自烘焙辅助 queue 测试包后六项检查通过；模型的 48 组数值包仍然共用。原失败日志保留在证据中。

本轮没有重新测总 D2D 次数、应用锁次数、物理 PCIe 流量、PPL、长上下文或 Linux GPU。
缓存策略对齐不能等同于这些指标全部相同。此前只替换运行时库的 [56.53 / 57.76 对照](orca-direct-current-runtime-2026-11-04.md)
仍保留其冻结 Host／数值包范围，不能替代本轮完整前端结果。

## 参数和复现

固定 Short28／自然76，实际 **5,771 槽／7,922 MiB**，预留 **1,536 MiB**；
`short-read=256`、`adapt-decay=0.95`、MTP4、step／last min-p **0.5**、累计门槛 **0**，匹配轮次的缓存热度差均为 **1.5**。
这是旧固定容量锚点，**不是日常平衡参数或自动缓存容量的成绩**。

[完整数据与哈希](../../../sycl/orca/direct-current-frontend-a770-windows-2026-11-04.json)及
原始证据归档 (`direct-current-frontend-a770-windows-2026-11-04.json.gz`, raw capture retained locally)
保存 955 个源文件／脚本／构建命令／日志／配置／协议文件。
EXE、数值包和专家 profile 有哈希；GGUF／权重 pack 记录路径，本轮未重新计算全部模型内容哈希。
同等条件复现需要相同硬件、驱动、编译器、外部资产、初始 profile、参数、预热历史和空闲条件，速率仍允许波动。

在仓库根目录导出匹配轮次的配置，输出目录必须尚不存在：

```powershell
@'
import json
from pathlib import Path
r = json.loads(Path('sycl/orca/direct-current-frontend-a770-windows-2026-11-04.json').read_text(encoding='utf-8'))
out = Path('build-orca-current-frontend-repro'); out.mkdir()
runs = r['results']['matched-l0-direct-l0']['runs']
for name, index in [('l0', 0), ('direct', 1)]:
    (out/(name+'.json')).write_text(json.dumps(runs[index]['config'], indent=2), encoding='utf-8')
'@ | python -X utf8 -

python -X utf8 tools/bench_orca_anchor.py --config build-orca-current-frontend-repro/direct.json --runtime direct --baseline-config build-orca-current-frontend-repro/l0.json --baseline-runtime l0 --out build-orca-current-frontend-repeat-forward
python -X utf8 tools/bench_orca_anchor.py --config build-orca-current-frontend-repro/l0.json --runtime l0 --baseline-config build-orca-current-frontend-repro/direct.json --baseline-runtime direct --out build-orca-current-frontend-repeat-reverse
```

配置内的路径指向本次本机构建，其他机器需按归档的源代码和命令重建并映射路径；不能直接使用旧 EXE 代替。
本机只做离线证据核对、不运行 GPU：

```powershell
python -X utf8 build-orca-direct-gap-20261104/publish_current_frontend.py --check
```
