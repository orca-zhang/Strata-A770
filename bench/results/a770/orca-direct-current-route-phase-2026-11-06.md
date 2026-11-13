# 当前 U4／HC：HC1 到路由／设备计划的 GPU 区间

2026-11-06，Windows10，Arc A77016GB，驱动32.0.101.9034，oneAPI2025.3.2。
沿用当前窗口诊断的冻结U4／M16／HC Q8 SG16、49个栈数组kernel Host和原生运行库；
每后端仅重编一个Verifier。保持prof_on_关闭、正常独立投影和共享／路由交错。

在已有stamp16（HC1前）和stamp17（路由／设备计划后）各写一次时间戳，每层两个，
加窗口首尾共98个标记／窗口；800字节回读仍随原有最终等待发布。
这比两标记父诊断增加96道排序边界，会改变依赖／排程；下表不是无探针吞吐或独占kernel时间。

原父诊断使用的同一固定5771槽、reserve1536MiB、MTP4/min-p0.5、U4/min16/M16和BCS1策略不变。
顺序Direct→L0→Direct，再L0→Direct→L0；各进程15次Short、丢弃十次保留五次。
窗口及输入／输出／草稿轨迹与自然70token参考一致；逐窗口检查48层完整性、GDN36／QSA12、
每段位于窗口首尾内、层间有序、原始tick／guard，以及Host分段和。

完整协议90/90匹配，共2610个窗口。各请求按窗口求和，进程内取后五请求中位数，外侧取几何均值。

| 顺序 | 区间 | Direct ms／请求 | L0 ms／请求 | Direct耗时变化 | 外侧漂移 |
| --- | --- | ---: | ---: | ---: | ---: |
| direct→l0→direct | 36层GDN的HC1→计划 | 161.9522 | 152.7942 | +5.994% | +0.219% |
| direct→l0→direct | 12层QSA的HC1→计划 | 55.7978 | 52.0462 | +7.208% | -0.638% |
| direct→l0→direct | 48层HC1→计划合计 | 217.8010 | 204.8404 | +6.327% | +0.045% |
| direct→l0→direct | 整个窗口GPU跨度 | 1218.7196 | 1177.6882 | +3.484% | +0.333% |
| direct→l0→direct | Host提交至返回 | 1228.5516 | 1183.4486 | +3.811% | +0.400% |
| l0→direct→l0 | 36层GDN的HC1→计划 | 161.9910 | 152.8325 | +5.993% | +0.055% |
| l0→direct→l0 | 12层QSA的HC1→计划 | 55.6938 | 52.0961 | +6.906% | -0.746% |
| l0→direct→l0 | 48层HC1→计划合计 | 217.7223 | 204.8611 | +6.278% | -0.113% |
| l0→direct→l0 | 整个窗口GPU跨度 | 1212.8188 | 1177.7957 | +2.974% | -0.151% |
| l0→direct→l0 | Host提交至返回 | 1225.1875 | 1183.0721 | +3.560% | -0.192% |

预设整体GPU窗口／Host墙钟漂移门（3%）：True。

阶段不等于纯HC1：它还包含router GEMV、Top-K、量化与device-plan及其排序空隙。
不得把本表百分比加到两标记父诊断，也不从CPU／GPU时钟差值推导唤醒延迟；
阶段排名仍受新增探针影响，不单凭本组采用新的运行库策略。

生产源码／默认／部署未改变；没有新增长上下文、PPL或LinuxGPU验证，不关闭原Direct8K间歇问题。
实际编译依赖、源转换、完整原始协议／时间戳与参数保存在JSON和文本归档，资产使用stat-cached SHA256校验。

```powershell
python -X utf8 build-orca-current-window-20261106/finish_route.py --check
```

[两标记父诊断](orca-direct-current-window-2026-11-06.md) · [已有Host分段](orca-direct-current-host-split-2026-11-06.md)
JSON (`direct-current-route-phase-a770-windows-2026-11-06.json`, raw capture retained locally) · 原始文本 (`direct-current-route-phase-a770-windows-2026-11-06.json.gz`, raw capture retained locally)

首次发布入口因搜索路径误导入旧目录的同名publish.py，在写入任何发布文件前退出1。
本入口显式加载当前目录的发布器；原采样脚本、测量结果和全部协议保持不变，未重跑GPU。
本组主判据只包含整体GPU跨度和Host墙钟；Host准备／提交等小分项有超过3%的漂移，不据其建立稳定改善。HC1→计划区间不等于纯HC1时间。
