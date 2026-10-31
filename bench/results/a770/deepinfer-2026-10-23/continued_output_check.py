"""Check completed continuation runs; python <this-file> <result-directory> [...]."""
import hashlib
import json
from pathlib import Path
import sys

HERE = Path(__file__).resolve().parent
sha = lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest()
old = json.loads((HERE/'functional-sampler-q8-short/results.json').read_text())
assert old['complete']
short = old['runs'][0]
report = {'complete': False, 'groups': []}
for name in sys.argv[1:]:
    path = HERE/name/'results.json'
    data = json.loads(path.read_text(encoding='utf-8'))
    assert data['complete'] and data['phase'] == 'functional', path
    reference = data['runs'][0]
    assert reference['mode'] == 'baseline' and reference['build'] == 'baseline'
    assert len(data['runs']) == len(data['modes'].split(','))
    expected = {k: v['tokens'] for k, v in reference['rows'].items()}
    assert expected['Short'] == short['rows']['Short']['tokens'][:data['max_tokens']]
    checks = []
    for run in data['runs']:
        assert {k: v['tokens'] for k, v in run['rows'].items()} == expected
        for row in run['rows'].values():
            assert row['generated'] == len(row['tokens']) > 0
            assert 0 <= row['accepted'] <= row['offered']
        functional = run['functional']
        assert functional['sampled']['tokens'] == functional['sampled_repeat']['tokens']
        assert functional['sampled']['tokens'] == short['functional']['sampled']['tokens']
        assert functional['cancel']['finish'] == 'cancel'
        assert functional['recovery']['tokens'] == expected['Short'][:16]
        if run['mode'] in ('baseline', 'combined'):
            assert functional['forced_eos']['finish'] == 'stop'
            assert functional['parked_resume']['reused'] > 0
            assert functional['live_resume']['reused'] > 0
        checks.append({'mode': run['mode'], 'greedy_cases_equal': list(expected),
                       'short_sampling_repeat_cancel_recovery': True})
    report['groups'].append({'directory': name, 'results_sha256': sha(path),
                             'threshold': data['threshold'], 'expert_slots': data['expert_slots'],
                             'max_tokens': data['max_tokens'], 'checks': checks})
assert report['groups'], 'Provide at least one completed result directory'
long_path = HERE/'functional-sampler-q8-long-sampling/results.json'
if long_path.exists():
    sampled = json.loads(long_path.read_text())
    if sampled['complete']:
        assert len(sampled['runs']) == 2
        reference = sampled['runs'][0]['cases']
        checks = []
        for run in sampled['runs']:
            assert set(run['cases']) == {'Short', 'Code2K', 'Prose2K', 'Long8K'}
            assert run['cases']['Short']['first']['tokens'] == short['functional']['sampled']['tokens']
            for name, row in run['cases'].items():
                assert row['first']['tokens'] == row['repeat']['tokens'] == reference[name]['first']['tokens']
                assert row['first']['generated'] == row['repeat']['generated'] == 48
                if name != 'Short':
                    assert row['repeat']['reused'] >= 512
            checks.append({'mode': run['mode'], 'seed_replay_and_baseline_equal': True,
                           'reused_tokens': {name: row['repeat']['reused'] for name, row in run['cases'].items()}})
        report['long_sampling'] = {'results_sha256': sha(long_path), 'checks': checks}
timing_names = ['model-sampler-q8-short', 'model-sampler-q8-short-confirm',
                'model-sampler-chain-short', 'model-sampler-zero-short-confirm']
timings = [json.loads((HERE/name/'results.json').read_text()) for name in timing_names if (HERE/name/'results.json').exists()]
if len(timings) == len(timing_names) and all(data['complete'] for data in timings):
    reference = timings[0]['runs'][0]['rows']['Short']['tokens']
    retained = 0
    for data in timings:
        assert data['expert_slots'] == 5771
        for run in data['runs']:
            for row in run['samples']['Short']:
                assert row['tokens'] == reference and row['generated'] == 78 and row['finish'] == 'stop'
                retained += 1
    assert reference[:48] == short['rows']['Short']['tokens']
    report['performance_outputs'] = {'retained_requests': retained, 'tokens_each': len(reference),
                                     'thresholds_and_modes_equal': True,
                                     'sources_sha256': {name: sha(HERE/name/'results.json') for name in timing_names}}
build = HERE/'sampler-fp32'
reused = json.loads((HERE/'reused-dependencies.json').read_text())
for name, expected_sha in reused.items():
    assert sha(HERE/'baseline'/name) == sha(HERE/'candidate'/name) == expected_sha, name
exe_hashes = {}
for side in ('baseline', 'candidate'):
    metadata = json.loads((build/side/'build.json').read_text())
    exe_hashes[side] = sha(build/side/'strata.exe')
    assert exe_hashes[side] == metadata['exe_sha256']
    assert sha(build/'sampler.obj') == metadata['sampler_obj_sha256']
sampler_build = json.loads((build/'build.json').read_text())
for name, expected_sha in sampler_build['sources'].items():
    assert sha(build/Path(name).name) == expected_sha, name
original = json.loads((HERE/'config-original.json').read_text(encoding='utf-8-sig'))
production = json.loads((HERE.parent/'strata-iq2_xs.json').read_text(encoding='utf-8-sig'))
assert original == production
production_hash = sha(original['exe'])
assert production_hash == '8f0d866ded297cf5a409e84cf5dd61f437c56c8a89aaeb2a85569265b6a3e122'
report['provenance'] = {'unchanged_dependency_objects': len(reused), 'exe_sha256': exe_hashes,
                        'production_config_matches_original': True, 'production_exe_sha256': production_hash,
                        'frozen_sampler_sources_verified': True}
report['complete'] = True
report['script_sha256'] = sha(__file__)
(HERE/'continued-output-equivalence.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print('PASS continuation output checks:', [(g['directory'], len(g['checks'])) for g in report['groups']])
