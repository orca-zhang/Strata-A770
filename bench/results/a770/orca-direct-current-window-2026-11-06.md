# Orca Direct：当前 U4／HC 的 Short 窗口定位

2026-11-06，Windows 10，Arc A770 16 GB，驱动32.0.101.9034，oneAPI2025.3.2。
本组L0指Orca L0，没有重测原生产SYCL；这是诊断区间，不是无探针吞吐成绩。

此前整窗口诊断使用旧通用M16 pack；本组改用完整源码对照保留的当前U4／M16／HC Q8 SG16 pack。
Direct复用已验收的49个栈数组kernel Host对象及匹配运行库，L0复用对应完整源码构建。
每后端仅重编一个Verifier TU；Verifier头布局、其它引擎对象、模型资产及数学内核保持原版本。
独立审计逐项解析实际编译依赖，并由冻结原头文件和原转换器重新推导每后端11个generated头；
Direct实际使用的栈数组头与已验收候选摘要一致，未把旧头快照当作本次编译输入。

每个Verifier窗口仅在输入复制前、head／采样后各写一个有序GPU时间戳；32字节回读排在原有最终等待之前。
保留正常共享／路由交错和独立投影；PROFILE只启用GEN请求边界，Verifier旧逐层profiler仍关闭。
检查前后guard、19.2MHz／36bit原始tick、回绕／单调性，以及逐窗口Host提交＋提交后至返回＝墙钟。
Host窗口输入准备单列；commit、MTP草稿及窗口之间的工作位于GPU首尾标记之外。

固定实际5771专家槽、reserve1536MiB、U4/min16/M16、read256、adapt32/decay0.95、MTP4、
step／last min-p0.5、cumulative/suffix/cost-window/prompt-cache0；Direct BCS1、ISA／线程内存device。
预先固定Direct→L0→Direct及L0→Direct→L0两个顺序；每个新进程15次Short，前十次丢弃、后五次保留。
每请求输入23、最多输出128、实际按EOS结束于70；所有预热和原始记录均保留。
逐请求完整比较tokens、草稿采纳／提供、结束及reuse／resume，并跨请求核对相同位置与实际T序列。
同一请求历史用于两后端；本组没有先跑长请求，不能直接与此前长请求之后的Short绝对耗时相比。

完整协议 **90/90匹配**，每请求29个窗口，共2610个窗口，其中870个保留。
先对每请求窗口求和，再取每进程后五请求中位数；外侧同后端取几何均值。
两种顺序分别报告，不选择其中更有利的一组。

| 顺序 | 每请求合计 | Direct ms | L0 ms | Direct多用时 | 外侧控制漂移 |
| --- | --- | ---: | ---: | ---: | ---: |
| direct→l0→direct | GPU首尾跨度 | 1201.9062 | 1172.4433 | +2.513% | +0.138% |
| direct→l0→direct | Host窗口输入准备 | 1.2141 | 0.6947 | +74.761% | +5.161% |
| direct→l0→direct | Host提交 | 2.8013 | 1.5398 | +81.924% | +2.019% |
| direct→l0→direct | Host提交后至返回 | 1209.0239 | 1176.4062 | +2.773% | +0.108% |
| direct→l0→direct | Host提交至返回墙钟 | 1211.7963 | 1177.9460 | +2.874% | +0.107% |
| l0→direct→l0 | GPU首尾跨度 | 1201.4364 | 1170.8447 | +2.613% | -0.032% |
| l0→direct→l0 | Host窗口输入准备 | 1.2174 | 0.6855 | +77.600% | +5.299% |
| l0→direct→l0 | Host提交 | 2.8558 | 1.5618 | +82.855% | -2.404% |
| l0→direct→l0 | Host提交后至返回 | 1207.8486 | 1174.8772 | +2.806% | -0.056% |
| l0→direct→l0 | Host提交至返回墙钟 | 1210.2414 | 1176.5262 | +2.866% | -0.053% |

两种顺序的GPU窗口跨度差为+2.513%／+2.613%，Host提交至返回差为+2.874%／+2.866%。
本组Host窗口输入准备约为每请求1毫秒量级，而GPU窗口合计约1秒量级；
这不支持继续只靠提前填表或扩大环来解释全部差距。两者时间域及覆盖范围不同，不作精确差值分摊。
下一步应在同一当前pack上细分GPU窗口及窗口外的MTP／commit，不能直接沿用旧M16的HC／router排名。

预设GPU跨度／Host墙钟外侧控制漂移不超过3%：**True**；漂移不是置信区间。

Host输入准备的外侧漂移超过3%，该小分项不用于建立稳定收益；主判据只覆盖预先指定的GPU跨度与Host墙钟。
GPU首尾跨度包含kernel、内存访问、依赖空隙和时间戳排序，不能称为纯kernel时间。
Host提交后至返回含未完成GPU、诊断回读、完成发布与通知，不能称为纯唤醒开销。
两种时钟没有绝对关联，不把Host墙钟减GPU跨度当作精确CPU开销；分项中位数也不保证相加等于墙钟中位数。
窗口之外仍有commit、草稿与缓存调整，不能从本组确认全部净decode差距或跨场景因果。

首版stage-r1在READY前以0xC0000005退出，未执行模型请求：旧诊断初始化插入点早于当前cs_队列创建，
读取时钟时访问了空队列。修正版stage-r2将诊断初始化移到队列创建后，并增加CPU顺序断言。
原失败源码／命令／日志与结果完整保留，不把该诊断错误归为模型或驱动故障。

没有修改生产源码、部署EXE或后端默认；Orca L0仍为推荐路径，Direct仍为WIP。
本组未测Linux GPU、长上下文或PPL，不关闭既有Direct8K间歇故障。
模型资产使用明确标注的stat-cached SHA256身份校验；未重哈希全部权重。

本机独立复查，不重新运行GPU：

```powershell
python -X utf8 build-orca-current-window-20261106/publish.py --check
```

同条件复测保留硬件／驱动／编译器、冻结源码／EXE／数学pack、资产、初始profile、argv／env和请求顺序。
使用新目录，拒绝覆盖旧尝试；以下未为取得通过而运行：

```powershell
python -X utf8 -c "import sys; sys.path.insert(0,'build-orca-current-window-20261106'); import run as m; m.STAGE=m.OUT/'stage-repro-r1'; m.prepare(); m.run()"
```

归档只含文本／精确字节编码、源码、命令、完整协议及初始失败，不含权重或二进制。
JSON (`direct-current-window-a770-windows-2026-11-06.json`, raw capture retained locally) · 原始文本归档 (`direct-current-window-a770-windows-2026-11-06.json.gz`, raw capture retained locally)
