"""Render English current/SYCL charts or --paths-only Windows call paths; --check-only needs no GPU."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import statistics
import xml.etree.ElementTree as ET

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'sycl/orca/default-hc256-a770-windows-2026-11-08.json'
OUT = ROOT / 'docs/media/orca-current-default'
CASES = ('Code2K', 'Prose2K')
KEYS = ('tokens', 'generated', 'prompt_tokens', 'accepted', 'offered', 'finish', 'reused', 'resume')
METRICS = ('decode_ms', 'prompt_ms', 'ttft_ms', 'request_ms')


def checked_values():
    data = json.loads(SOURCE.read_text(encoding='utf8'))
    assert data['schema'] == 1 and data['measured_date'] == '2026-11-08'
    assert hashlib.sha256(data['config_json']['candidate'].encode()).hexdigest() == data['primary_sha256']
    cfg = json.loads(data['config_json']['candidate'])
    for option, value in (('--spec', '6'), ('--spec-min-p', '0.6'), ('--expert-cache', '6200'),
                          ('--adapt-swaps', '16'), ('--max-context', '8192'), ('--prefill', '8192')):
        assert cfg['args'][cfg['args'].index(option) + 1] == value
    assert cfg['env']['STRATA_MTP_CUM_MIN_P'] == '0.6' and cfg['env']['STRATA_MTP_ACCEPTED_ONLY'] == '1'
    references = data['data']['references']
    for left, right in zip([references['old']['initializer'], *references['old']['rows']],
                           [references['new']['initializer'], *references['new']['rows']]):
        assert all(left[k] == right[k] for k in KEYS if k not in ('accepted', 'offered'))
    count = generated = 0
    for trial, order in zip(data['trials'], ('forward', 'reverse'), strict=True):
        result, review = trial['result'], trial['independent_review']
        assert result['complete'] and result['passes'] and not result['production_changed']
        assert review['complete'] and review['passes'] and review['raw_exact8_fields']
        assert review['all_exit_codes_zero'] and review['provenance_unchanged'] and trial['provenance_unchanged']
        assert review['comparison'] == result['comparison'] and review['requests'] == 21 and review['generated'] == 2352
        assert review['source_sha256'] == data['source_sha256'][data['source_directory']+'/performance-'+order+'-r1/results.json']
        policy = trial['gate_policy']
        assert policy['original_HC256_goal_pct'] == 20 and policy['max_abs_baseline_drift_pct'] == 3
        assert result['order'] == order and [a['mode'] for a in result['runs']] == (
            ['old', 'new', 'old'] if order == 'forward' else ['new', 'old', 'new'])
        for arm in result['runs']:
            assert arm['exit_code'] == 0 and not arm.get('watchdog_fired')
            protocol = trial['protocols'][arm['label']]
            source = data['source_directory']+'/performance-'+order+'-r1/'+arm['label']+'.protocol'
            assert hashlib.sha256(protocol.encode()).hexdigest() == data['source_sha256'][source]
            assert f"expert_slots={5771 if arm['mode'] == 'old' else 6509} " in arm['info']
            assert [(r['cycle'], r['case']) for r in arm['rows']] == [(i, c) for i in range(3) for c in CASES]
            expected = [arm['initializer'], *arm['rows']]
            ref = references[arm['mode']]
            parsed, tokens, resume = [], [], 0
            for line in protocol.splitlines():
                p = line.split()
                if not p:
                    continue
                assert p[0] != 'ERR', line
                if p[0] in ('T', 'TB'):
                    tokens.extend(map(int, p[1 if p[0] == 'T' else 2:]))
                elif p[0] == 'RESUME':
                    resume = int(p[1])
                elif p[0] == 'DONE':
                    parsed.append(dict(tokens=tokens, generated=int(p[1]), prompt_tokens=int(p[2]),
                        prompt_ms=float(p[3]), decode_ms=float(p[4]), finish=p[5], accepted=int(p[6]),
                        offered=int(p[7]), reused=int(p[8]), resume=resume, done=line))
                    assert len(tokens) == int(p[1])
                    tokens, resume = [], 0
            assert not tokens and not resume and len(parsed) == len(expected) == 7
            for raw, row, frozen in zip(parsed, expected, [ref['initializer'], *ref['rows']], strict=True):
                assert all(raw[k] == row[k] == frozen[k] for k in KEYS)
                assert all(raw[k] == row[k] for k in ('prompt_ms', 'decode_ms', 'done'))
                assert all(math.isfinite(row[k]) and row[k] > 0 for k in METRICS)
                count += 1
                generated += row['generated']
            for case in CASES:
                rows = [r for r in arm['rows'] if r['case'] == case]
                assert all(r['prompt_tokens'] == 2048 and r['generated'] == 128 for r in rows)
                assert all(arm['summary'][case][k] == statistics.median(r[k] for r in rows[1:]) for k in METRICS)
        for case in CASES:
            for metric in METRICS:
                a, b, c = [r['summary'][case][metric] for r in result['runs']]
                baseline, candidate = (math.sqrt(a*c), b) if order == 'forward' else (b, math.sqrt(a*c))
                row = result['comparison'][case][metric]
                assert row == dict(baseline=baseline, candidate=candidate,
                    throughput_change_pct=100*(baseline/candidate-1), baseline_drift_pct=100*(c/a-1))
            row = result['comparison'][case]['decode_ms']
            assert row['throughput_change_pct'] >= 20 and abs(row['baseline_drift_pct']) <= 3
    assert count == 42 and generated == 4704
    values = {c: {k: {field: [t['result']['comparison'][c][k][field] for t in data['trials']]
                     for field in ('baseline', 'candidate', 'throughput_change_pct', 'baseline_drift_pct')}
                  for k in METRICS} for c in CASES}
    print('PASS 42 requests/4704 outputs, raw exact8 fields, both20%/3% brackets, medians and all timing arithmetic; no GPU')
    return values


def span(values):
    return f'{min(values):.2f}' if min(values) == max(values) else f'{min(values):.2f}–{max(values):.2f}'


def checked_sycl_values():
    # ponytail: reuse the historical source/arithmetic checks; keep the two SYCL versions distinct.
    import render_orca_u4 as history
    current = checked_values()
    _, archive = history.checked_values()
    readme = (ROOT / 'README.md').read_text(encoding='utf8')
    assert re.search(r'!\[[^\]]*\]\(([^)]+)\)', readme).group(1) == 'docs/media/orca-runtime-paths.png'
    assert readme.index('docs/media/orca-current-vs-sycl.png') < readme.index('<a id="sycl-original-mainline">') < readme.index('docs/media/orca-current-default.png')
    for ratio in archive['throughput_ratios']['historical_decode_ratio'].values():
        assert f'{ratio:.2f}×' in readme
    assert f"{archive['throughput_ratios']['historical_long_prefill_ratio']:.2f}×" in readme
    sycl = json.loads(history.SOURCES[0].read_text(encoding='utf8'))['performance']['u4n16']
    values = {}
    for case in CASES:
        values[case] = {}
        for metric in METRICS:
            baseline = sycl[case][metric]['baseline']
            assert math.isfinite(baseline) and baseline > 0
            candidates = current[case][metric]['candidate']
            values[case][metric] = dict(baseline=[baseline]*2, candidate=candidates,
                throughput_change_pct=[100*(baseline/v-1) for v in candidates])
    print('PASS current/SYCL cross-cohort ratios, pinned first image/order and separate SYCL10-21/mainline history')
    return values, (SOURCE, history.SOURCES[0], history.SOURCES[3])


def render_paths(check_only):
    # ponytail: retain fixed rows from the old diagram; redraw standalone if that source layout changes.
    source = ROOT / 'docs/media/orca-direct-optimization'
    out = ROOT / 'docs/media/orca-runtime-paths'
    ns = 'http://www.w3.org/2000/svg'
    original = ET.parse(source.with_suffix('.svg')).getroot()
    nodes = [e for e in original if e.tag in (f'{{{ns}}}rect', f'{{{ns}}}text') and 693 <= float(e.get('y')) <= 1066]
    common = ['D3DKMT', 'Windows KMD', 'Intel GPU']
    rows = (
        (810, ['SYCL', 'Strata', 'SYCL', 'UR', 'Level Zero / Intel UMD', *common]),
        (900, ['L0 · Default', 'Strata', 'Orca L0', 'Level Zero / Intel UMD', *common]),
        (990, ['Direct WIP', 'Strata', 'Orca Direct · WIP · GPU command encoding', *common]),
    )
    for y, expected in rows:
        assert [e.text for e in nodes if e.tag == f'{{{ns}}}text' and float(e.get('y')) == y and e.text != '→'] == expected
    assert 'docs/media/orca-runtime-paths.png' in (ROOT / 'README.md').read_text(encoding='utf8')
    with Image.open(source.with_suffix('.png')) as original_png:
        assert original_png.size == (2000, 1450)
        cropped = original_png.crop((40, 665, 1960, 1090))
    print('PASS SYCL/UR/L0/Direct Windows path nodes and shared D3DKMT/kernel/GPU tail; no performance claims or GPU')
    if check_only:
        return
    ET.register_namespace('', ns)
    svg = ET.Element(f'{{{ns}}}svg', width='1920', height='425', viewBox='40 665 1920 425',
                     role='img', attrib={'aria-labelledby': 'title desc'})
    ET.SubElement(svg, f'{{{ns}}}title', id='title').text = 'Windows call paths for SYCL, Orca L0 and Orca Direct'
    ET.SubElement(svg, f'{{{ns}}}desc', id='desc').text = (
        'SYCL: Strata to SYCL to Unified Runtime to Level Zero/Intel UMD. '
        'Current default Orca L0: Strata to Orca L0 to Level Zero/Intel UMD. '
        'Orca Direct WIP: Strata to Orca Direct GPU command encoding. '
        'All three Windows paths continue through D3DKMT and the Windows kernel driver to Intel GPU. '
        'Offline kernel building still uses the Intel compiler. This execution-path diagram makes no speed claim.')
    for suffix in ('.svg', '.png'):
        path = source.with_suffix(suffix)
        ET.SubElement(svg, f'{{{ns}}}metadata', attrib={'data-source': path.relative_to(ROOT).as_posix(),
            'data-sha256': hashlib.sha256(path.read_text(encoding='utf8').encode('utf8') if suffix == '.svg' else path.read_bytes()).hexdigest()})
    ET.SubElement(svg, f'{{{ns}}}rect', x='40', y='665', width='1920', height='425', fill='#080f1c')
    svg.extend(nodes)
    ET.indent(svg, space='  ')
    ET.ElementTree(svg).write(out.with_suffix('.svg'), encoding='utf-8', xml_declaration=True)
    cropped.save(out.with_suffix('.png'), optimize=True)
    with Image.open(out.with_suffix('.png')) as check:
        assert check.size == (1920, 425)
        check.verify()
    assert ET.parse(out.with_suffix('.svg')).getroot().get('viewBox') == '40 665 1920 425'
    print('Restored and checked', out.relative_to(ROOT))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check-only', action='store_true')
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--sycl', action='store_true', help='Current default versus frozen SYCL11-04; cross-cohort ratios, not a new bracket')
    mode.add_argument('--paths-only', action='store_true', help='Restore only the Windows SYCL/L0/Direct call-path diagram')
    args = parser.parse_args()
    if args.paths_only:
        render_paths(args.check_only)
        return
    values, sources = checked_sycl_values() if args.sycl else (checked_values(), (SOURCE,))
    if args.check_only:
        return
    out = ROOT / 'docs/media/orca-current-vs-sycl' if args.sycl else OUT
    width, height = 2000, 1420
    canvas = Image.new('RGB', (width, height), '#080f1c')
    draw, fonts = ImageDraw.Draw(canvas), {}
    ns = 'http://www.w3.org/2000/svg'
    ET.register_namespace('', ns)
    svg = ET.Element(f'{{{ns}}}svg', width=str(width), height=str(height), viewBox=f'0 0 {width} {height}',
                     role='img', attrib={'aria-labelledby': 'title desc'})
    ET.SubElement(svg, f'{{{ns}}}title', id='title').text = (
        'Current default vs SYCL: Code decode +40.78–41.01%, Prose +49.12–49.31%; same-machine cross-cohort rates'
        if args.sycl else 'Current default vs original HC256: Code decode +21.21–21.64%, Prose +22.89–23.27%; full request +4.15–5.19%')
    ET.SubElement(svg, f'{{{ns}}}desc', id='desc').text = (
        '2026-11-08, Intel Arc A770 16GB, Xeon E5-2696 v4, 256 GiB DDR3, Windows 10 22H2. '
        'Current Orca L0 U4/min16/M16/HC SG16/spec6/confidence0.6/layer-profile default versus original HC256. '
        'Fixed greedy Code2K/Prose2K,2048 input/128 output. Forward old/new/old and independent reverse new/old/new; '
        'Short16 then three alternating cycles per process,cycle0 warmup,median cycles1/2,outer geometric times. '
        'Decode tok/s=128000/decode_ms; bars use forward rates,labels span both orders,not confidence intervals. '
        '42 requests/4704 tokens,full output and8 fields checked;6 exit codes0,max absolute decode control drift0.150334%. '
        'Full request includes prefill and excludes HTTP/startup. No new settings or kernels; no compounded gains. '
        'Earlier timing differences,Prose forks and8K low-VRAM flags retained; no repair or cause claimed.')
    if args.sycl:
        svg.find(f'{{{ns}}}desc').text = (
            'Same A77016GB/Xeon E5-2696v4/256GiB DDR3/Windows10. Current default measured2026-11-08 versus '
            'corrected SYCL native FP16 paired-M4 measured2026-11-04;2048 input/128 output,128000/decode_ms. '
            'SYCL durations are geometric means of native controls0/3 in the frozen native/U4/U2/native cohort; '
            'current durations come from the independent forward/reverse HC256/default comparisons. '
            'Bars use the first current summary;labels span its two orders,not confidence intervals. '
            'This is a cross-cohort configuration/rate reference,not a concurrent SYCL/default/SYCL bracket, '
            'not a pure API speedup and not proof of output/quality equivalence between versions. '
            'SYCL has5771slots/spec4;current has6509slots/spec6/confidence0.6/layer-profile/HC SG16. '
            'Current42requests/4704outputs pass full8-field references,6exit codes0. Full request includes prefill '
            'and excludes HTTP/startup. Separate original-mainline history uses SYCL2026-10-21;do not multiply ratios.')
    for source in sources:
        ET.SubElement(svg, f'{{{ns}}}metadata', attrib={'data-source': source.relative_to(ROOT).as_posix(),
            'data-sha256': hashlib.sha256(source.read_text(encoding='utf8').encode('utf8')).hexdigest()})
    cyan, green, muted = '#64e8ef', '#c1f787', '#a5b7cd'

    # ponytail: retain the existing Pillow/SVG style with a fixed layout and no chart dependency.
    def rect(x, y, w, h, fill, radius=16):
        assert 0 <= x <= x+w <= width and 0 <= y <= y+h <= height
        draw.rounded_rectangle((x, y, x+w, y+h), radius, fill=fill)
        ET.SubElement(svg, f'{{{ns}}}rect', x=str(x), y=str(y), width=str(w), height=str(h), rx=str(radius), fill=fill)

    def text(x, y, label, size=28, color='#eaf2ff', bold=False, right=False, limit=1850):
        key = size, bold
        if key not in fonts:
            fonts[key] = ImageFont.truetype('C:/Windows/Fonts/segoeuib.ttf' if bold else 'C:/Windows/Fonts/segoeui.ttf', size)
        box = draw.textbbox((x, y), label, font=fonts[key], anchor='rs' if right else 'ls')
        assert 0 <= box[0] <= box[2] <= width and 0 <= box[1] <= box[3] <= height, (label, box)
        assert box[2]-box[0] <= limit, (label, box, limit)
        draw.text((x, y), label, font=fonts[key], fill=color, anchor='rs' if right else 'ls')
        ET.SubElement(svg, f'{{{ns}}}text', x=str(x), y=str(y), fill=color, attrib={
            'font-family': 'Segoe UI, Arial, sans-serif', 'font-size': str(size),
            'font-weight': '700' if bold else '400', 'text-anchor': 'end' if right else 'start'}).text = label

    text(76, 76, 'STRATA-A770  /  U4 N16 + M16 · ORCA L0', 26, cyan, True)
    text(1924, 76, '2026-11-08 · WINDOWS · A770 16 GB', 24, muted, right=True)
    text(76, 190, 'Current default vs SYCL' if args.sycl else 'Decode gains above 20%', 88, bold=True)
    text(80, 249, 'SYCL Nov 4 → Current Nov 8 · 2048 input / 128 output · Cross-cohort rates'
         if args.sycl else 'Current vs original HC256 · 2048 input / 128 output · Forward and reverse brackets', 34, muted)
    for column, case in enumerate(CASES):
        x = 72+column*948
        color = green if column == 0 else cyan
        rect(x, 302, 908, 475, '#101d2e')
        text(x+32, 361, case, 36, bold=True)
        text(x+32, 456, '+'+span(values[case]['decode_ms']['throughput_change_pct'])+'%', 66, color, True, limit=844)
        text(x+32, 502, 'Decode throughput gain · Excludes prefill', 28, muted)
        for index, (field, label, bar_color) in enumerate((('baseline', 'SYCL' if args.sycl else 'HC256', '#90a5bd'), ('candidate', 'Current', color))):
            rates = [128000/v for v in values[case]['decode_ms'][field]]
            y = 550+index*80
            text(x+32, y+31, label, 28, bar_color, True)
            rect(x+190, y, 350*rates[0]/42, 40, bar_color, 6)
            text(x+876, y+31, span(rates)+' tok/s', 28, bar_color, True, True, limit=310)
        text(x+32, 744, 'Cross-cohort; current ranges span both orders, not CIs.' if args.sycl
             else 'Forward bars; ranges span both orders, not confidence intervals.', 25, muted, limit=844)
        rect(x, 812, 908, 320, '#101d2e')
        text(x+32, 868, 'Full-request throughput (includes prefill)', 32, bold=True)
        text(x+32, 944, '+'+span(values[case]['request_ms']['throughput_change_pct'])+'%', 58, color, True, limit=844)
        seconds = lambda k, field: span([v/1000 for v in values[case][k][field]])
        text(x+32, 1001, ('SYCL ' if args.sycl else 'HC256 ')+seconds('request_ms', 'baseline')+' s → Current '+seconds('request_ms', 'candidate')+' s', 27, muted, limit=844)
        text(x+32, 1051, 'Current prefill: '+seconds('prompt_ms', 'candidate')+' s', 29, color, limit=844)
        text(x+32, 1100, 'Full request excludes HTTP and process startup.', 25, muted, limit=844)
    if args.sycl:
        text(76, 1194, 'Current: 42 requests / 4704 output tokens / 8 fields pass · SYCL: separate frozen measurements', 29, green)
    else:
        drift = max(abs(v) for c in CASES for v in values[c]['decode_ms']['baseline_drift_pct'])
        text(76, 1194, f'42 requests / 4704 tokens match · 8 fields pass · 6 clean exits · Max decode control drift {drift:.3f}%', 29, green)
    text(76, 1244, 'Intel Arc A770 16 GB · Xeon E5-2696 v4 · 256 GiB DDR3 · Windows 10 · oneAPI 2025.3', 27, muted)
    text(76, 1293, 'SYCL: native FP16 M4 / 5771 slots / spec4 · Current: U4 / HC SG16 / 6509 slots / spec6'
         if args.sycl else 'Current: HC SG16 · spec6 / confidence 0.6 · Layer expert ordering · cache6200 / 6509 slots · adapt16 · 8K', 27, muted)
    text(76, 1352, 'Cross-cohort whole versions · No concurrent SYCL bracket or API-only attribution · Historical ratios not compounded'
         if args.sycl else 'Fixed 2K on this PC · Gains not compounded · Earlier output forks not claimed fixed · 8K low-VRAM flag retained', 26, cyan)
    ET.indent(svg, space='  ')
    ET.ElementTree(svg).write(out.with_suffix('.svg'), encoding='utf-8', xml_declaration=True)
    canvas.save(out.with_suffix('.png'), optimize=True)
    with Image.open(out.with_suffix('.png')) as check:
        assert check.size == (width, height)
        check.verify()
    metadata = ET.parse(out.with_suffix('.svg')).getroot().findall(f'{{{ns}}}metadata')
    assert [m.get('data-sha256') for m in metadata] == [hashlib.sha256(p.read_text(encoding='utf8').encode('utf8')).hexdigest() for p in sources]
    print('Rendered and checked', out.relative_to(ROOT))


if __name__ == '__main__':
    main()
