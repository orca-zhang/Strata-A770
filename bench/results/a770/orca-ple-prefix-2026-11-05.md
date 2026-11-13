# PLE 提前准备：复用前缀，先提交剩余行再等待

2026-11-05，Windows，Arc A770 16 GB，驱动32.0.101.9034，oneAPI2025.3.2。
服务默认在当前 commit 提交后预读下一窗口已知首 token；本次补齐消费端：
`PleTable::gather_batch` 复用顺序一致的预读前缀，先提交其余行，再 collect 并解码。
顺序不一致仍走整批回退；所有写者完成后才释放／复用目的缓冲，读取错误仍返回失败。
没有新增线程、GPU kernel、GPU复制或环容量；来源和数据就绪约束保留。

例如首 token 已预读、随后消费4个 token：旧路径请求16+64=80行，
新路径请求16+48=64行。这是逻辑请求量，不是实测SSD／PCIe物理流量或整模型提速。
匹配前缀全部覆盖时不新增后缀请求；更大批次仍对剩余行去重、排序后统一提交。

原有 `ple_reader_test --selftest` 增加零行缓存的回归检查，覆盖7种格式、同步／工作线程、
Direct／Mmap、空批和1／4／19-token批次、0／1／2／4／6／16个预读槽、完整数值、
边界保护及不匹配前缀的旧数据拒绝。最终每套reader检查336项前缀／批次组合。
旧gather与新检查器组合必须失败，且只报告重复请求；新gather分别链接上游与SYCL reader，
都通过完整自检。初始较小检查和最终扩展检查均保留，没有覆盖旧尝试。

Orca原生CMake新增纯CPU目标 `strata-orca-ple-reader-check`，实际Windows配置、构建及CTest通过；
不构建设备映像，也不链接GPU运行时。PE直接导入检查未发现SYCL／UR；
Direct及CPU检查器无Level Zero loader，L0服务保留预期loader。
Linux完整检查脚本的 `--frontend` 同时构建并执行该CPU检查，Linux GPU仍由后续实机验证。

真实U4服务重新生成并编译完整generate Host TU，链接已保留的U4/load4/M16依赖；
数学pack、5771实际专家槽、1536MiB预留、MTP4、step/last min-p0.5、累计门槛0、read256、
decay0.95、suffix0、prompt-cache0均固定。沿用128-GRF映像，本轮不是HC256的叠加成绩，
也不是全仓库当前源码的完整重建。镜像异步回填保持关闭。

顺序为L0关闭→L0缺省开启→Direct关闭→Direct缺省开启，均启动新进程，
直接执行Code2K→Long8K→Prose2K→Short，各最多生成128 token，不以Short预热8K；Short遇EOS结束。
关闭臂使用同一新Host TU和旧gather，开启臂同时采用新gather前缀复用。
全部16次完整token、生成数、草稿提供／采纳、结束条件和零前缀复用与冻结U4参考匹配，
四个进程正常退出。保留原始时间字段，但这组用于正确性，不建立新的TTFT或decode收益。
未替换生产EXE／主配置，源码重建后默认生效；`STRATA_ORCA_PLE_ROW0=0`关闭服务端提前预读。
未测广泛PPL或所有采样组合；原Direct8K间歇故障仍未关闭。

原生构建可复查：

```sh
cmake --build <orca-build> --target strata-orca-ple-reader-check
ctest --test-dir <orca-build> -R '^strata-orca-ple-reader$' --output-on-failure
```

在本次工作区／恢复相同依赖后核对完整证据：

```powershell
python -X utf8 build-orca-prepare-u4-20261105/publish.py --check
```

编译、GPU、重型哈希与归档使用共享实验锁。输入、命令、源码、完整协议、初始／最终CPU检查
和原有主配置哈希均随归档保留；不分发模型、可执行文件或对象文件。
[JSON](../../../sycl/orca/ple-prefix-a770-windows-2026-11-05.json) ·
文本归档 (`ple-prefix-a770-windows-2026-11-05.json.gz`, retained locally for privacy)
