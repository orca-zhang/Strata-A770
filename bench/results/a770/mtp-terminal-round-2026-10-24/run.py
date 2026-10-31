"""Isolated terminal-round regression; holds the shared GPU/compiler lock once."""
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import sys

H = Path(__file__).resolve().parent
R = H.parent
OLD = R/'build-sycl-deepinfer-20261023'
B = OLD/'candidate'
sys.path.insert(0, str(R/'tools'))
import bench_deepinfer as b
spec = importlib.util.spec_from_file_location('builder', OLD/'build.py')
builder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)


def replace(text, old, new):
    assert text.count(old) == 1, old
    return text.replace(old, new)


def verify_frozen():
    frozen = json.loads((H/'frozen.json').read_text())
    for group in ('before', 'headers'):
        for name, digest in frozen[group].items():
            assert b.sha(H/name) == digest, name
    for name, digest in frozen['dependencies'].items():
        assert b.sha(R/name) == digest, name
    assert b.sha(H/'config.json') == frozen['config_sha256']


def diagnostic(source):
    marker = '                const Clock::time_point tw2 = Clock::now();'
    source = replace(source, marker, marker + '''
                std::fprintf(stderr, "TC ROUND serve %lld %d %d %d %lld %lld %d %d\\n",
                             (long long) rounds, T, a, (int) timed_round, (long long) produced_n,
                             (long long) max_new, (int) eos, (int) stop_req.load());''')
    marker = '            if (eos) {\n                if (adapt_thr.joinable()) adapt_thr.join();'
    return replace(source, marker, '''            std::fprintf(stderr, "TC ROUND cli %lld %d %d %d %lld %lld %d 0\\n",
                         (long long) rounds, T, a, (int) timed_round, (long long) produced.size(),
                         (long long) o.max_new, (int) eos);
''' + marker)


def main():
    with b.gpu_lock():
        verify_frozen()
        commands = subprocess.check_output(['ninja', '-C', str(B), '-t', 'commands', 'strata'], text=True).splitlines()
        records = {'complete': False, 'commands': [], 'files_sha256': {},
                   'method': 'Recompile frozen current generator and MTP against frozen source-final headers with current MTP declarations. Shared sampler, verifier and archives on both arms. Diagnostics only in private source copies.'}
        prior = json.loads((H/'build-attempt1.json').read_text())
        records['prior_attempt'] = prior['failure']
        records['reused_compile_objects'] = {}

        def execute(label, cmd):
            print('BUILD', label, flush=True)
            records['commands'].append([label, cmd])
            b.save(H/'build.json', records)
            obj = H/(label.removeprefix('compile-')+'.obj')
            if label.startswith('compile-') and [label, cmd] in prior['commands']:
                assert b.sha(obj) == prior['files_sha256'][obj.name]
                records['reused_compile_objects'][obj.name] = b.sha(obj)
                return
            with (H/('retry-'+label+'.log')).open('w', encoding='utf-8') as log:
                p = subprocess.run(cmd, cwd=B, env=builder.environment(), stdout=log,
                                   stderr=subprocess.STDOUT, timeout=1200)
            if p.returncode:
                print((H/('retry-'+label+'.log')).read_text(errors='replace')[-8000:], flush=True)
            p.check_returncode()

        def compile_source(label, original, source):
            command = next(c for c in commands if c.endswith(' -c '+str(R/original)))
            command = command.split(' -QMD ')[0]
            for old, new in [(R/'sycl/../include', H/'headers/include'),
                             (R/'sycl/include', H/'headers/sycl/include')]:
                command = command.replace('-I'+str(old), '-I'+str(new))
            command += f' /Fo{H/label}.obj /Fd{H/label}.pdb -c {source}'
            execute('compile-'+label, command)
            records['files_sha256'][str(source.relative_to(H))] = b.sha(source)
            return H/(label+'.obj')

        mtp_source = H/'before/sycl/src/core/mtp.cpp'
        mtp = compile_source('mtp', 'sycl/src/core/mtp.cpp', mtp_source)
        debug_mtp = replace(mtp_source.read_text(),
                            'float* probs, float min_p, int* n_drafts, float cumulative_min_p) {',
                            'float* probs, float min_p, int* n_drafts, float cumulative_min_p) {\n    std::fprintf(stderr, "TC DRAFT\\n");')
        (H/'mtp-debug.cpp').write_text(debug_mtp)
        mtp_debug = compile_source('mtp-debug', 'sycl/src/core/mtp.cpp', H/'mtp-debug.cpp')
        policy_source = (H/'before/src/spec/draft_policy.cpp').read_text()
        policy_source = '#include <cstdio>\n'+replace(policy_source,
            'void DraftPolicy::observe(bool lookup, int t, int accepted, int match, double round_ms) {',
            'void DraftPolicy::observe(bool lookup, int t, int accepted, int match, double round_ms) {\n    std::fprintf(stderr, "TC OBSERVE %d %d %d\\n", (int) lookup, t, accepted);')
        (H/'policy-debug.cpp').write_text(policy_source)
        policy_debug = compile_source('policy-debug', 'src/spec/draft_policy.cpp', H/'policy-debug.cpp')
        link_template = commands[-1].replace(' -fsycl ', ' -fsycl -fsycl-max-parallel-link-jobs=8 ')
        for side, source in [('baseline', H/'before/sycl/src/program/generate.cpp'), ('candidate', H/'candidate.cpp')]:
            (H/(side+'-debug.cpp')).write_text(diagnostic(source.read_text()))
            for debug in (False, True):
                name = side+('-debug' if debug else '')
                output = H/name
                output.mkdir(exist_ok=True)
                obj = compile_source(name, 'sycl/src/program/generate.cpp', H/(side+'-debug.cpp') if debug else source)
                objects = [obj, mtp_debug if debug else mtp, OLD/'sampler-fp32/sampler.obj', OLD/'candidate-final/verify.obj']
                if debug:
                    objects.append(policy_debug)
                cmd = replace(link_template, 'CMakeFiles\\strata.dir\\src\\program\\generate.cpp.obj', ' '.join(map(str, objects)))
                cmd = cmd.replace('--intdir=CMakeFiles\\strata.dir', '--intdir='+str(output))
                for flag, file in [('out', 'strata.exe'), ('implib', 'strata.lib'), ('pdb', 'strata.pdb')]:
                    cmd = cmd.replace('/'+flag+':'+file, '/'+flag+':'+str(output/file))
                execute('link-'+name, cmd)
                records['files_sha256'][str((output/'strata.exe').relative_to(H))] = b.sha(output/'strata.exe')
        verify_frozen()
        records['files_sha256'].update({str(p.relative_to(H)): b.sha(p) for p in H.glob('*.obj')})
        records['complete'] = True
        b.save(H/'build.json', records)
        print('PASS isolated builds', flush=True)
        spec = importlib.util.spec_from_file_location('terminal_suite', H/'check.py')
        suite = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(suite)
        suite.main()
        verify_frozen()


if __name__ == '__main__':
    main()
