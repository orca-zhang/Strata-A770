from pathlib import Path
import hashlib
import importlib.util
import json
import math
import re
import statistics
import subprocess
import sys

H = Path(__file__).resolve().parent
R = H.parent
sys.path.insert(0,str(R/'tools'))
import bench_deepinfer as b
spec=importlib.util.spec_from_file_location('builder',R/'build-sycl-deepinfer-20261023/build.py')
builder=importlib.util.module_from_spec(spec);spec.loader.exec_module(builder)
manifest=json.loads((H/'preparation.json').read_text())
d=dict(complete=False,preparation=manifest,screen=[],confirmation=[],final=[],adopt=False)
compile_env=builder.environment()
runtime=b.environment({})
runtime.update(ONEAPI_DEVICE_SELECTOR='level_zero:0',SYCL_PROGRAM_COMPILE_OPTIONS='-cl-fp32-correctly-rounded-divide-sqrt')
runtime['PATH']=';'.join([str(builder.ICX/'bin'),str(builder.ICX/'bin/compiler'),
    'C:/Program Files (x86)/Intel/oneAPI/mkl/2025.3/bin',runtime['PATH']])
cxx=str(builder.ICX/'bin/icx.exe')
flags=['/nologo','/EHsc','/O2','/MD','-Qstd:c++20','-fsycl','-fsycl-default-sub-group-size=32',
    '-fsycl-device-code-split=per_kernel','/fp:precise','-DSTRATA_NATIVE_EXPERTS=1',
    '-DSTRATA_SYCL_SERIAL_Q8_1=0','-D_USE_STD_VECTOR_ALGORITHMS=0','-Wno-deprecated-declarations','-Wno-unused-parameter']
for path in [H/'headers/sycl/include',H/'headers/include',R/'src/core',R/'src/kernels',
             R/'third_party/ggml',R/'build-sycl/_deps/strata_llamacpp-src/ggml/include']:
    flags.append('-I'+str(path))
base=R/'build-sycl-a770-refresh-20261019/cmake'
libs=[str(base/p) for p in ('strata_kernels.lib','strata_core.lib','strata_kernels_cpu.lib','ggml/src/ggml-cpu.lib','ggml/src/ggml-base.lib')]
d['link_inputs']={str(Path(p).relative_to(R)):b.sha(p) for p in libs}
d['environment']={k:v for k,v in runtime.items() if k.startswith(('STRATA_','SYCL_','ONEAPI_','UR_'))}

def verify():
    for p,sha in manifest['files_sha256'].items():
        assert b.sha(R/p)==sha, p
    for p,sha in d['link_inputs'].items():
        assert b.sha(R/p)==sha,p

def call(args,label,env=runtime,cwd=R):
    verify();b.busy();print(label,flush=True)
    with (H/(label+'.stdout')).open('x') as out,(H/(label+'.stderr')).open('x') as err:
        p=subprocess.run(args,cwd=cwd,env=env,stdout=out,stderr=err,timeout=1800)
    if p.returncode:
        print((H/(label+'.stdout')).read_text(errors='replace')[-5000:],flush=True)
        print((H/(label+'.stderr')).read_text(errors='replace')[-5000:],flush=True)
    p.check_returncode();b.busy();verify()
    return (H/(label+'.stdout')).read_text()

def compile_obj(name,extra=()):
    call([cxx,*flags,*extra,'/c',str(H/(name+'.cpp')),'/Fo'+str(H/(name+'.obj'))],'compile-'+name,compile_env)

def link(name,objects,extra=()):
    call([cxx,'/nologo',*[str(H/(p+'.obj')) for p in objects],*extra,*libs,'-fsycl',
        '-fsycl-device-code-split=per_kernel','-Xsycl-target-backend=spir64','-cl-fp32-correctly-rounded-divide-sqrt',
        '/link','/INCREMENTAL:NO','/out:'+str(H/(name+'.exe'))],'link-'+name,compile_env)

def measure(exe,mode,label):
    out=call([str(H/(exe+'.exe')),'--bench',str(mode)],label)
    rows=[dict(T=int(t),begin=int(beg),arm=int(arm),index=int(i),us=float(us))
        for t,beg,arm,i,us in re.findall(r'sample T=(\d+) begin=(\d+) arm=(\d+) index=(\d+) us=([\d.]+)',out)]
    fps=re.findall(r'fingerprint T=(\d+) begin=(\d+) arm=(\d+) hash=(\w+)',out)
    assert len(rows)==405 and len(fps)==45
    shapes=[]
    for T,begin in manifest['shapes']:
        assert len({v[3] for v in fps if (int(v[0]),int(v[1]))==(T,begin)})==1
        ms=[statistics.median(r['us'] for r in rows if (r['T'],r['begin'],r['arm'])==(T,begin,a)) for a in range(5)]
        pairs=[b.paired(ms[a-1],ms[a],ms[a+1]) for a in (1,3)]
        stable=all(abs(p['baseline_drift_pct'])<=3 for p in pairs)
        gain=100*(math.exp(statistics.mean(math.log(p['baseline']/p['candidate']) for p in pairs))-1)
        shapes.append(dict(T=T,begin=begin,medians=ms,pairs=pairs,eligible=stable,gain_pct=gain))
    pool=100*(math.exp(statistics.mean(math.log1p(v['gain_pct']/100) for v in shapes if v['begin']==0))-1)
    score=statistics.mean(math.log1p(v['gain_pct']/100) for v in shapes)
    passed=all(v['eligible'] and v['gain_pct']>=(0 if v['begin'] else -1) for v in shapes) and pool>=1
    result=dict(label=label,mode=mode,samples=rows,fingerprints=fps,shapes=shapes,no_prefix_gain_pct=pool,score=score,passed=passed)
    print('MEASURE',label,'passed',passed,'plain',pool,'shapes',[(s['T'],s['begin'],round(s['gain_pct'],3),s['eligible']) for s in shapes],flush=True)
    return result

def native_audit(objects,tag):
    # Use the existing small Level Zero extractor; retain every target native module.
    src=(R/'build-sycl-a770-esimd-loads-v2-20261025/native-audit.cpp').read_text()
    a=src.index('        const bool baseline');z=src.index('        auto bundle',a)
    filter_text='gdn_sync_experiment' if tag=='screen' else 'gdn_step_norm_multi_kernel_42ae26'
    src=src[:a]+f'        if(name.find("{filter_text}")==std::string::npos) continue;\n'+src[z:]
    src=src.replace('index != 8','index != '+('20' if tag=='screen' else '8'))
    target=H/('native-'+tag);target.mkdir()
    (target/'audit.cpp').write_text(src)
    call([cxx,'/nologo','/EHsc','/O2','/MD','-Qstd:c++20','-fsycl','-fsycl-device-code-split=per_kernel',
        '-IC:/Program Files/LevelZeroSDK/1.33.1/include',str(target/'audit.cpp'),
        *[str(H/(p+'.obj')) for p in objects],*libs,'C:/Program Files/LevelZeroSDK/1.33.1/lib/ze_loader.lib',
        '-Xsycl-target-backend=spir64','-cl-fp32-correctly-rounded-divide-sqrt','/link','/INCREMENTAL:NO',
        '/out:'+str(target/'audit.exe')],'build-native-'+tag,compile_env)
    call([str(target/'audit.exe')],'native-'+tag,cwd=target)
    for binary in target.glob('native-*.bin'):
        call(['C:/Program Files (x86)/Intel/oneAPI/ocloc/2025.3/bin/ocloc.exe','disasm','-file',str(binary),
            '-dump',str(target/binary.stem),'-device','dg2-g10'],'disasm-'+tag+'-'+binary.stem,cwd=target)

with b.gpu_lock():
    verify()
    for name in ('candidate','experiment'):compile_obj(name)
    link('experiment',['experiment','candidate'])
    for mode,name in manifest['variants'].items():
        mode=int(mode)
        out=call([str(H/'experiment.exe'),'--check',str(mode)],'check-'+name)
        assert 'PASS checks=649 ' in out
        run=measure('experiment',mode,'screen-'+name);d['screen'].append(run);b.save(H/'partial.json',d)
    eligible=[v for v in d['screen'] if v['passed']]
    if eligible:
        winner=max(eligible,key=lambda v:v['score']);d['selected']=winner['mode']
        for i in range(2):
            d['confirmation'].append(measure('experiment',winner['mode'],'confirm-'+str(i)));b.save(H/'partial.json',d)
        if all(v['passed'] for v in d['confirmation']):
            # Generate the small final source, with no experimental selector in the kernel.
            spec=importlib.util.spec_from_file_location('preparation',H/'prepare.py')
            # Import only the transform definition, without replaying preparation side effects.
            code=(H/'prepare.py').read_text();a=code.index('def transform(');z=code.index('\nvariants =',a)
            ns={};exec(code[a:z],ns)
            source=(H/'before.cpp').read_text();a=source.index('template <int SG, bool Q>\n__dpct_inline__ void gdn_step_norm_multi_kernel(');z=source.index('\nconstexpr int kGdnSplitCols',a)
            production=source[:a]+ns['transform'](source[a:z],winner['mode'])+source[z:]
            (H/'production.cpp').write_text(production,encoding='utf-8')
            names=set(re.findall(r'^(?:void|bool) (\w+)\(',source,re.MULTILINE))-{'check'}
            names.update(re.findall(r'class (\w+)',source));names.add('gdn_state_replay_layers_esimd')
            reference=''.join(f'#define {n} {n}_baseline\n' for n in sorted(names))+source
            (H/'reference.cpp').write_text(reference,encoding='utf-8')
            (H/'final-experiment.cpp').write_text((H/'experiment.cpp').read_text())
            for name in ('production','reference'):compile_obj(name)
            compile_obj('final-experiment',['-DGDN_FINAL_BUILD'])
            link('final-experiment',['final-experiment','production','reference'])
            out=call([str(H/'final-experiment.exe'),'--check','1'],'final-check');assert 'PASS checks=649 ' in out
            for i in range(2):d['final'].append(measure('final-experiment',1,'final-confirm-'+str(i)));b.save(H/'partial.json',d)
            compile_obj('gdn-parity')
            prefill=[str(R/'build-sycl-deepinfer-20261023/baseline/CMakeFiles/strata_prefill.dir/src/prefill/kernels.dp.cpp.obj'),
                str(base/'strata_prefill.lib'),'C:/Program Files (x86)/Intel/oneAPI/mkl/2025.3/lib/mkl_sycl_blas_dll.lib']
            d['link_inputs'].update({str(Path(p).relative_to(R)) if Path(p).is_relative_to(R) else p:b.sha(p) for p in prefill})
            link('gdn-parity',['gdn-parity','production'],prefill)
            d['regression']={name:call([str(H/'gdn-parity.exe'),*args],'regression-'+name) for name,args in
                [('window',['--window-check']),('selftest',['--selftest','--prefill-tokens','7']),('layers',['--commit-layers-check'])]}
            d['adopt']=all(v['passed'] for v in d['final'])
            native_audit(['production','reference'],'final')
    native_audit(['candidate'],'screen')
    verify();d['complete']=True
    d['artifacts_sha256']={str(p.relative_to(R)):b.sha(p) for p in H.rglob('*') if p.is_file() and p.name not in ('results.json','partial.json')}
    b.save(H/'results.json',d)
print('DONE adopt',d['adopt'],flush=True)
