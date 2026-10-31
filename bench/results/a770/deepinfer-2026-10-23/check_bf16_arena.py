"""Run the pending arena regression using the same exclusive GPU lock."""
import runpy
import sys
import ctypes
from ctypes import wintypes
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

H = Path(__file__).resolve().parent
sys.path.insert(0, str(H.parent/'tools'))
import bench_deepinfer as b

sys.argv = [str(H/'test_gemm_arena.py'), '--build-root', str(H/'prefill-widen-bf16-final'),
            '--offset-gib','4','--out',str(H/'prefill-widen-bf16-arena')]
# ponytail: native directory notification avoids poll starvation; the existing exclusive lock still owns the GPU.
kernel = ctypes.WinDLL('kernel32',use_last_error=True)
first = kernel.FindFirstChangeNotificationW
first.argtypes = [wintypes.LPCWSTR,wintypes.BOOL,wintypes.DWORD]
first.restype = wintypes.HANDLE
wait = kernel.WaitForSingleObject
wait.argtypes = [wintypes.HANDLE,wintypes.DWORD]
wait.restype = wintypes.DWORD
again = kernel.FindNextChangeNotification
again.argtypes = [wintypes.HANDLE]
again.restype = wintypes.BOOL
close = kernel.FindCloseChangeNotification
close.argtypes = [wintypes.HANDLE]
close.restype = wintypes.BOOL
notification = first(str(H.parent),False,1)  # FILE_NOTIFY_CHANGE_FILE_NAME, root directory only
if notification == wintypes.HANDLE(-1).value:
    raise ctypes.WinError(ctypes.get_last_error())
def retry(delay):
    result = wait(notification,max(1,int(delay*1000)))
    if result == 0:
        if not again(notification):
            raise ctypes.WinError(ctypes.get_last_error())
    elif result != 258:  # WAIT_TIMEOUT
        raise ctypes.WinError(ctypes.get_last_error())
try:
    with patch.object(b,'time',SimpleNamespace(sleep=retry)):
        runpy.run_path(sys.argv[0],run_name='__main__')
finally:
    close(notification)
