from pathlib import Path
import hashlib
import json
import re
import shutil

H = Path(__file__).resolve().parent
R = H.parent
source = R / 'sycl/src/kernels/cuda/verify_kernels.dp.cpp'
s = source.read_text(encoding='utf-8')
assert hashlib.sha256(source.read_bytes()).hexdigest() == '31d71b9b5f7b20d8537e34dbc6243ad34f83835f203c2b13a0af0268b12fbb73'
(H / 'before.cpp').write_bytes(source.read_bytes())
a = s.index('template <int SG, bool Q>\n__dpct_inline__ void gdn_step_norm_multi_kernel(')
z = s.index('\nconstexpr int kGdnSplitCols', a)
original = s[a:z]

def transform(body, mode):
    # Each candidate includes local-only fences and omits the empty initial retirement barrier.
    body = body.replace('item_ct1.barrier(); // the previous token is done with sk/sq/red/wsum',
        'if (t > 0) item_ct1.barrier(); // retire the previous token before reusing local memory')
    if mode in (2, 4):
        needle = '    auto &wsum ='
        body = body.replace(needle,
            '    auto &kv_red = *sycl::ext::oneapi::group_local_memory_for_overwrite<float[RG][S]>(\n'
            '        sycl::ext::oneapi::this_work_item::get_work_group<3>());\n' + needle)
        body = body.replace('        red[rg][col] = kv;', '        kv_red[rg][col] = kv;')
        body = body.replace('const float kv_col = red[0][col] + red[1][col] + red[2][col] + red[3][col];',
            'const float kv_col = kv_red[0][col] + kv_red[1][col] + kv_red[2][col] + kv_red[3][col];')
        assert body.count('        item_ct1.barrier();\n        red[rg][col] = o;') == 1
        body = body.replace('        item_ct1.barrier();\n        red[rg][col] = o;',
            '        // Separate reductions keep output writes from racing state-dot readers.\n        red[rg][col] = o;')
    if mode in (3, 4):
        body = body.replace('sq[tid] = ht[qh * S + tid];', 'if (t >= t_out_begin) sq[tid] = ht[qh * S + tid];')
        body = body.replace('        float o = 0.0f;',
            '        if (t < t_out_begin) {\n'
            '            // ponytail: replay needs only the ordered state updates.\n'
            '#pragma unroll\n'
            '            for (int r = 0; r < RPG; ++r)\n'
            '                s[r] = sycl::fma((float)g, s[r], sk[rg * RPG + r] * delta);\n'
            '            continue;\n'
            '        }\n'
            '        float o = 0.0f;')
        body = body.replace('        if (t < t_out_begin) continue;   // a replayed token: its state update is needed, its output is not\n', '')
    return body.replace('item_ct1.barrier();', 'item_ct1.barrier(sycl::access::fence_space::local_space);')

variants = {1: 'local-fences-first', 2: 'separate-reductions', 3: 'skip-replay-output', 4: 'combined'}
includes = s[:s.index('namespace strata::kernels {')]
prefix = includes + '\nnamespace strata::kernels {\nnamespace {\nconstexpr int S=128, RG=4, RPG=32;\n'
prefix += s[s.index('struct GdnQ81'):a]
bodies = [original] + [transform(original, mode).replace('gdn_step_norm_multi_kernel(', f'gdn_sync_{mode}(') for mode in variants]
wrapper = '''
} // namespace
void gdn_sync_variant(int mode, float* state, const float* h, int C, const float* gate,
    const float* beta, const float* z, const float* gamma, float eps, float* y,
    int hk, int hv, int T, const int32_t* keep, void* stream, int begin, void* xq) {
    auto launch = [&]<int M, int SG, bool Q> {
        strata::q_of(stream)->parallel_for<
            dpct_kernel_name<class gdn_sync_experiment, dpct_kernel_scalar<M>, dpct_kernel_scalar<SG>, dpct_kernel_scalar<Q>>>(
            sycl::nd_range<3>(sycl::range(1,1,(unsigned)hv)*sycl::range(1,RG,S),sycl::range(1,RG,S)),
            sycl::ext::oneapi::experimental::properties{sycl::ext::oneapi::experimental::use_root_sync},
            [=](sycl::nd_item<3>) [[sycl::reqd_sub_group_size(SG)]] {
                CALLS
            });
    };
    auto select = [&]<int M> {
        const char* sg = std::getenv("STRATA_GDN_SG16");
        if (sg && sg[0]=='1') {
            if(xq) launch.template operator()<M,16,true>(); else launch.template operator()<M,16,false>();
        } else {
            if(xq) launch.template operator()<M,32,true>(); else launch.template operator()<M,32,false>();
        }
    };
    switch(mode) {
        case 0: select.template operator()<0>(); break;
        case 1: select.template operator()<1>(); break;
        case 2: select.template operator()<2>(); break;
        case 3: select.template operator()<3>(); break;
        case 4: select.template operator()<4>(); break;
        default: throw std::runtime_error("invalid experiment mode");
    }
}
}
'''
args = '(state,h,C,gate,beta,z,gamma,eps,y,hk,hv,T,keep,begin,static_cast<GdnQ81*>(xq));'
calls = '\n                '.join(f'if constexpr(M=={mode}) {"gdn_step_norm_multi_kernel" if mode==0 else "gdn_sync_"+str(mode)}<SG,Q>{args}' for mode in range(5))
(H / 'candidate.cpp').write_text(prefix + '\n'.join(bodies) + wrapper.replace('CALLS', calls), encoding='utf-8')
shutil.copytree(R / 'sycl/include', H / 'headers/sycl/include')
shutil.copytree(R / 'include', H / 'headers/include')
shutil.copyfile(R / 'sycl/src/kernels/gdn_parity.cpp', H / 'gdn-parity.cpp')
files = [p for p in H.rglob('*') if p.is_file()]
manifest = dict(variants=variants, shapes=[[1,0],[2,0],[3,0],[4,0],[6,0],[8,0],[4,2],[6,3],[8,4]],
    rule='Before timing: exact state/output/Q8 parity and graph replay; eager SG32 fused-Q8, 36 independent layer states, '
         'A/B/A/B/A, 3 warmups and 9 retained samples of 8 full layer sweeps/arm. '
         'A paired shape is eligible only when both adjacent baseline medians drift <=3%. '
         'All shapes must be eligible; every no-prefix shape gain >=-1%, pooled no-prefix gain >=1%, '
         'every prefix shape gain >0%. Highest geometric gain screens into two fresh confirmation processes; '
         'both must individually pass the same rule. Final source and independently renamed original must then '
         'pass fresh correctness and two confirmation processes plus existing GDN regression. No deployment.',
    files_sha256={str(p.relative_to(R)): hashlib.sha256(p.read_bytes()).hexdigest() for p in files})
(H / 'preparation.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
print('Frozen GDN synchronization candidates and inputs')
