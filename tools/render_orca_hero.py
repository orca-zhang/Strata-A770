"""Render the repository hero from published measurements; --check-only uses no GPU.

Uses the already installed Pillow for PNG and the standard library for SVG.
Optional --font / --bold-font select CJK fonts on other systems.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import statistics
import xml.etree.ElementTree as ET

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'sycl/orca/anchor50-a770-windows-2026-10-31.json'
PAIR = ROOT / 'sycl/orca/prefill-pair-minp05-a770-windows-2026-10-31.json'


def check_data():
    data = json.loads(SOURCE.read_text(encoding='utf-8'))
    fixture = json.loads((SOURCE.parent / 'short-anchor-v1.json').read_text(encoding='utf-8'))
    assert data['fixture'] == fixture
    assert data['measurement_complete'] and data['files_unchanged'] and data['images_unchanged']
    assert [r['label'] for r in data['runs']] == ['SYCL-before', 'Orca', 'SYCL-after']
    rates = []
    for run in data['runs']:
        rows = run['rows']
        assert run['exit_code'] == 0 and len(rows) == 15
        assert [r['retained'] for r in rows] == [False] * 10 + [True] * 5
        assert run['config']['args'] == data['runs'][0]['config']['args']
        for key, value in fixture['parameters'].items():
            args = run['config']['args']
            assert args.count(key) == 1 and args[args.index(key) + 1] == value
        for row in rows:
            assert row['tokens'] == fixture['expected_output_ids']
            assert row['generated'] == len(row['tokens']) == 76
            assert row['prompt_tokens'] == 28 and row['finish'] == 'stop'
            assert row['accepted'] == row['offered'] == 57 and row['reused'] == 0
        retained = [76000 / r['decode_ms'] for r in rows if r['retained']]
        rate = statistics.median(retained)
        assert math.isclose(rate, run['summary']['decode_tok_s'])
        assert run['summary']['range_tok_s'] == [min(retained), max(retained)]
        rates.append(rate)
    comparison = data['comparison']
    assert math.isclose(comparison['baseline_tok_s'], math.sqrt(rates[0] * rates[2]))
    assert math.isclose(comparison['throughput_change_pct'], (rates[1] / comparison['baseline_tok_s'] - 1) * 100)
    assert math.isclose(comparison['baseline_time_drift_pct'], (rates[0] / rates[2] - 1) * 100)
    assert min(data['runs'][1]['summary']['range_tok_s']) > 50
    assert (data['total_requests'], data['retained_requests'], data['total_generated_tokens']) == (45, 15, 3420)
    assert not data['acceptance_evidence'] and not data['quality_equivalence_established']
    assert not data['linux_gpu_validated'] and not data['memory_headroom_passed']
    assert data['physical_memory']['min_free_mib'] < 1024
    pair = json.loads(PAIR.read_text(encoding='utf-8'))
    ttft = {r['case']: r for r in pair['comparisons'] if r['metric'] == 'ttft_ms'}
    assert pair['exact_output_and_drafts'] and pair['files_unchanged'] and pair['images_unchanged']
    for row in ttft.values():
        assert math.isclose(row['latency_change_pct'], (row['candidate'] / row['baseline'] - 1) * 100)
    print('PASS source values, A/B/A formula, 45 complete outputs, 15 retained, fixed parameters and scope')
    return data, ttft


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check-only', action='store_true')
    parser.add_argument('--font', default='C:/Windows/Fonts/msyh.ttc')
    parser.add_argument('--bold-font', default='C:/Windows/Fonts/msyhbd.ttc')
    args = parser.parse_args()
    data, ttft = check_data()
    if args.check_only:
        return
    width, height = 2000, 1350
    canvas = Image.new('RGB', (width, height), '#080f1c')
    draw = ImageDraw.Draw(canvas)
    ns = 'http://www.w3.org/2000/svg'
    ET.register_namespace('', ns)
    svg = ET.Element(f'{{{ns}}}svg', width=str(width), height=str(height),
                     viewBox=f'0 0 {width} {height}', role='img', attrib={'aria-labelledby': 'title desc'})
    ET.SubElement(svg, f'{{{ns}}}title', id='title').text = 'Strata-A770 · Orca L0 原生 Level Zero 推理运行时'
    digest = hashlib.sha256(SOURCE.read_bytes()).hexdigest()
    c = data['comparison']
    ET.SubElement(svg, f'{{{ns}}}desc', id='desc').text = (
        f"Same-parameter whole-backend short coding anchor: SYCL {c['baseline_tok_s']:.2f} to "
        f"Orca {c['orca_tok_s']:.2f} tok/s, +{c['throughput_change_pct']:.2f}%. "
        'Three independent SYCL/Orca/SYCL processes, ten warmups and five retained per process; '
        '45 complete identical 76-token outputs. Decode excludes preparation and prefill. '
        'Independent FP16-pair experiment: Code2K TTFT -14.46%, Long8K -9.36%; not additive. '
        'Memory headroom check failed; general quality equivalence and Linux remain unverified. '
        f'Source {SOURCE.relative_to(ROOT).as_posix()}, SHA256 {digest}.')
    svg.set('data-source-sha256', digest)
    fonts = {}

    # ponytail: one fixed layout, shared drawing calls keep SVG and PNG in sync.
    def rect(x, y, w, h, fill, radius=0):
        draw.rounded_rectangle((x, y, x + w, y + h), radius, fill=fill)
        ET.SubElement(svg, f'{{{ns}}}rect', x=str(x), y=str(y), width=str(w), height=str(h), rx=str(radius), fill=fill)

    def line(x1, y1, x2, y2, color, thickness=1):
        draw.line((x1, y1, x2, y2), fill=color, width=thickness)
        ET.SubElement(svg, f'{{{ns}}}line', x1=str(x1), y1=str(y1), x2=str(x2), y2=str(y2),
                      stroke=color, attrib={'stroke-width': str(thickness)})

    def text(x, y, label, size=26, color='#eaf2ff', bold=False, anchor='ls', limit=None):
        key = size, bold
        if key not in fonts:
            fonts[key] = ImageFont.truetype(args.bold_font if bold else args.font, size)
        bounds = draw.textbbox((x, y), label, font=fonts[key], anchor=anchor)
        assert 0 <= bounds[0] <= bounds[2] <= width and 0 <= bounds[1] <= bounds[3] <= height, (label, bounds)
        if limit:
            assert bounds[2] - bounds[0] <= limit, (label, bounds, limit)
        draw.text((x, y), label, font=fonts[key], fill=color, anchor=anchor)
        ET.SubElement(svg, f'{{{ns}}}text', x=str(x), y=str(y), fill=color,
                      attrib={'font-family': 'Microsoft YaHei, Noto Sans CJK SC, sans-serif',
                              'font-size': str(size), 'font-weight': '700' if bold else '400',
                              'text-anchor': {'ls': 'start', 'rs': 'end', 'ms': 'middle'}[anchor]}).text = label

    cyan, green, muted = '#64e8ef', '#c1f787', '#a5b7cd'
    rect(0, 0, width, height, '#080f1c')
    for x in range(0, width, 80):
        line(x, 0, x, height, '#0d1929')
    for y in range(0, height, 80):
        line(0, y, width, y, '#0d1929')
    rect(72, 48, 5, 180, cyan)
    text(104, 77, 'STRATA-A770  /  NATIVE INFERENCE', 24, cyan, True)
    text(1928, 77, 'INTEL ARC A770 · 16 GB', 24, muted, anchor='rs')
    text(100, 193, 'Orca L0', 108, bold=True)
    text(104, 245, '原生 Level Zero 运行时，让本地 MoE 推理更快一步。', 32, muted)
    text(1928, 174, '125B MoE', 50, green, True, 'rs')
    text(1928, 224, '显存 + 系统内存协作', 27, muted, anchor='rs')

    rect(72, 285, 1120, 302, '#111f32', 18)
    rect(1212, 285, 716, 302, '#133033', 18)
    text(104, 331, '同参数整版本对照 · 固定短代码锚点', 27, muted)
    for y, label, value, color in [(366, '原生产 SYCL', c['baseline_tok_s'], '#7099c6'),
                                   (462, 'Orca L0', c['orca_tok_s'], cyan)]:
        text(104, y + 44, label, 28, color, True, limit=195)
        rect(310, y, 690, 58, '#1d3048', 8)
        rect(310, y, 690 * value / 55, 58, color, 8)
        text(1158, y + 47, f'{value:.2f}', 49, color, True, 'rs', 150)
    text(104, 562, '单位 tok/s · decode 排除准备与预填充', 24, muted)
    text(1248, 331, '实测吞吐提升', 27, '#b5d5cc')
    text(1240, 453, f"+{c['throughput_change_pct']:.2f}%", 100, green, True, limit=650)
    text(1248, 516, '41.42 → 50.54 tok/s', 35, '#e4f5e9', True)
    text(1248, 562, '基线取前后 SYCL 中位数的几何平均', 23, '#b5d5cc', limit=648)
    text(76, 636, '45 次完整输出逐 token 相同', 27, green, True)
    text(782, 636, '每进程 10 次预热 + 5 次保留', 27, muted, anchor='ms')
    text(1924, 636, f"前后基线漂移 {c['baseline_time_drift_pct']:.2f}%", 27, muted, anchor='rs')

    rect(72, 668, 1856, 228, '#101d2e', 18)
    text(104, 715, '直接对接 Level Zero', 30, cyan, True)
    text(1896, 715, '主机运行时移除 SYCL / UR；内核构建沿用 Intel 编译器', 23, muted, anchor='rs')
    for y, native in [(740, False), (816, True)]:
        text(104, y + 33, 'Orca 链路' if native else '原链路', 25, cyan if native else muted, True)
        nodes = [(260, 230, 'Strata'), (540, 650, 'Orca L0  /  原生命令列表')] if native else [
            (260, 230, 'Strata'), (540, 300, 'SYCL Runtime'), (890, 300, 'Unified Runtime')]
        nodes += [(1240, 300, 'Level Zero'), (1590, 290, 'Intel GPU')]
        for i, (x, w, label) in enumerate(nodes):
            rect(x, y, w, 46, '#17414b' if native else '#20314a', 7)
            text(x + w / 2, y + 33, label, 24, cyan if native else '#c5d2e4', True, 'ms', w - 24)
            if i + 1 < len(nodes):
                text(x + w + 25, y + 32, '→', 28, cyan if native else '#6c86a6', anchor='ms')

    cards = [
        ('01 / RUNTIME', '命令列表重放', '原生事件与内存池', '减少重复提交'),
        ('02 / SCHEDULER', '就绪任务批量', '独立投影并行', '尾部立即派发'),
        ('03 / CPU PATH', '缓存行级优化', 'SPSC 无锁环', '64 B 读写游标隔离'),
        ('04 / XMX', 'FP16 双专家预填充', f"2K TTFT {ttft['Code2K']['latency_change_pct']:.2f}%", '独立 OFF / ON / OFF 实验'),
    ]
    for x, card in zip((72, 542, 1012, 1482), cards):
        rect(x, 924, 446, 176, '#111f32', 14)
        text(x + 24, 959, card[0], 20, cyan, True)
        text(x + 24, 1003, card[1], 29, bold=True, limit=400)
        text(x + 24, 1043, card[2], 25, green if x == 1482 else muted, limit=400)
        text(x + 24, 1078, card[3], 23, muted, limit=400)
    text(76, 1154, 'A770 16 GB · Xeon E5-2696 v4 · 256 GiB DDR3 · IQ2_XS · Windows 10 · 2026-10-31', 25, '#c0d0e4')
    text(76, 1196, '28 输入 → 76 自然输出 · greedy · 5771 专家槽 · MTP4 / min-p 0.5 · adapt-decay 0.95', 25, muted)
    text(76, 1235, '固定锚点，非通用请求速度；预填充数据来自独立实验，收益不与解码相加。', 24, muted)
    text(76, 1274, f"采样最低空闲 {data['physical_memory']['min_free_mib']:.0f} MiB，低于 1024 MiB 验收线；整版本质量等价尚未闭环。", 23, '#f0ca91')
    text(76, 1313, 'Linux 优先设计（待验证） · 复现 tools/bench_orca_anchor.py · 完整条件、哈希与样本见报告', 23, muted)

    out = ROOT / 'docs/media/orca-l0-hero'
    ET.indent(svg, space='  ')
    ET.ElementTree(svg).write(out.with_suffix('.svg'), encoding='utf-8', xml_declaration=True)
    canvas.save(out.with_suffix('.png'), optimize=True)
    with Image.open(out.with_suffix('.png')) as check:
        assert check.size == (width, height)
        check.verify()
    assert ET.parse(out.with_suffix('.svg')).getroot().get('data-source-sha256') == digest
    print('Rendered and checked', out.relative_to(ROOT))


if __name__ == '__main__':
    main()
