import json
import re
import statistics
from pathlib import Path
H = Path(__file__).resolve().parent
data = {'complete': True, 'purpose': 'Bounded prefill diagnostics, not performance acceptance',
        'cache_slots': 5771, 'cache_mib': 7922, 'kv': 'int8', 'chunk': 256,
        'short_read': 0, 'first_prompt_tokens': 23, 'first_prefill_rows': 22, 'runtime_controls': []}
for name in ('prefill-health-257-default', 'prefill-health-257-v1', 'prefill-health-257-no-offload'):
    row = json.loads((H/name/'results.json').read_text())
    assert row['complete'] and row['timeout'] and row['done'] == []
    data['runtime_controls'].append({'directory': name, 'variant': row['variant'],
                                     'timeout_seconds': row['timeout_seconds'], 'completed_requests': 0})
trace = json.loads((H/'prefill-trace/results.json').read_text())
assert trace['complete'] and trace['timeout'] and trace['done'] == []
text = (H/'prefill-trace/baseline.stderr').read_text()
layers = [(int(i), float(ms)) for i, ms in re.findall(
    r'prefill layer (\d+), position 0, rows 22, after ([\d.]+) ms', text)]
assert [i for i, _ in layers] == list(range(len(layers))) and len(layers) > 2
intervals = [b[1]-a[1] for a, b in zip(layers, layers[1:])]
data['trace'] = {'directory': 'prefill-trace', 'timeout_seconds': trace['timeout_seconds'],
                 'last_layer_started': layers[-1][0], 'last_layer_start_ms': layers[-1][1],
                 'layer_start_ms': dict(layers), 'adjacent_layer_ms': intervals,
                 'median_adjacent_layer_ms': statistics.median(intervals), 'completed_requests': 0,
                 'observation': 'The trace run progressed through many layers; earlier bounded timeouts do not prove deadlock.'}
data['default_decision'] = 'Keep all four experimental switches disabled; no new model speed claim.'
probe = json.loads((H/'mkl-scalar-probe-final/results.json').read_text())
assert probe['complete'] and probe['returncode'] == 0 and len(probe['runs']) == 6
data['scalar_probe'] = {
    'directory': 'mkl-scalar-probe-final', 'exe_sha256': probe['exe_sha256'],
    'queries': probe['queries'], 'runs': probe['runs'],
    'decision': 'Do not replace DPCT with a typed call: scalar queries are negligible and typed GEMM has no stable advantage.'}
runtime = json.loads((H/'mkl-runtime-no-program-options/results.json').read_text())
assert runtime['complete'] and runtime['pass_check'] and runtime['returncode'] == 0
verbose = (H/'mkl-runtime-verbose/stdout.log').read_text()
assert 'gemm[half,half,float]' in verbose and 'gemm[float]' in verbose
trace_text = (H/'mkl-runtime-trace/stdout.log').read_text()
copies = re.findall(r'<--- urEnqueueUSMMemcpy\([^\n]+', trace_text)
data['runtime_probe'] = {
    'no_program_options_directory': 'mkl-runtime-no-program-options',
    'no_program_options_runs': runtime['runs'],
    'verbose_directory': 'mkl-runtime-verbose', 'trace_directory': 'mkl-runtime-trace',
    'device_to_host_weight_copy_evidence': next(s for s in copies if '.size = 6553600,' in s),
    'observation': 'FP16 mixed-output GEMM invokes FP32 GEMM internally and stages device inputs through host allocations. Removing program compile options did not remove the large per-call cost.',
    'next_check': 'Compare GPU widening followed by the same FP32 compute-mode-unset GEMM. No engine GEMM change yet.'}
widen = json.loads((H/'prefill-widen/results.json').read_text())
assert widen['complete'] and len(widen['runs']) == 5
assert all(r['returncode']==0 and r['summary']==[{'checks':96,'mismatched_cases':0}] for r in widen['runs'])
measurements = []
for name in ('prefill-widen-operators','prefill-widen-operators-confirm'):
    result = json.loads((H/name/'results.json').read_text())
    assert result['complete'] and len(result['runs']) == 5
    measurements.append({'directory':name,'comparisons':result['comparisons']})
data['gpu_widen'] = {
    'flag':'STRATA_PREFILL_GPU_WIDEN=1', 'default':'off', 'build_directory':'prefill-widen',
    'checks_per_process':96,'check_processes':5,'all_active_elements_bit_equal':True,
    'workspace':'Reuse existing 32 MiB workspace; no added device allocations; oversized products fall back.',
    'library':'Both arms use the same installed oneMKL 2025.3 BLAS DLL import, not the older static engine binary.',
    'measurements':measurements,
    'limits':'Synthetic T=4 N=1280 K=2560, no extra pinned expert allocations; not a full-model speed claim.',
    'original_padding_observation':'The original mixed-input beta=0 call overwrites unused row padding; GPU widening preserves it. Comparisons require exact active elements and untouched trailing guards.',
    'model_status':'Short forced-batch-prefill on/off/on diagnostic running.'}
data['runtime_probe']['next_check'] = 'GPU widening implemented as opt-in after parity and repeated operator measurements; model diagnostics pending.'
model = json.loads((H/'prefill-widen-model-short/results.json').read_text())
assert len(model['runs'])==3 and all(len(r['requests'])==1 for r in model['runs'])
original = next(r['requests'][0] for r in model['runs'] if not r['widen'])
candidates = [r['requests'][0] for r in model['runs'] if r['widen']]
assert any(t!=0 for t in original['tokens']) and all(s['tokens']==[0]*48 for s in candidates)
debug = json.loads((H/'prefill-widen-model-debug/results.json').read_text())
assert debug['complete'] and not debug['functional_acceptance']
data['gpu_widen']['model_status'] = 'FAILED: original produces normal tokens; both enabled prototype requests produce 48 zero tokens.'
data['gpu_widen']['failed_model'] = {'directory':'prefill-widen-model-short', 'reference':original,
                                    'candidate_requests':candidates, 'functional_acceptance':False}
data['gpu_widen']['debug'] = {'directory':'prefill-widen-model-debug', 'first_bad_moe_layer':0,
                             'activation_nonfinite':563200,'final_nonfinite_logits':248320}
data['gpu_widen']['extended_checks'] = [
    {'directory':'prefill-widen-hc-check','cases_per_process':216},
    {'directory':'prefill-widen-chain-check','cases_per_process':219}]
for row in data['gpu_widen']['extended_checks']:
    test = json.loads((H/row['directory']/'results.json').read_text())
    assert test['complete'] and all(r['summary']==[{'checks':row['cases_per_process'],'mismatched_cases':0}] for r in test['runs'])
data['gpu_widen']['synchronized_model_status'] = 'FAILED; waits alone did not resolve the model regression.'
data['gpu_widen']['failed_variants'] = []
for name,variant in [('prefill-widen-sync-model','FP16/BF16, explicit waits'),
                     ('prefill-widen-f16-model','FP16 only, explicit waits')]:
    result = json.loads((H/name/'results.json').read_text())
    assert result['complete'] and not result['functional_acceptance']
    data['gpu_widen']['failed_variants'].append({'directory':name,'variant':variant,
        'functional_acceptance':False,'requests':[s for r in result['runs'] for s in r['requests']]})
data['gpu_widen']['additional_checks'] = []
for name in ('prefill-widen-f16','prefill-widen-tight-check','prefill-widen-arena-check'):
    result = json.loads((H/name/'results.json').read_text())
    assert result['complete'] and all(r['summary']==[{'checks':219,'mismatched_cases':0}] for r in result['runs'])
    data['gpu_widen']['additional_checks'].append({'directory':name,'checks_per_process':219,'processes':2})
arena = json.loads((H/'prefill-widen-arena-results/results.json').read_text())
assert arena['complete'] and arena['functional_acceptance']
data['gpu_widen']['small_shared_arena'] = {'directory':'prefill-widen-arena-results','checks_per_process':219,'processes':2,'passed':True}
actual = json.loads((H/'prefill-widen-actual-compare-model/results.json').read_text())
assert actual['complete'] and actual['returncode']==86
data['gpu_widen']['actual_model_gemm'] = {'directory':'prefill-widen-actual-compare-model','comparisons':actual['comparisons'],
    'observation':'First optimized GEMM (T=22,N=640,K=2560) differs in all 14080 elements; raw FP16 inputs and reference output are finite.'}
large = json.loads((H/'mkl-large-offset-probe/results.json').read_text())
assert large['complete'] and large['returncode']==2 and large['summary']==[{'checks':16,'mismatched_cases':4}]
assert all(s['conversion_errors']==0 and bool(s['different'])==(s['offset_gib']==4 and s['shared_output']) for s in large['parity'])
data['gpu_widen']['large_offset_reproducer'] = {'directory':'mkl-large-offset-probe','cases':large['parity'],
    'observation':'On A770/oneMKL 2025.3, FP32 GEMM corrupts C when A/B/C share an allocation and C is above 4 GiB. Converted inputs match; independent C passes.'}
data['gpu_widen']['current_scope'] = 'FP16 only, opt-in; BF16 restored to its original implementation. One separate device output allocation per optimized call, copied back after GEMM.'
for name in ('prefill-widen-output-stage','prefill-widen-output-stage-arena','prefill-widen-output-stage-model',
             'prefill-widen-output-stage-operators','prefill-widen-output-stage-operators-confirm','mkl-output-stage-probe',
             'prefill-widen-output-stage-sampling','prefill-widen-final-check'):
    path=H/name/'results.json'
    if path.exists():
        result=json.loads(path.read_text())
        data['gpu_widen'].setdefault('output_staging_results',[]).append({'directory':name,'result':result})
data['default_decision'] = 'Keep all four original experiments and prefill GPU widening disabled; no new model speed claim.'
data['gpu_widen']['prototype_model_status'] = data['gpu_widen'].pop('model_status')
fixed = json.loads((H/'prefill-widen-output-stage-model/results.json').read_text())
sample = json.loads((H/'prefill-widen-output-stage-sampling/results.json').read_text())
assert fixed['complete'] and fixed['functional_acceptance'] and sample['complete'] and sample['functional_acceptance']
data['gpu_widen']['current_build_directory'] = 'prefill-widen-output-stage'
data['gpu_widen']['model_status'] = 'Fixed FP16 path: Short batch-prefill greedy and repeated fixed-seed sampling/penalties match the disabled reference. Full performance matrix remains incomplete.'
data['gpu_widen']['operator_decision'] = 'Retain fixed FP16 source after two independent A/B/A/B/A rounds with valid >=3% gains; BF16 is unchanged and is only a control.'
data['runtime_probe']['next_check'] = 'FP16 GPU widening plus separate output staging resolves the measured Short regression; four-scenario model performance acceptance remains outstanding.'
target = H/'prefill-investigation-20261024/summary.json'
target.write_text(json.dumps(data, indent=2), encoding='utf-8')
print(target)
