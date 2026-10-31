from pathlib import Path
import hashlib
import json
import re
import shutil

H=Path(__file__).resolve().parent
R=H.parent
P=R/'build-sycl-a770-gdn-sync-20261025'
original=(P/'before.cpp').read_text()
assert (R/'sycl/src/kernels/cuda/verify_kernels.dp.cpp').read_bytes()==(P/'before.cpp').read_bytes()

def grf(source):
    a=source.index('    auto launch_base =',source.index('void gdn_step_norm_multi('))
    z=source.index('    auto launch_split =',a)
    body=source[a:z]
    needle='                exp_props,\n'
    assert body.count(needle)==1
    body=body.replace(needle,'                sycl::ext::oneapi::experimental::properties{\n'
        '                    sycl::ext::oneapi::experimental::use_root_sync,\n'
        '                    sycl::ext::intel::experimental::grf_size<256>},\n')
    return source[:a]+body+source[z:]

def renamed(source,suffix):
    names=set(re.findall(r'^(?:void|bool) (\w+)\(',source,re.MULTILINE))-{'check'}
    names.update(re.findall(r'class (\w+)',source));names.add('gdn_state_replay_layers_esimd')
    return ''.join(f'#define {n} {n}_{suffix}\n' for n in sorted(names))+source

for name,source in [('grf-only',original),('replay-grf',(P/'production.cpp').read_text())]:
    result=grf(source)
    (H/(name+'-production.cpp')).write_text(result,encoding='utf-8')
    (H/(name+'.cpp')).write_text(renamed(result,name.replace('-','_')),encoding='utf-8')
shutil.copyfile(P/'before.cpp',H/'before.cpp')
shutil.copyfile(P/'reference.cpp',H/'reference.cpp')
shutil.copytree(P/'headers',H/'headers')
text=(P/'experiment.cpp').read_text()
a=text.index('namespace strata::kernels {');z=text.index('\nuint64_t hash_bytes',a)
signature='(float*,const float*,int,const float*,const float*,const float*,const float*,float,float*,int,int,int,const int32_t*,void*,int,void*);'
wrapper='namespace strata::kernels {\n'
for name in ('baseline','grf_only','replay_grf'):
    wrapper+='void gdn_step_norm_multi_'+name+signature+'\n'
wrapper+='''
void gdn_sync_variant(int mode,float* state,const float* h,int C,const float* gate,const float* beta,
    const float* z,const float* gamma,float eps,float* y,int hk,int hv,int T,const int32_t* keep,void* stream,int begin,void* xq) {
#ifdef GDN_FINAL_BUILD
    auto fn=mode?gdn_step_norm_multi:gdn_step_norm_multi_baseline;
#else
    auto fn=mode==1?gdn_step_norm_multi_grf_only:(mode==2?gdn_step_norm_multi_replay_grf:gdn_step_norm_multi_baseline);
#endif
    fn(state,h,C,gate,beta,z,gamma,eps,y,hk,hv,T,keep,stream,begin,xq);
}
}
'''
text=text[:a]+wrapper+text[z:]
text=text.replace('const bool bench=std::string(argv[1])=="--bench";',
    'const bool bench=std::string(argv[1]).starts_with("--bench");')
text=text.replace('        _putenv_s("STRATA_GDN_SG16","0");',
    '        _putenv_s("STRATA_GDN_SG16",std::string(argv[1])=="--bench16"?"1":"0");')
(H/'experiment.cpp').write_text(text)
manifest=dict(variants={1:'grf-only',2:'replay-grf'},shapes=[[1,0],[2,0],[3,0],[4,0],[6,0],[8,0],[4,2],[6,3],[8,4]],
    rule='New experiment after prefix-only regressed production SG16. Compare the exact original against '
        '256 GRF alone and 256 GRF plus the screened prefix changes. Both SG16 and SG32, fused Q8, '
        '36 independent states, A/B/A/B/A, three warmup and nine retained batches of eight sweeps. '
        'Same numerical rule per process: every shape has both baseline drifts <=3%; no-prefix shape gains >=-1%, '
        'pooled no-prefix gain >=1%, every prefix gain >=0%. A screen must pass both subgroup modes. '
        'Prefer the smaller GRF-only change if it qualifies; otherwise replay+GRF. '
        'Two fresh confirmations in each subgroup mode must all pass. Then remove renaming macros, '
        'compile the final public API independently, differential-check and confirm in two more fresh '
        'processes per subgroup. Existing GDN regressions and native GRF audit required. No deployment.',
    files_sha256={str(p.relative_to(R)):hashlib.sha256(p.read_bytes()).hexdigest()
        for p in H.rglob('*') if p.is_file()})
(H/'preparation.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
print('Frozen GRF256 candidates and dual-subgroup rule')
