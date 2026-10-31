"""No-GPU check of native/bridge benchmark selection and workload rejection."""
from contextlib import nullcontext, redirect_stdout
from pathlib import Path
from tempfile import TemporaryDirectory
from types import SimpleNamespace
from unittest.mock import patch
from itertools import product
import io
import json
import sys

import bench_orca_strata as runner


def main():
    with TemporaryDirectory() as temporary:
        root = Path(temporary)
        baseline, candidate, config = (root / name for name in ("baseline", "candidate", "config.json"))
        baseline.write_bytes(b"SYCL")
        candidate.write_bytes(b"Orca")
        config.write_text(json.dumps({"args": ["--spec", "4", "--suffix-draft", "1", "--mtp", "draft"]}))
        images = root / 'images'
        images.mkdir()
        (images / 'kernel.bin').write_bytes(b'kernel')
        launches = []
        mismatched = False

        class Engine:
            def __init__(self, executable, args, env, stem):
                self.active = env.get("STRATA_ORCA_L0") == "1"
                self.width_one = "--mtp" not in args
                self.info = "INFO fake"
                self.p = SimpleNamespace(returncode=0)
                launches.append((executable, env.copy(), args.copy()))
                witness = ("native Orca Level Zero" if executable == candidate else
                           "Orca L0 native graph replay active" if self.active else "SYCL baseline")
                stem.with_suffix(".stderr").write_text(witness)

            def gen(self, inputs, count, keys):
                if self.active and mismatched == 'candidate_mutation':
                    candidate.write_bytes(b'changed')
                if self.active and mismatched == 'images_mutation':
                    (images / 'kernel.bin').write_bytes(b'changed')
                if self.active and mismatched == 'config_mutation':
                    config.write_text('{}')
                token = 0 if mismatched == "baseline_zero" and not self.active else 99 if mismatched == "tokens" and self.active else 1
                return dict(tokens=[token] * count, accepted=0,
                            offered=(0 if self.width_one else 4) + int(mismatched == "offered" and self.active),
                            reused=0, finish="length",
                            generated=count, decode_ms=8 if self.active else 10,
                            prompt_ms=float('nan') if mismatched == 'nonfinite' else 0 if mismatched == 'prefill_zero' else 3,
                            ttft_ms=4)

            def close(self):
                pass

        for native, mtp_only in product((False, True), repeat=2):
            mutations = ('candidate_mutation', 'images_mutation', 'config_mutation') if native else ()
            for mismatch in (None, 'prefill_zero', "tokens", "offered", "baseline_zero", 'nonfinite', *mutations):
                candidate.write_bytes(b'Orca')
                (images / 'kernel.bin').write_bytes(b'kernel')
                config.write_text(json.dumps({"args": ["--spec", "4", "--suffix-draft", "1", "--mtp", "draft"]}))
                mismatched = mismatch
                expect_failure = mismatch not in (None, 'prefill_zero')
                launches.clear()
                output = root / f"{native}-{mtp_only}-{mismatch}"
                argv = ["bench", "--exe", str(baseline), "--config", str(config), "--out", str(output),
                        "--spec", "1,4", "--tokens", "4", "--retained", "1"]
                if native:
                    argv.extend(["--candidate-exe", str(candidate), "--images", str(images)])
                if mtp_only:
                    argv.append("--mtp-only")
                with patch.object(sys, "argv", argv), patch.object(runner.bench, "gpu_lock", nullcontext), \
                     patch.object(runner.bench, "model_args", lambda cfg, **kw: cfg["args"].copy()), \
                     patch.object(runner.bench, "prompts", lambda cfg: {"Short": [1], "Code2K": [2]}), \
                     patch.object(runner.bench, "environment", lambda cfg: {"SYCL_TEST": "1", "ORCA_L0_KERNEL_RESOURCES": "1",
                                                                           "STRATA_DECODE_TIMING": "1", "STRATA_VERIFY_PROFILE": "1"}), \
                     patch.object(runner.bench, "Engine", Engine), redirect_stdout(io.StringIO()):
                    try:
                        runner.main()
                        assert not expect_failure, "accepted invalid samples or artifacts"
                    except RuntimeError as error:
                        expected = "all-zero benchmark output" if mismatch == "baseline_zero" else "output/workload mismatch"
                        if mismatch in mutations:
                            expected = 'changed during the run'
                        elif mismatch == 'nonfinite':
                            expected = 'invalid prompt_ms samples'
                        assert expect_failure and expected in str(error), error
                result = json.loads((output / "results.json").read_text())
                assert result["complete"] == (not expect_failure)
                if not expect_failure:
                    assert len(result["comparisons"]) == 4
                    assert all(abs(row["throughput_change_pct"] - 25) < 1e-9 for row in result["comparisons"])
                    assert len(result['metrics']) == 16
                    for row in result['metrics']:
                        zero_prefill = mismatch == 'prefill_zero' and row['metric'] == 'prompt_ms'
                        assert row['comparison_valid'] != zero_prefill
                        assert ('throughput_change_pct' in row) != zero_prefill
                    assert result['exe_unchanged'] and result['config_unchanged']
                    if native:
                        assert result['candidate_unchanged'] and result['images_unchanged']
                for executable, env, args in launches:
                    active = env["STRATA_ORCA_L0"] == "1"
                    assert (executable == candidate) == (native and active)
                    assert ("SYCL_TEST" not in env) == (native and active)
                    assert "ORCA_L0_KERNEL_RESOURCES" not in env
                    assert 'STRATA_DECODE_TIMING' not in env and 'STRATA_VERIFY_PROFILE' not in env
                    assert ('STRATA_ORCA_IMAGES' in env) == (native and active)
                    if mtp_only:
                        assert args[args.index("--suffix-draft") + 1] == "0"
                        assert env["STRATA_MTP_COST_WINDOW"] == "0"
    print("PASS native/bridge selection, controlled MTP windows, full-request metrics, A/B/A ratios, invalid workload/timing and artifact mutation rejection")


if __name__ == "__main__":
    main()
