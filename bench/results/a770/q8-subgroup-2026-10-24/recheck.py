"""Rerun the retained OFF/ON binaries under the shared lock; keep new logs separate."""
import argparse
import json
from pathlib import Path
import subprocess
import sys

H = Path(__file__).resolve().parent
sys.path.insert(0,str(H))
import run as e

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--out',type=Path,required=True)
    ap.add_argument('--timing',action='store_true')
    args=ap.parse_args()
    args.out.mkdir(parents=True,exist_ok=False)
    manifest=json.loads((H/'integrated-build.json').read_text())
    for name,digest in manifest['artifacts'].items():
        assert e.sha(H/name)==digest,name
    env=e.b.environment(json.loads((H.parent/'strata-iq2_xs.json').read_text(encoding='utf-8-sig')))
    env['PATH']=e.build.environment()['PATH']
    with e.b.gpu_lock():
        for value in (0,1):
            for test in ('steps','q8','final'):
                with (args.out/f'{value}-{test}.log').open('x',encoding='utf-8') as log:
                    subprocess.run([str(H/f'integrated-{value}'/(test+'.exe'))],env=env,cwd=H,
                                   stdout=log,stderr=subprocess.STDOUT,check=True,timeout=120)
        if args.timing:
            for phase in ('screen','confirm'):
                for mode in (0,1):
                    for i,value in enumerate((0,1,0,1,0)):
                        e.b.busy()
                        with (args.out/f'{phase}-{mode}-{i}-{value}.log').open('x',encoding='utf-8') as log:
                            subprocess.run([str(H/f'integrated-{value}/steps.exe'),'--bench',str(mode)],
                                env=env,cwd=H,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=120)
    print('PASS retained OFF/ON checks; logs:',args.out)

if __name__=='__main__':
    main()
