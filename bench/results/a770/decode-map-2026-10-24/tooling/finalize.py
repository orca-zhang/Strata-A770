"""Apply the exact candidate if its declared operator rule passes; archive raw evidence."""
import importlib.util
import json
import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path

F = Path(__file__).resolve().parent
R = F.parent
A = R / 'bench/results/a770/decode-map-2026-10-24'
assert not (A / 'archive.json').exists()
assert json.loads((F / 'results.json').read_text())['complete']
spec = importlib.util.spec_from_file_location('decode_audit', A / 'check_results.py')
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)


def copy(source, target):
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, target)


def save(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8')


for p in F.iterdir():
    if p.is_file() and (p.suffix in ('.json', '.log') or p.name in ('run.py', 'prepare.py')):
        copy(p, A / p.name)
prep = audit.read('preparation.json')
for arm in ('baseline', 'candidate'):
    copy(F / arm / 'strata_tokenizer.py', A / arm / 'strata_tokenizer.py')
for name in prep['frozen_sha256']:
    if not name.endswith('.pyd'):
        copy(F / name, A / name)
runs, pairs = audit.derive()
result = audit.read('results.json')
assert result['runs'] == runs and result['pairs'] == pairs
measurement = audit.summarize(pairs)
effective = audit.user_rule(pairs)
live = R / 'tools/strata_tokenizer.py'
baseline, candidate = ((F / arm / 'strata_tokenizer.py').read_bytes() for arm in ('baseline', 'candidate'))
assert live.read_bytes() == baseline
for path, digest in prep['production_sha256'].items():
    assert audit.sha(Path(path)) == digest
decision = 'adopted' if effective['passed'] else 'not_adopted'
if decision == 'adopted':
    live.write_bytes(candidate)
try:
    env = dict(os.environ, STRATA_TOKENIZER=prep['tokenizer_path'], PYTHONUTF8='1')
    test = subprocess.run([sys.executable, '-m', 'unittest', 'serve.test_detok', '-v'],
                          cwd=R, env=env, capture_output=True, text=True)
    (A / 'live-tests.log').write_text(test.stdout + test.stderr, encoding='utf-8')
    test.check_returncode()
    assert 'Ran 11 tests' in test.stderr and 'skipped' not in test.stderr
except BaseException:
    if decision == 'adopted' and live.read_bytes() == candidate:
        live.write_bytes(baseline)
    raise
copy(R / 'serve/test_detok.py', A / 'regression/test_detok.py')
save(A / 'summary.json', dict(decision=decision, measurement=measurement, user_rule=effective,
                             scope='CPU batch token decode only; streaming is an unchanged-path control; no model/HTTP timing.'))
for path, digest in prep['production_sha256'].items():
    assert audit.sha(Path(path)) == digest
import regex
save(A / 'final-checks.json', dict(runtime_source_sha256=audit.sha(live),
                                  production_sha256={p: audit.sha(Path(p)) for p in prep['production_sha256']},
                                  live_regression_returncode=test.returncode,
                                  live_regression_source_sha256=audit.sha(R / 'serve/test_detok.py'),
                                  platform=platform.platform(), python=sys.version, regex_version=regex.__version__,
                                  cpu='Intel Xeon E5-2696 v4 @ 2.20GHz', logical_cpus=44,
                                  expert_cache_capacity=None, model_speedup_claim=False))
for path in ('tools/bench_deepinfer.py', 'build-sycl-deepinfer-20261023/check_bf16_arena.py'):
    copy(R / path, A / 'tooling' / Path(path).name)
copy(Path(__file__), A / 'tooling/finalize.py')
rows = []
for name in prep['cases']:
    values = [p for p in measurement['phases'] if p['name'] == name]
    cells = ['无合格配对' if p['pooled_gain_pct'] is None else f"{p['pooled_gain_pct']:+.2f}% ({p['eligible']}/2)" for p in values]
    rows.append(f"| `{name}` | {' | '.join(cells)} |")
excluded = sum(not p['eligible'] for p in pairs)
max_drift = max(abs(p['baseline_drift_pct']) for p in pairs)
(A / 'README.md').write_text(f'''# 批量 token 解码内置 map（2026-10-24）

结论：{'合入一行源码' if decision == 'adopted' else '不合入候选'}。基线已包含前两轮 token_bytes 优化。
使用 `b"".join(map(self.token_bytes, ids))` 替换生成器，字节转换、缓存和 UTF-8 错误策略不变。
这是 CPU 批量 token 解码算子测量，不代表模型 decode、TTFT 或 HTTP 吞吐提升；流式 push 不经过修改行。

Xeon E5-2696 v4、Windows、CPython 3.11.9、逻辑 CPU 40。
等待共享 GPU/编译锁后，两阶段独立进程 A/B/A/B/A；每进程每测点预热5次、计时7批。
每批调用数见 preparation.json；构建 tokenizer、encode 和模型加载均不计时，未开启 profiler。
decode64/16k 使用混合语言/代码输入；Code/Prose 为各2K token；cold4k 固定种子抽取4096个词表ID、每次清空字节缓存。
所有输出哈希跨进程相同。真实模型/专家缓存未加载，实际专家缓存容量不适用。

| 测点 | 筛选吞吐收益（合格配对） | 确认吞吐收益（合格配对） |
|---|---:|---:|
{chr(10).join(rows)}

相邻基线绝对漂移超过3%的配对排除；共排除{excluded}/24，最大漂移{max_drift:.2f}%，原因未单独隔离。
保留全部原始负值和排除项，不补采样寻求通过。收益采用相邻基线几何平均耗时除以候选耗时，再对合格配对取几何平均。
脚本事先额外要求 decode16k 两阶段均有合格正收益，该主测点门槛未通过，原规则与失败判定保留在 preparation.json / summary.json。
该额外门槛比用户早已指定的“门槛更新成1%，算子的收益只要是正的都接受”更严。
实际采用遵从用户指令：有合格正收益的解码算子可保留源码，不以脚本的额外覆盖要求否决；没有补采样、修改漂移规则或删除负值。
decode64 两阶段各仅一组合格对照，覆盖有限；长序列筛选无合格对照，冷缓存与stream4k两阶段均无合格对照，均不声称稳定加速。
stream4k 为未执行改动的对照，其变化不归因于本次优化。

正确性：248320项真实词表冷/热共496640次字节对照；7576次流式前缀/状态对照；192次随机解码错误策略对照；11项回归零跳过。
新增最小回归覆盖空输入、NUL、多字节字符、截断/非法UTF-8、六种错误策略、list/tuple/iterator和非法ID。
合入后再次运行全部11项检查；生产EXE和配置SHA256与冻结基线相同。

```powershell
python bench/results/a770/decode-map-2026-10-24/check_results.py
$env:STRATA_TOKENIZER = 'G:\\GitHub\\Strata-data\\packs\\iq2_xs\\tokenizer'
python -m unittest serve.test_detok -v
```

复测：将私有 build-sycl-a770-decode-map-20261024 下的 run.py、preparation.json、baseline、candidate、frozen
复制到仓库根目录下新的独立构建目录（不要复制旧结果），再用以下命令运行；路径替换为该目录。
冻结输入不变，若要测试更新后的基线须新建实验；权重、词表包及生产EXE不在归档中。
frozen 中的扩展二进制仅留哈希（见 archive.json），复测需使用准备时相同的已有二进制；不新增依赖。

```powershell
python -X utf8 build-sycl-deepinfer-20261023/check_bf16_arena.py <新目录>/run.py
```

preparation.json记录源码、依赖、词表及生产文件哈希和事先门槛；summary.json记录完整判定，results.json与screen/confirm文件保留原始数据。
独立校验脚本不需要GPU或词表，重算全部60个中位数、24组配对及采用判定。
''', encoding='utf-8')
save(A / 'archive.json', dict(sha256={p.relative_to(A).as_posix(): audit.sha(p) for p in sorted(A.rglob('*'))
                                     if p.is_file() and '__pycache__' not in p.parts and p.name != 'archive.json'},
                              omitted_binary_sha256={p: d for p, d in prep['frozen_sha256'].items() if p.endswith('.pyd')}))
audit.main()
