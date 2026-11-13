# A770：原生 Host 页面导入与大块分配（2026-11-08）

本轮继续移除 Windows Direct 的标准 Host 分配路径。下述为驱动校准和源码验证，没有新增 tok/s、TTFT 或 PPL 数据。部署 EXE、默认参数未替换；Linux GPU 待实机验证。

此前自有 Host 分配已采用原生路径；仍使用 Standard ExistingHeap 的是外部页面导入，以及超过 2 GiB 的自有分配。本候选对后两者使用调用者页面描述和已有连续 GPU VA 分段机制，移除隔离源码中全部 StandardAllocation／共享资源标志。自有大块内存仍由 Runtime 创建和释放一个连续 VirtualAlloc 映射，借用的调用者内存由调用者保有。

Arc A770 16GB，Windows 驱动 32.0.101.9034。通过实际 zexDriverImportExternalPointer 校准 64 KiB、2 MiB＋64 KiB、16 MiB、重复 64 KiB，以及 2 GiB−64 KiB、2 GiB、2 GiB＋64 KiB、4 GiB＋64 KiB。八次导入的全量 CPU 内容、内部地址基址查询、释放后调用者所有权全部通过，没有提交 GPU 命令。

八次请求产生九份私有分配描述，CreateAllocation 标志均为零。4 GiB＋64 KiB 请求在厂商路径分为 4 GiB−64 KiB 和 128 KiB；Orca 候选的编码器上限为每段 2 GiB，同请求分为三段。两者分段策略不同，不能从校准直接推定候选 GPU 行为或性能。

候选实际 C++ 编码器与适用范围内七份实测描述逐字节一致，包含 2 GiB 边界；仅归零已校准的驱动标识／填充共 60 字节，三个调用者指针字段保留并参与精确比较。另两份大于 2 GiB 的厂商描述明确不在当前编码器范围，未计为匹配或忽略差异。导入描述保持 64 KiB 对齐，不能套用自有 Host 分配的大块 2 MiB 对齐规则。

复用已校准的离线字段探针对全部九份导入描述核对 NoRestriction：仅 offset0x1e／mask0x04 影响该命名 token，2 GiB 以下为0，达到或超过2 GiB时为1。子进程退出0；外层打印WSL的UTF-16-LE stderr时误用UTF-8，导致日志解码异常。保留原始字节及外层失败，没有重跑探针或修改子进程结果。该检查没有GPU或编译调用。

第一版准备脚本在源码还原自检中因空字符串替换计数断言失败，尚未获取 GPU；原错误和源文件保留。下一版驱动校准和逐字节检查通过，但离线语义检查将新参考头的完整结构长度当作已安装驱动长度，提前失败。失败保留；修订检查改用驱动返回的动态 token 长度及命名字段 ID，复用九份实测描述，不重跑硬件校准。

隔离候选的检查包括 4 GiB＋64 KiB 自有／借用内存全量内容、跨 2 GiB／4 GiB 的 GPU 读写及全缓冲区保护、五处导入失败清理，以及已有完整注册 CTests。所有权检查要求同步完成解除固定后才释放自有页面，并确认借用页面在 Runtime 退出后仍有效。

19项注册CTests和全部离线语义检查已通过。五份变更文件按通过检查的冻结源码逐字节移入主工作树，其他所有源码文件也核对一致；未重复运行相同检查。Windows Direct源码中的StandardAllocation路径已全部移除，采用时源码及差异另行冻结归档。此为功能及依赖验证，不代表新增整模型性能收益；Direct仍为WIP，L0仍为推荐后端。

冻结模型对照使用 --stream-experts，没有外部 PinnedArena；专家镜像使用运行时自有 Host 分配。因此，外部页面导入校准用于补齐运行时能力，不能直接解释此前约 2% 的 Short decode 差距。超过 2 GiB 的自有分配是否影响模型也仍需实际调用与整模型核对。

[结构化记录](../../../sycl/orca/direct-native-import-a770-windows-2026-11-08.json) · 冻结源码与原始文本日志 (`direct-native-import-a770-windows-2026-11-08.json.gz`, raw capture retained locally)。归档排除带进程指针的原始私有二进制和仍在运行的检查文件。

复核：`python -X utf8 build-orca-native-import-20261108/publish.py check`。归档按 path 还原后可检查每个文件的 SHA256；保留每轮原始失败。新 GPU 采样使用新输出目录，不覆盖已有结果。
