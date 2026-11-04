"""Tests for setup.py --backend sycl, the experimental Intel Arc engine (sycl/, PR #423): it warns and hands the
run to sycl/setup_intel.py without its --backend flag, on Linux and on Windows (native Windows build,
sycl/tools/build.bat + sycl/serve/strata-sycl.bat). The CUDA / HIP paths never see it. No GPU, no network.

    python -m unittest tools.test_setup_sycl
"""
from __future__ import annotations

import contextlib
import io
import re
import sys
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
import setup  # noqa: E402


def run(argv, win, backend="sycl"):
    out = io.StringIO()
    with mock.patch.object(setup, "WIN", win), mock.patch.object(setup.subprocess, "call", return_value=0) as call, \
            contextlib.redirect_stdout(out):
        try:
            rc = setup.sycl_setup(argv, backend)
        except SystemExit as e:
            rc = ("exit", e.code)
    return rc, out.getvalue(), call


class SyclBackend(unittest.TestCase):
    def test_windows_hands_over_to_setup_intel_without_the_backend_flag(self):
        rc, out, call = run(["--backend", "sycl"], win=True)
        self.assertEqual(rc, 0)
        self.assertIn("EXPERIMENTAL", out)
        cmd = call.call_args[0][0]
        self.assertTrue(cmd[1].endswith(str(Path("sycl") / "setup_intel.py")))
        self.assertEqual(cmd[2:], ["--runtime", "sycl"])

    def test_linux_hands_over_to_setup_intel_without_the_backend_flag(self):
        rc, out, call = run(["--model", "IQ2_XS", "--backend", "sycl", "--port", "8085"], win=False)
        self.assertEqual(rc, 0)
        self.assertIn("EXPERIMENTAL", out)
        self.assertIn("built from source", out)
        cmd = call.call_args[0][0]
        self.assertTrue(cmd[1].endswith(str(Path("sycl") / "setup_intel.py")))
        self.assertEqual(cmd[2:], ["--runtime", "sycl", "--model", "IQ2_XS", "--port", "8085"])

    def test_backend_equals_form_is_dropped_too(self):
        _, _, call = run(["--backend=sycl", "--check"], win=False)
        self.assertEqual(call.call_args[0][0][2:], ["--runtime", "sycl", "--check"])

    def test_the_parser_accepts_sycl(self):
        src = (ROOT / "setup.py").read_text(encoding="utf-8")
        self.assertRegex(src, r'"--backend", choices=\["cuda", "hip", "sycl", "orca"\]')

    def test_orca_parser_handoff_preserves_native_paths_on_both_platforms(self):
        for win in (True, False):
            argv = ["setup.py", "--backend=orca", "--orca-engine", "a path/strata",
                    "--orca-images", "image packs", "--orca-lib-dir", "native lib", "--check"]
            with self.subTest(win=win), mock.patch.object(sys, "argv", argv), \
                    mock.patch.object(setup, "WIN", win), \
                    mock.patch.object(setup.subprocess, "call", return_value=7) as call, \
                    contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(setup.main(), 7)
            self.assertEqual(call.call_args[0][0][2:], ["--runtime", "orca", *argv[2:]])

    def test_native_options_require_the_orca_backend_before_any_install(self):
        for backend in ("cuda", "hip", "sycl"):
            with self.subTest(backend=backend), mock.patch.object(sys, "argv", ["setup.py", "--backend", backend,
                                            "--orca-engine", "strata"]), contextlib.redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit) as stopped:
                    setup.main()
                self.assertEqual(stopped.exception.code, 2)

    def test_setup_intel_finds_the_setup_functions_it_replaces(self):
        """sycl/setup_intel.py swaps these steps; setup.py must still have them (it stops with a message otherwise)."""
        src = (ROOT / "sycl" / "setup_intel.py").read_text(encoding="utf-8")
        names = re.search(r'for name in \(([^)]*)\):', src).group(1)
        for name in re.findall(r'"(\w+)"', names):
            self.assertTrue(callable(getattr(setup, name, None)), name)

    def test_setup_intel_replacements_take_every_argument_of_the_function_they_replace(self):
        """#870: setup.py grew write_run_script's fourth argument and the port's replacement still took three, so
        setup died with a TypeError after the download.  Every `S.<name> = <replacement>` in setup_intel.py must accept
        all the parameters of setup.<name> (or take *args / **kwargs)."""
        import ast
        import inspect
        tree = ast.parse((ROOT / "sycl" / "setup_intel.py").read_text(encoding="utf-8"))
        defs = {n.name: n for n in ast.walk(tree) if isinstance(n, ast.FunctionDef)}
        checked = 0
        for n in ast.walk(tree):
            if not (isinstance(n, ast.Assign) and len(n.targets) == 1 and isinstance(n.targets[0], ast.Attribute)
                    and isinstance(n.targets[0].value, ast.Name) and n.targets[0].value.id == "S"):
                continue
            name = n.targets[0].attr
            fn = n.value if isinstance(n.value, ast.Lambda) else defs.get(getattr(n.value, "id", ""))
            orig = getattr(setup, name, None)
            if fn is None or not callable(orig):
                continue
            a = fn.args
            if a.vararg is not None and a.kwarg is not None:
                checked += 1
                continue
            have = {x.arg for x in a.posonlyargs + a.args + a.kwonlyargs}
            for param in inspect.signature(orig).parameters.values():
                if param.kind in (param.VAR_POSITIONAL, param.VAR_KEYWORD):
                    continue
                self.assertIn(param.name, have, f"setup_intel's {name} lacks setup.{name}'s `{param.name}`")
            checked += 1
        self.assertGreaterEqual(checked, 8)

    def test_arc_pro_b60_is_known_under_both_pci_ids(self):
        src = (ROOT / "sycl" / "setup_intel.py").read_text(encoding="utf-8")
        self.assertIn('"e211": ("Arc Pro B60", 24.0)', src)
        self.assertIn('"e221": ("Arc Pro B60", 24.0)', src)


class CMakeOption(unittest.TestCase):
    def test_sycl_is_off_by_default_and_exclusive(self):
        src = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8-sig")
        self.assertIn('option(STRATA_ENABLE_SYCL "EXPERIMENTAL', src)
        self.assertRegex(src, r'option\(STRATA_ENABLE_SYCL "[^"]*" OFF\)')
        self.assertIn("add_subdirectory(sycl)", src)


class IntelIds(unittest.TestCase):
    """sycl/setup_intel.py names a card by its PCI device id. The Arc Pro B60s measured on 2x B60 (`lspci -nn` 8086:e211,
    bmg-g21) must not fall through to "Intel GPU e211 (xe)" with a guessed VRAM size."""

    @staticmethod
    def table():
        import importlib.util
        spec = importlib.util.spec_from_file_location("setup_intel", ROOT / "sycl" / "setup_intel.py")
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        return mod.INTEL_ARC

    def test_b60_e211_is_named_with_its_vram(self):
        self.assertEqual(self.table()["e211"], ("Arc Pro B60", 24.0))

    def test_the_ids_that_were_listed_before_are_kept(self):
        t = self.table()
        self.assertEqual(t["e223"][0], "Arc Pro B70")
        self.assertEqual(t["e221"][0], "Arc Pro B60")


class OrcaSetup(unittest.TestCase):
    @staticmethod
    def module():
        import importlib.util
        spec = importlib.util.spec_from_file_location("setup_intel_orca", ROOT / "sycl" / "setup_intel.py")
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        return mod

    def test_native_paths_and_runtime_without_sycl_wrapper_on_both_platforms(self):
        mod = self.module()
        cfg = dict(args=["--pack", "pack/full", "--native", "model.gguf", "--mtp", "mtp.bin",
                         "--control-vector-scaled", "control.gguf:0.5", "--max-context", "8192",
                         "--spec", "4", "--resident-experts"],
                   tokenizer="tokenizer", cwd=str(ROOT), vision={}, gpus=[0, 1],
                   sycl_root="old-container", env={"SYCL_DEVICE_FILTER": "gpu"}, lib_dirs=["hip-libraries"])
        import copy
        original = copy.deepcopy(cfg)
        for win in (True, False):
            with self.subTest(win=win), mock.patch.object(mod.S, "WIN", win), \
                    mock.patch.object(mod, "sycl_path", side_effect=AssertionError("native path used Docker")):
                out = mod.to_sycl(cfg, ROOT / "build-orca/strata", 96, {"sampling": {"temperature": 0}},
                                  native_images=ROOT / "build-orca/images", lib_dirs=[ROOT / "native-lib"])
            self.assertEqual(out["backend"], "orca")
            self.assertEqual(out["exe"], str((ROOT / "build-orca/strata").resolve()))
            self.assertEqual(mod.flag(out["args"], "--native"), str((ROOT / "model.gguf").resolve()))
            self.assertEqual(mod.flag(out["args"], "--control-vector-scaled"),
                             str((ROOT / "control.gguf").resolve()) + ":0.5")
            self.assertEqual(mod.flag(out["args"], "--spec"), "4")
            self.assertEqual(out["env"]["STRATA_VERIFY_NO_HOST"], "1")
            self.assertEqual(out["env"]["STRATA_ORCA_IMAGES"], str((ROOT / "build-orca/images").resolve()))
            self.assertNotIn("STRATA_DEVICE_FREE_MIB", out["env"])  # native device budget, no registry estimate
            self.assertEqual(out["lib_dirs"], [str((ROOT / "native-lib").resolve())])
            self.assertEqual(out["sampling"], {"temperature": 0})
            for key in ("vision", "gpus", "sycl_root"):
                self.assertNotIn(key, out)
            self.assertEqual(out["tokenizer"], cfg["tokenizer"])
            self.assertEqual(cfg, original)

    def test_default_native_engine_discovery_covers_both_cmake_entries(self):
        import tempfile
        mod = self.module()
        for win in (True, False):
            for nested in (True, False):
                with self.subTest(win=win, nested=nested), tempfile.TemporaryDirectory() as d:
                    root = Path(d)
                    binary = root / "build-strata-orca"
                    if nested:
                        binary /= "orca"
                    (binary / "images/kernel").mkdir(parents=True)
                    (binary / ("strata.exe" if win else "strata")).write_text("test")
                    (binary / "images/packs.txt").write_text("kernel\n")
                    (binary / "images/kernel/index.txt").write_text("test")
                    with mock.patch.object(mod, "ROOT", root), mock.patch.object(mod.S, "WIN", win), \
                            mock.patch.object(mod.S, "intel_gpus_windows", side_effect=RuntimeError("native selected")), \
                            mock.patch.object(mod, "intel_gpus", side_effect=RuntimeError("native selected")), \
                            mock.patch.object(mod, "sycl_engine", side_effect=AssertionError("used SYCL")):
                        with self.assertRaisesRegex(RuntimeError, "native selected"):
                            mod.install([])

    def test_linux_container_paths_and_repeated_control_vectors_migrate(self):
        import tempfile
        mod = self.module()
        with tempfile.TemporaryDirectory() as d, mock.patch.object(mod.S, "WIN", False):
            root = Path(d)
            cfg = dict(backend="sycl", sycl_root=d, args=["--native", "/work/model.gguf",
                       "--expert-profile-save", "/work/profile.bin", "--control-vector-scaled",
                       "/work/a.gguf:0.5,/work/b.gguf:-1", "--control-vector-scaled", "/work/c.gguf:2",
                       "--max-context", "65536", "--prefill", "512", "--vram-reserve-mib", "3072",
                       "--kv-resident", "4096", "--stream-experts"])
            out = mod.to_sycl(cfg, root / "strata", 96, {}, native_images=root / "images")
            self.assertEqual(out["args"][-9:], cfg["args"][-9:])  # keep hand-tuned options
            self.assertEqual(mod.flag(out["args"], "--native"), str((root / "model.gguf").resolve()))
            self.assertEqual(mod.flag(out["args"], "--expert-profile-save"), str((root / "profile.bin").resolve()))
            self.assertEqual(mod.flag(out["args"], "--control-vector-scaled"),
                             str((root / "a.gguf").resolve()) + ":0.5," + str((root / "b.gguf").resolve()) + ":-1")
            self.assertIn(str((root / "c.gguf").resolve()) + ":2", out["args"])
            cfg["args"] = ["--native", "/work/../outside/model.gguf"]
            with self.assertRaisesRegex(ValueError, "cannot migrate"):
                mod.to_sycl(cfg, root / "strata", 96, {}, native_images=root / "images")

    def test_existing_profile_activation_backup_and_idempotence(self):
        import json
        import tempfile
        mod = self.module()
        for win in (True, False):
            for runtime in ("orca", "sycl"):
                with self.subTest(win=win, runtime=runtime), tempfile.TemporaryDirectory() as d:
                    root = Path(d)
                    exe = root / ("custom-strata.exe" if win else "custom-strata")
                    exe.write_text("test")
                    images = root / "custom-images"
                    (images / "kernel").mkdir(parents=True)
                    (images / "packs.txt").write_text("kernel\n")
                    (images / "kernel/index.txt").write_text("test")
                    previous = "sycl" if runtime == "orca" else "orca"
                    old = dict(backend=previous, exe=str(exe), args=["--max-context", "65536", "--kv", "q4_0",
                               "--prefill", "512", "--vram-reserve-mib", "3072", "--stream-experts"],
                               port=8087, sampling={"temperature": 0.3}, lib_dirs=[d],
                               env={"STRATA_ORCA_IMAGES": str(images), "USER_TUNING": "kept", "SYCL_DEVICE_FILTER": "gpu",
                                    "UR_L0_USE_IMMEDIATE_COMMANDLISTS": "1", "STRATA_SYCL_BIN": "old",
                                    "STRATA_DEVICE_FREE_MIB": "99999", "STRATA_VERIFY_EAGER": "1"})
                    cfg_path = root / "strata-test.json"
                    cfg_path.write_text(json.dumps(old), encoding="utf-8-sig")
                    saved_native = root / "strata-native.json"
                    saved_native.write_text(json.dumps(dict(old, backend="orca")), encoding="utf-8")
                    script = root / ("run-test.bat" if win else "run-test.sh")
                    overridden = False

                    def write(model, path, port, open_browser=True):
                        self.assertEqual((model, path, port), ("test", cfg_path, 8181 if overridden else 8087))
                        self.assertEqual(open_browser, not overridden)
                        script.write_text(str(mod.ROOT / "serve/server.py"))
                        return script

                    def launch(*args):
                        cfg = json.loads(cfg_path.read_text(encoding="utf-8-sig"))
                        self.assertEqual(cfg["backend"], runtime)
                        expected = old["args"] if not overridden else [*old["args"][:-3], "--stream-experts",
                                                                      "--vram-reserve-mib", "4096"]
                        self.assertEqual(cfg["args"], expected)
                        self.assertEqual(cfg["sampling"], old["sampling"])
                        if overridden:
                            self.assertEqual(cfg["host"], "127.0.0.1")
                            self.assertEqual(cfg["api_key"], "test-only")
                            self.assertIs(cfg["open_browser"], False)
                        if runtime == "orca":
                            self.assertEqual(cfg["exe"], str(exe.resolve()))
                            self.assertEqual(cfg["lib_dirs"], [str(root.resolve())])
                            self.assertEqual(cfg["env"]["USER_TUNING"], "kept")
                            for key in ("SYCL_DEVICE_FILTER", "UR_L0_USE_IMMEDIATE_COMMANDLISTS", "STRATA_SYCL_BIN",
                                        "STRATA_DEVICE_FREE_MIB", "STRATA_VERIFY_EAGER"):
                                self.assertNotIn(key, cfg["env"])
                        self.assertEqual(script.read_text(), str(mod.SERVER))
                        return 0

                    def main():
                        nonlocal overridden
                        for _ in range(2):
                            with self.assertRaises(SystemExit) as stopped:
                                mod.S.start(cfg_path)
                            self.assertEqual(stopped.exception.code, 0)
                        overridden = True
                        with self.assertRaises(SystemExit) as stopped:
                            mod.S.start(cfg_path, 8181, keep=dict(host="127.0.0.1", api_key="test-only",
                                                               open_browser=False, vram_reserve_mib=4096))
                        self.assertEqual(stopped.exception.code, 0)
                        return 0

                    names = ("gpus", "amd_gpus", "amd_problem", "hip_vision", "build_engine_hip", "hipblaslt_table",
                             "ram_gb", "write_run_script", "start", "say", "main", "get_prebuilt_hip", "hip_card")
                    originals = {name: getattr(setup, name) for name in names}
                    with contextlib.ExitStack() as stack:
                        for patch in (
                                mock.patch.multiple(setup, **originals), mock.patch.object(setup, "WIN", win),
                                mock.patch.object(setup, "installed_configs", return_value=[cfg_path, saved_native]),
                                mock.patch.object(setup, "intel_gpus_windows", return_value=[{"vram_gb": 16}]),
                                mock.patch.object(mod, "intel_gpus", return_value=[{"vram_gb": 16}]),
                                mock.patch.object(setup, "intel_problem", return_value=None),
                                mock.patch.object(setup, "ram_gb", return_value=96),
                                mock.patch.object(setup, "write_run_script", side_effect=write),
                                mock.patch.object(setup, "main", side_effect=main),
                                mock.patch.object(mod, "ROOT", root), mock.patch.object(mod, "MOUNT", root),
                                mock.patch.object(mod, "sycl_version", return_value="0.1.40"),
                                mock.patch.object(mod, "sycl_engine", return_value=(exe, None)),
                                mock.patch.object(mod.subprocess, "call", side_effect=launch),
                                mock.patch.object(mod.os, "execv", side_effect=lambda *a: sys.exit(launch())),
                                mock.patch.object(sys, "argv", ["setup_intel.py"]),
                                contextlib.redirect_stdout(io.StringIO())):
                            stack.enter_context(patch)
                        save = stack.enter_context(mock.patch.object(setup, "write_config", wraps=setup.write_config))
                        with self.assertRaises(SystemExit) as stopped:
                            # No canonical build: native runtime must rediscover the saved custom executable.
                            mod.install([] if runtime == "orca" else ["--runtime", "sycl"])
                        self.assertEqual(stopped.exception.code, 0)
                    backup = Path(str(cfg_path) + f".{previous}-backup")
                    self.assertEqual(json.loads(backup.read_text(encoding="utf-8")), old)
                    self.assertEqual(save.call_count, 3)  # backup, conversion, explicit settings; unchanged starts do not rewrite

    def test_explicit_sycl_runtime_and_conflicting_native_options(self):
        mod = self.module()
        with mock.patch.object(mod.S, "WIN", True), \
                mock.patch.object(mod.S, "intel_gpus_windows", return_value=[{"vram_gb": 16}]), \
                mock.patch.object(mod, "sycl_engine", side_effect=RuntimeError("SYCL selected")):
            with self.assertRaisesRegex(RuntimeError, "SYCL selected"):
                mod.install(["--runtime", "sycl"])
        with contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit) as stopped:
                mod.install(["--runtime", "sycl", "--orca-engine", "strata"])
            self.assertEqual(stopped.exception.code, 2)

    def test_explicit_engine_reuses_installer_and_run_script(self):
        import json
        import tempfile
        mod = self.module()
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            exe = root / "strata.exe"
            exe.write_text("test")
            images = root / "images"
            (images / "kernel").mkdir(parents=True)
            (images / "packs.txt").write_text("kernel\n")
            (images / "kernel/index.txt").write_text("test")
            cfg_path = root / "strata-test.json"
            cfg_path.write_text(json.dumps(dict(args=["--max-context", "8192"], tokenizer="tokenizer")))
            script = root / "run-test.bat"

            def write(model, cfg_path, port, open_browser=True):
                script.write_text(str(mod.ROOT / "serve/server.py"))
                return script

            def main():
                self.assertNotIn("--orca-engine", sys.argv)
                self.assertIn("--model", sys.argv)
                mod.S.write_run_script("test", cfg_path, 8080, False)
                return 0

            names = ("gpus", "amd_gpus", "amd_problem", "hip_vision", "build_engine_hip", "hipblaslt_table",
                     "ram_gb", "write_run_script", "start", "say", "main", "get_prebuilt_hip", "hip_card")
            originals = {name: getattr(setup, name) for name in names}
            with mock.patch.multiple(setup, **originals), mock.patch.object(setup, "WIN", True), \
                    mock.patch.object(setup, "intel_gpus_windows", return_value=[{"vram_gb": 16}]), \
                    mock.patch.object(setup, "intel_problem", return_value=None), \
                    mock.patch.object(setup, "ram_gb", return_value=96), \
                    mock.patch.object(setup, "write_run_script", side_effect=write), \
                    mock.patch.object(setup, "main", side_effect=main), \
                    mock.patch.object(mod, "ROOT", root), \
                    mock.patch.object(mod, "sycl_version", return_value="0.1.39"), \
                    mock.patch.object(mod, "sycl_engine", side_effect=AssertionError("used SYCL engine")), \
                    mock.patch.object(sys, "argv", ["setup_intel.py"]):
                with self.assertRaises(SystemExit) as stopped:
                    mod.install(["--orca-engine", str(exe), "--orca-lib-dir", d, "--model", "IQ2_XS"])
                self.assertEqual(stopped.exception.code, 0)
            cfg = json.loads(cfg_path.read_text())
            self.assertEqual(cfg["backend"], "orca")
            self.assertEqual(cfg["env"]["STRATA_ORCA_IMAGES"], str(images.resolve()))
            self.assertEqual(script.read_text(), str(mod.SERVER))

    def test_invalid_native_inputs_fail_before_detection_or_config_writes(self):
        mod = self.module()
        import tempfile
        with tempfile.TemporaryDirectory() as d:
            exe = Path(d) / "strata"
            exe.write_text("test")
            for argv in (["--orca-images", d], ["--orca-engine", str(exe)],
                         ["--orca-engine", str(exe) + ".missing"]):
                with self.subTest(argv=argv), contextlib.redirect_stderr(io.StringIO()), \
                        mock.patch.object(mod, "sycl_engine", side_effect=AssertionError("detected GPU")):
                    with self.assertRaises(SystemExit) as stopped:
                        mod.install(argv)
                    self.assertEqual(stopped.exception.code, 2)


if __name__ == "__main__":
    unittest.main()
