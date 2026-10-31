import pathlib,sys,tempfile
F=pathlib.Path(__file__).resolve().parent;sys.path.insert(0,str(F.parent.parent/'tools'));import bench_deepinfer as h
with tempfile.TemporaryDirectory() as td:
    h.ROOT=pathlib.Path(td)
    path=h.ROOT/'gpu-experiment.lock'
    calls=0
    def busy(**kwargs):
        global calls
        assert path.exists(), 'reserve before waiting for old work'
        calls+=1
        if calls==1:raise RuntimeError('older process still closing')
    h.busy=busy
    h.time.sleep=lambda _:None
    with h.gpu_lock():
        try:path.open('x');raise AssertionError('lock is not exclusive')
        except FileExistsError:pass
    assert calls==2 and not path.exists()
    try:
        with h.gpu_lock():raise ValueError('test failure')
    except ValueError:pass
    assert not path.exists(), 'release on failure'
print('PASS lock reservation, exclusion and failure cleanup')
