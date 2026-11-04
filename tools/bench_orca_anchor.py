"""Replay the published short anchor, optionally bracketed by another runtime.

python tools/bench_orca_anchor.py --config strata-iq2_xs-anchor50.json --out build-anchor-repeat
Add --baseline-config <old-sycl.json> for three independent A/B/A processes.
Use --runtime direct --baseline-runtime l0 for Orca L0 / Direct / L0.
"""
import argparse
import json
import math
import os
from pathlib import Path
import re
import statistics
import subprocess
import time

import bench_deepinfer as b
from check_orca_http import audit

ROOT = Path(__file__).resolve().parents[1]


def summarize(rows):
    rates = [1000 * r['generated'] / r['decode_ms'] for r in rows if r['retained']]
    return dict(decode_tok_s=statistics.median(rates), range_tok_s=[min(rates), max(rates)], retained=len(rates))


def check_parameters(cfg, fixture):
    args = cfg['args']
    for key, value in fixture['parameters'].items():
        assert args.count(key) == 1 and args[args.index(key) + 1] == value, key
    assert all(key in args for key in fixture['required_flags'])
    assert not any(key in args for key in ('--batch', '--parallel', '--expert-profile-save'))
    assert not any(key in cfg.get('env', {}) for key in ('STRATA_DECODE_TIMING', 'STRATA_VERIFY_PROFILE', 'STRATA_TRACE'))


def modules(pid, runtime):
    if runtime != 'sycl':
        loaded = audit(pid)
        names = [Path(row['path']).name.lower() for row in loaded]
        driver = ('ze_loader', 'ze_intel', 'libze_', 'igc64', 'igc32', 'igdfcl', 'igdgmm', 'opencl', 'igdrcl')
        if runtime == 'direct':
            assert not any(part in name for name in names for part in driver), 'Direct loaded a vendor user-mode runtime'
        else:
            assert any(part in name for name in names for part in ('ze_loader', 'ze_intel', 'libze_')), 'L0 runtime missing'
        return loaded
    if os.name == 'nt':
        result = subprocess.run(['powershell', '-NoProfile', '-Command',
                                 f'@(Get-Process -Id {int(pid)} -ErrorAction Stop).Modules.FileName | ConvertTo-Json'],
                                capture_output=True, text=True, check=True, timeout=30)
        paths = json.loads(result.stdout)
        if isinstance(paths, str):
            paths = [paths]
    else:
        paths = sorted({line.split()[-1] for line in Path(f'/proc/{pid}/maps').read_text().splitlines()
                        if len(line.split()) >= 6 and line.split()[-1].startswith('/')})
    assert any('sycl' in Path(p).name.lower() for p in paths), 'baseline did not load SYCL'
    assert any('ur_loader' in Path(p).name.lower() for p in paths), 'baseline did not load Unified Runtime'
    return [dict(path=p, sha256=b.sha(p)) for p in paths if Path(p).is_file()]


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--config', type=Path)
    ap.add_argument('--baseline-config', type=Path)
    ap.add_argument('--runtime', choices=('l0', 'direct'), default='l0')
    ap.add_argument('--baseline-runtime', choices=('sycl', 'l0', 'direct'), default='sycl')
    ap.add_argument('--fixture', type=Path, default=ROOT/'sycl/orca/short-anchor-v1.json')
    ap.add_argument('--out', type=Path)
    ap.add_argument('--monitor', type=Path, help='Optional Sysman monitor.exe; stdout MEM free_bytes samples are retained')
    ap.add_argument('--self-check', action='store_true')
    a = ap.parse_args()
    read = lambda p: json.loads(p.read_text(encoding='utf-8-sig'))
    fixture = read(a.fixture)
    if a.self_check:
        rows = [dict(retained=False, generated=64, decode_ms=9000)] + [dict(retained=True, generated=76, decode_ms=x) for x in (1000, 3000, 2000)]
        assert summarize(rows) == dict(decode_tok_s=38, range_tok_s=[76000/3000, 76], retained=3)
        cfg = dict(args=[x for pair in fixture['parameters'].items() for x in pair] + fixture['required_flags'])
        check_parameters(cfg, fixture)
        cfg['args'][cfg['args'].index('--adapt-decay')+1] = '0.5'
        try:
            check_parameters(cfg, fixture)
        except AssertionError:
            pass
        else:
            raise AssertionError('wrong anchor parameters accepted')
        assert len(fixture['input_ids']) == 28 and len(fixture['expected_output_ids']) == fixture['expected_generated'] == 76
        print('PASS natural output count, warmup exclusion, fixed fixture and parameter rejection; no GPU')
        return
    assert a.config and a.out
    cfg = read(a.config)
    check_parameters(cfg, fixture)
    labels = dict(sycl='SYCL', l0='Orca L0', direct='Orca Direct')
    arms = [(labels[a.runtime], a.config, cfg, a.runtime)]
    if a.baseline_config:
        baseline = read(a.baseline_config)
        check_parameters(baseline, fixture)
        assert baseline['args'] == cfg['args'], 'A/B/A model arguments must match exactly'
        arms = [(labels[a.baseline_runtime]+'-before', a.baseline_config, baseline, a.baseline_runtime),
                *arms, (labels[a.baseline_runtime]+'-after', a.baseline_config, baseline, a.baseline_runtime)]
    out = a.out.resolve()
    out.mkdir()
    images = Path(cfg['env']['STRATA_ORCA_IMAGES'])
    manifest = lambda: {p.relative_to(images).as_posix(): b.sha(p) for p in images.rglob('*') if p.is_file()}
    paths = {Path(__file__), Path(b.__file__), a.fixture, *(path for _, path, _, _ in arms),
             *(Path(c['exe']) for _, _, c, _ in arms)}
    profile = Path(cfg['args'][cfg['args'].index('--expert-profile')+1])
    paths.add(profile)
    paths.update(Path(c['env']['STRATA_ORCA_DG2_MEMORY']) for _, _, c, _ in arms
                 if 'STRATA_ORCA_DG2_MEMORY' in c.get('env', {}))
    if a.monitor:
        paths.add(a.monitor)
    hashes = {str(p.resolve()): b.sha(p) for p in paths}
    d = dict(measurement_complete=False, production_changed=False, quality_equivalence_established=False,
             linux_gpu_validated=False, timing_scope=fixture['protocol'], fixture=fixture, runs=[],
             files_sha256=hashes, images_sha256=manifest())
    save = lambda: b.save(out/'results.json', d)
    save()
    monitor = None
    try:
        with b.gpu_lock():
            with (out/'memory.stdout').open('x', encoding='utf-8') as mlog, (out/'memory.stderr').open('x', encoding='utf-8') as merr:
                if a.monitor:
                    menv = os.environ | dict(ZES_ENABLE_SYSMAN='1')
                    monitor = subprocess.Popen([str(a.monitor.resolve()), '3600'], cwd=ROOT, env=menv, stdout=mlog, stderr=merr)
                    deadline = time.monotonic()+10
                    while 'MEM index=' not in (out/'memory.stdout').read_text(encoding='utf-8'):
                        assert monitor.poll() is None and time.monotonic() < deadline
                        time.sleep(.05)
                for label, path, settings, runtime in arms:
                    env = {k: v for k, v in os.environ.items() if not k.startswith(('STRATA_', 'ORCA_', 'SYCL_', 'UR_', 'ONEAPI_', 'ZES_'))}
                    env.update(settings['env'])
                    env['PATH'] = os.pathsep.join(settings['lib_dirs'])+os.pathsep+env.get('PATH', '')
                    run = dict(label=label, runtime=runtime, config=settings, rows=[], exe_sha256=b.sha(settings['exe']))
                    d['runs'].append(run)
                    print('START', label, flush=True)
                    engine = b.Engine(Path(settings['exe']), settings['args'], env, out/label)
                    try:
                        run['info'] = engine.info
                        info = dict(re.findall(r'(\w+)=([^ ]+)', engine.info))
                        assert all(info.get(k) == v for k, v in fixture['expected_runtime'].items()), engine.info
                        run['modules_startup'] = modules(engine.p.pid, runtime)
                        for index in range(fixture['warmup']+fixture['retained']):
                            assert monitor is None or monitor.poll() is None
                            row = engine.gen(fixture['input_ids'], fixture['max_new'], keys='temperature=0')
                            row.update(index=index, retained=index >= fixture['warmup'])
                            run['rows'].append(row)
                            save()
                            assert row['tokens'] == fixture['expected_output_ids']
                            assert row['generated'] == fixture['expected_generated'] and row['finish'] == fixture['expected_finish']
                            assert row['accepted'] == fixture['expected_accepted'] and row['offered'] == fixture['expected_offered']
                            assert row['reused'] == row['resume'] == 0
                            print(label, index, 'kept' if row['retained'] else 'warm', round(1000*row['generated']/row['decode_ms'], 3), flush=True)
                        run['modules_workload'] = modules(engine.p.pid, runtime)
                    finally:
                        engine.close()
                    run['exit_code'] = engine.p.returncode
                    run['summary'] = summarize(run['rows'])
                    save()
                if monitor:
                    monitor.terminate()
                    d['monitor_exit_code'] = monitor.wait(timeout=10)
                    monitor = None
            if a.monitor:
                samples = [int(x)/2**20 for x in re.findall(r'MEM index=0 error=0 free_bytes=(\d+)', (out/'memory.stdout').read_text(encoding='utf-8'))]
                assert samples
                d['physical_memory'] = dict(samples=len(samples), min_free_mib=min(samples), headroom_passed=min(samples) >= 1024)
            if a.baseline_config:
                before, candidate, after = [r['summary']['decode_tok_s'] for r in d['runs']]
                reference = math.sqrt(before*after)
                d['comparison'] = dict(baseline_tok_s=reference, orca_tok_s=candidate,
                                       throughput_change_pct=100*(candidate/reference-1),
                                       baseline_time_drift_pct=100*(before/after-1))
            d['files_unchanged'] = all(b.sha(Path(p)) == value for p, value in hashes.items())
            d['images_unchanged'] = manifest() == d['images_sha256']
            assert d['files_unchanged'] and d['images_unchanged']
            d['measurement_complete'] = True
    except BaseException as error:
        d['error'] = repr(error)
        raise
    finally:
        if monitor:
            monitor.terminate()
            monitor.wait(timeout=10)
        save()
    print('COMPLETE', [r['summary'] for r in d['runs']], d.get('comparison'), flush=True)


if __name__ == '__main__':
    main()
