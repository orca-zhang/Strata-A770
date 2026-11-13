"""CPU-only regression for transient Windows lock cleanup failures."""
import os
from pathlib import Path
import tempfile
from unittest.mock import patch

import bench_deepinfer as b


def check():
    unlink = Path.unlink
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        lock = root / "gpu-experiment.lock"
        for replacement in (None, "foreign owner"):
            attempts = []

            def flaky_unlink(path, *args, **kwargs):
                assert path == lock
                attempts.append(path)
                if len(attempts) == 1:
                    if replacement:
                        path.write_text(replacement)
                    raise PermissionError("simulated sharing violation")
                return unlink(path, *args, **kwargs)

            with patch.object(b, "ROOT", root), patch.object(b, "busy"), \
                    patch.object(Path, "unlink", flaky_unlink), patch.object(b.time, "sleep"):
                with b.gpu_lock():
                    assert lock.read_text() == f"{os.getpid()} deepinfer"
            if replacement:
                assert lock.read_text() == replacement and len(attempts) == 1
                lock.unlink()
            else:
                assert not lock.exists() and len(attempts) == 2
    print("PASS lock cleanup retries and preserves foreign ownership")


if __name__ == "__main__":
    check()
