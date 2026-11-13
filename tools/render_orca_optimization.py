"""Render English L0, WIP Direct and SYCL comparisons and call paths; --check-only uses no GPU."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import statistics
import xml.etree.ElementTree as ET

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'sycl/orca/direct-idle-audit-a770-windows-2026-11-04.json'
SYCL_SOURCE = ROOT / 'sycl/orca/direct-vs-sycl-a770-windows-2026-11-04.json'


def checked_rates(result):
    assert result['measurement_complete'] and result['files_unchanged'] and result['images_unchanged']
    assert not result['production_changed']
    rates = []
    for arm in result['runs']:
        assert arm['exit_code'] == 0 and len(arm['rows']) == 15
        assert arm['config']['args'] == result['runs'][0]['config']['args']
        for index, row in enumerate(arm['rows']):
            assert row['retained'] == (index >= 10)
            assert row['tokens'] == result['fixture']['expected_output_ids']
            assert row['generated'] == 76 and row['prompt_tokens'] == 28 and row['finish'] == 'stop'
            assert row['accepted'] == row['offered'] == 57 and row['reused'] == row['resume'] == 0
        rate = statistics.median(76000 / row['decode_ms'] for row in arm['rows'] if row['retained'])
        assert math.isclose(rate, arm['summary']['decode_tok_s'])
        rates.append(rate)
    return rates


def checked_values():
    idle = json.loads(SOURCE.read_text(encoding='utf-8'))
    pairs = []
    for name in ('idle-l0-direct-l0', 'idle-direct-l0-direct'):
        result = idle['results']['clean'][name]
        rates = checked_rates(result)
        flanks = math.sqrt(rates[0] * rates[2])
        assert math.isclose(flanks, idle['comparisons'][name]['flank_tok_s'])
        pairs.append((rates[1], flanks) if name == 'idle-l0-direct-l0' else (flanks, rates[1]))
    sycl = json.loads(SYCL_SOURCE.read_text(encoding='utf-8'))
    assert [arm['runtime'] for arm in sycl['runs']] == ['sycl', 'direct', 'sycl']
    rates = checked_rates(sycl)
    baseline = math.sqrt(rates[0] * rates[2])
    comparison = sycl['comparison']
    assert math.isclose(baseline, comparison['baseline_tok_s'])
    assert math.isclose(rates[1], comparison['orca_tok_s'])
    assert math.isclose(100 * (rates[1] / baseline - 1), comparison['throughput_change_pct'])
    assert math.isclose(100 * (rates[0] / rates[2] - 1), comparison['baseline_time_drift_pct'])
    assert sycl['production_files_unchanged']
    print('PASS current SYCL/Direct bracket and independent L0 orders: 135 exact requests and arithmetic')
    return pairs, sycl


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check-only', action='store_true')
    args = parser.parse_args()
    pairs, sycl = checked_values()
    if args.check_only:
        return
    width, height = 2000, 1450
    canvas = Image.new('RGB', (width, height), '#080f1c')
    draw = ImageDraw.Draw(canvas)
    ns = 'http://www.w3.org/2000/svg'
    ET.register_namespace('', ns)
    svg = ET.Element(f'{{{ns}}}svg', width=str(width), height=str(height), viewBox=f'0 0 {width} {height}',
                     role='img', attrib={'aria-labelledby': 'title desc'})
    comparison = sycl['comparison']
    direct, baseline = comparison['orca_tok_s'], comparison['baseline_tok_s']
    gain = comparison['throughput_change_pct']
    l0_gain = 100 * (pairs[0][1] / baseline - 1)
    ET.SubElement(svg, f'{{{ns}}}title', id='title').text = 'Recommended Orca L0 and WIP Orca Direct: performance comparisons and Windows call paths'
    ET.SubElement(svg, f'{{{ns}}}desc', id='desc').text = (
        f'Windows A770 16GB, 2026-11-04. Orca L0 is recommended; Orca Direct is a WIP alternative. '
        f'L0 {pairs[0][1]:.4f} versus independently measured SYCL {baseline:.4f} tok/s: '
        f'{l0_gain:+.4f}% descriptive cross-run reference, not an L0/SYCL bracket. '
        f'Same-parameter whole-version SYCL/Direct/SYCL: '
        f'SYCL {baseline:.4f}, Direct {direct:.4f} tok/s ({gain:+.4f}%). '
        'Includes numerical-kernel, scheduling and cache-policy differences, not an isolated API effect. '
        'Independent matched-policy L0 comparisons: '
        + '; '.join(f'Direct {d:.4f}, L0 {l:.4f} tok/s ({100*(d/l-1):+.4f}%)' for d, l in pairs)
        + '. Ten warmups and five retained requests per process; decode excludes preparation and prefill. '
        + 'SYCL uses UR and Level Zero/Intel UMD; Orca L0 uses Level Zero/Intel UMD; Orca Direct encodes GPU commands. '
        + 'All three Windows submission paths use D3DKMT and the kernel driver. '
        + 'Fixed repeated Short28/natural76, not general workload speed. No PPL or Linux GPU conclusion.')
    for source in (SOURCE, SYCL_SOURCE):
        ET.SubElement(svg, f'{{{ns}}}metadata', attrib={'data-source': source.relative_to(ROOT).as_posix(),
                      'data-sha256': hashlib.sha256(source.read_bytes()).hexdigest()})
    fonts = {}
    cyan, green, muted = '#64e8ef', '#c1f787', '#a5b7cd'

    # ponytail: one fixed layout; the same primitives draw the PNG and exportable SVG.
    def rect(x, y, w, h, fill, radius=16):
        draw.rounded_rectangle((x, y, x+w, y+h), radius, fill=fill)
        ET.SubElement(svg, f'{{{ns}}}rect', x=str(x), y=str(y), width=str(w), height=str(h), rx=str(radius), fill=fill)

    def text(x, y, label, size=28, color='#eaf2ff', bold=False, right=False, limit=None):
        key = size, bold
        if key not in fonts:
            fonts[key] = ImageFont.truetype('C:/Windows/Fonts/segoeuib.ttf' if bold else 'C:/Windows/Fonts/segoeui.ttf', size)
        anchor = 'rs' if right else 'ls'
        box = draw.textbbox((x, y), label, font=fonts[key], anchor=anchor)
        assert 0 <= box[0] <= box[2] <= width and 0 <= box[1] <= box[3] <= height, (label, box)
        assert limit is None or box[2] - box[0] <= limit, (label, box, limit)
        draw.text((x, y), label, font=fonts[key], fill=color, anchor=anchor)
        ET.SubElement(svg, f'{{{ns}}}text', x=str(x), y=str(y), fill=color, attrib={
            'font-family': 'Segoe UI, Arial, sans-serif', 'font-size': str(size),
            'font-weight': '700' if bold else '400', 'text-anchor': 'end' if right else 'start'}).text = label

    text(76, 76, 'STRATA-A770  /  NATIVE INFERENCE', 26, cyan, True)
    text(1924, 76, '2026-11-04 · WINDOWS · A770 16 GB', 24, muted, right=True)
    text(76, 194, 'Orca L0', 108, bold=True)
    text(80, 249, 'Recommended · Native Level Zero inference · Orca Direct is a WIP alternative', 34, muted)
    rect(72, 294, 890, 308, '#133033')
    rect(984, 294, 944, 308, '#111f32')
    paired_direct, l0 = pairs[0]
    text(104, 347, 'Recommended L0 · Warm fixed-code input', 30, '#b5d5cc')
    text(100, 508, f'{l0:.2f}', 154, green, True)
    text(920, 505, 'tok/s', 46, green, True, True)
    text(104, 564, f'vs independently measured SYCL  {l0_gain:+.2f}%', 34, green, True)
    text(1018, 347, 'Matched-policy bracket: L0 → Direct → L0', 30, muted)
    maximum = math.ceil(max(l0, paired_direct) / 10) * 10
    for y, label, rate, color in ((376, 'Orca L0', l0, green), (465, 'Direct · WIP', paired_direct, '#90a5bd')):
        text(1018, y+36, label, 26, color, True)
        rect(1220, y, 480, 48, '#1d3048', 7)
        rect(1220, y, 480*rate/maximum, 48, color, 7)
        text(1894, y+38, f'{rate:.2f}', 38, color, True, True)
    text(1018, 559, f'Direct is {100*(1-paired_direct/l0):.2f}% slower here · Reverse L0: {pairs[1][1]:.2f}', 27, muted)
    text(76, 653, f'Direct WIP: {gain:+.2f}% vs SYCL'
         f' ({baseline:.2f} → {direct:.2f} tok/s, separate whole-version bracket)', 29, muted)
    rect(72, 693, 1856, 373, '#101d2e')
    text(104, 744, 'Three Windows call paths', 34, cyan, True)
    common = [(1205, 180, 'D3DKMT'), (1415, 255, 'Windows KMD'), (1700, 190, 'Intel GPU')]
    paths = [
        ('SYCL', 775, '#20314a', '#c5d2e4', [(295, 180, 'Strata'), (505, 170, 'SYCL'),
            (705, 120, 'UR'), (855, 320, 'Level Zero / Intel UMD')]),
        ('L0 · Default', 865, '#17414b', green, [(295, 180, 'Strata'), (505, 320, 'Orca L0'),
            (855, 320, 'Level Zero / Intel UMD')]),
        ('Direct WIP', 955, '#20314a', '#90a5bd', [(295, 180, 'Strata'), (505, 670, 'Orca Direct · WIP · GPU command encoding')]),
    ]
    for label, y, fill, color, nodes in paths:
        text(104, y+35, label, 26, color, True)
        nodes = nodes + common
        for index, (x, w, name) in enumerate(nodes):
            rect(x, y, w, 52, fill, 7)
            text(x+15, y+35, name, 24, color, True, limit=w-25)
            if index+1 < len(nodes):
                text(x+w+5, y+35, '→', 24, color)
    text(104, 1045, 'UR = Unified Runtime · UMD / KMD = user-mode / kernel-mode driver · Offline kernels use the Intel compiler', 25, muted)
    rect(72, 1096, 1856, 238, '#101d2e')
    text(104, 1145, '90 L0 comparison requests + 45 separate SYCL requests match · 10 warmups + 5 retained per process', 29, green, True)
    text(104, 1194, '28 input / 76 natural output · 5771 slots · read256 / decay0.95 · MTP4 · Cumulative threshold 0', 27, muted)
    text(104, 1240, 'Matched parameters; whole versions differ in numerical kernels, scheduling and cache policies. No API-only attribution.', 27, muted)
    text(104, 1286, 'L0/SYCL is a cross-cohort reference. Decode excludes preparation/prefill. Fixed-input speed is workload-specific.', 27, muted)
    text(76, 1396, 'Orca L0 recommended · Direct is WIP · GPU clocks unlocked · No new PPL or Linux GPU conclusions', 25, muted)
    out = ROOT / 'docs/media/orca-direct-optimization'
    ET.indent(svg, space='  ')
    ET.ElementTree(svg).write(out.with_suffix('.svg'), encoding='utf-8', xml_declaration=True)
    canvas.save(out.with_suffix('.png'), optimize=True)
    with Image.open(out.with_suffix('.png')) as check:
        assert check.size == (width, height)
        check.verify()
    assert len(ET.parse(out.with_suffix('.svg')).getroot().findall(f'{{{ns}}}metadata')) == 2
    print('Rendered and checked', out.relative_to(ROOT))


if __name__ == '__main__':
    main()
