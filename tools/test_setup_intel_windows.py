"""Tests for setup.py's Intel Arc detection on Windows (mocked Win32_VideoController rows and display-class
registry values): the discrete Arc cards from their PCI device ids with the registry's 64-bit VRAM size, the
integrated GPU listed but not supported, automatic native routing on Windows and Linux, and explicit backend
choices preserved. No GPU, no downloads.

    python -m unittest tools.test_setup_intel_windows
"""
from __future__ import annotations

import contextlib
import io
import sys
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
import setup  # noqa: E402


ADAPTERS = [  # Win32_VideoController: name, PNPDeviceID, AdapterRAM (32-bit: at most 4 GB)
    {"name": "Intel(R) UHD Graphics 630", "pnp": r"PCI\VEN_8086&DEV_3E98&SUBSYS_08591028&REV_02\3&11583659&0&10",
     "ram": 1073741824},
    {"name": "Intel(R) Arc(TM) Pro B70 Graphics",
     "pnp": r"PCI\VEN_8086&DEV_E223&SUBSYS_17018086&REV_00\6&33BC15E2&0&00080008", "ram": 2147479552},
    {"name": "NVIDIA GeForce GTX 1080 Ti", "pnp": r"PCI\VEN_10DE&DEV_1B06&SUBSYS_36091462&REV_A1\4&23F45F8A&0&0008",
     "ram": 4293918720},
]
REGISTRY = [  # the display class's driver instances: 64-bit VRAM size
    {"DriverDesc": "Intel(R) UHD Graphics 630", "MatchingDeviceId": r"PCI\VEN_8086&DEV_3E98",
     "DriverVersion": "31.0.101.2115"},
    {"DriverDesc": "NVIDIA GeForce GTX 1080 Ti", "MatchingDeviceId": r"pci\ven_10de&dev_1b06",
     "HardwareInformation.qwMemorySize": 11811160064},
    {"DriverDesc": "Intel(R) Arc(TM) Pro B70 Graphics",
     "MatchingDeviceId": r"PCI\VEN_8086&DEV_E223&SUBSYS_17018086",
     "HardwareInformation.qwMemorySize": 34139537408, "DriverVersion": "32.0.101.8805"},
]


class WindowsDetection(unittest.TestCase):
    def test_discrete_arc_and_integrated(self):
        g = setup.intel_gpus_windows(ADAPTERS, REGISTRY)
        self.assertEqual([x["name"] for x in g],
                         ["Intel(R) UHD Graphics 630", "Intel(R) Arc(TM) Pro B70 Graphics"])  # Intel only, in order
        self.assertEqual([x["index"] for x in g], [0, 1])
        self.assertAlmostEqual(g[1]["vram_gb"], 34139537408 / 2 ** 30, places=1)  # the registry's 64-bit size
        self.assertEqual(g[1]["driver"], "32.0.101.8805")
        self.assertEqual(g[1]["pci_id"], 0xE223)
        self.assertIsNone(setup.intel_problem(g[1]))  # a known discrete Arc
        self.assertIn("Arc", setup.intel_problem(g[0]) + "discrete Arc cards only")

    def test_table_fallback_when_the_registry_has_no_size(self):
        g = setup.intel_gpus_windows(ADAPTERS[:2], [])
        b70 = [x for x in g if x["pci_id"] == 0xE223][0]
        self.assertAlmostEqual(b70["vram_gb"], 32.0)  # sycl/setup_intel.py INTEL_ARC

    def test_registry_alone(self):
        g = setup.intel_gpus_windows([], REGISTRY)
        self.assertEqual({x["pci_id"] for x in g}, {0x3E98, 0xE223})

    def test_device_ids_match_the_sycl_port(self):
        import importlib.util
        spec = importlib.util.spec_from_file_location("setup_intel", ROOT / "sycl" / "setup_intel.py")
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        for did, (name, gb) in mod.INTEL_ARC.items():
            self.assertEqual(setup._WIN_INTEL_DID[int(did, 16)], (name, gb), did)

    def test_automatic_orca_handoff_preserves_other_backend_choices(self):
        arc = dict(index=0, name="Arc A770", vram_gb=16, pci_id=0x56A0)
        igpu = dict(index=0, name="UHD 630", vram_gb=1, pci_id=0x3E98)
        cases = (
            (None, [], [], [arc], True, 1),
            (None, [dict(problem="unsupported")], [], [arc], True, 1),
            (None, [], [], [igpu], False, 1),
            (None, [dict(problem=None)], [], [arc], False, 0),
            (None, [], [{}], [arc], False, 0),
            ("cuda", [], [], [arc], False, 0),
            ("hip", [], [], [arc], False, 0),
            ("12", [], [], [arc], False, 0),
            ("13", [], [], [arc], False, 0),
            ("auto", [], [], [arc], True, 1),
        )
        for win in (False, True):
            for backend, nv, amd, intel, delegated, queries in cases:
                argv = ["setup.py", "--check", "--model", "IQ2_XS"]
                if backend in ("12", "13", "auto"):
                    argv += ["--cuda", backend]
                elif backend:
                    argv += ["--backend", backend]
                with self.subTest(win=win, backend=backend, nv=nv, amd=amd, intel=intel), \
                        mock.patch.object(sys, "argv", argv), \
                        mock.patch.object(setup, "WIN", win), \
                        mock.patch.object(setup, "OLD_GPUS", False), \
                        mock.patch.object(setup, "data_folder", return_value=(ROOT, [])), \
                        mock.patch.object(setup, "installed_configs", return_value=[]), \
                        mock.patch.object(setup, "start_gpus", return_value=None), \
                        mock.patch.object(setup, "gpus", return_value=nv), \
                        mock.patch.object(setup, "gpu_problem", side_effect=lambda g: g["problem"]), \
                        mock.patch.object(setup, "amd_gpus", return_value=amd), \
                        mock.patch.object(setup, "amd_problem", return_value=None), \
                        mock.patch.object(setup, "intel_gpus", return_value=intel) as discover, \
                        mock.patch.object(setup, "sycl_setup", return_value=17) as handoff, \
                        mock.patch.object(setup, "old_gpus_opt_in", side_effect=None if delegated else RuntimeError("kept backend"),
                                          return_value=False), contextlib.redirect_stdout(io.StringIO()):
                    if delegated:
                        self.assertEqual(setup.main(), 17)
                        handoff.assert_called_once_with(argv[1:], "orca")
                    else:
                        with self.assertRaisesRegex(RuntimeError, "kept backend"):
                            setup.main()
                        handoff.assert_not_called()
                    self.assertEqual(discover.call_count, queries)

    def test_explicit_cuda_failure_names_orca(self):
        out = io.StringIO()
        with mock.patch.object(sys, "argv", ["setup.py", "--backend", "cuda", "--check"]), \
                mock.patch.object(setup, "data_folder", return_value=(ROOT, [])), \
                mock.patch.object(setup, "installed_configs", return_value=[]), \
                mock.patch.object(setup, "start_gpus", return_value=None), \
                mock.patch.object(setup, "OLD_GPUS", False), \
                mock.patch.object(setup, "old_gpus_opt_in", return_value=False), \
                mock.patch.object(setup, "gpus", return_value=[]), \
                mock.patch.object(setup, "amd_gpus", return_value=[]), \
                mock.patch.object(setup, "intel_gpus", return_value=[dict(index=0, name="Arc A770", vram_gb=16,
                                                                         pci_id=0x56A0)]) as discover, \
                contextlib.redirect_stdout(out):
            with self.assertRaises(SystemExit):
                setup.main()
        discover.assert_called_once()
        self.assertIn("Arc A770", out.getvalue())
        self.assertIn("--backend orca", out.getvalue())

    def test_saved_intel_profiles_use_orca_without_overriding_explicit_backends(self):
        import json
        import tempfile
        cases = [(None, ["sycl"], True), ("auto", ["orca", "sycl"], True),
                 ("12", ["sycl"], False), ("13", ["orca"], False),
                 ("cuda", ["sycl"], False), ("hip", ["orca"], False),
                 (None, ["cuda", "sycl"], False)]
        with tempfile.TemporaryDirectory() as d:
            for win in (True, False):
                for backend, profiles, delegated in cases:
                    have = []
                    for i, value in enumerate(profiles):
                        path = Path(d) / f"strata-{i}.json"
                        path.write_text(json.dumps({"backend": value}), encoding="utf-8-sig")
                        have.append(path)
                    argv = ["setup.py"]
                    if backend in ("12", "13", "auto"):
                        argv += ["--cuda", backend]
                    elif backend:
                        argv += ["--backend", backend]
                    with self.subTest(win=win, backend=backend, profiles=profiles), \
                            mock.patch.object(sys, "argv", argv), mock.patch.object(setup, "WIN", win), \
                            mock.patch.object(setup, "data_folder", return_value=(ROOT, [])), \
                            mock.patch.object(setup, "installed_configs", return_value=have), \
                            mock.patch.object(setup, "start_gpus", side_effect=RuntimeError("kept backend")), \
                            mock.patch.object(setup, "sycl_setup", return_value=17) as handoff, \
                            contextlib.redirect_stdout(io.StringIO()):
                        if delegated:
                            self.assertEqual(setup.main(), 17)
                            handoff.assert_called_once_with(argv[1:], "orca")
                        else:
                            with self.assertRaisesRegex(RuntimeError, "kept backend"):
                                setup.main()
                            handoff.assert_not_called()

    def test_sycl_windows_hands_over_to_setup_intel(self):
        import contextlib
        out = io.StringIO()
        with mock.patch.object(setup, "WIN", True), \
                mock.patch.object(setup.subprocess, "call", return_value=0) as call, \
                contextlib.redirect_stdout(out):
            rc = setup.sycl_setup(["--backend", "sycl"])
        self.assertEqual(rc, 0)
        self.assertIn("EXPERIMENTAL", out.getvalue())
        cmd = call.call_args[0][0]
        self.assertTrue(cmd[1].endswith(str(Path("sycl") / "setup_intel.py")))
        self.assertEqual(cmd[2:], ["--runtime", "sycl"])

    def test_sycl_engine_windows_finds_the_exe_without_docker(self):
        import tempfile
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            (root / "build-sycl-aot").mkdir()
            (root / "build-sycl-aot" / "strata.exe").write_text("x")
            import importlib.util
            spec = importlib.util.spec_from_file_location("setup_intel_win", ROOT / "sycl" / "setup_intel.py")
            mod = importlib.util.module_from_spec(spec)
            with mock.patch.object(setup, "WIN", True):
                spec.loader.exec_module(mod)
                with mock.patch.object(mod, "ROOT", root), mock.patch.object(mod.S, "WIN", True):
                    exe, why = mod.sycl_engine()
                    self.assertIsNotNone(exe)
                    self.assertIsNone(why)


if __name__ == "__main__":
    unittest.main()
