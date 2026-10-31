"""Publish/audit the frozen option screens; no GPU needed. Each publication is write-once."""
import argparse
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import statistics

H = Path(__file__).resolve().parent
R = H.parents[2]
CASES = ('Code2K', 'Prose2K')
LABELS = ('A1', 'B', 'A2', 'C', 'A3')


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def requests(raw):
    return [dict(mode=run['label'], case=row['case'], warmup=row['warmup'],
        input_tokens=row['prompt_tokens'], output_tokens=row['generated'], reused=row['reused'],
        accepted=row['accepted'], offered=row['offered'], prompt_ms=row['prompt_ms'], decode_ms=row['decode_ms'],
        output_sha256=hashlib.sha256(json.dumps(row['tokens'], separators=(',', ':')).encode()).hexdigest())
        for run in raw['runs'] for row in run['rows']]


def summarize(rows):
    medians, ranges, paired = {}, {}, {}
    for label in LABELS:
        medians[label], ranges[label] = {}, {}
        for case in CASES:
            group = [r for r in rows if r['mode'] == label and r['case'] == case and not r['warmup']]
            assert len(group) == 3
            rates = [128000 / r['decode_ms'] for r in group]
            medians[label][case] = statistics.median(rates)
            ranges[label][case] = [min(rates), max(rates)]
    for index in (1, 3):
        before, mode, after = LABELS[index - 1:index + 2]
        paired[mode] = {}
        for case in CASES:
            a, b, c = (medians[label][case] for label in (before, mode, after))
            drift = 100 * (a / c - 1)
            paired[mode][case] = dict(throughput_change_pct=100 * (b / math.sqrt(a * c) - 1),
                baseline_time_drift_pct=drift, drift_eligible=abs(drift) <= 3,
                required_gain_to40_pct=100 * (40 / b - 1), target_tps_reached=b >= 40)
    return dict(median_tps=medians, range_tps=ranges, paired=paired)


def audit(d, local):
    assert d['complete'] and d['scope'] == 'screen' and d['target_achieved'] is False
    assert len(d['requests']) == 40 and d['summary'] == summarize(d['requests'])
    assert list(d['expert_slots']) == list(LABELS)
    for case in CASES:
        group = [r for r in d['requests'] if r['case'] == case]
        assert len({r['output_sha256'] for r in group}) == 1
    for row in d['requests']:
        assert row['input_tokens'] == 2048 and row['output_tokens'] == 128 and row['reused'] == 0
        assert row['decode_ms'] > 0 and row['prompt_ms'] > 0 and 0 <= row['accepted'] <= row['offered']
    if local:
        for name, digest in d['local_files_sha256'].items():
            assert sha(R / name) == digest, name
        raw = json.loads((R / d['raw_results']).read_text(encoding='utf-8'))
        assert requests(raw) == d['requests']
        assert d['expert_slots'] == {r['label']: r['expert_slots'] for r in raw['runs']}
    print('PASS all40 requests, complete2K outputs, medians/ranges/drift, output and evidence hashes')


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('kind', choices=('cache-draft', 'batch'))
    parser.add_argument('--publish', action='store_true')
    parser.add_argument('--local', action='store_true')
    args = parser.parse_args()
    target = H / f'decode2k-{args.kind}-2026-10-25'
    if args.publish:
        folder = R / f'build-sycl-a770-decode2k-{args.kind}-20261025'
        spec = importlib.util.spec_from_file_location('screen', folder / 'run.py')
        screen = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(screen)
        raw_path = folder / 'results.json'
        raw = json.loads(raw_path.read_text(encoding='utf-8'))
        screen.audit(raw)
        rows = requests(raw)
        d = dict(complete=True, scope='screen', target_achieved=False, rule=raw['rule'], requests=rows,
            expert_slots={r['label']: r['expert_slots'] for r in raw['runs']}, summary=summarize(rows),
            raw_results=raw_path.relative_to(R).as_posix(), local_files_sha256=dict(raw['files_sha256']))
        for path in (raw_path, Path(__file__)):
            d['local_files_sha256'][path.relative_to(R).as_posix()] = sha(path)
        # ponytail: keep write-once evidence; a correction needs a fresh publication.
        assert not target.with_suffix('.json').exists() and not target.with_suffix('.md').exists()
        audit(d, True)
        notes = ('A=完整248320草稿词表、缓存请求5500/实际5771槽、reserve1536、min_p0.5；'
                 'B=106299词表子集、缓存请求6400/实际6718槽、reserve1024、min_p0.5；C=B且min_p0.25。'
                 '词表子集额外分配137.9MiB。此处筛选组合，不能单独归因于缓存、词表或内存换页。'
                 if args.kind == 'cache-draft' else
                 '全部保留完整248320词表和5771缓存槽。A=CLI spec4/min_p0.5；B=spec4/min_p0.25；'
                 'C=spec6/min_p0.25；内部verify/MTP容量分别为6/4、6/4、8/6，suffix lookup3不变。'
                 'B检验概率门槛；C检验长窗口与较低门槛的组合。')
        lines = []
        for mode in LABELS:
            for case in CASES:
                lo, hi = d['summary']['range_tps'][mode][case]
                lines.append(f"| {mode} | {case} | {d['expert_slots'][mode]} | "
                    f"{d['summary']['median_tps'][mode][case]:.3f} | {lo:.3f}–{hi:.3f} |")
        pairs = []
        for mode, cases in d['summary']['paired'].items():
            for case, values in cases.items():
                pairs.append(f"| {mode} | {case} | {values['throughput_change_pct']:+.2f}% | "
                    f"{values['baseline_time_drift_pct']:+.2f}% | {values['drift_eligible']} |")
        report = f'''# A770：2K {args.kind} 选项筛选

目标为 Code2K / Prose2K 完整解码均稳定达到40 tok/s；本轮仅为筛选，未完成独立确认，未修改生产配置。

A770 16GB、E5-2696 v4、PCIe3.0、Windows、驱动32.0.101.9034、DPC++2025.3.2。IQ2_XS、FP16 KV、context/prefill8192、prompt-cache0。
同一冻结较新完整引擎，所有模式均启用已有命令图。{notes}

A1/B/A2/C/A3五个新进程；每进程Short16初始化，各场景一次预热，再三轮交错保留请求。
原始输入2048 IDs，每次完整生成128 token，全部输出逐token等于冻结参考。共享GPU/编译锁；吞吐组不启用剖析。

| 模式 | 场景 | 实际缓存槽 | 中位数tok/s | 全部保留样本范围 |
| --- | --- | ---: | ---: | ---: |
{chr(10).join(lines)}

| 候选 | 场景 | 配对吞吐变化 | 相邻基线耗时漂移 | 漂移≤3% |
| --- | --- | ---: | ---: | --- |
{chr(10).join(pairs)}

收益按候选吞吐除以前后A吞吐几何均值计算。范围不是置信区间，未删去低值或重跑挑选样本。
JSON保留全部预热和计时请求、接受/提议数、输出哈希及本机原始证据哈希。此处没有同条件eager对照，不将绝对速度差归为命令图的独立收益。

[全部数据]({target.name}.json)。无需GPU核对：

```powershell
python bench/results/a770/{Path(__file__).name} {args.kind} --local
```
'''
        with target.with_suffix('.json').open('x', encoding='utf-8') as file:
            json.dump(d, file, indent=2, ensure_ascii=False)
        target.with_suffix('.md').open('x', encoding='utf-8').write(report)
    else:
        audit(json.loads(target.with_suffix('.json').read_text(encoding='utf-8')), args.local)


if __name__ == '__main__':
    main()
