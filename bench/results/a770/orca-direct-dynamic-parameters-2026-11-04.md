# Orca Direct：动态参数留在主机，静态图保留 device 参数

2026-11-04，Windows 10 22H2，Intel Arc A770 16 GB，驱动 `32.0.101.9034`。

这轮验证动态路径避免同步上传参数的可行性。原 Direct／候选／原 Direct 夹测中，候选 2K／8K 预填充分别减少约 **5.77%／2.77%**；长请求每个进程各一次，是有限观察，未建立稳态或长请求重复性。Short 未见吞吐收益。候选保持隔离，**未采用到生产源码、默认配置或部署 EXE**。

## 变化与生命周期检查

[此前诊断](orca-direct-current-contexts-2026-11-04.md)记录每个 8K prompt 约 17,000～19,000 次同步参数上传；范围重叠的探针时间不能当端到端可节省时间。

先将所有参数改为主机放置作单变量正确性消融；再实现动静态分流：eager 列表使用 host 参数，录制图保留原 device mirror 策略。内存辅助模块继续使用既有策略。放置模式变化时强制重新捕获参数，避免 host 更新未上传却复用旧 device mirror。只有 runtime、参数缓存和适配器的六个源码文件变化；重编译 80 个 Host 单元，69 个实际使用新适配器，数学模块与主程序策略保持冻结。

真实 GPU 专用检查通过 32 次提交：16 个交替 host／device 的保留列表，真正修改 inline 区外的 indirect row-count 参数，覆盖相同 payload 缓存命中，再逆序重放全部旧列表。核对 Q3_K 数学参考、完整输出、未写尾部及输入／scratch／private guards。CPU ABI／encoder 检查也通过。

## 模型夹测

同一固定配置：实际 5,771 专家槽、1,536 MiB 预留、read256／decay0.95、MTP4、step／last 门槛 0.5、cumulative0、margin1.5，关闭 suffix draft 和 MTP cost window。这是锚定配置，不能当日常平衡参数吞吐。

每进程先 15 次 Short（丢弃 10 次、保留 5 次），再各一次 2K、8K。三个进程共 **51 次请求，全部输出 token、终止条件及草稿工作量匹配**。两侧参照取几何均值。净 decode 用 DONE 时间，排除初始化、准备和预填充；TTFT 独立统计。

没有内部计时／探针／GPU 时间戳／遥测，也没有并行编译，未锁 GPU 时钟。

| 输入 | 原 Direct 预填充 ms | 候选 ms | 时间变化 | TTFT 变化 | 净 decode 吞吐变化 | 两侧预填充漂移 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Short28 | 893.90 | 894.30 | +0.0449% | +0.0722% | -0.5905% | +0.3137% |
| Code2K | 28611.30 | 26961.80 | -5.7652% | -5.7545% | -0.6312% | -0.9797% |
| Long8K | 81776.27 | 79511.40 | -2.7696% | -2.7700% | +1.0435% | -0.1664% |

这里只比较 Direct 自身的参数策略。长请求的缓存历史与此前 L0 对照不同，不把两组绝对值拼接为“已经追平／超过 L0”。Short 的 -0.59% 也没有通过这一组有限样本建立退步显著性。

## 额外正确性检查与未解决问题

四组新进程冷历史检查，各为两次 2K 后一次 8K：普通 Direct、尾行 finite 探针、全 host 参数、动静态分流，合计 12 次均匹配。finite 探针只检查 96 个半层的尾行，未记录非有限值；它扰动调度，不能排除其他行或其他时序的问题。

此前普通 Direct 和诊断 EXE 都曾在缩短历史的 8K 请求中输出连续 token0、草稿 0/0；后续同历史又可能通过。**这个间歇性正确性异常仍未解决**，本轮通过不能关闭它。没有测 PPL、Linux GPU 或全模型质量等价。

第一次 placement 检查构建因单文件 `/Fo` 与静态库作为输入冲突而失败，日志保留；v2 拆分编译与链接后成功。冷历史仅用于正确性，不使用其 pilot 耗时作为性能结论。

## 复现与证据

同一冻结构建、模型、数学模块、驱动、输入和参数下可复现此流程。二进制与模型不分发，原日志、命令、六处源码、依赖文件、配置与哈希均保留；基础完整前端快照见[此前报告](orca-direct-current-frontend-2026-11-04.md)。模型数据文件本轮未完整重哈希。

```powershell
python -X utf8 build-orca-direct-gap-20261104/dynamic_parameter_ablation.py --verify
python -X utf8 build-orca-direct-gap-20261104/parameter_placement_checks.py --verify
python -X utf8 build-orca-direct-gap-20261104/parameter_context_bracket.py --verify r1
```

新测量使用 `parameter_context_bracket.py r2` 等新编号，原有目录不能覆盖；重新构建前须准备报告链接中的冻结基础源及对象文件。

精确 JSON (`direct-dynamic-parameters-a770-windows-2026-11-04.json`, raw capture retained locally) · 原始证据档案 (`direct-dynamic-parameters-a770-windows-2026-11-04.json.gz`, raw capture retained locally)
