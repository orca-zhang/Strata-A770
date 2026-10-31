import os, subprocess, sys, json, hashlib
from pathlib import Path
H=Path(__file__).resolve().parent
R=H.parent
ICX=Path(r"C:/Program Files (x86)/Intel/oneAPI/compiler/2025.3")
def environment():
    setup=r'call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -no_logo -arch=x64 >nul && set'
    p=subprocess.run(['cmd','/d','/c',str(H/'environment.cmd')],capture_output=True,text=True,check=True)
    env=os.environ.copy()
    for line in p.stdout.splitlines():
        if '=' in line and not line.startswith('='):
            k,v=line.split('=',1);env[k.upper()]=v
    env['LIB']=str(ICX/'lib')+';'+env.get('LIB','')
    env['PATH']=str(ICX/'bin')+';'+str(ICX/'bin/compiler')+';'+str(ICX/'redist/intel64_win/compiler')+';'+env['PATH']
    return env
def run(args,label):
    print(label,flush=True)
    with (H/(label+'.log')).open('w',encoding='utf8') as log:
        p=subprocess.run([str(a) for a in args],env=environment(),stdout=log,stderr=subprocess.STDOUT,cwd=R)
    if p.returncode:
        print((H/(label+'.log')).read_text(errors='replace')[-10000:],flush=True)
    p.check_returncode()
if __name__=='__main__':
    side=sys.argv[1];build=H/side
    source=H/'source/sycl' if side=='baseline' else R/'sycl'
    run(['cmake','-S',source,'-B',build,'-G','Ninja','-DCMAKE_BUILD_TYPE=Release',
         '-DCMAKE_C_COMPILER='+str(ICX/'bin/icx.exe'),'-DCMAKE_CXX_COMPILER='+str(ICX/'bin/icx.exe'),
         '-DFETCHCONTENT_SOURCE_DIR_STRATA_LLAMACPP='+str(R/'build-sycl/_deps/strata_llamacpp-src'),
         '-DSTRATA_SYCL_PARITY=ON','-DSTRATA_LEVEL_ZERO_ROOT=C:/Program Files/LevelZeroSDK/1.33.1'],side+'-configure')
    run(['cmake','--build',build,'--target',*(sys.argv[2:] or ['strata','qsa_parity','kv_q8_parity','kv_q4_parity']),'-j','3'],side+'-build')
    print('PASS',side,flush=True)
