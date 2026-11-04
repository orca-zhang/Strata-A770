<div align="center">
  <h1>Strata-A770</h1>
  <p><strong>一张 Intel Arc A770 16 GB，配合系统内存，在本地运行 125B MoE 模型。</strong></p>
  <p>Qwen3.8-Flash-Next · 推荐 Orca L0 · Orca Direct（WIP／备选）· 网页聊天与兼容 API</p>
  <p><a href="#在-windows-上开始运行">开始运行</a> · <a href="#速度有多快">性能实测</a> · <a href="#继续了解">文档</a> · <a href="README.upstream.md">Upstream / English</a> · <a href="LICENSE">MIT</a></p>
</div>

[![推荐 Orca L0：固定短代码 59.38 tok/s，相对独立实测 SYCL 42.05 tok/s 的跨组速率增幅为 41.21%；Orca Direct 标为 WIP／备选。图中展示三种 Windows 调用链路。](docs/media/orca-direct-optimization.png)](bench/results/a770/orca-direct-vs-sycl-2026-11-04.md)

**推荐 Orca L0：固定短代码热态 59.38 tok/s，反向复测 59.57 tok/s。** 2026-11-04 的 A770 同策略双向对照中，Direct 为 **58.32／58.30 tok/s**，低于各自同期 L0 **1.80%／2.13%**。每个进程十次预热、五次保留，两轮共 90 次完整输出和草稿计数一致。[L0 对照与条件](bench/results/a770/orca-direct-idle-audit-2026-11-04.md)。

以独立实测的原生产 SYCL **42.05 tok/s** 为参照，L0 **59.38 tok/s** 的速率增幅为 **41.21%**。这是同机固定输入、相同 CLI／MTP 设置下的**跨组参考**，不是 L0／SYCL 同期夹测。图中保留这一口径和三种调用链路。[数据来源与计算](bench/results/a770/orca-direct-vs-sycl-2026-11-04.md)。

**Orca Direct 标为 WIP／备选。** 独立的 SYCL／Direct／SYCL 夹测得到 **42.05 → 58.48 tok/s（+39.07%）**，45 次完整输出及草稿计数一致，SYCL 前后耗时漂移 **−0.97%**。这是包含数值内核、调度与缓存策略差异的整版本对照；固定输入不代表日常混合请求速度。[完整样本与哈希](sycl/orca/direct-vs-sycl-a770-windows-2026-11-04.json) · [旧版本实测](bench/results/a770/orca-direct-anchor-2026-11-04.md)

Strata-A770 基于 [Strata](https://github.com/Niko1221/Strata)，针对 Intel Arc A770 优化专家缓存、计算内核和任务提交。常用专家保留在显存，其余专家由系统内存承载；你可以通过网页聊天，也可以让已有应用调用本机 API。

**优先使用 Orca L0，Direct 作为 WIP 备选。** L0 通过 Level Zero 提交任务；Direct 自行编码 GPU 命令，经 Windows D3DKMT 向内核驱动提交。Windows / A770 16 GB 已完成真实模型与服务适配器检查，Linux 已有构建与 CPU 检查记录，GPU 运行尚未验证。[原生后端覆盖范围](docs/ORCA_NATIVE_BACKEND_STATUS.md)。NVIDIA / AMD 用户可从[上游使用说明](README.upstream.md)开始。

| 你想做什么 | 从这里开始 |
| --- | --- |
| 在 A770 上运行模型、使用网页聊天 | [Windows 安装与启动](#在-windows-上开始运行) |
| 接入聊天客户端、代码工具或自己的应用 | [API 接入](#接入你的应用) |
| 判断短输入和长输入的实际表现 | [性能实测与适用条件](#速度有多快) |
| 复用 Direct 或 Level Zero 运行时 | [独立 orca-l0 库](orca-l0/README.md) |

<a id="how-does-it-work"></a>

## 这个分支好在哪里

| 能力 | 实际用途与边界 |
| --- | --- |
| **显存与系统内存协作** | 查询实际显存余量，预留运行空间，让热点专家优先驻留显存。16 GB 是显卡容量，完整模型仍需要系统内存与磁盘。 |
| **Orca L0（推荐）** | 通过 Level Zero 管理设备、内存、队列与事件，支持已验证的模型和服务功能，是当前优先使用的原生路径。 |
| **Orca Direct（WIP／备选）** | 自行管理内存、队列、依赖、命令编码与回放，经 D3DKMT 对接 Windows 内核驱动。执行无需 Level Zero、Intel GPU 用户态运行库、SYCL、UR 或 oneMKL；离线构建内核仍需要 Intel 编译器。Linux DRM 路径尚待 GPU 验证。 |
| **A770 计算与调度优化** | 调整专家换入、量化与激活融合、subgroup 和 XMX 矩阵计算路径；MTP 推测解码可在一次验证中接受多个 token。逐项收益见[性能记录](docs/A770_PERFORMANCE.md)。 |
| **本地服务与应用接入** | 沿用 Strata 网页界面，以及 OpenAI Chat Completions、Responses 和 Anthropic Messages 兼容接口。 |
| **会话与可选批量执行** | Orca L0 路径已验证会话保存、恢复及继续生成，以及 2 / 4 / 8 槽位批量。批量槽位占用专家缓存，默认关闭；磁盘会话当前要求关闭多请求批量。Direct 的功能覆盖见原生后端说明。 |
| **可单独使用的运行时** | `orca-l0/` 可独立构建，包含 Orca L0 与 Orca Direct（内部目标 `Orca::DG2`），提供模块加载、内存池、命令回放等接口；Strata 集成位于 `sycl/orca/`。 |

完整功能、检查入口和限制见 [Orca 原生引擎](sycl/orca/README.md)。不同 Arc 型号与其他模型包的速度需要分别验证。

<a id="install"></a>

## 在 Windows 上开始运行

下面是推荐的 **Orca L0 安装流程**：从源码构建引擎，再由安装程序准备模型和服务。**Orca Direct 为 WIP 备选，使用独立构建与配置**，需要匹配的离线 zebin 内核包、原生内存模块和运行时环境。[Direct 实测版本与复现](bench/results/a770/orca-direct-vs-sycl-2026-11-04.md) · [原生后端说明](docs/ORCA_NATIVE_BACKEND_STATUS.md)。

### 1. 准备环境

| 项目 | 要求或参考 |
| --- | --- |
| 显卡与驱动 | 本分支已验证 Intel Arc A770 16 GB，Windows 驱动 32.0.101.9034。 |
| 操作系统与 CPU | 已测系统为 Windows 10 22H2；使用支持 AVX2 的 x86-64 CPU。 |
| 系统内存 | 需容纳模型和专家镜像。IQ2_XS 可参考[上游 48–64 GB 的选型建议](docs/MODELS.md#pick-by-ram)；本分支测机为 256 GiB DDR3，较小内存配置尚未在此验证。 |
| 磁盘 | 为模型下载、pack 和编译产物预留空间；上游安装建议约 80 GB 空闲，优先使用 SSD。 |
| 构建工具 | VS Build Tools 的 x64 C++ 开发环境、Windows SDK、Intel oneAPI 2025.3 编译器、Level Zero SDK、CMake 3.24+、Ninja、Python 3.10+。 |

在已初始化 VS x64 与 oneAPI 2025.3 工具链的终端中操作。下面使用 PowerShell 语法，Level Zero SDK 路径按实际安装位置修改。

### 2. 构建引擎

```powershell
git clone https://github.com/orca-zhang/Strata-A770.git
cd Strata-A770
cmake -S . -B build-strata-orca -G Ninja -DSTRATA_ENABLE_ORCA=ON -DCMAKE_C_COMPILER=icx -DCMAKE_CXX_COMPILER=icx -DCMAKE_BUILD_TYPE=Release "-DORCA_L0_SDK=C:/Program Files/LevelZeroSDK/1.33.1"
cmake --build build-strata-orca -j 1
ctest --test-dir build-strata-orca --output-on-failure
```

引擎输出为 `build-strata-orca/orca/strata.exe`，内核包位于同目录的 `images/`。两者必须来自同一次构建。GPU 检查需要可用的 A770 驱动与运行环境。

A770 不使用 B 系列的 `bmg-*` AOT 目标。其他构建位置、Linux 命令与 SYCL 对照方式见[原生引擎文档](sycl/orca/README.md)。

### 3. 准备模型并启动

在仓库根目录运行：

```powershell
python setup.py --backend orca --model IQ2_XS --context 8192
```

安装程序会发现上一步的引擎，按提示下载和准备模型、pack 与 MTP 文件，生成本机配置和启动脚本，并启动服务。切换已有 Intel 配置的后端时，会备份旧配置并保留模型参数。

安装完成后，浏览器打开 [http://127.0.0.1:8080](http://127.0.0.1:8080)。以后可使用安装程序生成的脚本启动；默认 Qwen IQ2_XS 对应：

```powershell
.\run-iq2_xs.bat
```

在另一个 PowerShell 窗口检查服务：

```powershell
Invoke-RestMethod http://127.0.0.1:8080/health
Invoke-RestMethod http://127.0.0.1:8080/v1/models
```

健康检查返回 `status: ok`、`loaded: true` 后即可使用。默认监听 `127.0.0.1`；需要对外提供服务时，必须设置 `--api-key`。

<details>
<summary>手动启动与自定义引擎路径</summary>

安装程序生成的配置包含本机模型、pack、MTP 和可执行文件路径，不能直接照搬其他机器的配置。需要手动启动时，使用安装生成的 Python 环境：

```powershell
.\.venv\Scripts\python.exe sycl/serve/server_intel.py --engine strata --config strata-iq2_xs.json --host 127.0.0.1 --port 8080
```

自定义构建位置可通过 `sycl/setup_intel.py` 的 `--orca-engine` 和 `--orca-images` 指定；编译器数学库 DLL 不在 `PATH` 时，可加 `--orca-lib-dir`。原生配置使用 `backend: orca`，并通过 `STRATA_ORCA_IMAGES` 指向匹配的内核包。见[完整安装说明](sycl/orca/README.md)。

</details>

## 接入你的应用

| 使用方式 | 地址或设置 |
| --- | --- |
| 浏览器聊天 | `http://127.0.0.1:8080` |
| OpenAI 兼容客户端 | Base URL：`http://127.0.0.1:8080/v1`；模型列表：`GET /v1/models` |
| Chat Completions | `POST http://127.0.0.1:8080/v1/chat/completions` |
| Responses API | `POST http://127.0.0.1:8080/v1/responses` |
| Anthropic Messages | `POST http://127.0.0.1:8080/v1/messages` |

模型名可填 `strata`；服务接受兼容客户端传入的模型名，不会据此自动切换模型。未启用认证时，要求填写 API Key 的客户端可填任意非空值；启用认证后使用配置的密钥。

用 PowerShell 发送一次聊天请求：

```powershell
$body = '{"model":"strata","messages":[{"role":"user","content":"Say hello in one sentence."}],"max_tokens":128,"reasoning_effort":"none"}'
Invoke-RestMethod http://127.0.0.1:8080/v1/chat/completions -Method Post -ContentType "application/json" -Body $body
```

启用认证后，为请求加上配置的密钥，例如 `-Headers @{ Authorization = "Bearer <你的密钥>" }`。流式输出、工具调用及 API 参数见[技术说明](docs/DETAILS.md#using-it)。

## 速度有多快

以下结果来自 **A770 16 GB + Xeon E5-2696 v4 + 256 GiB DDR3 + Windows 10 22H2**，使用 Qwen3.8-Flash-Next IQ2_XS。各实验分别注明日期与协议。生成速度用 tok/s 表示；首 token 延迟（TTFT）与完整请求耗时越低越好。

### 当前推荐：Orca L0

2026-11-04，完整前端重建后的同策略双向对照如下。每个进程十次预热、五次保留；两侧参考取两个进程中位数的几何均值。

| 对照顺序 | Orca L0（tok/s） | Direct（WIP，tok/s） | Direct 相对 L0 |
| --- | ---: | ---: | ---: |
| L0 → Direct → L0 | **59.3843** | 58.3180 | −1.7956% |
| Direct → L0 → Direct | **59.5705** | 58.3002 | −2.1324% |

两轮共 90 次请求，28-token 输入、76-token 自然输出、57／57 草稿计数均一致，`reused=resume=0`。固定 5771 槽／7922 MiB 缓存、预留 1536 MiB、`short-read=256`、`adapt-decay=0.95`、MTP4、累计／末草稿门槛 0／0.5。[完整条件、样本与复现](bench/results/a770/orca-direct-idle-audit-2026-11-04.md)。

相对独立实测的原生产 SYCL 基线 **42.0545 tok/s**，L0 **59.3843 tok/s** 的速率增幅为 **41.2079%**。这是跨组参考，不是 L0／SYCL 同期夹测。[数据来源与计算](bench/results/a770/orca-direct-vs-sycl-2026-11-04.md)。

### Orca Direct：WIP／备选

独立的 SYCL／Direct／SYCL 夹测为 **41.8502 → 58.4840 → 42.2598 tok/s**，Direct 相对 SYCL 几何基线提升 **39.0672%**。45 次完整输出及草稿计数一致；这是包含数值内核、调度与缓存策略差异的整版本对照，不是纯 API 消融。[本轮记录与调用链](bench/results/a770/orca-direct-vs-sycl-2026-11-04.md)。

同策略下 L0 仍更快，Direct 暂不作为优先路径。当前前端的 2K／8K 对照和长输入的已知问题见[上下文测试记录](bench/results/a770/orca-direct-current-contexts-2026-11-04.md)。

### 如何理解这些数字

速度取引擎 DONE 的净解码时间，排除加载、请求准备、预填充与 HTTP。`temperature=0` 选择贪心采样，不启用回答缓存；测试显式关闭 prompt 缓存，但重复同一输入仍会预热专家缓存。固定短输入的热态速度不代表混合请求或长上下文的速度。GPU 时钟未锁定，单组样本范围不代表跨机器保证。

当前的 FP16 双专家预填充、缓存调度和命令回放优化及其独立实验，统一保留在 [A770 性能记录](docs/A770_PERFORMANCE.md)和[原生后端状态](docs/ORCA_NATIVE_BACKEND_STATUS.md)中。历史结果按原构建和协议归档，不再在首页并列表述为当前性能。

新图可用 `python tools/render_orca_optimization.py` 从正式记录重画，`--check-only` 离线核对数据；[SVG](docs/media/orca-direct-optimization.svg) 可用于导出。

<a id="something-went-wrong"></a>

## 常见问题

**16 GB 显存能装下完整模型吗？** 完整模型由显存、系统内存与磁盘共同承载。16 GB 指 A770 显存容量，系统内存仍需按模型选择，见[模型与内存说明](docs/MODELS.md)。

**可以直接用上游安装包吗？** A770 / Orca 路径需要本地构建的引擎与配套内核包；完成构建后使用 `--backend orca` 安装。需要对照旧 SYCL 路径时使用 `--backend sycl`。

**Linux 或其他 Arc 显卡能用吗？** Orca Direct 已有 Linux 构建和 CPU 检查记录，Linux GPU 运行与模型性能尚待验证；其他 Arc 型号没有本轮性能数据。具体覆盖范围见[原生后端说明](docs/ORCA_NATIVE_BACKEND_STATUS.md)，上游 SYCL 的社区验证情况见 [Intel Arc 文档](docs/INTEL_ARC.md)。

**启动失败或长输入很慢，先看哪里？** 核对引擎与 `images/` 是否来自同一构建、配置中的模型路径是否有效，以及内存和显存余量。保留 `strata-<model>.log`；已知性能边界见[上下文测试记录](bench/results/a770/orca-direct-current-contexts-2026-11-04.md)，其他排查见[故障处理](docs/TROUBLESHOOTING.md)。

## 继续了解

| 想了解什么 | 文档 |
| --- | --- |
| 每项优化、测量条件与历史结果 | [A770 性能记录](docs/A770_PERFORMANCE.md) |
| 原生引擎构建、功能与检查 | [Orca 集成说明](sycl/orca/README.md) |
| Direct 原生后端覆盖、实测与限制 | [原生后端状态](docs/ORCA_NATIVE_BACKEND_STATUS.md) |
| 可独立复用的 Direct / Level Zero 运行时 | [orca-l0](orca-l0/README.md) |
| Intel 平台支持与 SYCL 背景 | [Intel Arc](docs/INTEL_ARC.md) |
| 模型选择与系统内存 | [模型指南](docs/MODELS.md) |
| API、参数与引擎结构 | [技术说明](docs/DETAILS.md) |
| 安装自动化与 MCP 管理 | [AI 安装指南](docs/AI_SETUP.md) · [MCP Server](docs/MCP_SERVER.md) |
| NVIDIA / AMD 安装与其他语言 | [上游 English](README.upstream.md) · [简体中文](README.zh-CN.md) |

开发者可先运行无需 GPU 的安装检查；GPU 检查使用前面的 CTest 命令，性能测试需串行运行并固定模型、缓存与预热条件：

```powershell
python tools/test_setup_sycl.py
python tools/bench_a770.py --self-test
```

## 致谢与许可证

感谢 [Strata](https://github.com/Niko1221/Strata) 作者与贡献者、最初的 Intel SYCL 移植作者 maxfridbe、Qwen 团队，以及 [llama.cpp / ggml](https://github.com/ggml-org/llama.cpp) 和 Intel oneAPI。

项目沿用 [MIT License](LICENSE)。第三方代码及模型遵循各自许可证，原有版权与出处保留。
