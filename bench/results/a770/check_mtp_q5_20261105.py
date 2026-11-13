"""Recompute the Q5 MTP comparison from complete saved protocols; no GPU required."""
import json
import math
from pathlib import Path
import statistics


def protocol_requests(protocol):
    raw, pending = [], []
    for line in protocol.splitlines():
        p = line.split()
        if not p:
            continue
        assert p[0] != 'ERR'
        if p[0] == 'RESUME':
            assert int(p[1]) == 0
        elif p[0] in ('T', 'TB'):
            if p[0] == 'TB':
                assert int(p[1]) == len(p) - 2
            pending.extend(map(int, p[1:] if p[0] == 'T' else p[2:]))
        elif p[0] == 'DONE':
            assert len(pending) == int(p[1])
            raw.append((pending, line))
            pending = []
    assert not pending
    return raw


def check_practical(data):
    assert data['complete'] and data['correctness'] and 'error' not in data
    assert [r['mode'] for r in data['runs']] == data['order'] == ['q2', 'q5', 'q2']
    q2, q5 = (data['configs'][k] for k in ('q2', 'q5'))
    a, b = (dict(zip(c['args'][::2], c['args'][1::2])) for c in (q2, q5))
    assert a['--expert-cache'] == '5500' and b['--expert-cache'] == '5000'
    assert a['--spec-min-p'] == b['--spec-min-p'] == '0.5'
    assert a['--suffix-draft'] == b['--suffix-draft'] == '0'
    assert {k: v for k, v in a.items() if k not in ('--expert-cache', '--mtp')} == {
        k: v for k, v in b.items() if k not in ('--expert-cache', '--mtp')}
    assert q5['env']['STRATA_MTP_EXPERTS'] == 'q5_0'
    assert q2['env'] == {k: v for k, v in q5['env'].items() if k != 'STRATA_MTP_EXPERTS'}
    cases = ('Code2K', 'Prose2K')
    reference = {row['case']: row for row in data['runs'][0]['rows'][:2]}
    for run, slots in zip(data['runs'], (5771, 5243, 5771)):
        assert run['exit_code'] == 0 and f'expert_slots={slots} ' in run['info']
        assert run['info'] in run['protocol'].splitlines() and 'lookup=0 ' in run['info']
        rows = run['rows']
        assert [(s['cycle'], s['case'], s['retained']) for s in rows] == [
            (cycle, case, cycle > 0) for cycle in range(4) for case in cases]
        raw = protocol_requests(run['protocol'])
        assert len(raw) == 9
        initializer = run['initializer']
        assert raw[0] == (initializer['tokens'], initializer['done'])
        assert initializer['tokens'] == data['runs'][0]['initializer']['tokens']
        for (output, done), row in zip(raw[1:], rows):
            p = done.split()
            assert row['matches'] and output == row['tokens'] == reference[row['case']]['tokens']
            assert done == row['done'] and row['generated'] == int(p[1]) == 128
            assert row['prompt_tokens'] == int(p[2]) == len(data['inputs'][row['case']]) == 2048
            assert row['prompt_ms'] == float(p[3]) and row['decode_ms'] == float(p[4])
            assert row['finish'] == p[5] == 'length'
            assert row['accepted'] == int(p[6]) and row['offered'] == int(p[7])
            assert 0 <= row['accepted'] <= row['offered']
            assert row['reused'] == int(p[8]) == row['resume'] == 0
            assert row['request_ms'] >= row['last_delivery_ms'] >= row['ttft_ms'] > 0
            assert math.isclose(row['delivery_span_ms'], row['last_delivery_ms'] - row['ttft_ms'])
            assert math.isclose(row['post_first_delivery_tok_s'],
                                1000 * (128 - row['first_delivery_tokens']) / row['delivery_span_ms'])
    for case, metrics in data['comparison'].items():
        for metric, saved in metrics.items():
            medians = [statistics.median(s[metric] for s in run['rows'] if s['case'] == case and s['retained'])
                       for run in data['runs']]
            first, candidate, last = medians
            base = math.sqrt(first * last)
            change = candidate / base - 1 if metric.endswith('tok_s') else base / candidate - 1
            expected = dict(baseline=base, candidate=candidate, throughput_change_pct=100 * change,
                            baseline_drift_pct=100 * (last / first - 1))
            assert saved.keys() == expected.keys()
            assert all(math.isclose(saved[k], v, rel_tol=1e-10, abs_tol=1e-10) for k, v in expected.items())
        print('practical', case, {k: round(metrics[k]['throughput_change_pct'], 3)
                                 for k in ('decode_ms', 'request_ms')})
    gate = data['default_replacement_gate']
    assert gate['min_throughput_change_pct'] == 1 and gate['max_abs_baseline_drift_pct'] == 3
    passes = all(data['comparison'][case][metric]['throughput_change_pct'] >= gate['min_throughput_change_pct']
                 and abs(data['comparison'][case][metric]['baseline_drift_pct']) <= gate['max_abs_baseline_drift_pct']
                 for case in cases for metric in ('decode_ms', 'request_ms'))
    assert passes == gate['passes']
    print('PASS practical: 24 complete requests, 3072 output tokens; default replacement gate:', passes)


def main():
    data = json.loads(Path(__file__).with_name('mtp-q5-2026-11-05.json').read_text(encoding='utf8'))
    requests = tokens = 0
    for cohort in data['cohorts']:
        assert cohort['complete']
        runs = cohort['runs']
        assert [r['kind'] for r in runs] == ['q2_0', 'q5_0', 'q2_0']
        for run in runs:
            assert run['exit_code'] == 0 and 'expert_slots=5243 ' in run['info'] and 'lookup=0 ' in run['info']
            assert [s['case'] for s in run['rows']] == list(cohort['inputs'])
            raw = protocol_requests(run['protocol'])
            assert len(raw) == len(run['rows']) + 1
            assert raw[0] == (run['warmup']['tokens'], run['warmup']['done'])
            accepted = offered = 0
            for (output, done), row, reference in zip(raw[1:], run['rows'], runs[0]['rows']):
                p = done.split()
                assert output == row['tokens'] == reference['tokens'] and done == row['done']
                assert row['case'] == reference['case'] and row['generated'] == int(p[1])
                assert row['prompt_tokens'] == int(p[2])
                assert row['prompt_tokens'] == len(cohort['inputs'][row['case']])
                assert row['prompt_ms'] == float(p[3]) and row['decode_ms'] == float(p[4])
                assert row['accepted'] == int(p[6]) and row['offered'] == int(p[7])
                assert row['reused'] == int(p[8]) == 0 and row['resume'] == 0
                assert 0 <= row['accepted'] <= row['offered']
                accepted += row['accepted']
                offered += row['offered']
                requests += 1
                tokens += len(output)
            assert accepted == run['accepted'] and offered == run['offered']
            assert run['rate'] == accepted / offered
        signature = lambda run: [(s['case'], s['tokens'], s['accepted'], s['offered']) for s in run['rows']]
        assert signature(runs[0]) == signature(runs[2])
        print(cohort['name'], f"{runs[0]['rate']:.6%} -> {runs[1]['rate']:.6%}")
    assert data['expert_bytes'] == {'q2_0': 707788800, 'q5_0': 1730150400, 'q8_0': 2673868800}
    profile = data['optional_profile']
    assert profile['exe_sha256'] == data['current_build']['exe_sha256']
    assert profile['smoke']['complete'] and profile['smoke']['matches']
    reference = data['cohorts'][1]['runs'][1]['rows'][0]
    assert all(profile['smoke']['row'][k] == reference[k] for k in
               ('tokens', 'accepted', 'offered', 'generated', 'prompt_tokens', 'finish', 'reused', 'resume'))
    print(f'PASS {requests} complete measured protocols, {tokens} output tokens; warmups also replayed')
    check_practical(data['practical'])


if __name__ == '__main__':
    main()
