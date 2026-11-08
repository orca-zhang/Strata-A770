"""Lead with Short decode, then compare 2K prefill; --check-only also checks historical ratios."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import statistics
import xml.etree.ElementTree as ET

from PIL import Image, ImageDraw, ImageFont
import render_orca_optimization as short_anchor

ROOT = Path(__file__).resolve().parents[1]
SOURCES = [ROOT / 'sycl/orca/lowbit-fp16-m4-a770-windows-2026-11-04.json',
           ROOT / 'sycl/orca/hc1-q8-grf-u4-a770-windows-2026-11-05.json',
           ROOT / 'sycl/orca/hc1-q8-grf-u4-adoption-a770-windows-2026-11-05.json',
           ROOT / 'bench/results/a770/refresh-2026-10-21.json',
           ROOT / 'sycl/orca/u4-m16-combined-a770-windows-2026-11-05.json']
SHORT_SOURCES = (short_anchor.SOURCE, short_anchor.SYCL_SOURCE)
CASES = ('Code2K', 'Prose2K')


def checked_values():
    sycl, hc, adoption, history, combined = [json.loads(p.read_text(encoding='utf-8')) for p in SOURCES]
    assert sycl['complete'] and sycl['quality']['complete']
    assert adoption['adopted'] and adoption['service']['complete']
    controls = [sycl['performance_runs'][n] for n in (0, 3)]
    assert [r['label'] for r in controls] == ['0-native', '3-native']
    native = sycl['performance']['u4n16']
    for case in CASES:
        for metric in ('prompt_ms', 'decode_ms', 'request_ms'):
            value = native[case][metric]
            left, right = [r['summary'][case][metric] for r in controls]
            assert all(r['exit_code'] == 0 and not r['coverage']['u4'] and not r['coverage']['u2'] for r in controls)
            assert math.isclose(value['baseline'], math.sqrt(left * right))
            assert math.isclose(value['baseline_drift_pct'], 100 * (right / left - 1))
            assert abs(value['baseline_drift_pct']) <= 3
    assert history['status'] == 'complete' and history['standalone_checks']['all_passed']
    assert len(history['rows']) == 39
    ratios = history['throughput_ratios']
    assert not ratios['controlled_software_speedup']
    for case, initial in ratios['historical_decode_baseline_tok_s'].items():
        measured = history['normal_decode_prefill'][case]
        assert measured['samples'] == 5
        assert math.isclose(measured['decode_tok_s']['median'] / initial, ratios['historical_decode_ratio'][case])
    assert math.isclose(history['normal_decode_prefill']['long']['prefill_tok_s']['median'] /
                        ratios['historical_long_prefill_baseline_tok_s'], ratios['historical_long_prefill_ratio'])
    reference, count = {}, 0
    assert [t['reverse'] for t in hc['trials']] == [False, True]
    for trial in hc['trials']:
        assert trial['complete'] and trial['passes']
        assert [r['enabled'] for r in trial['runs']] == ([1, 0, 1] if trial['reverse'] else [0, 1, 0])
        for run in trial['runs']:
            assert run['exit_code'] == 0 and len(run['rows']) == 6
            initializer = run['initializer']
            assert initializer['prompt_tokens'] == 23 and initializer['generated'] == len(initializer['tokens']) == 16
            signature = initializer['tokens'], initializer['accepted'], initializer['offered']
            assert signature == reference.setdefault('Short', signature)
            count += 1
            for row in run['rows']:
                assert row['prompt_tokens'] == 2048 and row['generated'] == len(row['tokens']) == 128
                assert row['finish'] == 'length' and row['reused'] == row['resume'] == 0
                signature = row['tokens'], row['accepted'], row['offered']
                assert signature == reference.setdefault(row['case'], signature)
                count += 1
            for case in CASES:
                rows = [r for r in run['rows'] if r['case'] == case]
                assert [r['cycle'] for r in rows] == [0, 1, 2]
                for metric, value in run['summary'][case].items():
                    assert math.isclose(value, statistics.median(r[metric] for r in rows[1:]))
        for case in CASES:
            for metric, value in trial['comparison'][case].items():
                left, middle, right = [r['summary'][case][metric] for r in trial['runs']]
                outer = math.sqrt(left * right)
                baseline, candidate = (middle, outer) if trial['reverse'] else (outer, middle)
                assert math.isclose(value['baseline'], baseline) and math.isclose(value['candidate'], candidate)
                assert math.isclose(value['throughput_change_pct'], 100 * (baseline / candidate - 1))
                assert math.isclose(value['control_drift_pct'], 100 * (right / left - 1))
            decode = trial['comparison'][case]['decode_ms']
            assert decode['throughput_change_pct'] >= 1 and abs(decode['control_drift_pct']) <= 3
    assert count == 42
    direct = combined['model']['direct']
    assert direct['quality_tsv_byte_equal_previous_u4']
    assert direct['complete_generated_outputs_equal_previous_u4']
    assert direct['speculative_accepted_and_offered_equal_between_arms']
    cohorts = list(direct['cohorts'].values())
    assert [c['order'] for c in cohorts] == ['forward', 'reverse']
    for cohort in cohorts:
        assert cohort['passes']
        for case in CASES:
            for metric in ('prompt_ms', 'decode_ms'):
                value = cohort['warm'][case][metric]
                assert math.isclose(value['throughput_change_pct'], 100*(value['baseline']/value['candidate']-1))
                assert abs(value['baseline_drift_pct']) <= 3
    rates = {}
    for case in CASES:
        rates[case] = {}
        for metric, tokens in (('decode_ms', 128), ('prompt_ms', 2048)):
            rates[case][metric] = {
                'SYCL': [1000*tokens/native[case][metric]['baseline']],
                'L0': [1000*tokens/t['comparison'][case][metric]['candidate'] for t in hc['trials']],
                'Direct': [1000*tokens/c['warm'][case][metric]['candidate'] for c in cohorts]}
    pairs, short = short_anchor.checked_values()
    rates['Short28'] = {'decode_ms': {
        'SYCL': [short['comparison']['baseline_tok_s']],
        'L0': [pairs[0][1]],
        'Direct': [short['comparison']['orca_tok_s']]}}
    print('PASS SYCL native controls; L0 42 complete requests/brackets; Direct both orders; decode/prefill and historical arithmetic')
    return rates, history


def span(values):
    return f'{values[0]:.2f}' if len(values) == 1 else f'{min(values):.2f}–{max(values):.2f}'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check-only', action='store_true')
    args = parser.parse_args()
    rates, _ = checked_values()
    if args.check_only:
        return
    width, height = 2000, 1390
    canvas = Image.new('RGB', (width, height), '#080f1c')
    draw = ImageDraw.Draw(canvas)
    ns = 'http://www.w3.org/2000/svg'
    ET.register_namespace('', ns)
    svg = ET.Element(f'{{{ns}}}svg', width=str(width), height=str(height), viewBox=f'0 0 {width} {height}',
                     role='img', attrib={'aria-labelledby': 'title desc'})
    ET.SubElement(svg, f'{{{ns}}}title', id='title').text = 'Short decode：L0 59.38、Direct 58.48、SYCL 42.05 tok/s；另列 2K prefill'
    ET.SubElement(svg, f'{{{ns}}}desc', id='desc').text = (
        'Windows A770 16GB / Xeon E5-2696 v4 / 256 GiB DDR3, driver 32.0.101.9034, oneAPI 2025.3, 2026-11-05. '
        'Lead example: 2026-11-04 frozen Short28/natural76 decode, ten warmups and five retained per process. '
        'SYCL/Direct/SYCL bracket gives Direct +39.07%; independent L0 is +41.21% versus that SYCL cross-cohort baseline. '
        'These Short figures are not current-U4 measurements. '
        'Separate 2048-input/128-output Code and Prose prefill panels follow. '
        'SYCL native is the corrected FP16 paired-M4 engine measured 2026-11-04; '
        'current U4/min16/M16/HC256 L0 comes from the independent 2026-11-05 cohort summaries. '
        'Direct uses the independently measured U4/min32/M16 HC128 optional build. '
        'Each comparison shows SYCL, L0 and Direct in tok/s. '
        'Percentages compare each candidate rate with SYCL. Bars use forward summaries; labels include both orders. '
        'This is a cross-cohort throughput ratio, not a concurrent three-backend bracket; '
        'L0 reverse Code prefill drift exceeds 3%. Ranges are cohort summaries, not confidence intervals. '
        'Warm requests exclude process startup, lazy first loading and HTTP. '
        'Direct remains WIP, Linux GPU unvalidated, U4 model uploads deferred.')
    for source in [*SOURCES, *SHORT_SOURCES]:
        ET.SubElement(svg, f'{{{ns}}}metadata', attrib={'data-source': source.relative_to(ROOT).as_posix(),
                      'data-sha256': hashlib.sha256(source.read_text(encoding='utf8').encode('utf8')).hexdigest()})
    fonts = {}
    cyan, green, muted = '#64e8ef', '#c1f787', '#a5b7cd'

    # ponytail: one fixed layout; Pillow and SVG share drawing primitives, no chart framework.
    def rect(x, y, w, h, fill, radius=16):
        assert 0 <= x <= x+w <= width and 0 <= y <= y+h <= height
        draw.rounded_rectangle((x, y, x+w, y+h), radius, fill=fill)
        ET.SubElement(svg, f'{{{ns}}}rect', x=str(x), y=str(y), width=str(w), height=str(h), rx=str(radius), fill=fill)

    def text(x, y, label, size=28, color='#eaf2ff', bold=False, right=False, limit=None):
        key = size, bold
        if key not in fonts:
            fonts[key] = ImageFont.truetype('C:/Windows/Fonts/msyhbd.ttc' if bold else 'C:/Windows/Fonts/msyh.ttc', size)
        anchor = 'rs' if right else 'ls'
        box = draw.textbbox((x, y), label, font=fonts[key], anchor=anchor)
        assert 0 <= box[0] <= box[2] <= width and 0 <= box[1] <= box[3] <= height, (label, box)
        assert limit is None or box[2]-box[0] <= limit, (label, box, limit)
        draw.text((x, y), label, font=fonts[key], fill=color, anchor=anchor)
        ET.SubElement(svg, f'{{{ns}}}text', x=str(x), y=str(y), fill=color, attrib={
            'font-family': 'Microsoft YaHei, Noto Sans CJK SC, sans-serif', 'font-size': str(size),
            'font-weight': '700' if bold else '400', 'text-anchor': 'end' if right else 'start'}).text = label

    text(76, 76, 'STRATA-A770  /  SYCL COMPARISON', 26, cyan, True)
    text(1924, 76, '2026-11-05 · WINDOWS · A770 16 GB', 24, muted, right=True)
    text(76, 194, 'Direct / L0 vs SYCL', 96, bold=True)
    text(80, 249, 'DECODE / Short 固定短代码 · 28 输入 / 76 自然输出 · 11-04 冻结版本', 34, muted)
    short = rates['Short28']['decode_ms']
    for index, (backend, color) in enumerate((('SYCL', '#90a5bd'), ('L0', green), ('Direct', cyan))):
        x = 72+index*630
        value = short[backend][0]
        rect(x, 300, 596, 300, '#133033' if backend == 'L0' else '#101d2e')
        text(x+32, 353, backend, 36, color, True)
        text(x+26, 495, f'{value:.2f}', 108, color, True, limit=410)
        text(x+564, 492, 'tok/s', 30, color, right=True)
        gain = '对照基线' if backend == 'SYCL' else f'相对 SYCL +{100*(value/short["SYCL"][0]-1):.2f}%'
        text(x+32, 561, gain, 32, color, True, limit=532)
    text(76, 648, 'Short：每进程 10 次预热 + 5 次保留，输出与 57/57 草稿一致；净 decode 排除预填充。', 28, muted)
    text(76, 694, 'Direct 对 SYCL 为同期夹测；L0 对 SYCL 为跨组参考。该 Short 成绩不代表当前 U4 配置。', 28, muted)
    text(76, 758, 'PREFILL / 2K 输入预填充（11-04 / 11-05）', 36, cyan, True)
    for column, case in enumerate(CASES):
        x, y = 72+column*948, 805
        rect(x, y, 908, 350, '#101d2e')
        text(x+32, y+54, case, 33, bold=True)
        text(x+708, y+98, 'tok/s', 24, muted, right=True)
        text(x+876, y+98, '相对 SYCL', 24, muted, right=True)
        series = rates[case]['prompt_ms']
        maximum = max(max(values) for values in series.values())*1.04
        for index, (backend, color) in enumerate((('SYCL', '#90a5bd'), ('L0', green), ('Direct', cyan))):
            values = series[backend]
            bar_y = y+133+index*80
            text(x+32, bar_y+31, backend, 28, color, True)
            rect(x+180, bar_y, 320*values[0]/maximum, 40, color, 6)
            text(x+708, bar_y+31, span(values), 27, color, True, True)
            gains = [100*(value/series['SYCL'][0]-1) for value in values]
            gain = '基线' if backend == 'SYCL' else f'+{min(gains):.0f}–{max(gains):.0f}%'
            text(x+876, bar_y+31, gain, 26, color, True, True)
    text(76, 1200, '2K prefill：SYCL FP16 M4 修复版 · L0 U4/min16/M16/HC256 · Direct U4/min32/M16/HC128', 27, muted)
    text(76, 1246, '2K 为跨组实测，条形取正序；L0 反序 Code prefill 控制漂移 −3.56%，区间非置信区间。', 27, muted)
    text(76, 1292, 'A770 16 GB · Xeon E5-2696 v4 · 256 GiB DDR3 · Windows · oneAPI 2025.3 · 5771 槽 / MTP4', 27, muted)
    text(76, 1344, '推荐 L0 · Direct 为 WIP（8K 间歇问题未关闭）· 最早历史见文档 · HF / ModelScope 权重稍后发布', 25, cyan)
    out = ROOT / 'docs/media/orca-u4-optimization'
    ET.indent(svg, space='  ')
    ET.ElementTree(svg).write(out.with_suffix('.svg'), encoding='utf-8', xml_declaration=True)
    canvas.save(out.with_suffix('.png'), optimize=True)
    with Image.open(out.with_suffix('.png')) as check:
        assert check.size == (width, height)
        check.verify()
    assert len(ET.parse(out.with_suffix('.svg')).getroot().findall(f'{{{ns}}}metadata')) == len(SOURCES)+len(SHORT_SOURCES)
    print('Rendered and checked', out.relative_to(ROOT))


if __name__ == '__main__':
    main()
