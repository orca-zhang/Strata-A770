"""Compile the real SYCL output-window limits without loading a model."""
import argparse
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=ROOT/'sycl/src/program/generate.cpp')
    parser.add_argument('--cxx', default='c++')
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=False)
    source = args.source.read_text(encoding='utf-8')
    limits = re.findall(r'^\s*(T = \(int\) std::min<int64_t>\(T, (?:max_new - produced_n|o.max_new - \(int64_t\) produced.size\(\))\);)$', source, re.M)
    assert len(limits) == 2, limits
    cpp = args.out/'check.cpp'
    cpp.write_text('''#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>
int main() {
    int checks = 0, trimmed = 0;
    for (int64_t max_new : {1LL, 2LL, 4LL, 6LL, 32LL})
    for (int64_t produced_n = 0; produced_n < max_new; ++produced_n)
    for (int proposed = 1; proposed <= 8; ++proposed) {
        struct { int64_t max_new; } o{max_new};
        std::vector<int> produced((size_t) produced_n);
        int T = proposed;
        '''+limits[0]+'''
        const int service = T;
        T = proposed;
        '''+limits[1]+'''
        assert(T == service && T >= 1 && T <= proposed);
        // An unlimited request must keep its original window.
        assert((max_new - produced_n >= proposed) == (T == proposed));
        for (int accepted = 0; accepted < T; ++accepted) {
            // Even full acceptance cannot commit more rows than the caller can return.
            assert(produced_n + accepted + 1 <= max_new);
            ++checks;
        }
        // One free context cell is enough for the last output, regardless of the proposal.
        if (produced_n + 1 == max_new) assert(8191 + T <= 8192);
        trimmed += T < proposed;
    }
    assert(trimmed > 0);
    std::printf("PASS %d acceptance cases; %d oversized windows trimmed\\n", checks, trimmed);
}
''', encoding='utf-8')
    exe = args.out/'check.exe'
    flags = ['-Qstd:c++20', '/Od'] if Path(args.cxx).name.lower() == 'icx.exe' else ['-std=c++20', '-O0']
    subprocess.run([args.cxx, *flags, str(cpp), '-o', str(exe)], check=True)
    result = subprocess.check_output([str(exe.resolve())], text=True)
    (args.out/'stdout.txt').write_text(result, encoding='utf-8')
    print(result.strip())


if __name__ == '__main__':
    main()
