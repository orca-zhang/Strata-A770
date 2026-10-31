"""Build native-loadable kernel images; no SYCL runtime registration or host linking."""
import argparse
from fnmatch import fnmatchcase
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess


def tool(compiler, name):
    suffix = compiler.suffix
    for folder in (compiler.parent / "compiler", compiler.parent):
        candidate = folder / (name + suffix)
        if candidate.is_file():
            return candidate
    found = shutil.which(name)
    if found:
        return Path(found)
    raise FileNotFoundError(name)


def image_flags(properties, module_flags="", large_grf=False):
    lines = properties.splitlines()
    esimd = "isEsimdImage=1|1" in lines
    flags = ["-vc-codegen"] if esimd else []
    grf = [line for line in lines if line.startswith("sycl-grf-size=")]
    if len(grf) > 1:
        raise ValueError("duplicate compiler GRF size property")
    if grf:
        # Match SYCL's program manager: this image property is not an SPIR-V
        # execution mode. The native loader must forward it to the GPU compiler.
        options = {"sycl-grf-size=1|0": "-ze-intel-enable-auto-large-GRF-mode",
                   "sycl-grf-size=1|128": "",
                   "sycl-grf-size=1|256": "-doubleGRF" if esimd else "-ze-opt-large-register-file",
                   "sycl-grf-size=1|512": "-ze-opt-register-file-size=512"}
        if grf[0] not in options:
            raise ValueError("unsupported compiler GRF size property")
        flags.append(options[grf[0]])
    result = " ".join(filter(None, (*flags, module_flags)))
    if large_grf:
        if esimd or grf:
            raise ValueError("large GRF override requires an ordinary kernel without a GRF property")
        result = " ".join(filter(None, (result, "-ze-opt-large-register-file")))
    return result


def build(compiler, source, output, flags, module_flags="", large_grf_kernel=""):
    if any(flag != "-cl-fp32-correctly-rounded-divide-sqrt" for flag in module_flags.split()):
        raise ValueError("unsupported native module build flags")
    output.mkdir(parents=True, exist_ok=True)
    abi, bitcode, table = (output / name for name in ("abi.hpp", "input.bc", "images.table"))
    commands = []

    def run(args):
        args = [str(arg) for arg in args]
        commands.append(args)
        subprocess.run(args, check=True)

    # Match this compiler's normal SPIR-V feature set. Enabling every extension
    # makes llvm-spirv emit untyped pointers unsupported by the A770 driver.
    probe = [str(arg) for arg in [compiler, *flags, "-fsycl", "-fsycl-device-only",
             "-fsycl-device-obj=spirv", "-###", "-c", source]]
    commands.append(probe)
    phases = subprocess.run(probe, capture_output=True, text=True, check=True)
    extensions = re.search(r'(-spirv-ext=[^"\s]+)', phases.stdout + phases.stderr)
    if not extensions:
        raise ValueError("cannot determine compiler SPIR-V extension set")
    run([compiler, *flags, "-fsycl", "-fsycl-device-only", "-fsycl-device-obj=llvmir",
         "-fno-sycl-dead-args-optimization", "-Xclang", f"-fsycl-int-header={abi}",
         "-c", source, "-o", bitcode])
    run([tool(compiler, "sycl-post-link"), "-O2", "-split=kernel", "-split-esimd",
         "-lower-esimd", "-spec-const=native", "-symbols", "-properties", bitcode, "-o", table])
    lines = table.read_text(encoding="utf-8").splitlines()
    if not lines or lines[0] != "[Code|Properties|Symbols]":
        raise ValueError("unexpected sycl-post-link image table")
    entries = []
    grf_matches = 0
    for line in lines[1:]:
        fields = line.split("|")
        if len(fields) != 3:
            raise ValueError("invalid compiler image table row")
        code, properties, symbols = (Path(field) for field in fields)
        # Some compiler versions emit paths relative to the table directory.
        code, properties, symbols = (
            p if p.is_absolute() else output / p for p in (code, properties, symbols))
        image = code.with_suffix(".spv")
        run([tool(compiler, "llvm-spirv"), "-spirv-max-version=1.5",
             "-spirv-allow-unknown-intrinsics=llvm.genx.", extensions.group(1), code, "-o", image])
        props = properties.read_text(encoding="utf-8")
        for name in symbols.read_text(encoding="utf-8").splitlines():
            if not name or any(char.isspace() for char in name):
                raise ValueError("invalid exported kernel name")
            large_grf = bool(large_grf_kernel) and fnmatchcase(name, large_grf_kernel)
            grf_matches += large_grf
            build_flags = image_flags(props, module_flags, large_grf)
            entries.append(f"{image.name}\t{name}\t{build_flags}\n")
    if not entries:
        raise ValueError("compiler produced no kernels")
    if large_grf_kernel and not grf_matches:
        raise ValueError("large GRF kernel pattern matched no exported kernel")
    (output / "index.txt").write_text("".join(entries), encoding="utf-8")
    # Fingerprint the actual build inputs and outputs for later model comparisons.
    paths = [source, Path(__file__), *sorted(output.glob("*.spv")), abi, output / "index.txt"]
    (output / "build.json").write_text(json.dumps({
        "source": str(source), "compiler": str(compiler), "commands": commands, "module_flags": module_flags,
        "large_grf_kernel": large_grf_kernel, "large_grf_matches": grf_matches,
        "sha256": {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in paths},
    }, indent=2), encoding="utf-8")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", required=True, type=Path)
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--module-flags", default="")
    parser.add_argument("--large-grf-kernel", default="", help="Kernel-name glob for the native large-register flag; must match an ordinary kernel without a GRF property")
    parser.add_argument("flags", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    build(args.compiler.resolve(), args.source.resolve(), args.out.resolve(),
          args.flags[1:] if args.flags[:1] == ["--"] else args.flags, args.module_flags, args.large_grf_kernel)
