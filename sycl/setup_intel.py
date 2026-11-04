"""setup.py for an Intel Arc: upstream's installer, steered onto the SYCL port from outside.

    python sycl/setup_intel.py [setup.py's options]
    python sycl/setup_intel.py --orca-engine <native-strata> [--orca-images <packs>] [--orca-lib-dir <dir>]

The SYCL port keeps out of the shared files (upstream merges stay clean), so this does not edit setup.py: it imports
it and replaces the few steps that are NVIDIA/AMD-specific, then runs setup's own main().  Everything else - the
model choice, the download, the pack, the tokenizer, the MTP draft layer, the context and KV questions - is
setup.py's, unchanged.  What is replaced:

  - the GPU check: the Intel Arc is offered through setup's AMD path (the one that compiles locally and has no
    images), named and sized from sysfs;
  - the engine step: the SYCL build (build-sycl-aot/strata, run in the strata-sycl-dev image by
    sycl/serve/strata-sycl.sh; docs/INTEL.md) instead of a CUDA/HIP build;
  - the RAM rule: the CUDA engine keeps every expert in RAM, the SYCL port streams them from the GGUF into VRAM
    (--stream-experts), so RAM only bounds the KV streaming;
  - the config: the container's paths, the SYCL flags, backend "sycl"; the run script starts
    sycl/serve/server_intel.py (serve/server.py plus the Intel Monitor readings and the model menu).

The explicit Orca option uses a separately built executable and host paths without the SYCL wrapper or Docker.
Orca is the Intel installer's default; --runtime sycl selects the comparison baseline.
See sycl/orca/README.md for native validation limits.
When setup.py changes the steps this relies on, this stops with a message rather than writing a wrong config.
"""
from __future__ import annotations

import argparse
import atexit
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path, PurePosixPath

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))
import setup as S  # noqa: E402

SYCL_WRAPPER = ROOT / "sycl" / "serve" / "strata-sycl.sh"
SYCL_WRAPPER_WIN = ROOT / "sycl" / "serve" / "strata-sycl.bat"
SYCL_IMAGE = "strata-sycl-dev"
SERVER = ROOT / "sycl" / "serve" / "server_intel.py"
MOUNT = Path(os.environ.get("STRATA_SYCL_ROOT") or ROOT.parent)   # what strata-sycl.sh mounts at /work

# Battlemage / Alchemist PCI device ids -> (name, VRAM GB). lspci's database lags new cards (an Arc Pro B70 reads
# "Intel Corporation Device [8086:e223]"), so the sysfs id is the reliable signal and the name table is ours.
INTEL_ARC = {"e223": ("Arc Pro B70", 32.0), "e221": ("Arc Pro B60", 24.0), "e211": ("Arc Pro B60", 24.0),
             "e20b": ("Arc B580", 12.0),
             "e20c": ("Arc B570", 10.0), "e212": ("Arc B50", 16.0),
             "56a0": ("Arc A770", 16.0), "56a1": ("Arc A750", 8.0), "56a2": ("Arc A580", 8.0),
             "56a5": ("Arc A380", 6.0), "56a6": ("Arc A310", 4.0), "5690": ("Arc A770M", 16.0)}


def intel_gpus():
    """Intel discrete GPUs from sysfs: vendor 0x8086 under the xe or i915 driver, named by PCI device id. VRAM comes
    from the id table, else from the size of the card's VRAM BAR."""
    found = []
    for card in sorted(Path("/sys/class/drm").glob("card[0-9]*")):
        if "-" in card.name:                            # connectors (card0-DP-1) share the card's device
            continue
        dev = card / "device"
        try:
            vendor = (dev / "vendor").read_text().strip().lower()
            devid = (dev / "device").read_text().strip().lower().replace("0x", "")
            driver = os.path.basename(os.path.realpath(dev / "driver")) if (dev / "driver").exists() else ""
        except OSError:
            continue
        if vendor != "0x8086" or driver not in ("xe", "i915"):
            continue
        name, vram = INTEL_ARC.get(devid, (None, 0.0))
        if name is None:
            if driver != "xe":                          # i915 without a known Arc id is an integrated GPU
                continue
            name = f"Intel GPU {devid} (xe)"
        if not vram:                                    # resource line 2 (BAR 2) is the VRAM aperture on xe cards
            try:
                start, end, _ = (dev / "resource").read_text().splitlines()[2].split()
                vram = (int(end, 16) - int(start, 16) + 1) / 2**30
            except (OSError, IndexError, ValueError):
                vram = 0.0
        found.append({"index": len(found), "name": f"Intel {name}", "vram_gb": vram, "arch": "xe", "driver": driver})
    return found


def sycl_engine():
    """The SYCL build: (binary, why-not)."""
    if S.WIN:
        exe = next((b for b in (ROOT / "build-sycl-aot" / "strata.exe", ROOT / "build-sycl" / "strata.exe")
                    if b.exists()), None)
        if exe is None:
            return None, "it is not built (sycl\\tools\\build.bat; docs/INTEL.md: install oneAPI, then build)"
        return exe, None
    exe = next((b for b in (ROOT / "build-sycl-aot" / "strata", ROOT / "build-sycl" / "strata") if b.exists()), None)
    if exe is None:
        return None, "it is not built (sycl/tools/build.sh; docs/INTEL.md)"
    if not shutil.which("docker"):
        return None, "docker is not installed (the engine runs in the oneAPI image)"
    if subprocess.run(["docker", "image", "inspect", SYCL_IMAGE], capture_output=True).returncode != 0:
        return None, f"the runtime image {SYCL_IMAGE} is missing (sycl/tools/Dockerfile)"
    return exe, None


def sycl_path(path) -> str:
    """A host path as the engine sees it. On Linux the engine runs in a container: the folder above the Strata
    checkout (or STRATA_SYCL_ROOT) is mounted at /work. On Windows the native exe runs directly: the host path."""
    if S.WIN:
        return str(Path(path).resolve())
    p, root = Path(path).resolve(), MOUNT.resolve()
    try:
        return "/work/" + p.relative_to(root).as_posix()
    except ValueError:
        S.fail(f"{p} is outside {root}, which the SYCL engine's container mounts",
               f"keep the models and the data folder under {root} (--models-dir / --data-dir)")


def sycl_version() -> str:
    m = re.search(r"project\(\s*\S+\s+VERSION\s+([\d.]+)", (ROOT / "sycl" / "CMakeLists.txt").read_text())
    return m.group(1) if m else "0"


def flag(args, name):
    return args[args.index(name) + 1] if name in args else None


def drop(args, name, value=False):
    while name in args:
        i = args.index(name)
        del args[i:i + 1 + int(value)]


def to_sycl(cfg: dict, exe: Path, ram: float, keep: dict, vram_gb: float = 0.0,
            *, native_images: Path | None = None, lib_dirs=()) -> dict:
    """setup's config (written for its HIP path) -> the SYCL port's: the container's paths, experts streamed from the
    GGUF into VRAM (every expert must fit, so the VRAM reserve is the smallest that leaves the KV and the prompt
    buffers room - docs/INTEL.md), KV streaming from 64K up when the RAM holds the KV (the B70 at 256K decodes at
    40+ tok/s with it, 4-9 without). native_images selects the direct Orca executable and host paths instead.
    vram_gb: the card's VRAM (Windows: the free-VRAM query needs Level Zero
    sysman, absent on the OpenCL backend, so setup reports registry VRAM minus 1 GiB for the desktop)."""
    args = list(cfg["args"])
    intel_profile = cfg.get("backend") in ("sycl", "orca")
    if not intel_profile:
        for f in ("--resident-experts", "--mmap-experts"):  # setup's low-RAM mode is the CUDA engine's
            drop(args, f)
        drop(args, "--kv-resident", True)                   # decided below on the real RAM
    for i, f in enumerate(args[:-1]):
        if f in ("--pack", "--native", "--ple-gguf", "--expert-profile", "--expert-profile-save", "--mtp",
                 "--control-vector", "--control-vector-scaled"):
            mapped_paths = []
            for v in args[i + 1].split(",") if f == "--control-vector-scaled" else [args[i + 1]]:
                path, _, scale = v.rpartition(":") if f == "--control-vector-scaled" else (v, "", "")
                if intel_profile and cfg.get("backend") == "sycl" and not S.WIN:
                    container = PurePosixPath(path)
                    if container.is_relative_to("/work"):
                        tail = container.relative_to("/work")
                        if ".." in tail.parts:
                            raise ValueError(f"cannot migrate a container path with '..': {path}")
                        path = str(Path(cfg.get("sycl_root") or MOUNT).joinpath(*tail.parts))
                mapped = str(Path(path).resolve()) if native_images is not None else sycl_path(path)
                mapped_paths.append(mapped + (f":{scale}" if scale else ""))
            args[i + 1] = ",".join(mapped_paths)
    ctx = int(flag(args, "--max-context") or 32768)
    kv = flag(args, "--kv") or "int8"
    kv_ram = ctx * (13 * (576 if kv == "q4_0" else 1056)) / 1e9
    if not intel_profile and kv != "k8v4" and ctx >= 65536 and ram >= kv_ram + 6:
        args += ["--kv-resident", "32768"]
        S.ok(f"KV streaming on: the context's KV cache lives in RAM ({kv_ram:.1f} GB), every expert stays in VRAM")
    if not intel_profile:
        drop(args, "--vram-reserve-mib", True)
        drop(args, "--stream-experts")
        args += ["--stream-experts", "--vram-reserve-mib", "1024" if ctx <= 32768 else "2048"]
        if ctx > 32768:                                 # long contexts: 4096-token chunks keep the prompt buffers small
            drop(args, "--prefill", True)
            args += ["--prefill", "4096"]
    out = {k: v for k, v in cfg.items() if k not in ("lib_dirs", "env", "vision", "gpus")}
    if native_images is not None:
        # ponytail: reuse setup's model/pack flow; the native executable needs no container or SYCL wrapper.
        out.pop("sycl_root", None)
        env = {k: v for k, v in (cfg.get("env") or {}).items()
               if intel_profile and not k.startswith(("SYCL_", "UR_", "ONEAPI_", "STRATA_SYCL_"))
               and k not in ("STRATA_DEVICE_FREE_MIB", "STRATA_DEVICE_TOTAL_MIB", "STRATA_VERIFY_EAGER")}
        env.update(STRATA_ORCA_L0="1", STRATA_ORCA_IMAGES=str(native_images.resolve()),
                   STRATA_VERIFY_NO_HOST="1", STRATA_VERIFY_DEVICE_PLAN="1", STRATA_VERIFY_ADAPT="1")
        out.update(backend="orca", exe=str(exe.resolve()), args=args,
                   lib_dirs=[str(Path(p).resolve()) for p in
                             (lib_dirs or (cfg.get("lib_dirs", []) if intel_profile else []))], env=env)
        out.update(keep)
        return out
    if S.WIN:
        out.update({"backend": "sycl", "exe": str(SYCL_WRAPPER_WIN), "args": args})
    else:
        out.update({"backend": "sycl", "exe": str(SYCL_WRAPPER), "args": args, "sycl_root": str(MOUNT)})
    env = {}
    aot = ROOT / "build-sycl-aot" / ("strata.exe" if S.WIN else "strata")
    if exe != aot:
        try:
            env["STRATA_SYCL_BIN"] = str(exe.relative_to(ROOT))
        except ValueError:
            env["STRATA_SYCL_BIN"] = str(exe)
    if S.WIN and vram_gb > 0:
        # The OpenCL backend has no free-VRAM query (dpct get_memory_info reads STRATA_DEVICE_FREE_MIB).
        env["STRATA_DEVICE_FREE_MIB"] = str(int((vram_gb - 1.0) * 1024))
        env["STRATA_DEVICE_TOTAL_MIB"] = str(int(vram_gb * 1024))
    if S.WIN and vram_gb <= 0:
        # Detection flaked on a rewrite (WMI hiccup): keep the previous config's values rather than
        # silently dropping them - without them the cache sizes to 0 slots and nothing runs.
        old_env = cfg.get("env") or {}
        for k in ("STRATA_DEVICE_FREE_MIB", "STRATA_DEVICE_TOTAL_MIB", "STRATA_SYCL_BIN"):
            if k in old_env and k not in env:
                env[k] = old_env[k]
    if S.WIN and vram_gb > 0:
        # The OpenCL backend has no free-VRAM query (dpct get_memory_info reads STRATA_DEVICE_FREE_MIB).
        env["STRATA_DEVICE_FREE_MIB"] = str(int((vram_gb - 1.0) * 1024))
        env["STRATA_DEVICE_TOTAL_MIB"] = str(int(vram_gb * 1024))
    if not S.WIN and MOUNT.resolve() != ROOT.parent.resolve():
        env["STRATA_SYCL_ROOT"] = str(MOUNT)
    if env:
        out["env"] = env
    out.update(keep)
    return out


def install(argv) -> None:
    ap = argparse.ArgumentParser(add_help=False, allow_abbrev=False)
    ap.add_argument("--runtime", choices=("orca", "sycl"), default="orca")
    ap.add_argument("--orca-engine", type=Path, help="Use an explicitly built native Orca executable")
    ap.add_argument("--orca-images", type=Path, help="Kernel packs (default: images beside --orca-engine)")
    ap.add_argument("--orca-lib-dir", type=Path, action="append", default=[], help="Native library search directory")
    native, argv = ap.parse_known_args(argv)
    if native.runtime == "sycl" and (native.orca_engine is not None or native.orca_images is not None or native.orca_lib_dir):
        ap.error("Orca options cannot be used with --runtime sycl")
    if native.runtime == "orca" and native.orca_engine is None:
        name = "strata.exe" if S.WIN else "strata"
        native.orca_engine = next((p for p in (ROOT / "build-strata-orca/orca" / name,
                                              ROOT / "build-strata-orca" / name) if p.is_file()), None)
        if native.orca_engine is None:
            for profile in S.installed_configs():
                cfg = json.loads(profile.read_text(encoding="utf-8-sig"))
                candidate = Path(cfg.get("exe") or "")
                if cfg.get("backend") == "orca" and candidate.is_file():
                    native.orca_engine = candidate
                    native.orca_images = native.orca_images or Path((cfg.get("env") or {}).get(
                        "STRATA_ORCA_IMAGES") or candidate.resolve().parent / "images")
                    native.orca_lib_dir = native.orca_lib_dir or [Path(p) for p in cfg.get("lib_dirs", [])]
                    break
        if native.orca_engine is None:
            ap.error("native Orca engine not found; build with -DSTRATA_ENABLE_ORCA=ON "
                     "or pass --orca-engine (sycl/orca/README.md)")
    if native.orca_engine is not None:
        if not native.orca_engine.is_file():
            ap.error(f"Orca executable does not exist: {native.orca_engine}")
        native.orca_images = native.orca_images or native.orca_engine.resolve().parent / "images"
        manifest = native.orca_images / "packs.txt"
        packs = manifest.read_text(encoding="utf-8").split() if manifest.is_file() else []
        if not packs or any(not (native.orca_images / pack / "index.txt").is_file() for pack in packs):
            ap.error(f"Orca images need packs.txt and every named pack's index.txt: {native.orca_images}")
        if any(not p.is_dir() for p in native.orca_lib_dir):
            ap.error("every --orca-lib-dir must be an existing directory")
    if S.WIN:
        try:
            intel = S.intel_gpus_windows()
        except (OSError, ValueError):
            intel = []
    else:
        intel = intel_gpus()
    if not intel:
        where = "the display adapters" if S.WIN else "an xe or i915 card in /sys/class/drm"
        S.fail(f"no Intel Arc found ({where})", "on an NVIDIA or AMD card, run "
               + ("START-HERE.bat" if S.WIN else "./setup.sh"))
    exe, why = (native.orca_engine, None) if native.orca_engine is not None else sycl_engine()
    if exe is None:
        S.fail(f"Strata's SYCL engine cannot be used: {why}", "docs/INTEL.md: build it, then run this again")
    for name in ("gpus", "amd_gpus", "amd_problem", "hip_vision", "build_engine_hip", "hipblaslt_table", "ram_gb",
                 "write_run_script", "start", "say", "main", "get_prebuilt_hip", "hip_card", "intel_problem"):
        if not callable(getattr(S, name, None)):
            S.fail(f"setup.py has no {name}() any more: sycl/setup_intel.py needs updating for this setup.py")
    if S.WIN and not callable(getattr(S, "intel_gpus_windows", None)):
        S.fail("setup.py has no intel_gpus_windows() any more: sycl/setup_intel.py needs updating for this setup.py")

    real_ram = S.ram_gb()
    keep = {}                                           # hand-set keys setup does not write: kept across a rerun
    for p in ROOT.glob("strata-*.json"):
        try:
            c = json.loads(p.read_text(encoding="utf-8-sig"))
            keep[p.name] = {k: c[k] for k in ("model_switcher", "sampling") if k in c}
        except (OSError, ValueError):
            pass
    stub = Path(tempfile.mkdtemp(prefix="strata-sycl-"))  # setup reads the engine's BUILD.json; the SYCL build has none
    atexit.register(shutil.rmtree, stub, True)
    (stub / "BUILD.json").write_text(json.dumps({"version": sycl_version(), "source": "local", "lib_dirs": []}))

    say = S.say
    fake_ram = max(real_ram, 1024.0)

    def say_intel(msg=""):
        """setup's words for its AMD path and its RAM rule, said for the Intel card."""
        msg = str(msg).replace("(AMD, experimental: docs/AMD_HIP.md)", "(Intel Arc: the SYCL port, docs/INTEL.md)")
        if native.orca_engine is not None:
            msg = msg.replace("the SYCL port, docs/INTEL.md", "Orca L0, sycl/orca/README.md")
        msg = msg.replace("Your AMD GPUs:", "Your Intel GPUs:").replace("just run ./setup.sh", "just run sycl/setup_intel.py")
        msg = msg.replace(f"RAM: {fake_ram:.0f} GB", f"RAM: {real_ram:.0f} GB (the experts are streamed into VRAM)")
        if re.match(r"\s+\S+\s+needs ~\d+ GB RAM:", msg):   # --check's CUDA verdicts: replaced by the Intel one
            m = msg.split()[0]
            d = S.MODELS.get(m, {})
            shard1 = d.get("download_gb", 0) - 28.8          # all but the per-layer lookup table (read from disk)
            room = intel[0]["vram_gb"] - 3 + max(0.0, real_ram - 10)   # VRAM, plus a pinned host mirror
            msg = (f"  {m:8s} ~{shard1:.0f} GB of weights: " +
                   ("fits in VRAM" if shard1 <= intel[0]["vram_gb"] - 1.5 else
                    "fits with part of its experts mirrored in RAM (slower)" if shard1 <= room else "does not fit"))
        say(msg)
    S.say = say_intel
    S.gpus = lambda *a, **k: []
    S.amd_gpus = lambda *a, **k: intel
    # On Windows the list holds integrated GPUs too (like amd_gpus_windows): filter by the Intel table.
    # On Linux intel_gpus() already drops the iGPU, so every entry is usable.
    S.amd_problem = (lambda g: S.intel_problem(g)) if S.WIN else (lambda g: None)
    S.hip_vision = lambda asked: "none"                 # images are not wired on the SYCL port yet
    S.build_engine_hip = lambda *a, **k: stub
    S.get_prebuilt_hip = lambda *a, **k: stub   # Windows: skip the ready-made HIP engine (its arch check
    S.hip_card = lambda eng, gpu, listed: {**gpu, "count": len(listed)}  # would refuse xe; the SYCL build is
    # already in place (sycl_engine above), and there is no HIP runtime to number against on Intel
    S.hipblaslt_table = lambda *a, **k: None
    S.ram_gb = lambda: fake_ram                         # the experts are in VRAM: setup's RAM rule does not apply

    write = S.write_run_script

    def write_run_script(model, cfg_path, port, open_browser=True):   # setup.write_run_script's signature (#870)
        old = json.loads(Path(cfg_path).read_text(encoding="utf-8-sig"))
        usable = [g for g in intel if S.intel_problem(g) is None] if S.WIN else intel
        vram = max([g["vram_gb"] for g in usable] or [0.0])
        cfg = to_sycl(old, exe, real_ram, keep.get(Path(cfg_path).name, {}), vram if S.WIN else 0.0,
                      native_images=native.orca_images, lib_dirs=native.orca_lib_dir)
        if old.get("backend") in ("sycl", "orca") and old.get("backend") != cfg["backend"]:
            S.write_config(Path(str(cfg_path) + f'.{old["backend"]}-backup'), old)
        if cfg != old:
            S.write_config(Path(cfg_path), cfg)
        script = write(model, cfg_path, port, open_browser)
        script.write_text(script.read_text().replace(str(ROOT / "serve" / "server.py"), str(SERVER)))
        return script
    S.write_run_script = write_run_script

    start = S.start

    def start_sycl(cfg_path, *a, **k):
        cfg = json.loads(Path(cfg_path).read_text(encoding="utf-8-sig"))
        if cfg.get("backend") not in ("sycl", "orca"):
            return start(cfg_path, *a, **k)
        updates = {key: value for key, value in (k.get("keep") or {}).items() if value is not None}
        reserve = updates.pop("vram_reserve_mib", None)
        changed = any(cfg.get(key) != value for key, value in updates.items())
        if reserve is not None and flag(cfg["args"], "--vram-reserve-mib") != str(reserve):
            drop(cfg["args"], "--vram-reserve-mib", True)
            cfg["args"] += ["--vram-reserve-mib", str(reserve)]
            changed = True
        cfg.update(updates)
        if changed:
            S.write_config(Path(cfg_path), cfg)
        stem = Path(cfg_path).stem[len('strata-'):]
        port = (a[0] if a else k.get("port")) or cfg.get("port") or 8080
        browser = (a[2] if len(a) > 2 else k.get("open_browser", True)) and cfg.get("open_browser") is not False
        write_run_script(stem, cfg_path, port, browser)
        script = ROOT / (f"run-{stem}.bat" if S.WIN else f"run-{stem}.sh")
        S.say(f"\nstarting {script.name} ...")
        if S.WIN:
            sys.exit(subprocess.call([str(script)]))
        os.execv("/bin/sh", ["/bin/sh", str(script)])
    S.start = start_sycl

    sys.argv = [str(ROOT / "setup.py"), *argv]
    if "--backend" not in argv:
        sys.argv += ["--backend", "hip"]
    if "--vision" not in argv:
        sys.argv += ["--vision", "none"]
    if "--low-ram" not in argv:
        sys.argv += ["--low-ram", "off"]
    sys.exit(S.main())


if __name__ == "__main__":
    try:
        install(sys.argv[1:])
    except KeyboardInterrupt:
        S.say("\nstopped.")
        sys.exit(1)
