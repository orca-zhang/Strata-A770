"""A transient Windows sharing violation must retry without deleting another owner's lock."""
import io
import sys
from pathlib import Path
from unittest.mock import MagicMock, patch

sys.path.insert(0,str(Path(__file__).resolve().parent.parent/'tools'))
import bench_deepinfer as b

root = MagicMock()
lock = root.__truediv__.return_value
lock.open.side_effect = [FileExistsError(),io.StringIO()]
lock.read_text.side_effect = PermissionError()
lock.exists.return_value = False
with patch.object(b,'ROOT',root), patch.object(b,'busy'), patch.object(b.time,'sleep') as sleep:
    with b.gpu_lock():
        pass
    sleep.assert_called_once_with(.05)
assert lock.open.call_count == 2
lock.unlink.assert_not_called()
print('PASS transient lock read retry; no foreign lock deletion')
