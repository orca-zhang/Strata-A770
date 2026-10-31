"""Compare real Strata HTTP requests on SYCL and native Orca, under the GPU lease.

python tools/check_orca_http.py --config config.json --candidate-exe strata.exe \
    --images images --out build-http-check
Uses an ephemeral loopback port; does not change the production config.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import threading
import urllib.request

import bench_deepinfer as bench

sys.path.insert(0, str(bench.ROOT))
from serve.frontend import ChatTemplate
from serve.server import Server, Service, StrataEngine, make_handler
from strata_tokenizer import Tokenizer


class RecordingEngine(StrataEngine):
    def generate(self, ids, max_new, sampling, cancel, embeddings=None):
        row = dict(input_ids=list(ids), max_new=max_new, tokens=[])
        try:
            for token in super().generate(ids, max_new, sampling, cancel, embeddings):
                if token is not None:
                    row['tokens'].append(token)
                yield token
        finally:
            row['done'] = dict(self.last)
            self.records.append(row)


def audit(pid):
    if os.name != 'nt':
        paths = sorted({line.split()[-1] for line in Path(f'/proc/{pid}/maps').read_text().splitlines()
                        if len(line.split()) >= 6 and line.split()[-1].startswith('/')})
    else:
        code = f'@(Get-Process -Id {int(pid)} -ErrorAction Stop).Modules.FileName | ConvertTo-Json'
        result = subprocess.run(['powershell', '-NoProfile', '-Command', code],
                                capture_output=True, text=True, check=True, timeout=30)
        paths = json.loads(result.stdout)
        if isinstance(paths, str):
            paths = [paths]
    assert not any(any(s in Path(p).name.lower() for s in ('sycl', 'ur_adapter', 'ur_loader', 'mkl'))
                   for p in paths), 'native process loaded an excluded runtime'
    return [dict(path=p, sha256=bench.sha(p)) for p in paths if Path(p).is_file()]


def api_result(path, raw, stream):
    if stream:
        events = [line[6:] for line in raw.splitlines() if line.startswith('data: ')]
        assert events and events[-1] == '[DONE]'
        chunks = [json.loads(e) for e in events[:-1]]
        assert chunks and all('error' not in e for e in chunks)
        choices = [choice for e in chunks for choice in e['choices']]
        return dict(content=''.join(c['delta'].get('content') or '' for c in choices),
                    reasoning=''.join(c['delta'].get('reasoning_content') or '' for c in choices),
                    finish=[c['finish_reason'] for c in choices if c.get('finish_reason')])
    value = json.loads(raw)
    assert not value.get('error')
    if path == '/v1/chat/completions':
        return dict(choice=value['choices'][0], usage=value['usage'])
    if path == '/v1/messages':
        return {k: value[k] for k in ('content', 'stop_reason', 'usage')}
    return dict(output=[{k: v for k, v in item.items() if k != 'id'} for item in value['output']],
                status=value['status'], usage=value['usage'])


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--config', type=Path, required=True)
    ap.add_argument('--baseline-exe', type=Path, help='Defaults to the config executable')
    ap.add_argument('--candidate-exe', type=Path, required=True)
    ap.add_argument('--images', type=Path, required=True)
    ap.add_argument('--out', type=Path, required=True)
    ap.add_argument('--spec', type=int, choices=range(2, 9), default=4)
    ap.add_argument('--long-prefill', action='store_true', help='Also check repeated long code prompts')
    args = ap.parse_args()
    cfg = json.loads(args.config.read_text(encoding='utf-8-sig'))
    if any(x in cfg['args'] for x in ('--batch', '--parallel')):
        ap.error('use a single-sequence config for this exact request-history comparison')
    directory = Path(cfg['tokenizer'])
    vocab = json.loads((directory / 'vocab.json').read_text(encoding='utf-8'))
    tokens = [None] * len(vocab)
    for word, number in vocab.items():
        tokens[number] = word
    tokenizer = Tokenizer(tokens, (directory / 'merges.txt').read_text(encoding='utf-8').splitlines(),
                          json.loads((directory / 'token_type.json').read_text(encoding='utf-8')))
    args.out.mkdir(parents=True, exist_ok=False)
    report = dict(complete=False, performance_measurement=False, linux_gpu_validated=False,
                  full_model_stability_established=False, spec_capacity=args.spec,
                  config_sha256=bench.sha(args.config), runs=[])
    report['source_sha256'] = {name: bench.sha(bench.ROOT / name) for name in
                              ('tools/check_orca_http.py', 'serve/server.py', 'serve/frontend.py',
                               'serve/responses.py', 'sycl/orca_queue.hpp', 'sycl/orca/gemm.cpp')}
    save = lambda: bench.save(args.out / 'results.json', report)
    save()
    messages = [dict(role='user', content='Explain why the sky is blue in three sentences.')]
    common = dict(model=cfg['model_name'], temperature=0, max_tokens=64, messages=messages)
    cases = [('/v1/chat/completions', dict(common, reasoning_effort='none')),
             ('/v1/chat/completions', dict(common, reasoning_effort='none', stream=True)),
             ('/v1/messages', dict(common, thinking=dict(type='disabled'))),
             ('/v1/responses', dict(model=cfg['model_name'], temperature=0, max_output_tokens=64,
                                    input=messages, reasoning=dict(effort='none')))]
    if args.long_prefill:
        code = tokenizer.decode(bench.prompts(cfg)['Code2K'])
        long_body = dict(common, reasoning_effort='none', messages=[dict(role='user', content=code)])
        cases.extend([('/v1/chat/completions', long_body)] * 2)
    with bench.gpu_lock():
        report['image_sha256'] = {str(p.relative_to(args.images)): bench.sha(p)
                                  for p in sorted(args.images.rglob('*')) if p.is_file()}
        for native, executable in ((False, args.baseline_exe or Path(cfg['exe'])), (True, args.candidate_exe)):
            launch = bench.model_args(cfg, cache_mib=6000)
            launch[launch.index('--spec') + 1] = str(args.spec)
            launch.extend(['--suffix-draft', '0'])
            env = bench.environment(cfg)
            env.pop('STRATA_VERIFY_EAGER', None)
            env.update(STRATA_MTP_COST_WINDOW='0', STRATA_PREFILL_XMX_SHARED_GATE='1')
            if native:
                env = {k: v for k, v in env.items() if not k.startswith(('SYCL_', 'UR_', 'ONEAPI_'))}
                env.update(STRATA_ORCA_L0='1', STRATA_ORCA_IMAGES=str(args.images.resolve()),
                           STRATA_VERIFY_ZERO_DIAG='1')
            else:
                env['STRATA_PREFILL_NATIVE_XMX'] = '1'
            label = 'native' if native else 'sycl'
            run = dict(label=label, exe=str(executable.resolve()), exe_sha256=bench.sha(executable),
                       args=launch, rows=[])
            report['runs'].append(run)
            save()
            print('HTTP', label, 'loading', flush=True)
            engine = RecordingEngine(str(executable.resolve()), launch, cwd=str(bench.ROOT),
                                     env=env, log=str((args.out / f'{label}.stderr').resolve()))
            engine.records = []
            server = None
            try:
                if native:
                    run['modules_startup'] = audit(engine.proc.pid)
                service = Service(engine, tokenizer, ChatTemplate(directory / 'chat_template.jinja'),
                                  model_name=cfg['model_name'], serve_quiet=True)
                server = Server(('127.0.0.1', 0), make_handler(service))
                thread = threading.Thread(target=server.serve_forever, daemon=True)
                thread.start()
                base = f'http://127.0.0.1:{server.server_address[1]}'
                for path in ('/health', '/v1/models'):
                    with urllib.request.urlopen(base + path, timeout=30) as response:
                        value = json.load(response)
                    assert value.get('loaded') if path == '/health' else value['data']
                for number, (path, body) in enumerate(cases):
                    request = urllib.request.Request(base + path, data=json.dumps(body).encode(),
                                                     headers={'Content-Type': 'application/json'})
                    with urllib.request.urlopen(request, timeout=1200) as response:
                        raw = response.read().decode('utf-8')
                        assert response.status == 200
                    (args.out / f'{label}-{number}.http').write_text(raw, encoding='utf-8')
                    api_output = api_result(path, raw, body.get('stream'))
                    assert len(engine.records) == number + 1, 'missing/extra native request'
                    row = engine.records[-1]
                    run['rows'].append(dict(path=path, stream=bool(body.get('stream')),
                                           api_output=api_output, **row))
                    save()
                    assert row['tokens'] and any(row['tokens']), 'empty/all-zero output'
                    assert row['done']['generated'] == len(row['tokens'])
                    if args.long_prefill and number == 4:
                        assert len(row['input_ids']) > 2000 and row['done']['prompt_read'] > 2000
                    print('HTTP', label, number, path, len(row['tokens']), 'tokens', flush=True)
                if native:
                    run['modules_workload'] = audit(engine.proc.pid)
            finally:
                if server:
                    server.shutdown()
                    server.server_close()
                    thread.join(timeout=5)
                proc = engine.proc
                engine.close()
                run['exit_code'] = proc.returncode
                save()
            assert run['exit_code'] == 0
        for number, (a, b) in enumerate(zip(*(run['rows'] for run in report['runs']), strict=True)):
            for key in ('path', 'stream', 'input_ids', 'max_new', 'tokens', 'api_output'):
                assert a[key] == b[key], f'HTTP request {number}: {key} mismatch'
            for key in ('generated', 'finish', 'drafts_accepted', 'drafts_offered', 'reused'):
                assert a['done'][key] == b['done'][key], f'HTTP request {number}: {key} mismatch'
        report['complete'] = True
        report['linux_gpu_validated'] = sys.platform.startswith('linux')
        save()
    print('PASS exact HTTP tokens and draft workloads', flush=True)


if __name__ == '__main__':
    main()
