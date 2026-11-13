"""Replay complete protocols and recompute the current Q2 optimization trials; CPU only."""
import hashlib
import json
import math
from pathlib import Path
import statistics

from check_mtp_q5_20261105 import protocol_requests


def main():
    data = json.loads(Path(__file__).with_name('mtp-q2-optimization-2026-11-05.json').read_text(encoding='utf8'))
    assert {'activation_calibration', 'quality', 'optional_early_profile',
            'optional_calibrated_profile', 'optional_combined_profile'} <= data.keys()
    assert [study['name'] for study in data['experiments']] == [
        'cumulative-0.5-current', 'cumulative-hc-sg16-r1', 'weighted-current-r1', 'combined-current-r1']
    for study in data['experiments']:
        assert study['complete'] and study['correctness'] and 'error' not in study
        runs = study['runs']
        assert len(runs) == 3 and [r['mode'] for r in runs] == study['order']
        assert study['order'][0] == study['order'][2] == 'baseline'
        candidate = study['order'][1]
        baseline = study['configs']['baseline']
        cfg = study['configs'][candidate]
        assert cfg['exe'] == baseline['exe']
        if candidate == 'early':
            assert baseline['args'] == cfg['args']
            expected = dict(baseline['env'], STRATA_MTP_CUM_MIN_P='0.5')
            assert baseline['env']['STRATA_MTP_CUM_MIN_P'] == '0' and cfg['env'] == expected
            assert 'MTP cumulative confidence floor 0.5 (draft and verify windows)' in runs[1]['stderr']
        else:
            assert candidate in ('calibrated', 'combined')
            expected_env = dict(baseline['env'])
            if candidate == 'combined':
                expected_env['STRATA_MTP_CUM_MIN_P'] = '0.5'
                assert 'MTP cumulative confidence floor 0.5 (draft and verify windows)' in runs[1]['stderr']
            assert cfg['env'] == expected_env
            expected = baseline['args'].copy()
            expected[expected.index('--mtp') + 1] = cfg['args'][cfg['args'].index('--mtp') + 1]
            assert expected == cfg['args']
            assert study['weight_manifest']['complete'] and study['weight_manifest']['bytes'] == 707788800
            assert study['fallback_experts_unchanged'] == sum(
                c < 8 for c in data['activation_calibration']['imatrix']['counts'])
        cases = ('Code2K', 'Prose2K')
        reference = {s['case']: s for s in runs[0]['rows'][:2]}
        for run in runs:
            assert run['exit_code'] == 0 and 'expert_slots=5771 ' in run['info'] and 'lookup=0 ' in run['info']
            assert run['info'] in run['protocol'].splitlines()
            raw = protocol_requests(run['protocol'])
            assert len(raw) == 9
            assert raw[0] == (run['initializer']['tokens'], run['initializer']['done'])
            assert run['initializer']['tokens'] == runs[0]['initializer']['tokens']
            assert [(s['case'], s['cycle'], s['retained']) for s in run['rows']] == [
                (case, cycle, cycle > 0) for cycle in range(4) for case in cases]
            for (tokens, done), row in zip(raw[1:], run['rows']):
                fields = done.split()
                assert row['matches'] and tokens == row['tokens'] == reference[row['case']]['tokens']
                assert done == row['done'] and row['generated'] == int(fields[1]) == 128
                assert row['prompt_tokens'] == int(fields[2]) == len(study['inputs'][row['case']]) == 2048
                assert row['prompt_ms'] == float(fields[3]) and row['decode_ms'] == float(fields[4])
                assert row['finish'] == fields[5] == 'length'
                assert row['accepted'] == int(fields[6]) and row['offered'] == int(fields[7])
                assert 0 <= row['accepted'] <= row['offered']
                assert row['reused'] == int(fields[8]) == row['resume'] == 0
                assert row['request_ms'] >= row['last_delivery_ms'] >= row['ttft_ms'] > 0
                assert math.isclose(row['delivery_span_ms'], row['last_delivery_ms'] - row['ttft_ms'])
                assert math.isclose(row['post_first_delivery_tok_s'],
                                    1000 * (128 - row['first_delivery_tokens']) / row['delivery_span_ms'])
        signature = lambda r: [(s['case'], s['tokens'], s['accepted'], s['offered']) for s in r['rows']]
        assert signature(runs[0]) == signature(runs[2])
        assert set(study['comparison']) == set(cases)
        for case, metrics in study['comparison'].items():
            assert set(metrics) == {'decode_ms', 'prompt_ms', 'request_ms', 'ttft_ms', 'post_first_delivery_tok_s'}
            for metric, saved in metrics.items():
                first, middle, last = [statistics.median(s[metric] for s in r['rows']
                                                       if s['case'] == case and s['retained']) for r in runs]
                base = math.sqrt(first * last)
                change = middle / base - 1 if metric.endswith('tok_s') else base / middle - 1
                expected = dict(baseline=base, candidate=middle, throughput_change_pct=100 * change,
                                baseline_drift_pct=100 * (last / first - 1))
                assert saved.keys() == expected.keys()
                assert all(math.isclose(saved[k], v, rel_tol=1e-10, abs_tol=1e-10) for k, v in expected.items())
            print(study['name'], case, {k: round(metrics[k]['throughput_change_pct'], 3)
                                       for k in ('decode_ms', 'request_ms')})
        assert study['gate'] == dict(min_throughput_change_pct=1, max_abs_baseline_drift_pct=3,
                                    metrics=['decode_ms', 'request_ms'], exact_output_required=True)
        passes = all(study['comparison'][case][metric]['throughput_change_pct'] >= 1 and
                     abs(study['comparison'][case][metric]['baseline_drift_pct']) <= 3
                     for case in cases for metric in study['gate']['metrics'])
        assert passes == study['passes_gate']
        print('PASS 24 complete requests, 3072 output tokens; gate:', passes)
    if 'activation_calibration' in data:
        calibration = data['activation_calibration']
        result, prepared, moments, weights = [calibration[k] for k in ('results', 'prepared', 'imatrix', 'weights')]
        assert result['complete'] and result['exit_code'] == 0
        assert result['source_sha256'] == prepared['source_sha256']
        assert result['trace_sha256'] == moments['trace_sha256']
        assert weights['complete'] and weights['bytes'] == 707788800
        for source, digest in ((calibration['source'], result['source_sha256']),
                               (calibration['quantizer'], weights['quantizer_sha256'])):
            assert digest in {hashlib.sha256(s.encode('utf8')).hexdigest()
                              for s in (source, source.replace('\n', '\r\n'))}
        assert weights['imatrix_sha256'] == moments['imatrix_sha256']
        assert moments['observations'] == sum(moments['counts']) == 10 * moments['records']
        assert moments['calibrated_experts'] == sum(c >= 8 for c in moments['counts'])
        assert len(moments['counts']) == 512 and all(c >= 0 for c in moments['counts'])
        assert len(result['rows']) == len(prepared['inputs']) == 12
        assert not ({tuple(ids) for ids in prepared['inputs']} &
                    {tuple(ids) for ids in data['quality']['inputs'].values()})
        audits = result['reference_rows']
        assert len(audits) == 2 and moments['excluded_prefix_bytes'] == max(r['excluded_bytes'][1] for r in audits)
        raw = protocol_requests(calibration['protocol'])
        rows = [result['initializer'], *(r['row'] for r in audits), *result['rows']]
        assert len(raw) == len(rows) == 15
        for (tokens, done), row in zip(raw, rows):
            assert tokens == row['tokens'] and done == row['done'] and len(tokens) == row['generated']
        for row, ids in zip(result['rows'], prepared['inputs']):
            assert row['prompt_tokens'] == len(ids) and 0 < row['generated'] <= 96
        reference = {r['case']: r for r in data['experiments'][0]['runs'][0]['rows']}
        for audit in audits:
            assert audit['row']['tokens'] == reference[audit['case']]['tokens']
        print('PASS real activation collection, complete audit outputs and excluded evaluation prefix')
    if 'quality' in data:
        quality = data['quality']
        assert quality['complete'] and 'error' not in quality
        assert [r['mode'] for r in quality['runs']] == ['baseline', 'early', 'calibrated', 'combined']
        baseline = quality['runs'][0]
        reference = {r['case']: r for r in baseline['rows']}
        assert len(reference) == len(quality['inputs']) == 9
        for run in quality['runs']:
            assert run['exit_code'] == 0 and run['info']['expert_slots'] == 5771 and run['info']['lookup'] == 0
            assert run['initializer']['tokens'] == baseline['initializer']['tokens']
            assert [r['case'] for r in run['rows']] == list(quality['inputs'])
            for row in run['rows']:
                assert row['matches'] and row['tokens'] == reference[row['case']]['tokens']
                assert row['generated'] == len(row['tokens']) == reference[row['case']]['generated']
                assert row['prompt_tokens'] == len(quality['inputs'][row['case']])
                assert row['finish'] == reference[row['case']]['finish'] and row['reused'] == 0
                assert 0 <= row['drafts_accepted'] <= row['drafts_offered']
            assert run['accepted'] == sum(r['drafts_accepted'] for r in run['rows'])
            assert run['offered'] == sum(r['drafts_offered'] for r in run['rows'])
            assert math.isclose(run['rate'], run['accepted'] / run['offered'])
        assert data['optional_early_profile']['smoke_complete']
        print('PASS four actual server engine adapters, nine cases each, complete outputs unchanged')
    for mode in ('calibrated', 'combined'):
        if 'optional_' + mode + '_profile' not in data:
            continue
        profile = data['optional_' + mode + '_profile']
        assert profile['complete'] and profile['exit_code'] == 0 and 'error' not in profile
        assert profile['default_replaced'] is (mode == 'combined')
        assert profile['config_sha256'] in {hashlib.sha256(text.encode('utf8')).hexdigest()
                                            for text in (profile['config_text'], profile['config_text'].replace('\n', '\r\n'))}
        assert json.loads(profile['config_text']) == profile['cfg']
        expected = json.loads(json.dumps(data['quality']['configs'][mode]))
        expected['args'][expected['args'].index('--mtp') + 1] = profile['runtime']
        expected['log'] = profile['cfg']['log']
        assert profile['cfg'] == expected and 'build-' not in profile['runtime']
        assert profile['info']['expert_slots'] == 5771 and profile['info']['lookup'] == 0
        assert profile['runtime_sha256']['experts.bin'] == data['activation_calibration']['weights']['sha256']
        assert all(data['quality']['input_sha256'][path] == digest
                   for path, digest in profile['input_sha256'].items())
        reference = next(run for run in data['quality']['runs'] if run['mode'] == mode)
        assert profile['initializer']['tokens'] == reference['initializer']['tokens']
        assert [row['case'] for row in profile['rows']] == ['Code2K', 'Prose2K']
        assert profile['inputs'] == {case: data['quality']['inputs'][case]
                                     for case in ('Chinese', 'Code2K', 'Prose2K')}
        for row in profile['rows']:
            expected = next(r for r in reference['rows'] if r['case'] == row['case'])
            assert all(row[k] == expected[k] for k in ('tokens', 'generated', 'prompt_tokens', 'finish',
                                                      'reused', 'drafts_accepted', 'drafts_offered'))
        if mode == 'combined':
            assert profile['primary_sha256'] == profile['config_sha256']
            assert profile['backup'].endswith('strata-iq2_xs-before-mtp-combined-20261105.json')
            assert 'User selected' in profile['adoption_reason']
            assert 'MTP cumulative confidence floor 0.5 (draft and verify windows)' in profile['stderr']
        print('PASS', mode, 'profile, stable runtime and complete server adapter outputs')


if __name__ == '__main__':
    main()
