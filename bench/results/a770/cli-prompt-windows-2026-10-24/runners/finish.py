"""Finish the isolated queue fix and final model checks under the existing shared lock."""
import importlib.util
from pathlib import Path
import sys

H = Path(__file__).resolve().parent
sys.path.insert(0, str(H.parent/'tools'))
import bench_deepinfer as b


def module(name):
    spec = importlib.util.spec_from_file_location(name, H/(name+'.py'))
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


with b.gpu_lock():
    runner = module('run')
    runner.verify()
    module('queue_fix').prepare(runner.builder)
    module('check').main(H/'checks-final-queue')
    runner.verify()
