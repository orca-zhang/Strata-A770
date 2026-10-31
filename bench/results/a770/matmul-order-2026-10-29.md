# A770：DeepGEMM SwapAB 的来源与适用范围

找到了与用户描述高度吻合的来源：[DeepGEMM PR #192](https://github.com/deepseek-ai/DeepGEMM/pull/192)，补丁头为 `2991c774d0ae7ee066622bcbec27f7a71ff31ef3`。作者在H20的FP8分组GEMM上报告了超过50%的算子吞吐提升。本轮没有取得A770的同等收益，也没有新增整模型速度结论。

## 原始记录与单组问题

以下只重算作者报告的耗时，均为16组、严格对齐的M；不是本机测量。吞吐变化按`原耗时/新耗时−1`计算。

| 每组M | N | K | 原版→SwapAB | 吞吐变化 |
| ---: | ---: | ---: | ---: | ---: |
| 32 | 4096 | 7168 | 257→154µs | +66.883% |
| 32 | 7168 | 2048 | 127→84µs | +51.190% |
| 160 | 4096 | 7168 | 921→572µs | +61.014% |

`num_groups=1`只取消专家分组，整张矩阵仍会拆成许多CTA。对用户引用的形状，A为1024×7168，W为4096×7168，结果为1024×4096。SwapAB实际计算转置关系`D=A·Wᵀ`、`Dᵀ=W·Aᵀ`，并配套改变写回布局。它不依赖矩阵乘法可交换。

原WGMMA指令的M固定为64，N可按8的倍数变化。交换输入角色后，问题的M映射到指令的N，允许问题的`BLOCK_M=32`。PR同时把`BLOCK_N`设为256，并调整math warpgroup数与多波MMA；即使M完全对齐，也可能因CTA粒度、输入复用、每组资源、流水阶段及末波利用率的变化而加速。不能把全部收益归于减少尾块空算，也不能在未核对BLOCK_N和线程数时断言每CTA寄存器一定减少。

该单组other case报告349→304µs，即吞吐+14.803%、耗时−12.894%。这一组使用`random.uniform(0.7,1.3)`，1024是期望M；页面没有给出此条记录的实际M或逐次原始样本。不能将它称为已严格控制的固定1024矩阵对照。严格对齐的六组另用`random.uniform(1,1)`，两类结果分开保留。PR评论还有H100退化报告，说明方向与硬件及调度有关。

## 固定版本与本机路径

DeepInfer `9bd9cf243fc53ef9afbf8f04d391d989117d6ab9`的gitlink指向DeepGEMM `1fc9c66975ee322d628b3288534ca0e9ce12a64d`。DeepInfer的Linear仍把x作为第一个参数；换向由库内部选择。这一固定版本的SM100实现对m-grouped强制SwapAB、普通GEMM枚举两个方向，并按有效token数更新UMMA N；SM90同版本仍明确要求`swap_ab==0`。不能把历史H20补丁当作该固定版本SM90已合并的功能。

| 本机位置 | 已核对事实 | 对本次decode工作的意义 |
| --- | --- | --- |
| 当前IQ2_S GU热点 | WG256/SG16，标量dp4a，真实组长度按4/2/1处理；现有原生审计零spill | 不存在同一条WGMMA固定64行补齐；后续改lane/行/token映射须给出新的访存或依赖证据 |
| 旧FP16 XMX变体2 | 确实交换A/B与输出布局，但仍为8×8方形tile；56条投影检查packed mismatch均为0 | 未独立确认变体2；不能用它否定所有细粒度矩形tile，也不能写成已有50%收益 |
| 旧XMX变体3 | M=1/4/6等小行数已有实际检查；正式确认只选变体1/3 | A770已有小M路径；量化专家整链仍未胜出现有dp4a路径，详见[旧报告](xmx-iq2-q2-2026-10-21.md) |
| HC注入4×8转置 | 已有具体源码猜想：省N4补N8和scatter，检查repeat-count=4 | 当前是prefill候选；实际decode链接的hc-streams用BF16权重/FP32激活FMA，尚不能移作同精度decode候选 |
| GU组坐标交换 | 混合驻留双基线重复筛选未通过 | 这是网格坐标变化，不能当作矩阵指令SwapAB，详见[坐标实验](gu-interleave-2026-10-28.md) |

本机DPC++2025.3的`sycl/ext/intel/esimd/xmx/dpas.hpp`另明确规定DPAS的M等于repeat count（1–8），N等于execution size（8或16）。这与WGMMA的固定M/灵活N不同，不能直接照搬“把token移到N侧”。这是已安装头文件中的参数约束，尚不是新候选的实际native或性能结果。

按ezpp层次，当前属于第二层的矩阵映射/流程检查；进入实现时仍须用第一层的原生指令、GRF、spill、SLM核对，第三层只删除被证明不需要的padding/转换/提交，第四层记录A770实际带宽与频率。现有证据支持继续寻找相同原理的机会，尚不支持启动无目标的SwapAB选项扫描。

## 可复核材料

本机`build-sycl-a770-matmul-order-review-20261029/`保留两个固定源码zip、PR及相关issue原页面、完整补丁、全部16组报告值，以及本地变体2的56条记录。只读检查验证gitlink、实际SwapAB/tile源码、原始数值与全部材料哈希，不运行GPU：

```powershell
python -X utf8 build-sycl-a770-matmul-order-review-20261029/review.py --check
```

[Issue114](https://github.com/deepseek-ai/DeepGEMM/issues/114)另有小M约30%–50%的报告，但比较了masked与offset grouped接口；不将它单独当作只交换A/B的因果证明。[Issue320](https://github.com/deepseek-ai/DeepGEMM/issues/320)解释了SM100的动态UMMA和双CTA，并补充纯grouped约15%的报告；与历史H20的50%以上分开记录。

最终目标仍是同一个EXE、相同负载下decode吞吐至少+20%，先Windows、后Linux。上游算子数字、静态指令下降、独立组件收益均不能替代这一验收。
