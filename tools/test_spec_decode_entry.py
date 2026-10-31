"""Compile the real CLI handoff/entry conditions without loading a model.

python tools/test_spec_decode_entry.py --cxx c++ --out build-entry-check
The compiler must already be configured in the environment (no dependencies installed).
"""
import argparse
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def extract(source, pattern):
    matches = re.findall(pattern, source, re.M)
    assert len(matches) == 1, (pattern, len(matches))
    return matches[0]


def check(source, cxx, out, index, expect_zero_bug=False):
    text = source.read_text(encoding='utf-8')
    init = extract(text, r'^    (int64_t spec_pos = [^;]+;)')
    native = extract(text, r'^        (if \(native_pack\) \{ spec_pos = pos; break; \})$')
    handoff = extract(text, r'^        (if \(o.spec > 0 && pos >= n_prompt - 1\) \{ spec_pos = pos \+ 1; break; \})$')
    guard = extract(text, r'^    (if \(spec_pos [^\n]+\{)$')
    cpp = out/f'entry-{index}.cpp'
    cpp.write_text('''#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>
bool enter(bool native_pack, int64_t pos_start, int spec, int64_t max_new, int count, bool ended) {
    struct { int spec; int64_t max_new; } o{spec, max_new};
    std::vector<int> produced(count);
    const int64_t n_prompt = pos_start + 1;
    '''+init+'''
    for (int64_t pos = pos_start;; ++pos) {
        '''+native+'''
        '''+handoff+'''
        // The harness omits token computation; stop after the last prompt position.
        if (pos >= n_prompt - 1) break;
    }
    '''+guard+''' return true; }
    return false;
}
int main() {
    int checks = 0;
    for (bool native : {false, true})
    for (int spec : {0, 4}) {
        if (native && spec == 0) continue; // Native packs require spec >= 2 at the CLI boundary.
        for (int pos : {0, 7})
        for (int cap : {1, 4, 6})
        for (int count : {0, cap, cap+1})
        for (bool ended : {false, true}) {
            bool expected = spec > 0 && count < cap && !ended;
            if ('''+str(expect_zero_bug).lower()+''' && native && pos == 0) expected = false;
            assert(enter(native, pos, spec, cap, count, ended) == expected);
            ++checks;
        }
    }
    std::printf("PASS %d CLI entry checks\\n", checks);
}
''', encoding='utf-8')
    exe = out/f'entry-{index}.exe'
    subprocess.run([cxx, '-std=c++20', '-O0', str(cpp), '-o', str(exe)], check=True)
    result = subprocess.check_output([str(exe.resolve())], text=True)
    print(source, result.strip(), flush=True)
    (out/f'entry-{index}.stdout').write_text(result, encoding='utf-8')


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--cxx', default='c++')
    ap.add_argument('--source', type=Path, action='append')
    ap.add_argument('--out', type=Path, required=True)
    ap.add_argument('--expect-zero-bug', action='store_true', help='Demonstrate the old position-zero failure')
    args = ap.parse_args()
    args.out.mkdir(parents=True, exist_ok=False)
    sources = args.source or [ROOT/'src/program/generate.cpp', ROOT/'sycl/src/program/generate.cpp']
    for i, source in enumerate(sources):
        check(source, args.cxx, args.out, i, args.expect_zero_bug)


if __name__ == '__main__':
    main()
