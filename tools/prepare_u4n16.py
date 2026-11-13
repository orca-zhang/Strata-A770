"""Preserve a verified A770 executable and prepare an optional U4 N16 config.

python tools/prepare_u4n16.py --self-check
python tools/prepare_u4n16.py --out strata-iq2_xs-u4n16-sycl.json
python tools/prepare_u4n16.py --backend orca-l0 --out strata-iq2_xs-u4n16-orca-l0.json
Without --out, only check RAM, the executable and all U4 assets. No GPU work.
"""
import argparse
import gzip
import hashlib
import json
from pathlib import Path
import re
import shutil
import sys

ROOT = Path(__file__).resolve().parents[1]
GIB = 2**30


def sha(path):
    with path.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def check_files(directory, hashes):
    for name, digest in hashes.items():
        path = directory / name
        if not path.resolve().is_relative_to(directory.resolve()):
            raise ValueError(f'asset outside directory: {name}')
        if sha(path) != digest:
            raise ValueError(f'U4 asset hash mismatch: {path}')


def check_ram(available, required):
    if available is None or available < required:
        raise ValueError(f'U4 N16 needs {required/GIB:.2f} GiB available before model startup; '
                         f'available={available/GIB if available is not None else "unknown"} GiB')


def profile(base, measured, exe, assets, backend='sycl'):
    env = dict(base.get('env', {})) if backend!='sycl' else {}
    env.update(measured['environment'])
    env.update(STRATA_PREFILL_U4_EXPERTS='1', STRATA_U4_TILE_N8='0', STRATA_U4_MIN_ROWS='128',
               STRATA_U4_GROUP_PRIORITY='id', STRATA_PREFILL_U4_EXPERT_DIR=str(assets), STRATA_PREFILL_U2_DOWN='0')
    if backend=='sycl':
        env.update(STRATA_PREFILL_FP16_PAIR='0', STRATA_WARM_GRAPHS='0', STRATA_ORCA_L0='0')
    return dict(exe=str(exe), args=measured['args'], cwd=str(ROOT), tokenizer=base['tokenizer'],
                backend='sycl' if backend=='sycl' else 'orca', model_name='qwen3.8-flash-next-iq2_xs-u4n16'+('' if backend=='sycl' else '-'+backend),
                host='127.0.0.1', port=8080, lib_dirs=base['lib_dirs'], env=env,
                log=str(ROOT/('strata-iq2_xs-u4n16-'+backend+'.log')))


def self_check():
    import tempfile
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        (root/'weight.u4').write_bytes(b'original')
        hashes = {'weight.u4': sha(root/'weight.u4')}
        check_files(root, hashes)
        (root/'weight.u4').write_bytes(b'corrupt!')
        try:
            check_files(root, hashes)
        except ValueError:
            pass
        else:
            raise AssertionError('corrupt U4 weights accepted')
        try:
            check_files(root, {'../outside.u4': 'unused'})
        except ValueError:
            pass
        else:
            raise AssertionError('asset path escaped its directory')
    check_ram(10, 10)
    for available in (9, None):
        try:
            check_ram(available, 10)
        except ValueError:
            pass
        else:
            raise AssertionError('insufficient/unknown RAM accepted')
    cfg = profile(dict(tokenizer='tokens', lib_dirs=[]), dict(args=[], environment={}), Path('engine'), Path('assets'))
    assert cfg['backend']=='sycl' and cfg['env']['STRATA_U4_TILE_N8']=='0'
    assert cfg['env']['STRATA_PREFILL_U4_EXPERTS']=='1' and cfg['env']['STRATA_PREFILL_U2_DOWN']=='0'
    for backend,flag in (('orca-l0','1'),('orca-direct','1')):
        env=dict(STRATA_ORCA_L0=flag,STRATA_ORCA_IMAGES='images',STRATA_PREFILL_FP16_PAIR='1',
                 STRATA_PREFILL_XMX_EXPERT_BATCH='1',STRATA_WARM_GRAPHS='1')
        if backend=='orca-direct':env['STRATA_ORCA_DG2_MEMORY']='memory.zebin'
        cfg=profile(dict(tokenizer='tokens',lib_dirs=[],env=env),dict(args=[],environment={}),Path('engine'),Path('assets'),backend)
        assert cfg['backend']=='orca' and all(cfg['env'][k]==v for k,v in env.items())
        assert cfg['env']['STRATA_U4_TILE_N8']=='0'
    print('PASS corrupt/escaping assets, RAM boundary/unknown RAM, N16 and retained SYCL/L0/Direct policies; no GPU')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path)
    parser.add_argument('--asset-dir', type=Path)
    parser.add_argument('--backend', choices=('sycl','orca-l0','orca-direct'), default='sycl')
    parser.add_argument('--self-check', action='store_true')
    args = parser.parse_args()
    if args.self_check:
        self_check()
        return
    def archive(name):
        return json.loads(gzip.decompress((ROOT/'sycl/orca'/name).read_bytes()))
    if args.backend=='sycl':
        evidence = archive('lowbit-fp16-m4-a770-windows-2026-11-04.json.gz')
        summary = evidence['summary']
        if not summary['complete'] or not summary['quality']['raw_scores_identical_to_healthy_m8']['u4n16']:
            raise ValueError('U4 N16 model gate is incomplete')
        prefix = 'build-lowbit-model-m4pair-20261104/'
        measured = json.loads(evidence['files'][prefix+'performance-r1/results.json'])
        source = ROOT/prefix/'bin/strata-unified-all-operators.exe'
        expected = summary['exe_sha256']
        base = json.loads((ROOT/'strata-iq2_xs.json').read_text(encoding='utf8'))
    else:
        evidence = archive('u4n16-a770-windows-2026-11-04.json.gz')
        mode = args.backend.removeprefix('orca-')
        prefix = 'build-orca-u4n16-20261104/'
        measured = json.loads(evidence['files'][prefix+mode+'-r3/performance-r1/results.json'])
        gate = json.loads(evidence['files'][prefix+mode+'-r3/quality-r1/results.json'])
        if not gate['complete'] or gate['passes']!={'u4n16':True} or not measured['complete']:
            raise ValueError('Orca U4 N16 model gate is incomplete')
        source = ROOT/prefix/'model-r3'/(mode+'.exe')
        expected = evidence['summary']['backends'][mode]['exe_sha256']
        base = json.loads(evidence['files'][prefix+mode+'-r3/config.json'])
        source_images = ROOT/prefix/'check-r2/images'
        preserved_images = ROOT/'engine/u4n16-orca-images'
        image_hashes = evidence['summary']['image_sha256']
        check_files(preserved_images if preserved_images.exists() else source_images, image_hashes)
        if mode=='direct':
            source_memory = Path(base['env']['STRATA_ORCA_DG2_MEMORY'])
            preserved_memory = ROOT/'engine/u4n16-orca-direct/memory.zebin'
            memory = preserved_memory if preserved_memory.exists() else source_memory
            if sha(memory)!=evidence['summary']['direct_memory_sha256']:
                raise ValueError('Direct memory kernel differs from the verified image')
    original = archive('lowbit-model-a770-windows-2026-11-04.json.gz')
    manifest = json.loads(original['files']['build-sycl-a770-u4s4-wide-experts-20261027/prepared.json'])
    assets = (args.asset_dir or Path(manifest['physical_asset_directory'])).resolve()
    hashes = {p.split('/assets/', 1)[1]:h for p,h in manifest['files_sha256'].items()
              if '/assets/' in p and p.endswith(('.u4', 'experts.list'))}
    if len(hashes)!=6469:
        raise ValueError('expected 6468 U4 weight files and experts.list')
    arena_mib = max(int(re.search(r'\barena_mib=(\d+)', r['info']).group(1)) for r in measured['runs'])
    # ponytail: measured arena + U4 payload + 6 GiB headroom; a budget, not a measured minimum-RAM claim.
    required = arena_mib*2**20 + manifest['u4_payload_bytes'] + 6*GIB
    sys.path.insert(0, str(ROOT))
    from serve.telemetry import _CpuRamFallback
    used, total = _CpuRamFallback.ram()
    available = None if used is None or total is None else total-used
    check_ram(available, required)
    preserved = ROOT/('engine/u4n16-'+args.backend+'/strata.exe')
    exe = preserved if preserved.exists() else source
    if sha(exe)!=expected:
        raise ValueError(f'executable differs from verified U4 build: {exe}')
    check_files(assets, hashes)
    print(f'PASS 6469 assets; U4 payload {manifest["u4_payload_bytes"]/GIB:.2f} GiB; '
          f'available RAM {available/GIB:.2f} GiB >= budget {required/GIB:.2f} GiB')
    if args.out:
        if args.out.exists():
            raise FileExistsError(f'keeping existing config: {args.out}')
        preserved.parent.mkdir(parents=True, exist_ok=True)
        if not preserved.exists():
            with source.open('rb') as src, preserved.open('xb') as dst:
                shutil.copyfileobj(src, dst)
        if sha(preserved)!=expected:
            raise ValueError('preserved executable hash mismatch')
        if args.backend!='sycl':
            if not preserved_images.exists():
                shutil.copytree(source_images,preserved_images)
            check_files(preserved_images,image_hashes)
            base['env']['STRATA_ORCA_IMAGES']=str(preserved_images)
            measured['environment']['STRATA_ORCA_IMAGES']=str(preserved_images)
            if args.backend=='orca-direct':
                if not preserved_memory.exists():
                    with source_memory.open('rb') as src, preserved_memory.open('xb') as dst:
                        shutil.copyfileobj(src,dst)
                if sha(preserved_memory)!=evidence['summary']['direct_memory_sha256']:
                    raise ValueError('preserved Direct memory kernel hash mismatch')
                base['env']['STRATA_ORCA_DG2_MEMORY']=str(preserved_memory)
                measured['environment']['STRATA_ORCA_DG2_MEMORY']=str(preserved_memory)
        cfg = profile(base, measured, preserved, assets,args.backend)
        with args.out.open('x', encoding='utf8') as f:
            json.dump(cfg, f, indent=2, ensure_ascii=False)
            f.write('\n')
        print('PREPARED', args.out, 'and preserved verified engine at', preserved)


if __name__=='__main__':
    main()
