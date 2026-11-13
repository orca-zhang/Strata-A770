#!/usr/bin/env python3
"""Build native DG2 on Linux and run CPU checks only; --frontend also builds Strata."""
import argparse
from datetime import datetime, timezone
from pathlib import Path
import os
import platform
import shutil
import subprocess
import sys

import bench_deepinfer as bench


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True, help='New output directory')
    parser.add_argument('--frontend', action='store_true', help='Compile the full engine with icpx; do not execute it')
    parser.add_argument('--ggml-dir', type=Path, help='Existing pinned llama.cpp source, needed by --frontend')
    args = parser.parse_args()
    if sys.platform != 'linux':
        parser.error('Run this CPU-only check under Linux or WSL')
    compiler = shutil.which('icpx' if args.frontend else 'g++')
    if not compiler:
        parser.error('Initialize the Linux oneAPI compiler environment' if args.frontend else 'g++ is missing')
    root = Path(__file__).resolve().parents[1]
    ggml = (args.ggml_dir or root / 'build-sycl/_deps/strata_llamacpp-src').resolve()
    if args.frontend and not (ggml / 'ggml/CMakeLists.txt').is_file():
        parser.error('--frontend requires --ggml-dir pointing to the pinned llama.cpp source')
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    (out / 'runner.py').write_bytes(Path(__file__).read_bytes())
    build = out / 'build'
    # Hash inputs rather than claiming a clean Git tree in a handoff snapshot.
    prefixes = ['orca-l0'] + (['include', 'src', 'sycl'] if args.frontend else [])
    inputs = [p for prefix in prefixes for p in (root / prefix).rglob('*')
              if p.is_file() and p.suffix in ('.cpp', '.hpp', '.h', '.inl', '.inc', '.cl', '.cmake', '.txt', '.py')
              and '__pycache__' not in p.parts]
    inputs.append(Path(__file__))
    hashes = {p.relative_to(root).as_posix(): bench.sha(p) for p in inputs}
    result = dict(complete=False, gpu_executed=False, linux_gpu_validated=False,
        frontend_requested=args.frontend, frontend_built=False, commands=[],
        source_sha256=hashes, compiler=compiler, platform=platform.platform(),
        recorded_utc=datetime.now(timezone.utc).isoformat())

    def save():
        bench.save(out / 'results.json', result)

    def run(label, command):
        print('RUN', label, flush=True)
        command = list(map(str, command))
        with (out / (label + '.log')).open('xb') as log:
            p = subprocess.run(command, cwd=root, stdout=log, stderr=subprocess.STDOUT, timeout=3600)
        result['commands'].append(dict(label=label, args=command, exit_code=p.returncode))
        save()
        if p.returncode:
            print((out / (label + '.log')).read_text(errors='replace')[-4000:], flush=True)
            p.check_returncode()
        print('PASS', label, flush=True)

    try:
        with bench.gpu_lock():
            options = ['-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_CXX_COMPILER=' + compiler,
                '-DORCA_L0_BUILD_RUNTIME=OFF', '-DORCA_L0_BUILD_CHECKS=ON',
                '-DORCA_L0_BUILD_DRM_CHECKS=ON', '-DORCA_L0_BUILD_GPU_CHECKS=OFF',
                '-DORCA_L0_DG2_MEMORY_ZEBIN=', '-DORCA_L0_WDDM_ZEBIN=']
            if shutil.which('ninja') and not os.environ.get('CMAKE_GENERATOR'):
                options.extend(['-G', 'Ninja'])
            if args.frontend:
                options.extend(['-DCMAKE_C_COMPILER=' + (shutil.which('icx') or compiler),
                    '-DSTRATA_ORCA_DG2=ON', '-DSTRATA_ORCA_BUILD_ENGINE=ON',
                    '-DSTRATA_GGML_DIR=' + str(ggml)])
            run('configure', ['cmake', '-S', root / ('sycl/orca' if args.frontend else 'orca-l0'),
                              '-B', build, *options])
            targets = ['orca-l0-ring-check', 'orca-l0-drm-submit-check', 'orca-l0-drm-compute-check',
                       'orca-l0-blitter-check']
            run('build', ['cmake', '--build', build, '--target', *targets,
                          *(['strata', 'strata-orca-ple-reader-check', 'strata-orca-hc-check'] if args.frontend else []), '-j', '2'])
            binary_dir = build / 'runtime' if args.frontend else build
            # Explicit CPU entry points: frontend CMake registers GPU tests even
            # when kernel compilation/GPU-check options are disabled.
            run('ring-cpu', [binary_dir / 'orca-l0-ring-check'])
            run('packets-cpu', [binary_dir / 'orca-l0-drm-submit-check', '--self-check'])
            run('encoder-cpu', [binary_dir / 'orca-l0-drm-compute-check', '--self-check'])
            run('blitter-cpu', [binary_dir / 'orca-l0-blitter-check'])
            binaries = [binary_dir / name for name in targets]
            if args.frontend:
                ple_check = build / 'strata-orca-ple-reader-check'
                run('ple-reader-cpu', [ple_check, '--selftest', '--dir', out])
                binaries.extend([build / 'strata', ple_check, build / 'strata-orca-hc-check'])
            for index, binary in enumerate(binaries):
                label = 'dependencies-' + str(index)
                run(label, ['ldd', binary])
                libraries = (out / (label + '.log')).read_text().lower()
                removed = ('ze_loader', 'ze_intel', 'igc', 'igdfcl', 'libsycl', 'ur_loader', 'ur_adapter', 'libdrm.so')
                if any(name in libraries for name in removed):
                    raise RuntimeError('Native executable links a removed runtime: ' + str(binary))
            assert all(bench.sha(root / p) == h for p, h in hashes.items()), 'Source changed during check'
            result.update(complete=True, cpu_checks=5 if args.frontend else 4, frontend_built=args.frontend,
                executable_sha256={str(p.relative_to(out)): bench.sha(p) for p in binaries})
            save()
            print(f'PASS Linux native build, {result["cpu_checks"]} CPU checks and dependency audit; no GPU execution', flush=True)
    except BaseException as error:
        result['error'] = repr(error)
        save()
        raise


if __name__ == '__main__':
    main()
