# Direct SG16 HC Q8：采用小幅正收益的128 GRF默认值

2026-11-06，用户明确允许采用此前幅度略低的改善后，Direct构建默认取消这一融合Q8映像的256-GRF覆盖。
在已校准的Windows／Arc A77016GB／驱动32.0.101.9034／oneAPI2025.3.2上对应128 GRF；L0继续256 GRF。
CMake仅改变一个条件；其余数学内核及运行库不变。重建并重新烘焙后生效；现有部署EXE、启动配置及生产L0包未修改。

采用依据为[已归档的相同SPIR-V寄存器对照](orca-hc-sg16-grf-2026-11-06.md)：
Direct净decode正反顺序分别 **+1.142%／+0.743%**，TTFT分别−0.237%／−0.385%，整请求−0.803%／−0.686%。
两顺序均改善，原正确性、控制漂移和时间非回退门均通过；原两顺序各≥1%的收益门仍记为失败。
本次采用依据是用户接受小幅改善，未改写历史门槛、结果或追加重复取通过。
L0仅+0.070%／+0.059%的旧观察不据此切换；当前变更聚焦Direct。

旧试验已有256项完整HC检查和180次完整Short协议。128／256来自相同SPIR-V，
参数ABI相同，均为SG16、9728字节声明private、零spill，代码也随GRF策略改变。
这些是有限同日Short观察，不能视为置信区间或广泛PPL／Linux GPU／长期稳定性结论。
本次另核对当前数学源码经原prepare变换与已测SG16完全相同，并实际配置Direct／L0、
读取生成的Ninja命令，验证Direct省略该覆盖、L0保留，其他单内核GRF策略保持。
本次未新增模型性能或长上下文测量；历史Direct8K间歇故障仍未关闭。

变更前CMake原始字节及摘要已加入现有冻结源登记，确保旧试验仍能精确复核。
归档保留用户采用依据、原失败门、前后源码、差异、四项配置／命令检查和源摘要。
构建配置、重型哈希及归档持共享锁，不分发模型或二进制。

复核本次采用，不运行GPU：

```powershell
python -X utf8 build-orca-hc-grf-adopt-20261106/adopt.py check
python -X utf8 build-orca-hc-grf-adopt-20261106/finish.py --check
```

[JSON](../../../sycl/orca/hc-sg16-grf-adoption-a770-windows-2026-11-06.json) · 精确文本归档 (`hc-sg16-grf-adoption-a770-windows-2026-11-06.json.gz`, retained locally for privacy)
