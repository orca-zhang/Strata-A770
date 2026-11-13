# 当前 Direct 前端：计算与 BCS 复制流并用

2026-11-04，Windows 10 22H2，Intel Arc A770 16 GB，驱动 `32.0.101.9034`，oneAPI 2025.3。

这次将此前隔离的原生 BCS 复制实现合并到当前 Direct 前端，保留动态参数分流策略及原 M4 FP16 pair 数学。
同一 EXE 通过启动时缓存的 `ORCA_DG2_BCS_COPY=0/1/0` 做计算复制／BCS／计算复制对照。
51 次请求的全部输出、终止及草稿工作量匹配。候选未采用到生产源码、默认配置或部署 EXE。

## 实际并用的资源

计算队列仍用已验证的 compute node5，复制队列用 Copy node2。三个现有复制角色分别来自
预填充、验证、专家补槽；其他队列及图内操作继续走计算路径，不将任意任务强行拆到 BCS。
BCS 只做原生复制／填充和同步，不执行普通计算内核。

多条逻辑队列共享这些节点，不代表多个物理复制引擎。EU/XMX 计算与 BCS 搬运可以通过依赖组成流水线；
同专家的 GU→SwiGLU→Down 依赖仍保留。GuC 若启用则是驱动的提交／调度固件，并非第三个独立计算引擎。
没有测量本轮活跃 EU 周期或物理引擎同时执行比例，旧实验的时间包络交集不能当本轮利用率。

三方合并只涉及四个 runtime 文件和适配器；主程序两处补槽流改用已有 copy-role API。
重编全部 80 个 Host 单元，69 个适配器使用者均来自新头文件，避免混用内联定义。
数学 pack 保持固定，本轮没有混入[寄存器复用 pair 候选](orca-fp16-pair-tiles-2026-11-04.md)。

## 整模型对照

每个进程先跑 Short 15 次（丢弃 10 次，保留 5 次），再各跑一次 2K、8K，无前缀复用。
两侧控制取几何均值。固定 5,771 专家槽、1,536 MiB 预留、read256、decay0.95、margin1.5、MTP4、
step／last 门槛 0.5、cumulative0，关闭 suffix draft 和 MTP cost window。这是锚定配置，不是日常平衡配置。
净 decode 使用 DONE 时间，排除初始化、准备和预填充；TTFT 为协议墙钟时间。
计时期间无内部探针、GPU 时间戳、遥测或并行编译，GPU 时钟未锁定。

| 输入 | EU-copy 预填充 ms | BCS ms | 预填充时间变化 | TTFT 变化 | 净 decode 吞吐变化 | 预填充控制漂移 | decode 吞吐控制漂移 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Short28 | 891.64 | 885.80 | -0.6548% | -0.6305% | +0.9780% | +1.0258% | -0.8013% |
| Code2K | 27241.94 | 25247.40 | -7.3216% | -7.3074% | +1.3724% | +1.2893% | -0.7681% |
| Long8K | 79287.83 | 76993.90 | -2.8932% | -2.8918% | +0.7985% | +0.8244% | -0.0503% |

长请求每进程仅一次，是有限的序号匹配观察，没有建立稳态或重复性；小幅 decode 变化需要结合控制漂移判断。
此表只比较同一 Direct EXE 的复制路径，没有 L0 性能 arm，不能与其他历史下的绝对值拼接宣称超过 L0。
热 Short 此前同配置诊断没有显式 H2D／专家补槽；那是独立旧诊断，本轮没有重新统计传输量或锁调用次数。

## 原生多路正确性

原有 BCS 填充检查在当前 runtime 重跑，通过 **1,632 次完整缓冲／源／guards 检查**，覆盖 1～128 字节 pattern、
奇数长度、未对齐、改变输入及不可变列表重放，不加载 EU memory module。
另通过 **12 组 BCS→EU→BCS 依赖链**，含主机门控、GPU 内存标签、批量完成查询和 5 个拒绝输入检查。

新增双路检查用 **两条计算逻辑队列、两条 BCS 逻辑队列**，各自提交上传→EU 处理→回传，全部提交后才释放共享
host gate，不在中间插入 CPU wait。8 轮变化输入共 **16 个完整输出／源／guards 检查**通过，并核对四队列的
批量完成点。EU 处理使用既有 copy kernel；这是多队列数据和同步正确性检查，不是矩阵性能或物理并行度测量。

首次双路检查末尾错误地释放 runtime-owned staging，runtime 正确拒绝；失败脚本和日志保留。
修正检查程序仅释放自行管理的 device 分配后，r2 完整重跑通过，没有因此修改 runtime。

此前缩短历史后偶发的 Direct 8K 连续 token0／草稿 0/0 问题仍未关闭。
本轮通过不能排除它；未测 PPL 或 Linux GPU。BCS 合并是 Windows 隔离候选，Linux Direct 尚无独立 BCS 实现。

## 复现与证据

档案包含合并源码、主程序、全部编译／链接命令、依赖、配置、原始协议／日志与 SHA-256；模型及二进制不分发。
完整前端基础与对象文件要求见[前端快照](orca-direct-current-frontend-2026-11-04.md)，参数策略见
[动态参数报告](orca-direct-dynamic-parameters-2026-11-04.md)。同冻结构建、机器、驱动、输入历史和参数可复现流程；
时钟未锁定，具体百分比会随负载改变，本轮没有重哈希完整模型。

```powershell
python -X utf8 build-orca-slm-async-20261104/build_current_bcs.py --verify
python -X utf8 build-orca-slm-async-20261104/check_current_bcs_v2.py --verify
python -X utf8 build-orca-slm-async-20261104/bcs_context_bracket.py --verify r1
python -X utf8 build-orca-slm-async-20261104/publish_current_bcs.py --check
```

新测量使用 `bcs_context_bracket.py r2` 等新编号，不能覆盖冻结目录。

[精确 JSON](../../../sycl/orca/direct-current-bcs-a770-windows-2026-11-04.json) ·
原始证据档案 (`direct-current-bcs-a770-windows-2026-11-04.json.gz`, raw capture retained locally)
