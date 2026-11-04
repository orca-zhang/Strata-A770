"""Compile the real Orca draft-budget bounds without loading a model.

Run with a configured C++ compiler: --cxx <compiler> --out <fresh-directory>.
"""
import argparse
import importlib.util
from pathlib import Path
import re
import subprocess

from bench_deepinfer import ROOT, gpu_lock


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--cxx', default='c++')
    ap.add_argument('--out', type=Path, required=True)
    a = ap.parse_args()
    a.out = a.out.resolve()
    a.out.mkdir(parents=True, exist_ok=False)
    spec = importlib.util.spec_from_file_location('prepare', ROOT / 'sycl/orca/prepare.py')
    prepare = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(prepare)
    generated = prepare.engine_source((ROOT / 'sycl/src/program/generate.cpp').read_text('utf8'))
    bounds = re.findall(r'#ifndef STRATA_ORCA_DG2\n\s*//[^\n]*\n'
                        r'\s*if \([^\n]+\)\n\s*mtp.set_max_drafts\([^\n]+\);\n#endif', generated)
    assert len(bounds) == 2, bounds
    cpp = a.out / 'budget.cpp'
    cpp.write_text('''#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <vector>
struct Draft { int cap = -7; void set_max_drafts(int k) { cap = k; } };
int main() {
    int checks = 0;
    for (int S_mtp = 1; S_mtp <= 8; ++S_mtp)
    for (int64_t max_new : {int64_t(1), int64_t(2), int64_t(3), int64_t(8), int64_t(128),
                            std::numeric_limits<int64_t>::max()})
    for (int64_t produced_n : {int64_t(0), max_new / 2, max_new - 1, max_new})
    for (bool use_mtp : {false, true}) {
        const bool last = produced_n >= max_new;
        struct { int64_t max_new; } o{max_new};
        // Only size() is used in the real CLI bound; keep INT64_MAX cases allocation-free.
        struct { int64_t n; size_t size() const { return (size_t)n; } } produced{produced_n};
        Draft mtp;
        ''' + bounds[0] + '''
        const int service = mtp.cap;
        mtp.cap = -7;
        ''' + bounds[1] + '''
        assert(mtp.cap == service);
#ifdef STRATA_ORCA_DG2
        assert(service == -7);
#else
        if (!use_mtp || last) { assert(service == -7); continue; }
        const int64_t remaining = max_new - produced_n;
        assert(service >= 0 && service < S_mtp && service < remaining);
        // A later long request restores full depth after an earlier short request.
        assert((remaining >= S_mtp) == (service == S_mtp - 1));
        // With both confidence floors zero, the existing verify budget still excludes uncomputed drafts.
        const int generated = std::max(1, service); // The first draft also maintains KV.
        for (int proposed = 1; proposed <= S_mtp; ++proposed)
            assert(std::min<int64_t>(proposed, remaining) - 1 <= generated);
#endif
        ++checks;
    }
    assert(checks > 0);
    std::printf("PASS %d budget cases\\n", checks);
}
''', encoding='utf8')
    with gpu_lock():
        for backend in ('l0', 'dg2'):
            exe = a.out / (backend + '.exe')
            flags = ['-Qstd:c++20', '/Od'] if Path(a.cxx).name.lower() == 'icx.exe' else ['-std=c++20', '-O0']
            command = [a.cxx, *flags, str(cpp), '-o', str(exe)]
            if backend == 'dg2':
                command.append('-DSTRATA_ORCA_DG2=1')
            subprocess.run(command, cwd=a.out, check=True)
            result = subprocess.check_output([str(exe.resolve())], text=True)
            (a.out / (backend + '.stdout')).write_text(result, encoding='utf8')
            print(backend, result.strip(), flush=True)


if __name__ == '__main__':
    main()
