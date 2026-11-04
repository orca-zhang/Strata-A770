"""Recheck the published checkpoint-restore evidence without a GPU."""
import hashlib
import json
import math
from pathlib import Path
import re
import sys

from check_snapshot_copy_batch_20261031 import check_model


def digest(text):
    return hashlib.sha256(text.encode('utf-8')).hexdigest()


def check(report):
    built = report['build']
    assert built['complete'] and built['fixed_unchanged'] and built['libraries_unchanged']
    assert not built['production_changed'] and len(built['commands']) == 14
    assert all(command['exit_code'] == 0 for command in built['commands'])
    assert report['current_build']['executables_sha256'] == built['executable_sha256']
    assert report['source_and_images_unchanged'] and len(built['image_sha256']) == 5370
    assert {command['label'] + '.log' for command in built['commands']} == report['raw_build_logs'].keys()
    for name, raw in report['raw_build_logs'].items():
        assert digest(raw) == report['build_log_sha256'][name]
    for label, count in (('A', 4392), ('B', 4680)):
        assert f'conversation_snapshot_test: {count} checks passed' in report['raw_build_logs'][label + '-checks.log']
    assert built['source_sha256']['A/sycl/src/core/conversation_snapshot.cpp'] == \
           built['source_sha256']['B/sycl/src/core/conversation_snapshot.cpp']
    for name, value in built['source_sha256'].items():
        arm, path = name.split('/', 1)
        assert value == report['before_sha256' if arm == 'A' else 'source_sha256'][path]
    for runner in report['runners'].values():
        assert digest(runner['source']) == runner['sha256']
    assert report['runners']['model.py']['sha256'] == report['model']['runner_source_sha256']
    if 'recheck' in report:
        recheck = report['recheck']
        assert recheck['exit_code'] == 0
        assert digest(recheck['raw_log']) == recheck['log_sha256']
        assert 'conversation_snapshot_test: 4680 checks passed' in recheck['raw_log']
        assert recheck['exe_sha256'] == built['executable_sha256'][recheck['exe']]
    assert check_model(report)
    model = report['model']
    assert model['args'][model['args'].index('--spec') + 1] == '4'
    assert model['args'][model['args'].index('--spec-min-p') + 1] == '0.5'
    assert '--mtp-max-t' not in model['args']
    for arm in model['runs']:
        assert all(row['offered'] > 0 and 0 <= row['accepted'] <= row['offered'] for row in arm['rows'])
        assert len(arm['restore_ms']) == 6
    if 'reconfirmation' in report:
        prior_raw = Path(__file__).with_name('snapshot-target-restore-2026-10-31.json').read_bytes().decode('utf-8')
        assert digest(prior_raw) == report['reconfirmation']['original_sha256']
        prior = json.loads(prior_raw)
        for key in ('args', 'runtime_env', 'input_ids', 'runtime_contract', 'reference', 'files_sha256'):
            assert model[key] == prior['model'][key]
        for arm, old in zip(model['runs'], prior['model']['runs'], strict=True):
            assert arm['label'] == old['label']
            for row, old_row in zip([arm['warmup'], *arm['rows']], [old['warmup'], *old['rows']], strict=True):
                assert all(row[key] == old_row[key] for key in
                           ('tokens', 'generated', 'accepted', 'offered', 'reused', 'resume', 'finish'))
        assert report['http']['config'] == prior['http']['config']
        for row, old in zip(report['http']['rows'], prior['http']['rows'], strict=True):
            assert row['input_ids'] == old['input_ids'] and row['tokens'] == old['tokens']
    http = report.get('http')
    if http:
        assert http['complete'] and http['files_unchanged'] and http['exit_code'] == 0
        assert not http['production_changed'] and not http['performance_measurement']
        assert len(http['rows']) == 4 and len(http['restores']) >= 2
        assert [row['expected'] for row in http['rows']] == ['731', '946', '731', '946']
        assert [row['path'] for row in http['rows']] == [
            '/v1/chat/completions', '/v1/messages', '/v1/chat/completions', '/v1/responses']
        assert [row['stream'] for row in http['rows']] == [False, False, True, False]
        for name, raw in http['raw_logs'].items():
            assert digest(raw) == http['log_sha256'][name]
        for i, row in enumerate(http['rows']):
            assert row['answer'] == row['expected']
            raw = http['raw_logs'][str(i) + '.http']
            if row['stream']:
                events = [line[6:] for line in raw.splitlines() if line.startswith('data: ')]
                assert events[-1] == '[DONE]'
                chunks = [json.loads(event) for event in events[:-1]]
                assert chunks and all('error' not in chunk for chunk in chunks)
                content = ''.join(choice['delta'].get('content') or '' for chunk in chunks for choice in chunk['choices'])
            else:
                value = json.loads(raw)
                assert not value.get('error')
                if row['path'] == '/v1/messages':
                    content = ''.join(part.get('text', '') for part in value['content'] if part['type'] == 'text')
                elif row['path'] == '/v1/responses':
                    content = ''.join(part.get('text', '') for item in value['output'] if item['type'] == 'message'
                                      for part in item['content'] if part['type'] == 'output_text')
                else:
                    content = value['choices'][0]['message']['content']
            assert content.strip() == row['answer']
            assert len(row['input_ids']) >= 2000 and len(row['tokens']) == row['done']['generated'] > 0
            if i >= 2:
                assert row['tokens'] == http['rows'][i - 2]['tokens']
                assert row['input_ids'] == http['rows'][i - 2]['input_ids']
                assert row['done']['reused'] >= .9 * len(row['input_ids'])
        assert sum(row['done']['drafts_offered'] for row in http['rows']) > 0
        log = http['raw_logs']['engine.stderr']
        assert [list(pair) for pair in re.findall(
            r'conversation cache: restored (\d+) tokens \((\w+)\)', log)] == http['restores']
        assert all(kind == 'checkpoint' for _, kind in http['restores'])
        memory = http['raw_logs']['memory.stdout'].replace('\r\n', '\n')
        assert len(re.findall(r'^MEM_PROP index=0 error=0 location=1 physical_bytes=\d+$', memory, re.M)) == 1
        samples = [tuple(map(int, fields)) for fields in re.findall(
            r'^MEM index=0 error=(\d+) free_bytes=(\d+) size_bytes=(\d+) health=(\d+)$', memory, re.M)]
        assert samples and all(error == 0 and 0 <= free <= size and size > 0 for error, free, size, _ in samples)
        assert math.isclose(min(free for _, free, _, _ in samples) / 2**20, http['min_free_mib'])
        assert http['min_free_mib'] >= 1024
        cfg = http['config']
        assert cfg['exe'] in built['executable_sha256']
        for flag, value in {'--expert-cache': '4500', '--vram-reserve-mib': '3072', '--prompt-cache': '6',
                            '--conversation-cache-mib': '2048', '--conversation-cache-slots': '4',
                            '--adapt-swaps': '32', '--pcie-frac': '0.16', '--spec': '4', '--spec-min-p': '0.5'}.items():
            assert cfg['args'][cfg['args'].index(flag) + 1] == value
    return bool(http)


if __name__ == '__main__':
    path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).with_name('snapshot-target-restore-2026-10-31.json')
    report = json.loads(path.read_text(encoding='utf-8'))
    http_checked = check(report)
    print('PASS 4392/4680 snapshot checks; 5 arms,40 full requests,real MTP,raw logs,memory and timing decisions; no GPU')
    print('Optional-profile HTTP:', 'PASS 4 alternating requests and 2 checkpoint restores' if http_checked else 'not included')
    print('Confirmed TTFT scenarios:', ','.join(c['case'] for c in report['model']['comparisons']
          if c['metric'] == 'ttft_ms' and c['confirmed']) or 'none')
